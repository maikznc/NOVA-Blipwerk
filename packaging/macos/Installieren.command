#!/bin/bash
# Nova Blipwerk – Installation für macOS
# Kopiert AU und VST3 in die Plug-in-Ordner deines Benutzers, entfernt das
# Quarantäne-Flag von macOS und lädt den Audio-Unit-Cache neu.
#
# Start im Terminal:   bash "/Pfad/zu/Nova Blipwerk/Installieren.command"

set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
COMP="$HOME/Library/Audio/Plug-Ins/Components"
VST3="$HOME/Library/Audio/Plug-Ins/VST3"

echo ""
echo "  NOVA · Blipwerk wird installiert"
echo "  --------------------------------"

if [ ! -d "$DIR/Nova Blipwerk.component" ] || [ ! -d "$DIR/Nova Blipwerk.vst3" ]; then
  echo "  Die Plugin-Dateien liegen nicht neben diesem Script."
  echo "  Bitte das Script im entpackten Ordner \"Nova Blipwerk\" starten."
  exit 1
fi

mkdir -p "$COMP" "$VST3"
rm -rf "$COMP/Nova Blipwerk.component" "$VST3/Nova Blipwerk.vst3"
cp -R "$DIR/Nova Blipwerk.component" "$COMP/"
cp -R "$DIR/Nova Blipwerk.vst3" "$VST3/"
echo "  ✓ Audio Unit  →  $COMP"
echo "  ✓ VST3        →  $VST3"

# Nicht notarisierte Plugins: Quarantäne-Flag entfernen, sonst blockiert macOS das Laden
xattr -dr com.apple.quarantine "$COMP/Nova Blipwerk.component" 2>/dev/null || true
xattr -dr com.apple.quarantine "$VST3/Nova Blipwerk.vst3" 2>/dev/null || true
echo "  ✓ Quarantäne-Flag entfernt"

if [ -d "$DIR/Nova Blipwerk.app" ]; then
  rm -rf "/Applications/Nova Blipwerk.app" 2>/dev/null || true
  if cp -R "$DIR/Nova Blipwerk.app" /Applications/ 2>/dev/null; then
    xattr -dr com.apple.quarantine "/Applications/Nova Blipwerk.app" 2>/dev/null || true
    echo "  ✓ Standalone-App  →  /Applications"
  else
    echo "  – Standalone-App nicht kopiert (keine Schreibrechte für /Applications)"
  fi
fi

# Audio-Unit-Cache neu laden und prüfen
killall -9 AudioComponentRegistrar 2>/dev/null || true
echo "  … Audio Unit wird geprüft (dauert ein paar Sekunden)"
if auval -v aumu Nblw Nova > /tmp/nova-blipwerk-auval.log 2>&1; then
  echo "  ✓ Audio Unit geprüft: OK"
else
  echo "  ! auval meldet ein Problem. Details: /tmp/nova-blipwerk-auval.log"
fi

echo ""
echo "  Fertig. DAW neu starten oder Plugins neu scannen."
echo "  Zu finden unter Hersteller \"Nova\", Name \"Nova Blipwerk\"."
echo ""
