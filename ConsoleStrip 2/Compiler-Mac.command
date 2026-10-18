#!/bin/bash
# Double-cliquez sur ce fichier pour compiler le plugin.
export PATH="$PATH:/opt/homebrew/bin:/usr/local/bin:/Applications/CMake.app/Contents/bin"
cd "$(dirname "$0")" || { echo "Impossible d'ouvrir le dossier du projet."; read -n 1 -p "Appuyez sur une touche..."; exit 1; }
LOG="$HOME/Desktop/ConsoleStrip-log.txt"
{
  echo "Dossier : $(pwd)"
  echo "macOS : $(sw_vers -productVersion) / $(uname -m)"
  echo "CMake : $(cmake --version 2>&1 | head -1)"
  echo "Outils Xcode : $(xcode-select -p 2>&1)"
  echo "Git : $(git --version 2>&1)"
} | tee "$LOG"
rm -rf build

fail() {
  echo ""
  echo "############ ERREUR a l'etape $1 ############"
  echo "Les dernieres lignes importantes :"
  grep -iE "error|fatal|not found|failed" "$LOG" | tail -15
  echo ""
  echo "Le journal complet est sur le Bureau : ConsoleStrip-log.txt"
  read -n 1 -p "Appuyez sur une touche pour fermer..."
  exit 1
}

echo "=== 1/3 Preparation (telechargement de JUCE, quelques minutes) ===" | tee -a "$LOG"
cmake -B build -DCMAKE_BUILD_TYPE=Release 2>&1 | tee -a "$LOG"
[ "${PIPESTATUS[0]}" -eq 0 ] || fail 1

echo "=== 2/3 Compilation (5 a 15 minutes) ===" | tee -a "$LOG"
cmake --build build --config Release -j2 2>&1 | tee -a "$LOG"
[ "${PIPESTATUS[0]}" -eq 0 ] || fail 2

echo "=== 3/3 Autorisation du plugin ===" | tee -a "$LOG"
for p in ~/Library/Audio/Plug-Ins/VST3/"Console Strip.vst3" ~/Library/Audio/Plug-Ins/Components/"Console Strip.component"; do
  if [ -e "$p" ]; then xattr -cr "$p"; codesign --force --deep -s - "$p"; echo "OK : $p"; fi
done
echo ""
echo "TERMINE. Relancez votre DAW et rescannez les plugins."
read -n 1 -p "Appuyez sur une touche pour fermer..."
