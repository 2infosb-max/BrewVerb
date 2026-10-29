/*
    BrewVerb - open-source convolution reverb
    Copyright (C) 2026 Karun Prasad / Sound Brewery Studio
    Licensed under the GNU GPL v3 (see LICENSE).
*/
#include "IRGenerator.h"

namespace brew
{
namespace
{
    constexpr float ln1000 = 6.907755f;   // ln(1000) -> 60 dB of decay

    enum class Pattern { Normal, Box, Flutter, Slap, Plate };

    struct SpaceDef
    {
        const char* name;
        const char* category;
        const char* description;
        Scene scene;
        float w, d, h;     // metres
        float rt60;        // seconds (mid band)
        float lowRatio;    // RT60 multiplier below ~250 Hz
        float highRatio;   // RT60 multiplier above ~3.5 kHz
        float erWindowMs;  // early-reflection window
        int   erTaps;      // number of early reflections
        float lateDelayMs; // where the diffuse tail starts
        float attackMs;    // diffuse build-up time
        float toneHz;      // overall low-pass of the space
        float earlyGain;   // early reflection level (linear)
        Pattern pattern;
    };

    // All spaces are synthesized from first principles (filtered noise with
    // frequency-dependent exponential decay + modelled early reflections).
    const SpaceDef spaces[] =
    {
        { "Vocal Booth",        "Post",    "Dead, tight booth. Adds air without a tail",                  Scene::Booth,      1.5f,   1.8f,  2.2f, 0.22f, 1.10f, 0.45f,   6.0f, 10,  3.0f,  2.0f,  8000.0f, 1.00f, Pattern::Box     },
        { "Living Room",        "Post",    "Soft furnished room. Natural for dialogue and ADR matching",  Scene::Living,     4.5f,   5.5f,  2.7f, 0.45f, 1.15f, 0.50f,  14.0f, 16,  6.0f,  5.0f,  9500.0f, 0.90f, Pattern::Normal  },
        { "Small Room",         "Rooms",   "Small untreated room with quick, close reflections",          Scene::Room,       3.0f,   3.5f,  2.6f, 0.55f, 1.10f, 0.60f,  18.0f, 18,  8.0f,  6.0f, 11000.0f, 0.90f, Pattern::Normal  },
        { "Studio Live Room",   "Studios", "Treated tracking room. Smooth and musical for vocals",        Scene::Studio,     7.0f,   9.0f,  4.0f, 0.80f, 1.10f, 0.60f,  30.0f, 24, 14.0f, 10.0f, 13000.0f, 0.80f, Pattern::Normal  },
        { "Office",             "Post",    "Hard-walled office. Useful for dialogue placement",           Scene::Office,     6.0f,   8.0f,  2.8f, 0.65f, 0.95f, 0.70f,  20.0f, 20, 10.0f,  6.0f, 11500.0f, 0.95f, Pattern::Normal  },
        { "Tiled Bathroom",     "Rooms",   "Bright, ringing tiled room",                                  Scene::Bathroom,   2.5f,   3.0f,  2.5f, 0.95f, 0.90f, 0.90f,  12.0f, 30,  6.0f,  4.0f, 14000.0f, 1.00f, Pattern::Normal  },
        { "Car Interior",       "Post",    "Very short, boxy cabin sound",                                Scene::Car,        1.4f,   2.0f,  1.2f, 0.09f, 1.30f, 0.35f,   4.0f, 12,  2.0f,  1.0f,  5500.0f, 1.20f, Pattern::Box     },
        { "Long Corridor",      "Post",    "Narrow corridor with flutter echo",                           Scene::Corridor,   2.0f,  30.0f,  3.0f, 1.10f, 1.10f, 0.60f, 120.0f, 14, 25.0f, 20.0f, 10000.0f, 0.80f, Pattern::Flutter },
        { "Wooden Chamber",     "Studios", "Warm echo chamber. Classic vocal reverb",                     Scene::Chamber,    5.0f,   7.0f,  4.0f, 1.30f, 1.20f, 0.55f,  40.0f, 28, 20.0f, 15.0f,  9000.0f, 0.70f, Pattern::Normal  },
        { "Concrete Stairwell", "Rooms",   "Hard, diffuse stairwell",                                     Scene::Stairwell,  4.0f,   5.0f, 15.0f, 2.30f, 1.00f, 0.65f,  60.0f, 30, 18.0f, 25.0f, 11000.0f, 0.70f, Pattern::Normal  },
        { "Vintage Plate",      "Gear",    "Dense, bright plate. Instant build-up, no early reflections", Scene::Plate,      2.0f,   1.0f,  0.0f, 2.00f, 0.85f, 0.90f,   0.0f,  0,  0.5f,  1.0f, 13000.0f, 0.00f, Pattern::Plate   },
        { "Concert Hall",       "Halls",   "Large hall with warm low end",                                Scene::Hall,      30.0f,  45.0f, 18.0f, 2.20f, 1.30f, 0.60f,  80.0f, 36, 28.0f, 40.0f, 10000.0f, 0.55f, Pattern::Normal  },
        { "Small Church",       "Churches","Stone church. Long and dark",                                 Scene::Church,    12.0f,  25.0f, 12.0f, 2.60f, 1.25f, 0.50f,  70.0f, 32, 30.0f, 35.0f,  9000.0f, 0.60f, Pattern::Normal  },
        { "Warehouse",          "Rooms",   "Big empty industrial space",                                  Scene::Warehouse, 40.0f,  60.0f, 12.0f, 3.20f, 1.10f, 0.55f, 100.0f, 30, 40.0f, 50.0f,  9000.0f, 0.60f, Pattern::Normal  },
        { "Cathedral",          "Churches","Huge, slow-building cathedral",                               Scene::Cathedral, 30.0f, 100.0f, 35.0f, 6.00f, 1.35f, 0.45f, 120.0f, 40, 50.0f, 80.0f,  8000.0f, 0.45f, Pattern::Normal  },
        { "Outdoor Courtyard",  "Outdoor", "Open air with building slap-backs",                           Scene::Courtyard, 20.0f,  25.0f,  0.0f, 0.70f, 0.80f, 0.50f, 180.0f,  4, 30.0f, 30.0f,  9000.0f, 1.00f, Pattern::Slap    },
    };

    constexpr int numSpaces = (int) (sizeof (spaces) / sizeof (spaces[0]));

    struct OnePole
    {
        float a = 0.0f, z = 0.0f;
        void setCutoff (float hz, double sr) { a = std::exp (-juce::MathConstants<float>::twoPi * hz / (float) sr); }
        float lp (float x) noexcept { z = x + a * (z - x); return z; }
    };

    struct TwoPoleLP
    {
        OnePole p1, p2;
        void setCutoff (float hz, double sr) { p1.setCutoff (hz, sr); p2.setCutoff (hz, sr); }
        float process (float x) noexcept { return p2.lp (p1.lp (x)); }
    };

    void writePulse (juce::AudioBuffer<float>& ir, int ch, int pos, float gain)
    {
        static const float kernel[] = { 1.0f, 0.55f, 0.22f, 0.08f };
        for (int k = 0; k < 4; ++k)
        {
            const int i = pos + k;
            if (i >= 0 && i < ir.getNumSamples())
                ir.addSample (ch, i, gain * kernel[k]);
        }
    }

    void panGains (float pan, float& l, float& r)
    {
        const float a = (juce::jlimit (-1.0f, 1.0f, pan) + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        l = std::cos (a) * 1.41421356f;
        r = std::sin (a) * 1.41421356f;
    }

    void applyFilters (juce::AudioBuffer<float>& ir, double sr, float lowpassHz, float highpassHz)
    {
        const float nyq = (float) sr * 0.45f;
        auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sr, juce::jlimit (200.0f, nyq, lowpassHz));
        auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, juce::jlimit (10.0f, 2000.0f, highpassHz));

        for (int ch = 0; ch < ir.getNumChannels(); ++ch)
        {
            juce::dsp::IIR::Filter<float> f1 (lp), f2 (hp);
            auto* d = ir.getWritePointer (ch);
            for (int i = 0; i < ir.getNumSamples(); ++i)
                d[i] = f2.processSample (f1.processSample (d[i]));
        }
    }

    int findOnset (const juce::AudioBuffer<float>& ir)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < ir.getNumChannels(); ++ch)
            peak = juce::jmax (peak, ir.getMagnitude (ch, 0, ir.getNumSamples()));

        const float threshold = peak * 0.01f; // -40 dB
        for (int i = 0; i < ir.getNumSamples(); ++i)
            for (int ch = 0; ch < ir.getNumChannels(); ++ch)
                if (std::abs (ir.getSample (ch, i)) >= threshold)
                    return i;
        return 0;
    }

    /** Attack: slow fade-in of the reverb onset (0 = untouched). */
    void applyAttack (juce::AudioBuffer<float>& ir, double sr, float attack, float rt, int onset)
    {
        if (attack <= 0.001f) return;
        const float rampSec = juce::jmax (0.005f, attack * 0.45f * rt);
        const int rampLen = (int) (rampSec * sr);
        for (int ch = 0; ch < ir.getNumChannels(); ++ch)
        {
            auto* d = ir.getWritePointer (ch);
            for (int i = 0; i < rampLen && onset + i < ir.getNumSamples(); ++i)
                d[onset + i] *= std::pow ((float) i / (float) rampLen, 1.5f);
        }
    }

    void finalise (juce::AudioBuffer<float>& ir, const ShapeSettings& s)
    {
        const double sr = s.sampleRate;
        const int maxLen = juce::jmax (64, (int) (s.lengthSec * sr));

        if (ir.getNumSamples() > maxLen)
            ir.setSize (ir.getNumChannels(), maxLen, true, false, false);

        const int n = ir.getNumSamples();

        // Fade the end: 15% of the length (min 10 ms) so a shortened tail
        // dies smoothly rather than being chopped.
        const int fadeLen = juce::jlimit (1, n, juce::jmax ((int) (0.01 * sr), (int) ((float) n * 0.15f)));
        for (int ch = 0; ch < ir.getNumChannels(); ++ch)
        {
            auto* d = ir.getWritePointer (ch);
            for (int i = 0; i < fadeLen; ++i)
            {
                const float x = (float) i / (float) fadeLen;
                const float g = std::cos (x * juce::MathConstants<float>::halfPi);
                d[n - fadeLen + i] *= g * g;
            }
        }

        if (s.reverse)
            for (int ch = 0; ch < ir.getNumChannels(); ++ch)
                std::reverse (ir.getWritePointer (ch), ir.getWritePointer (ch) + n);
    }

    // Source-position model shared by built-in and user IRs
    float distanceEarlyGain (float dist) { return juce::jmap (dist, 1.35f, 0.45f); }
    float distanceTailGain  (float dist) { return juce::jmap (dist, 0.85f, 1.20f); }
    void  balanceGains (float posX, float dist, float amount, float& l, float& r)
    {
        const float k = amount * (1.0f - 0.5f * dist);
        l = 1.0f - k * juce::jmax (0.0f, posX);
        r = 1.0f - k * juce::jmax (0.0f, -posX);
    }
}

//==============================================================================
const juce::StringArray& getSpaceNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray a;
        for (auto& s : spaces) a.add (s.name);
        return a;
    }();
    return names;
}

juce::String getSpaceDescription (int index)
{
    return juce::isPositiveAndBelow (index, numSpaces) ? juce::String (spaces[index].description) : juce::String();
}

SpaceInfo getSpaceInfo (int index)
{
    const auto& d = spaces[juce::jlimit (0, numSpaces - 1, index)];
    return { d.name, d.description, d.category, d.scene, d.w, d.d, d.h, d.rt60 };
}

int getNumSpaces() { return numSpaces; }

//==============================================================================
juce::AudioBuffer<float> generateSpace (const ShapeSettings& s)
{
    const auto& d  = spaces[juce::jlimit (0, numSpaces - 1, s.space)];
    const double sr = s.sampleRate;
    const float size = juce::jlimit (0.25f, 4.0f, s.size);
    const float dist = juce::jlimit (0.0f, 1.0f, s.posDist);
    const float posX = juce::jlimit (-1.0f, 1.0f, s.posX);

    const float rt     = juce::jmax (0.03f, d.rt60 * s.decay * std::sqrt (size));
    const float rtLow  = rt * d.lowRatio;
    const float rtMid  = rt;
    const float rtHigh = juce::jmax (0.02f, rt * d.highRatio * juce::jmap (s.damping, 1.0f, 0.2f));

    const float lateStart = d.lateDelayMs * 0.001f * size * juce::jmap (dist, 1.0f, 0.6f);
    const float attack    = juce::jmax (0.0005f, d.attackMs * 0.001f * size);
    const float erWindow  = d.erWindowMs * 0.001f * size;

    const float longest = juce::jmax (rtLow, rtMid, rtHigh);
    const float lenSec  = juce::jmin (s.lengthSec + 0.05f, 20.0f,
                                      juce::jmax (lateStart, erWindow) + longest * 1.05f + 0.05f);
    const int n = juce::jmax (64, (int) (lenSec * sr));

    juce::AudioBuffer<float> ir (2, n);
    ir.clear();

    // ---- Diffuse late tail: 3-band noise with per-band decay ----------------
    const float kL = -ln1000 / rtLow;
    const float kM = -ln1000 / rtMid;
    const float kH = -ln1000 / rtHigh;
    const int lateStartSample = (int) (lateStart * sr);

    float tailBal[2];
    balanceGains (posX, dist, 0.25f, tailBal[0], tailBal[1]);
    const float tailGain = juce::Decibels::decibelsToGain (s.tailDb, -60.0f) * distanceTailGain (dist);

    for (int ch = 0; ch < 2; ++ch)
    {
        juce::Random rng (0x5B4E + s.space * 7919 + ch * 104729);
        TwoPoleLP lowSplit, highSplit;
        lowSplit.setCutoff (250.0f, sr);
        highSplit.setCutoff (3500.0f, sr);

        auto* out = ir.getWritePointer (ch);
        const float g = tailGain * tailBal[ch];

        for (int i = 0; i < n; ++i)
        {
            const float noise = rng.nextFloat() * 2.0f - 1.0f;
            const float lo   = lowSplit.process (noise);
            const float lp35 = highSplit.process (noise);
            const float hi   = noise - lp35;
            const float mid  = lp35 - lo;

            if (i < lateStartSample)
                continue;

            const float t = (float) (i - lateStartSample) / (float) sr;
            const float onset = 1.0f - std::exp (-t / attack);
            out[i] = g * onset * (1.6f * lo * std::exp (kL * t)
                                       + mid * std::exp (kM * t)
                                + 0.85f * hi * std::exp (kH * t));
        }
    }

    // ---- Early reflections ------------------------------------------------
    const float erLevel = d.earlyGain * juce::Decibels::decibelsToGain (s.earlyDb, -60.0f)
                          * distanceEarlyGain (dist) * 0.03f * std::sqrt ((float) sr);

    if (d.pattern != Pattern::Plate && erLevel > 0.0f)
    {
        juce::Random rng (0xE4 + s.space * 31);
        const float panBias = posX * 0.8f * (1.0f - 0.4f * dist);
        const float itd = posX * 0.0006f;   // inter-channel delay towards the source side

        auto addTap = [&] (float timeSec, float gain, float pan)
        {
            float l, r;
            panGains (pan * 0.75f + panBias, l, r);
            writePulse (ir, 0, (int) ((timeSec + juce::jmax (0.0f,  itd)) * sr), gain * l);
            writePulse (ir, 1, (int) ((timeSec + juce::jmax (0.0f, -itd)) * sr), gain * r);
        };

        const float nearScale = juce::jmap (dist, 1.0f, 0.35f);   // far source: reflections follow the direct sound sooner

        switch (d.pattern)
        {
            case Pattern::Normal:
            case Pattern::Box:
            {
                const float first = erWindow * (d.pattern == Pattern::Box ? 0.05f : 0.08f) * nearScale;
                for (int k = 0; k < d.erTaps; ++k)
                {
                    const float u = ((float) k + rng.nextFloat()) / (float) d.erTaps;
                    const float t = first + (erWindow - first) * std::sqrt (u);   // denser later, like a real room
                    const float g = std::exp (-3.0f * t / juce::jmax (rt, 0.05f)) * (0.55f + 0.45f * rng.nextFloat())
                                    * (rng.nextBool() ? 1.0f : -1.0f);
                    addTap (t, g * erLevel, rng.nextFloat() * 1.8f - 0.9f);
                }
                break;
            }
            case Pattern::Flutter:
            {
                const float period = erWindow / (float) juce::jmax (1, d.erTaps);
                for (int k = 0; k < d.erTaps; ++k)
                    addTap (0.004f * size * nearScale + (float) k * period, std::pow (0.82f, (float) k) * erLevel,
                            (k % 2 == 0) ? -0.6f : 0.6f);
                break;
            }
            case Pattern::Slap:
            {
                const float times[] = { 0.045f, 0.095f, 0.150f, 0.210f };
                const float gains[] = { 0.80f, 0.55f, 0.40f, 0.25f };
                const float pans[]  = { -0.7f, 0.6f, -0.3f, 0.8f };
                for (int k = 0; k < 4; ++k)
                    addTap (times[k] * size, gains[k] * erLevel, pans[k]);
                for (int k = 0; k < 8; ++k)   // ground / near-wall reflections
                    addTap ((0.002f + 0.012f * rng.nextFloat()) * nearScale, 0.3f * erLevel * (rng.nextBool() ? 1.0f : -1.0f),
                            rng.nextFloat() * 1.6f - 0.8f);
                break;
            }
            case Pattern::Plate: break;
        }
    }

    applyFilters (ir, sr, d.toneHz, d.pattern == Pattern::Plate ? 120.0f : 45.0f);
    applyAttack (ir, sr, s.attack, rt, findOnset (ir));
    finalise (ir, s);
    return ir;
}

//==============================================================================
float estimateRT60 (const juce::AudioBuffer<float>& ir, double sampleRate)
{
    const int n = ir.getNumSamples();
    if (n < 16) return 0.1f;

    // Schroeder backward integration of the summed energy
    std::vector<double> edc ((size_t) n);
    double acc = 0.0;
    for (int i = n - 1; i >= 0; --i)
    {
        double e = 0.0;
        for (int ch = 0; ch < ir.getNumChannels(); ++ch)
            e += (double) ir.getSample (ch, i) * ir.getSample (ch, i);
        acc += e;
        edc[(size_t) i] = acc;
    }
    if (acc <= 0.0) return 0.1f;

    auto timeAtDb = [&] (double db) -> int
    {
        const double target = acc * std::pow (10.0, db / 10.0);
        for (int i = 0; i < n; ++i)
            if (edc[(size_t) i] <= target) return i;
        return n - 1;
    };

    const int t5  = timeAtDb (-5.0);
    const int t25 = timeAtDb (-25.0);
    const double t20 = (double) (t25 - t5) / sampleRate;
    return (float) juce::jlimit (0.05, 30.0, t20 * 3.0);
}

//==============================================================================
juce::AudioBuffer<float> shapeUserIR (const juce::AudioBuffer<float>& source, double sourceRate, const ShapeSettings& s)
{
    const double sr = s.sampleRate;
    const int srcCh = source.getNumChannels();
    const int srcN  = source.getNumSamples();

    if (srcCh == 0 || srcN == 0)
        return generateSpace (s);

    // ---- Fold to stereo (true-stereo 4-channel IRs: L = LL+RL, R = LR+RR) ---
    juce::AudioBuffer<float> st (2, srcN);
    if (srcCh >= 4)
    {
        st.copyFrom (0, 0, source, 0, 0, srcN);  st.addFrom (0, 0, source, 2, 0, srcN);
        st.copyFrom (1, 0, source, 1, 0, srcN);  st.addFrom (1, 0, source, 3, 0, srcN);
        st.applyGain (0.5f);
    }
    else
    {
        st.copyFrom (0, 0, source, 0, 0, srcN);
        st.copyFrom (1, 0, source, juce::jmin (1, srcCh - 1), 0, srcN);
    }

    // ---- Resample to host rate and stretch by Size ------------------------
    const double step = (sourceRate / sr) / (double) juce::jlimit (0.25f, 4.0f, s.size);
    const int outN = juce::jlimit (64, (int) (20.0 * sr), (int) ((double) srcN / step));
    juce::AudioBuffer<float> ir (2, outN);

    for (int ch = 0; ch < 2; ++ch)
    {
        const float* in = st.getReadPointer (ch);
        float* out = ir.getWritePointer (ch);
        for (int i = 0; i < outN; ++i)
        {
            const double pos = i * step;
            const int i0 = (int) pos;
            const float frac = (float) (pos - i0);
            const float a = i0 < srcN ? in[i0] : 0.0f;
            const float b = i0 + 1 < srcN ? in[i0 + 1] : 0.0f;
            out[i] = a + frac * (b - a);
        }
    }

    const float rt = estimateRT60 (ir, sr);
    const int onset = findOnset (ir);

    // ---- Decay (RT60 multiplier) and Brightness (extra HF decay) ----------
    const float kDecay = ln1000 * (1.0f / rt - 1.0f / (rt * juce::jlimit (0.1f, 4.0f, s.decay)));
    const float kDamp  = -s.damping * ln1000 / juce::jmax (0.05f, rt * 0.5f);

    for (int ch = 0; ch < 2; ++ch)
    {
        TwoPoleLP split;
        split.setCutoff (3000.0f, sr);
        float* d = ir.getWritePointer (ch);

        for (int i = 0; i < outN; ++i)
        {
            const float t = (float) juce::jmax (0, i - onset) / (float) sr;
            const float x = d[i];
            const float low = split.process (x);
            const float high = x - low;
            const float env = juce::jmin (8.0f, std::exp (kDecay * t));
            d[i] = env * (low + high * std::exp (kDamp * t));
        }
    }

    // ---- Levels + source position: early window (first 50 ms x size) vs tail
    {
        const float dist = juce::jlimit (0.0f, 1.0f, s.posDist);
        float ebL, ebR, tbL, tbR;
        balanceGains (s.posX, dist, 0.45f, ebL, ebR);
        balanceGains (s.posX, dist, 0.15f, tbL, tbR);
        const float eg = juce::Decibels::decibelsToGain (s.earlyDb, -60.0f) * distanceEarlyGain (dist);
        const float tg = juce::Decibels::decibelsToGain (s.tailDb,  -60.0f) * distanceTailGain (dist);
        const float early[2] = { eg * ebL, eg * ebR };
        const float tail[2]  = { tg * tbL, tg * tbR };

        const int erLen = (int) (0.05 * sr * s.size);
        const int xfade = juce::jmax (1, (int) (0.02 * sr));
        for (int ch = 0; ch < 2; ++ch)
        {
            float* d = ir.getWritePointer (ch);
            for (int i = onset; i < outN; ++i)
            {
                const int rel = i - onset;
                const float w = rel < erLen ? 1.0f : juce::jmax (0.0f, 1.0f - (float) (rel - erLen) / (float) xfade);
                d[i] *= tail[ch] + (early[ch] - tail[ch]) * w;
            }
        }
    }

    applyAttack (ir, sr, s.attack, rt * s.decay, onset);
    finalise (ir, s);
    return ir;
}

} // namespace brew
