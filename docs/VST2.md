# Adding the VST (VST2) version

BrewVerb always builds **AU** and **VST3**, plus **AAX** for Pro Tools. The older **VST (VST2)** format
needs one extra ingredient: Steinberg's **VST2 SDK**.

Steinberg stopped issuing VST2 licences in 2018 and does not allow the SDK to be shared, so it can't be
included in BrewVerb. If you (or Sound Brewery) already have a copy from before then, BrewVerb builds VST
automatically as soon as the SDK is in the project. No settings need changing.

> Most DAWs today use VST3, including Nuendo, Cubase, Reaper, Studio One, Ableton Live, FL Studio,
> Bitwig and Resolve. VST2 is only needed for older hosts.

## Steps (GitHub website only, no Terminal)

1. **Make your repository private.** The VST2 SDK licence doesn't allow it to be shared publicly.
   In your BrewVerb repository: **Settings** → scroll to **Danger Zone** → **Change visibility** → **Make private**.
   (Private repositories get a monthly allowance of free build minutes, which covers several builds a month.)
2. Find your VST2 SDK folder on your Mac. It contains a folder called **pluginterfaces**, and inside that,
   **vst2.x** with the files `aeffect.h` and `aeffectx.h`.
3. In the repository, click **Add file** → **Upload files**.
4. Drag in the SDK folder, then rename it on the page so the path reads **`vst2sdk/pluginterfaces/vst2.x/...`**.
   A folder called `vst2sdk/VST2_SDK/...` or `vst2sdk/vstsdk2.4/...` works too.
5. Click **Commit changes**. This starts a new build automatically. You can also start one from **Actions**
   → **Build macOS installer** → **Run workflow**.
6. When the run is green, open it. The summary at the top lists the formats built (`aax au vst3 vst app`).
   Download **BrewVerb-macOS-installer** (the .pkg) or **BrewVerb-macOS-plugins** (the plug-in files).

The installer then has a **VST (VST2 hosts)** checkbox, which installs to `/Library/Audio/Plug-Ins/VST`.
