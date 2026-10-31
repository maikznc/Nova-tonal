#!/bin/bash
# Signiert die gebauten Plugins (ad hoc) und packt sie als Installer (.pkg) und ZIP nach dist/.
# Läuft auf macOS (GitHub Actions oder lokal nach dem Bauen).
set -euo pipefail

cd "$(dirname "$0")/.."

VERSION="$(sed -nE 's/^project \(NOVA VERSION ([0-9.]+).*/\1/p' CMakeLists.txt)"
NAME="NOVA Leitstern"
FILE_NAME="NOVA-Leitstern-${VERSION}"
ART="build/plugins/TonalBalance/TonalBalance_artefacts/Release"
VST3="$ART/VST3/$NAME.vst3"
AU="$ART/AU/$NAME.component"

for bundle in "$VST3" "$AU"; do
    [ -d "$bundle" ] || { echo "Fehlt: $bundle (erst bauen)"; exit 1; }
done

rm -rf dist pkgroot pkgscripts component.plist
mkdir -p dist

# Ad-hoc-Signatur – ohne Signatur laden Apple-Silicon-Macs die Plugins nicht
codesign --force --deep --sign - "$VST3"
codesign --force --deep --sign - "$AU"

# ---- Installer: legt die Plugins in die System-Plugin-Ordner
mkdir -p "pkgroot/Library/Audio/Plug-Ins/VST3" "pkgroot/Library/Audio/Plug-Ins/Components"
ditto "$VST3" "pkgroot/Library/Audio/Plug-Ins/VST3/$NAME.vst3"
ditto "$AU"   "pkgroot/Library/Audio/Plug-Ins/Components/$NAME.component"

# Bundles immer an diesen Ort installieren (nicht in eine gefundene ältere Kopie "umziehen").
# plutil -replace legt den Schalter an, falls pkgbuild ihn nicht eingetragen hat.
pkgbuild --analyze --root pkgroot component.plist
i=0
while /usr/libexec/PlistBuddy -c "Print :$i" component.plist >/dev/null 2>&1; do
    plutil -replace "$i.BundleIsRelocatable" -bool NO component.plist \
        || echo "Hinweis: Eintrag $i konnte nicht angepasst werden (unkritisch)"
    i=$((i + 1))
done
plutil -p component.plist

# Nach der Installation den AU-Cache auffrischen, damit Logic/Ableton das AU sofort sehen
mkdir -p pkgscripts
cat > pkgscripts/postinstall <<'EOF'
#!/bin/sh
killall -9 AudioComponentRegistrar 2>/dev/null || true
exit 0
EOF
chmod +x pkgscripts/postinstall

pkgbuild --root pkgroot \
         --component-plist component.plist \
         --scripts pkgscripts \
         --identifier com.nova.leitstern \
         --version "$VERSION" \
         --install-location / \
         "dist/${FILE_NAME}-Installer.pkg"

# ---- ZIP zum manuellen Kopieren
mkdir -p "zip/${FILE_NAME}"
ditto "$VST3" "zip/${FILE_NAME}/$NAME.vst3"
ditto "$AU"   "zip/${FILE_NAME}/$NAME.component"
cp packaging/INSTALLATION.txt "zip/${FILE_NAME}/"
ditto -c -k --keepParent "zip/${FILE_NAME}" "dist/${FILE_NAME}-Plugins.zip"
cp packaging/INSTALLATION.txt dist/

rm -rf pkgroot pkgscripts component.plist zip
ls -la dist
