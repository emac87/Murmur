#!/bin/bash
# Double-click in Finder to build dist/Murmur-<version>-mac.pkg (takes 10-15 minutes).
cd "$(dirname "$0")/../.."
LOG="$(pwd)/installer-log.txt"
exec > >(tee "$LOG") 2>&1
echo "=== Murmur Mac installer build ($(date)) ==="
if bash installer/mac/make-pkg.sh; then
  echo
  echo "=== DONE ==="
  echo "Your installer is in the 'dist' folder."
  echo "Note: if you also built Murmur before with 'Build Murmur.command', this Mac has a copy in"
  echo "~/Library/Audio/Plug-Ins too. Before installing the .pkg on THIS Mac, remove those copies"
  echo "or Logic will list Murmur twice."
  open dist 2>/dev/null
else
  echo
  echo "INSTALLER BUILD FAILED - tell Claude to read installer-log.txt"
fi
read -p "Press Return to close"
