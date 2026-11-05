/*
    BrewVerb - open-source convolution reverb
    Copyright (C) 2026 Karun Prasad / Sound Brewery Studio
    Licensed under the GNU GPL v3 (see LICENSE).
*/
#include "IRLibrary.h"

namespace brew
{
namespace
{
    const char* const audioPattern = "*.wav;*.aif;*.aiff;*.flac";

    bool isImage (const juce::File& f)
    {
        return f.hasFileExtension ("jpg;jpeg;png");
    }

    juce::File siblingPicture (const juce::File& ir)
    {
        const auto dir = ir.getParentDirectory();
        for (auto ext : { ".jpg", ".jpeg", ".png" })
        {
            auto f = dir.getChildFile (ir.getFileNameWithoutExtension() + ext);
            if (f.existsAsFile()) return f;
        }
        for (auto n : { "picture.jpg", "picture.png" })
            if (dir.getChildFile (n).existsAsFile())
                return dir.getChildFile (n);
        // folder named after the space with "<Space>.png" inside (as in the Sound Brewery IR Pack)
        for (auto ext : { ".png", ".jpg" })
        {
            auto f = dir.getChildFile (dir.getFileName() + ext);
            if (f.existsAsFile()) return f;
        }
        // exactly one image in the folder
        auto images = dir.findChildFiles (juce::File::findFiles, false, "*.jpg;*.jpeg;*.png");
        return images.size() == 1 ? images.getFirst() : juce::File();
    }
}

IRLibrary& IRLibrary::get()
{
    static IRLibrary instance;
    return instance;
}

juce::Array<juce::File> IRLibrary::getLibraryFolders()
{
    const juce::String sub = "Sound Brewery/BrewVerb/IR Library";
    juce::Array<juce::File> folders;
#if JUCE_MAC
    folders.add (juce::File ("/Library/Application Support").getChildFile (sub));
    folders.add (juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                    .getChildFile ("Library/Application Support").getChildFile (sub));
#else
    folders.add (juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory).getChildFile (sub));
    folders.add (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile (sub));
#endif
    // optional override for testing / custom locations
    if (auto env = juce::SystemStats::getEnvironmentVariable ("BREWVERB_IR_LIBRARY", {}); env.isNotEmpty())
        folders.insert (0, juce::File (env));
    return folders;
}

void IRLibrary::rescan()
{
    juce::Array<LibraryEntry> found;
    for (auto& root : getLibraryFolders())
        if (root.isDirectory())
            scanFolder (root, found);

    const juce::ScopedLock sl (lock);
    entries = std::move (found);
    scanned = true;
}

void IRLibrary::ensureScanned() const
{
    {
        const juce::ScopedLock sl (lock);
        if (scanned) return;
    }
    const_cast<IRLibrary*> (this)->rescan();
}

void IRLibrary::scanFolder (const juce::File& root, juce::Array<LibraryEntry>& out) const
{
    const auto index = root.getChildFile ("library.json");
    if (index.existsAsFile())
    {
        const auto json = juce::JSON::parse (index);
        if (auto* list = json.getProperty ("entries", {}).getArray())
        {
            for (auto& v : *list)
            {
                LibraryEntry e;
                e.name        = v.getProperty ("name", {}).toString();
                e.position    = v.getProperty ("position", {}).toString();
                e.category    = v.getProperty ("category", "Library").toString();
                e.description = v.getProperty ("description", {}).toString();
                e.file        = root.getChildFile (v.getProperty ("file", {}).toString());
                const auto pic = v.getProperty ("picture", {}).toString();
                e.picture     = pic.isNotEmpty() ? root.getChildFile (pic) : siblingPicture (e.file);
                e.rt60        = (float) (double) v.getProperty ("rt60", 0.0);
                if (auto* sz = v.getProperty ("size", {}).getArray(); sz != nullptr && sz->size() == 3)
                {
                    e.width  = (float) (double) (*sz)[0];
                    e.depth  = (float) (double) (*sz)[1];
                    e.height = (float) (double) (*sz)[2];
                }
                if (e.name.isEmpty()) e.name = e.file.getFileNameWithoutExtension();
                if (e.file.existsAsFile())
                    out.add (e);
            }
        }
        return;
    }

    // No index: every audio file in the tree becomes an entry
    auto files = root.findChildFiles (juce::File::findFiles, true, audioPattern);
    files.sort();
    for (auto& f : files)
    {
        LibraryEntry e;
        e.name = f.getFileNameWithoutExtension();
        const auto rel = f.getRelativePathFrom (root);
        e.category = rel.containsChar (juce::File::getSeparatorChar())
                       ? rel.upToFirstOccurrenceOf (juce::File::getSeparatorString(), false, false)
                       : root.getFileName();
        e.file = f;
        e.picture = siblingPicture (f);
        out.add (e);
    }
}

int IRLibrary::getNumEntries() const
{
    ensureScanned();
    const juce::ScopedLock sl (lock);
    return entries.size();
}

LibraryEntry IRLibrary::getEntry (int index) const
{
    ensureScanned();
    const juce::ScopedLock sl (lock);
    return juce::isPositiveAndBelow (index, entries.size()) ? entries.getReference (index) : LibraryEntry {};
}

juce::StringArray IRLibrary::getCategories() const
{
    ensureScanned();
    const juce::ScopedLock sl (lock);
    juce::StringArray cats;
    for (auto& e : entries)
        cats.addIfNotAlreadyThere (e.category);
    return cats;
}

int IRLibrary::indexOf (const juce::File& irFile) const
{
    if (irFile == juce::File()) return -1;
    ensureScanned();
    const juce::ScopedLock sl (lock);
    for (int i = 0; i < entries.size(); ++i)
        if (entries.getReference (i).file == irFile)
            return i;
    return -1;
}

juce::File IRLibrary::findPictureFor (const juce::File& irFile) const
{
    const int i = indexOf (irFile);
    if (i >= 0)
    {
        auto pic = getEntry (i).picture;
        if (pic.existsAsFile()) return pic;
    }
    return irFile.existsAsFile() ? siblingPicture (irFile) : juce::File();
}

juce::Image IRLibrary::getPicture (const juce::File& picture)
{
    if (! picture.existsAsFile() || ! isImage (picture))
        return {};
    const auto key = picture.getFullPathName();
    if (auto it = pictureCache.find (key); it != pictureCache.end())
        return it->second;
    auto img = juce::ImageFileFormat::loadFrom (picture);
    if (pictureCache.size() > 24) pictureCache.clear();
    pictureCache[key] = img;
    return img;
}
}
