#!/bin/bash
# Builds, from a finished CMake Release build:
#   BrewVerb-<version>-macOS.pkg          installer with a checkbox per format
#   BrewVerb-<version>-macOS-plugins.zip  plain plug-in bundles for drag-and-drop install
# Usage: packaging/make_pkg.sh <build-dir> <version>
set -euo pipefail

BUILD_DIR="${1:-build}"
VERSION="${2:-1.1.0}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ART="$(find "$BUILD_DIR" -type d -path '*BrewVerb_artefacts/Release' | head -1)"
OUT="$BUILD_DIR/installer"
STAGE="$OUT/stage"
rm -rf "$OUT"; mkdir -p "$STAGE" "$OUT/plugins"

echo "Artefacts: $ART"

#        id    bundle                          install folder                                       installer title
FORMATS=(
  "aax|AAX/BrewVerb.aaxplugin|/Library/Application Support/Avid/Audio/Plug-Ins|AAX (Pro Tools)"
  "au|AU/BrewVerb.component|/Library/Audio/Plug-Ins/Components|Audio Unit (Logic, Final Cut, GarageBand)"
  "vst3|VST3/BrewVerb.vst3|/Library/Audio/Plug-Ins/VST3|VST3 (Nuendo, Cubase, Reaper, Resolve)"
  "vst|VST/BrewVerb.vst|/Library/Audio/Plug-Ins/VST|VST (VST2 hosts)"
  "app|Standalone/BrewVerb.app|/Applications|Standalone app"
)

OUTLINE=""; CHOICES=""; REFS=""; BUILT=""
for entry in "${FORMATS[@]}"; do
    IFS='|' read -r id bundle dest title <<< "$entry"
    src="$ART/$bundle"
    if [ ! -d "$src" ]; then
        echo "  - $title: not built, skipped"
        continue
    fi
    name="$(basename "$src")"
    root="$STAGE/$id/root$dest"
    mkdir -p "$root"
    ditto "$src" "$root/$name"
    # Ad-hoc signature so macOS / Apple Silicon will load the bundle.
    # (AAX additionally needs PACE signing for retail Pro Tools; see docs/PRO_TOOLS_SIGNING.md)
    codesign --force --deep --sign - "$root/$name" || true

    pkgbuild --root "$STAGE/$id/root" --identifier "com.soundbrewery.brewverb.$id" \
             --version "$VERSION" --install-location "/" "$OUT/brewverb-$id.pkg"

    selected="true"; [ "$id" = "app" ] && selected="false"
    OUTLINE+="        <line choice=\"$id\"/>"$'\n'
    CHOICES+="    <choice id=\"$id\" title=\"$title\" start_selected=\"$selected\"><pkg-ref id=\"com.soundbrewery.brewverb.$id\"/></choice>"$'\n'
    REFS+="    <pkg-ref id=\"com.soundbrewery.brewverb.$id\" version=\"$VERSION\">brewverb-$id.pkg</pkg-ref>"$'\n'
    BUILT+="$id "

    # plug-in bundles (not the app) also go into the drag-and-drop zip
    if [ "$id" != "app" ]; then
        mkdir -p "$OUT/plugins/BrewVerb $VERSION/$(basename "$(dirname "$bundle")")"
        ditto "$root/$name" "$OUT/plugins/BrewVerb $VERSION/$(basename "$(dirname "$bundle")")/$name"
    fi
done

echo "Formats packaged: $BUILT"

{
    while IFS= read -r line; do
        case "$line" in
            *@CHOICES_OUTLINE@*) printf '%s' "$OUTLINE" ;;
            *@CHOICES@*)         printf '%s' "$CHOICES" ;;
            *@PKGREFS@*)         printf '%s' "$REFS" ;;
            *)                   printf '%s\n' "${line//@VERSION@/$VERSION}" ;;
        esac
    done < "$HERE/distribution.xml"
} > "$OUT/distribution.xml"

productbuild --distribution "$OUT/distribution.xml" --resources "$HERE/resources" \
             --package-path "$OUT" "$OUT/BrewVerb-$VERSION-macOS.pkg"

# Optional: sign the installer with a Developer ID Installer certificate
if [ -n "${INSTALLER_SIGN_ID:-}" ]; then
    productsign --sign "$INSTALLER_SIGN_ID" "$OUT/BrewVerb-$VERSION-macOS.pkg" "$OUT/BrewVerb-$VERSION-macOS-signed.pkg"
    mv "$OUT/BrewVerb-$VERSION-macOS-signed.pkg" "$OUT/BrewVerb-$VERSION-macOS.pkg"
fi

cp "$HERE/resources/HOW_TO_INSTALL.txt" "$OUT/plugins/BrewVerb $VERSION/"
( cd "$OUT/plugins" && ditto -c -k --sequesterRsrc --keepParent "BrewVerb $VERSION" "../BrewVerb-$VERSION-macOS-plugins.zip" )

echo "Installer: $OUT/BrewVerb-$VERSION-macOS.pkg"
echo "Plug-in zip: $OUT/BrewVerb-$VERSION-macOS-plugins.zip"
