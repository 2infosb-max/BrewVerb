# Making BrewVerb load in retail Pro Tools (AAX signing)

Pro Tools refuses AAX plug-ins without a PACE "Eden" digital signature. Every commercial AAX plug-in
(including Soundly's PlaceIt and Audio Ease's Altiverb) is signed this way. Signing is free for developers.

## Route A: test right away with Pro Tools Developer
1. Create a free account at **developer.avid.com** and join the AAX developer program.
2. Download **Pro Tools Developer** from your Avid developer account. It loads unsigned AAX plug-ins,
   including BrewVerb from the installer.

## Route B: sign BrewVerb for normal Pro Tools
1. In your Avid developer account, request the **PACE Eden signing tools**. Avid connects you with PACE, which
   issues them at no cost to AAX developers.
2. PACE gives you **wraptool** and a **wrap configuration (WCGUID)** tied to your iLok account.
3. A signing identity from an **Apple Developer ID** certificate is also normally required (Apple Developer Program).
4. Sign the AAX built by the GitHub action. A developer can do this for you if you prefer not to use Terminal:

   ```
   wraptool sign --verbose --account <iLok-account> --wcguid <WCGUID> \
       --signid "Developer ID Application: <Your Name> (<TEAMID>)" \
       --in  BrewVerb.aaxplugin --out BrewVerb.aaxplugin
   ```
5. Rebuild the `.pkg` with the signed AAX (`packaging/make_pkg.sh`), or copy the signed
   `BrewVerb.aaxplugin` into `/Library/Application Support/Avid/Audio/Plug-Ins`.

Once you have PACE credentials, the signing step can be added to the GitHub workflow on a
self-hosted Mac runner, so every build comes out signed.
