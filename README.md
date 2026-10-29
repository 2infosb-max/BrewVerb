# BrewVerb

**Open-source convolution reverb and space designer** in **AU**, **VST3**, **VST** and **AAX** (Pro Tools) formats.
By Karun Prasad / Sound Brewery Studio, Kochi. Licensed under GPLv3.

BrewVerb is an independent, from-scratch plug-in in the spirit of Altiverb and PlaceIt. No code or
impulse responses from either product are used: the 16 built-in spaces are synthesized from
first principles, and you can load any IR library you own.

## Features

The layout follows the familiar Altiverb-style workflow. The artwork, branding and code are all BrewVerb's own.

| Area | What it does |
|---|---|
| **Big Decay knob** | RT60 from x0.25 to x3 of the selected space |
| **Space picture** | Original illustration of every space with the source and listener drawn in. Click the name to browse by category, use < > to step through spaces, and read the size and RT60 underneath |
| **Stage pad** | Drag the source left/right and near/far. This changes early/tail balance, reflection timing and stereo image |
| **16 built-in spaces** | Post: Vocal Booth, Living Room, Office, Car Interior, Long Corridor. Rooms: Small Room, Tiled Bathroom, Concrete Stairwell, Warehouse. Studios: Studio Live Room, Wooden Chamber. Halls: Concert Hall. Churches: Small Church, Cathedral. Gear: Vintage Plate. Outdoor: Courtyard |
| **Your own IRs** | WAV / AIFF / FLAC, mono, stereo or 4-channel true-stereo, any sample rate. **LOAD IR** or drag the file onto the picture. Every control reshapes them too |
| **HOME tab** | Brightness, Bass, Treble, Pre-delay, Attack, EQ On |
| **COLOR tab** | Low Cut, High Cut, Bass, Treble, Brightness, EQ On |
| **TIME tab** | IR decay display, Size, Length, Attack, Pre-delay, Reverse |
| **LEVELS tab** | Early Reflections, Tail, Distance, Left/Right, Dry/Wet, Output Gain |
| **SNAPSHOTS tab** | Four slots (A to D) store and recall every setting, including the loaded IR, and save with the session |
| **Main controls** | Output Gain, Spread (stereo width 0 to 200%), Dry/Wet (equal-power) |
| **Routing** | Mono→Mono, Mono→Stereo (classic vocal insert), Stereo→Stereo |
| **Session recall** | All settings, snapshots and the IR file path are saved. Every control can be automated |

An amber dot on a tab means something on it differs from the default.

Shaping changes rebuild the IR on a background thread and swap it in without glitches. The convolution engine
has zero latency, so Pro Tools needs no delay compensation.

## Getting the installer (.pkg)

macOS plug-ins have to be compiled on a Mac. This project includes a **GitHub Actions** recipe that does it
for you in the cloud. No Terminal required; see **[docs/INSTALL.md](docs/INSTALL.md)**.

Every build produces two downloads. Both are universal, so they run on Apple Silicon and Intel Macs:

| Download | What's inside |
|---|---|
| `BrewVerb-1.1.0-macOS.pkg` | Installer with a checkbox per format: AAX, AU, VST3, VST, Standalone |
| `BrewVerb-1.1.0-macOS-plugins.zip` | The plug-in files themselves (`.component`, `.vst3`, `.vst`, `.aaxplugin`) plus a how-to, for drag-and-drop installs |

**Formats:** AU, VST3 and AAX are always built. **VST (VST2)** is built when you add Steinberg's VST2 SDK,
which can't be shared publicly; see **[docs/VST2.md](docs/VST2.md)**.

> **Pro Tools note:** retail Pro Tools only loads AAX plug-ins that carry a PACE/iLok signature.
> See **[docs/PRO_TOOLS_SIGNING.md](docs/PRO_TOOLS_SIGNING.md)** for the free signing route.
> AU and VST3 work immediately in Logic, Nuendo, Cubase, Reaper and DaVinci Resolve.

## Building yourself (developers)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
bash packaging/make_pkg.sh build 1.1.0      # macOS only
# optional VST2: put the SDK in ./vst2sdk or add -DBREWVERB_VST2_SDK=/path/to/sdk
```

JUCE 8.0.4 is downloaded automatically (it bundles the AAX SDK under its GPLv3 option).
Tests: configure with `-DBREWVERB_BUILD_TESTS=ON` and run `BrewVerbIRTest <out-dir>`. The test tool renders every
space to WAV, checks RT60, routing, latency, levels and state recall.

## Project layout

```
Source/IRGenerator.*      space synthesis + user-IR reshaping (size/decay/damping/length/reverse)
Source/PluginProcessor.*  parameters, pre-delay, convolution, filters, width, mix, state
Source/PluginEditor.*     UI (big Decay, space picture, stage pad, tabs, snapshots)
Source/SpaceArt.*         original vector illustrations of each space
packaging/                .pkg installer (pkgbuild + productbuild)
.github/workflows/        one-click cloud build
tests/                    offline render + DSP tests
```
