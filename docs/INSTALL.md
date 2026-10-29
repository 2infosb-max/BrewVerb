# Getting the BrewVerb installer: no Terminal needed

A Mac plug-in can only be compiled on a Mac. GitHub provides free Macs in the cloud ("Actions") that
build the `.pkg` for you. You only click buttons in your web browser.

## 1. Put the project on GitHub (one time)

1. Sign in at **github.com** (create a free account if needed).
2. Click the **+** at the top right → **New repository**.
3. Name it `BrewVerb`, choose **Public** (Actions minutes are unlimited for public repos), and click **Create repository**.
4. On the empty repository page, click **uploading an existing file**.
5. Unzip `BrewVerb-source.zip` on your Mac. Open the `BrewVerb` folder, select **everything inside it**
   (including the hidden `.github` folder: press **Cmd + Shift + .** in Finder to show hidden files), and drag it all onto the page.
6. Click **Commit changes**.

## 2. Build the installer

1. In your repository, click the **Actions** tab.
2. If asked, click **I understand my workflows, go ahead and enable them**.
3. Click **Build macOS installer** in the left list → **Run workflow** → green **Run workflow** button.
4. Wait about 10 to 15 minutes until the run shows a green tick.
5. Click the finished run. The summary at the top lists the formats built. Under **Artifacts** there are two downloads:
   - **BrewVerb-macOS-installer**: the installer (**BrewVerb-1.1.0-macOS.pkg**), the easiest option.
   - **BrewVerb-VST3-macOS**: only **BrewVerb.vst3**. Unzip it and drag BrewVerb.vst3 into
     **Macintosh HD → Library → Audio → Plug-Ins → VST3** (in Finder: **Go → Go to Folder…**, type `/Library/Audio/Plug-Ins/VST3`, press Return).
   - **BrewVerb-macOS-plugins**: the plug-in files (AU, VST3, VST, AAX) to copy by hand. A how-to file is included.
6. Double-click the downloaded zip to unpack it.

To also get the **VST (VST2)** version, see [VST2.md](VST2.md).

## 3. Install

1. Double-click the `.pkg`. If macOS says it "cannot be opened because it is from an unidentified developer":
   open **System Settings → Privacy & Security**, scroll down, and click **Open Anyway** next to the BrewVerb message.
2. Click through the installer. On **Installation Type** click **Customize** to choose AAX / AU / VST3 / VST / Standalone.
3. Restart your DAW. BrewVerb is listed under **Reverb** (Pro Tools) or under **Sound Brewery** (Logic and others).

## 4. Pro Tools

Retail Pro Tools only loads PACE-signed AAX plug-ins. Until BrewVerb is signed, use one of these routes:

- **Pro Tools Developer** (free for registered Avid developers) loads unsigned AAX plug-ins.
- Sign the AAX once with Avid's free PACE tools. See [PRO_TOOLS_SIGNING.md](PRO_TOOLS_SIGNING.md).

The AU and VST3 versions work immediately in Logic, Nuendo, Cubase, Reaper and Resolve.

## Using BrewVerb on vocals

- **On an aux (send):** set **Mix** to 100%, then use the send level for the amount.
- **As an insert:** start at around 15 to 30% Mix.
- **Placing a voice in a room:** drag the dot on the **Stage** pad. Moving it further away gives more room and less early reflection, and moving it sideways shifts the image.
- **A/B-ing settings:** on the **SNAPSHOTS** tab, turn on **STORE** and click A, then try something else and store it in B. Click A or B to compare.
- **Placing ADR or dialogue into a location:** start from *Living Room*, *Office* or *Car Interior*. Raise **Early Ref.**,
  keep **Length** short, and use **Low Cut** around 150 Hz.
- **Your own IR library:** drag any WAV/AIFF IR onto the display. Size, Decay, Damping, Length and Reverse reshape it as well.
