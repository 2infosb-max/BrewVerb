// Offline test: renders every built-in space to WAV, checks RT60 / sanity,
// then pushes an impulse and a vocal-like burst through the full processor.
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "IRGenerator.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SpaceArt.h"

static bool writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, (unsigned) b.getNumChannels(), 24, {}, 0));
    if (w == nullptr) return false;
    os.release();
    return w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? argv[1] : "ir_renders");
    outDir.createDirectory();
    int failures = 0;

    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        for (int i = 0; i < brew::getNumSpaces(); ++i)
        {
            brew::ShapeSettings s; s.space = i; s.sampleRate = sr;
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            auto ir = brew::generateSpace (s);
            const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
            bool finite = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int n = 0; n < ir.getNumSamples(); ++n)
                    if (! std::isfinite (ir.getSample (ch, n))) finite = false;
            const float rt = brew::estimateRT60 (ir, sr);
            if (! finite || ir.getMagnitude (0, ir.getNumSamples()) <= 0.0f) { ++failures; std::printf ("FAIL "); }
            if (sr == 48000.0)
            {
                std::printf ("%-20s len %5.2fs  RT60~%5.2fs  build %6.1f ms\n",
                             brew::getSpaceNames()[i].toRawUTF8(), ir.getNumSamples() / sr, rt, ms);
                auto norm = ir; norm.applyGain (0.9f / juce::jmax (norm.getMagnitude (0, norm.getNumSamples()), 1e-9f));
                writeWav (outDir.getChildFile (juce::String (i).paddedLeft ('0', 2) + "_" + brew::getSpaceNames()[i].replace (" ", "_") + ".wav"), norm, sr);
            }
        }
    }

    // Shaping a "user" IR: decay/size/reverse/length changes
    {
        brew::ShapeSettings s; s.space = 11; s.sampleRate = 48000.0;
        auto src = brew::generateSpace (s);
        const float base = brew::estimateRT60 (src, 48000.0);
        for (float decay : { 0.5f, 1.0f, 2.0f })
        {
            brew::ShapeSettings u = s; u.decay = decay; u.damping = 0.0f;
            auto shaped = brew::shapeUserIR (src, 44100.0, u);
            std::printf ("user IR decay x%.1f -> RT60 %.2fs (source %.2fs @44.1k->48k)\n", decay,
                         brew::estimateRT60 (shaped, 48000.0), base);
        }
        brew::ShapeSettings r = s; r.reverse = true; r.lengthSec = 1.0f;
        auto rev = brew::shapeUserIR (src, 48000.0, r);
        std::printf ("reverse+1s length -> %.2fs, last-sample |%.4f| first |%.4f|\n",
                     rev.getNumSamples() / 48000.0, std::abs (rev.getSample (0, rev.getNumSamples() - 1)), std::abs (rev.getSample (0, 0)));
        writeWav (outDir.getChildFile ("user_reverse.wav"), rev, 48000.0);
    }

    // Full processor: mono->stereo and stereo->stereo, 100% wet impulse
    for (int inCh : { 1, 2 })
    {
        BrewVerbProcessor p;
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (inCh == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        layout.outputBuses.add (juce::AudioChannelSet::stereo());
        if (! p.setBusesLayout (layout)) { std::printf ("FAIL layout %d\n", inCh); ++failures; continue; }

        p.apvts.getParameter (ParamID::mix)->setValueNotifyingHost (1.0f);
        p.apvts.getParameter (ParamID::space)->setValueNotifyingHost (p.apvts.getParameter (ParamID::space)->convertTo0to1 (8.0f)); // Wooden Chamber
        p.apvts.getParameter (ParamID::predelay)->setValueNotifyingHost (p.apvts.getParameter (ParamID::predelay)->convertTo0to1 (20.0f));
        p.setRateAndBufferSizeDetails (48000.0, 512);
        p.prepareToPlay (48000.0, 512);

        const int total = 48000 * 3;
        juce::AudioBuffer<float> out (2, total);
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> block (2, 512);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        for (int pos = 0; pos < total; pos += 512)
        {
            block.clear();
            if (pos == 0) { block.setSample (0, 0, 1.0f); if (inCh == 2) block.setSample (1, 0, 1.0f); }
            p.processBlock (block, midi);
            const int n = juce::jmin (512, total - pos);
            for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, block, ch, 0, n);
        }
        const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;

        int firstNonZero = -1;
        for (int n = 0; n < total && firstNonZero < 0; ++n)
            if (std::abs (out.getSample (0, n)) > 1e-6f) firstNonZero = n;
        const float peak = out.getMagnitude (0, total);
        const double corr = [&] { double lr = 0, ll = 0, rr = 0;
            for (int n = 0; n < total; ++n) { double l = out.getSample (0, n), r = out.getSample (1, n); lr += l*r; ll += l*l; rr += r*r; }
            return lr / std::sqrt (ll * rr + 1e-20); }();
        std::printf ("processor %s->stereo: onset %.1f ms (20 ms pre-delay + first reflection), peak %.3f, L/R corr %.2f, 3s rendered in %.0f ms, RT60 %.2fs\n",
                     inCh == 1 ? "mono" : "stereo", firstNonZero / 48.0, peak, corr, ms, brew::estimateRT60 (out, 48000.0));
        if (firstNonZero < 960 || firstNonZero > 1440 || peak <= 0.0f || ! std::isfinite (peak)) ++failures;
        writeWav (outDir.getChildFile (juce::String ("processor_") + (inCh == 1 ? "mono" : "stereo") + ".wav"), out, 48000.0);

        // loudness: white noise in, 100% wet out (expect about -6 dB)
        {
            juce::Random rng (7);
            double inE = 0, outE = 0;
            for (int pos = 0; pos < 48000 * 4; pos += 512)
            {
                for (int ch = 0; ch < 2; ++ch)
                    for (int n = 0; n < 512; ++n) block.setSample (ch, n, (rng.nextFloat() * 2.0f - 1.0f) * 0.25f);
                if (pos > 48000 * 2) for (int n = 0; n < 512; ++n) inE += block.getSample (0, n) * block.getSample (0, n);
                p.processBlock (block, midi);
                if (pos > 48000 * 2) for (int n = 0; n < 512; ++n) outE += block.getSample (0, n) * block.getSample (0, n);
            }
            const double db = 10.0 * std::log10 (outE / inE);
            std::printf ("   wet level vs dry (noise): %+.1f dB\n", db);
            if (db < -10.0 || db > -2.0) ++failures;
        }

        // state round-trip
        juce::MemoryBlock mb; p.getStateInformation (mb);
        BrewVerbProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        if (q.apvts.getRawParameterValue (ParamID::space)->load() != 8.0f) { std::printf ("FAIL state\n"); ++failures; }
    }

    // New shaping controls: source position, attack, tail level
    {
        auto energy = [] (const juce::AudioBuffer<float>& b, int ch, int from, int to)
        { double e = 0; for (int i = from; i < juce::jmin (to, b.getNumSamples()); ++i) e += b.getSample (ch, i) * b.getSample (ch, i); return e; };

        brew::ShapeSettings s; s.space = 3; s.sampleRate = 48000.0;
        s.posX = 0.9f; s.posDist = 0.0f;
        auto right = brew::generateSpace (s);
        const double balDb = 10.0 * std::log10 (energy (right, 1, 0, 2400) / energy (right, 0, 0, 2400));
        std::printf ("source hard right: early R vs L %+.1f dB (expect > +3)\n", balDb);
        if (balDb < 3.0) ++failures;

        s.posX = 0.0f;
        s.posDist = 0.0f; auto nearIR = brew::generateSpace (s);
        s.posDist = 1.0f; auto farIR  = brew::generateSpace (s);
        const double nearRatio = energy (nearIR, 0, 0, 2400) / energy (nearIR, 0, 2400, 96000);
        const double farRatio  = energy (farIR, 0, 0, 2400)  / energy (farIR, 0, 2400, 96000);
        std::printf ("early/tail ratio near %.2f vs far %.2f (expect near > far)\n", nearRatio, farRatio);
        if (nearRatio <= farRatio) ++failures;

        s.posDist = 0.3f; s.attack = 1.0f; auto slow = brew::generateSpace (s);
        s.attack = 0.0f; auto fast = brew::generateSpace (s);
        std::printf ("attack 100%%: first 100 ms energy %.1f dB vs attack 0\n",
                     10.0 * std::log10 (energy (slow, 0, 0, 4800) / energy (fast, 0, 0, 4800)));
        if (energy (slow, 0, 0, 4800) >= energy (fast, 0, 0, 4800)) ++failures;

        s.tailDb = -12.0f; auto lowTail = brew::generateSpace (s);
        const double tailDrop = 10.0 * std::log10 (energy (lowTail, 0, 9600, 48000) / energy (fast, 0, 9600, 48000));
        std::printf ("tail -12 dB -> measured %+.1f dB\n", tailDrop);
        if (std::abs (tailDrop + 12.0) > 1.0) ++failures;
    }

    // Snapshots store / recall
    {
        BrewVerbProcessor p;
        auto* decayP = p.apvts.getParameter (ParamID::decay);
        decayP->setValueNotifyingHost (decayP->convertTo0to1 (180.0f));
        p.storeSnapshot (1);
        decayP->setValueNotifyingHost (decayP->convertTo0to1 (60.0f));
        p.recallSnapshot (1);
        const float v = p.apvts.getRawParameterValue (ParamID::decay)->load();
        juce::MemoryBlock mb; p.getStateInformation (mb);
        BrewVerbProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        std::printf ("snapshot recall decay = %.0f (expect 180), survives save: %s\n", v, q.hasSnapshot (1) ? "yes" : "no");
        if (std::abs (v - 180.0f) > 1.0f || ! q.hasSnapshot (1)) ++failures;
    }

    // UI snapshots: every tab, plus a contact sheet of every space illustration
    if (juce::SystemStats::getEnvironmentVariable ("BREWVERB_UI_SNAPSHOT", {}).isNotEmpty())
    {
        auto savePng = [&] (const juce::Image& img, const juce::String& name)
        {
            juce::PNGImageFormat png; auto f = outDir.getChildFile (name); f.deleteFile();
            if (auto os = f.createOutputStream()) png.writeImageToStream (img, *os);
        };

        BrewVerbProcessor p;
        p.setRateAndBufferSizeDetails (48000.0, 512);
        auto setP = [&p] (const char* id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); };
        setP (ParamID::space, 11.0f);   // Concert Hall
        setP (ParamID::posx, -35.0f);
        setP (ParamID::posdist, 55.0f);
        setP (ParamID::predelay, 18.0f);
        setP (ParamID::bass, -2.0f);
        p.prepareToPlay (48000.0, 512);
        p.storeSnapshot (0);
        p.storeSnapshot (2);

        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto* bed = dynamic_cast<BrewVerbEditor*> (ed.get());
        ed->setVisible (true);
        bed->refreshIndicators();
        const char* tabs[] = { "home", "color", "time", "levels", "snapshots" };
        for (int t = 0; t < 5; ++t)
        {
            bed->showTab (t);
            savePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f), juce::String ("ui_") + tabs[t] + ".png");
        }

        const int cols = 4, cw = 472, chh = 324;
        juce::Image sheet (juce::Image::RGB, cols * cw, 4 * chh, true);
        juce::Graphics g (sheet);
        for (int i = 0; i < brew::getNumSpaces(); ++i)
        {
            const auto area = juce::Rectangle<float> ((float) ((i % cols) * cw), (float) ((i / cols) * chh), (float) cw, (float) chh).reduced (4.0f);
            {
                juce::Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (area.toNearestInt());
                brewui::drawSpaceArt (g, area, brew::getSpaceInfo (i), 1.0f, 0.2f, 0.4f);
            }
            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
            g.drawText (brew::getSpaceNames()[i], area.reduced (10.0f).removeFromBottom (24.0f), juce::Justification::centredLeft);
        }
        savePng (sheet, "spaces_sheet.png");
    }

    std::printf (failures == 0 ? "ALL TESTS PASSED\n" : "%d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}
