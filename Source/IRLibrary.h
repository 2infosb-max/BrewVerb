/*
    BrewVerb - open-source convolution reverb
    Copyright (C) 2026 Karun Prasad / Sound Brewery Studio
    Licensed under the GNU GPL v3 (see LICENSE).

    IRLibrary: the bundled Sound Brewery IR Library (impulse responses with a
    picture of each space), installed by the .pkg into
        /Library/Application Support/Sound Brewery/BrewVerb/IR Library
    plus anything you add to the same folder under your own user Library.

    Each library folder holds a library.json index. Folders without one are
    still scanned: every WAV/AIFF/FLAC becomes an entry, its parent folder is
    the category, and a picture next to it (same name, "picture.jpg", or any
    single image in the folder) is shown in the plug-in.
*/
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace brew
{
    struct LibraryEntry
    {
        juce::String name;          // space name, e.g. "Kerala Church"
        juce::String position;      // "Near" / "Far" / "" when there is only one
        juce::String category;
        juce::String description;
        juce::File   file;          // the impulse response
        juce::File   picture;       // may not exist
        float rt60 = 0.0f;          // mid-band, seconds (0 = unknown)
        float width = 0.0f, depth = 0.0f, height = 0.0f;   // metres (0 = open air / unknown)

        juce::String displayName() const { return position.isEmpty() ? name : name + " - " + position; }
    };

    class IRLibrary
    {
    public:
        static IRLibrary& get();

        /** Re-reads every library folder. Called once automatically on first use. */
        void rescan();

        int getNumEntries() const;
        LibraryEntry getEntry (int index) const;
        juce::StringArray getCategories() const;

        /** Index of the entry using this IR file, or -1. */
        int indexOf (const juce::File& irFile) const;

        /** Picture for any IR file: its library picture, or an image sitting next to it. */
        juce::File findPictureFor (const juce::File& irFile) const;

        /** Cached, decoded picture (empty Image if none). Message thread only. */
        juce::Image getPicture (const juce::File& picture);

        static juce::Array<juce::File> getLibraryFolders();

    private:
        IRLibrary() = default;
        void ensureScanned() const;
        void scanFolder (const juce::File& root, juce::Array<LibraryEntry>& out) const;

        mutable juce::CriticalSection lock;
        mutable bool scanned = false;
        mutable juce::Array<LibraryEntry> entries;
        std::map<juce::String, juce::Image> pictureCache;
    };
}
