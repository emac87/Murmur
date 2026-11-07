#!/bin/bash
# Double-click this file in Finder to build and install the Murmur plugin.
cd "$(dirname "$0")"
LOG="$(pwd)/build-log.txt"
exec > >(tee "$LOG") 2>&1
export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"

echo "=== Murmur build ($(date)) ==="
sw_vers; uname -m

# make future Terminal windows find Homebrew too
if [ -x /opt/homebrew/bin/brew ] && ! grep -q "brew shellenv" ~/.zprofile 2>/dev/null; then
  echo 'eval "$(/opt/homebrew/bin/brew shellenv zsh)"' >> ~/.zprofile
fi

if ! command -v cmake >/dev/null; then
  echo "--> Installing CMake with Homebrew..."
  brew install cmake || { echo "CMake install failed."; read -p "Press Return to close"; exit 1; }
fi
cmake --version | head -1

if ! xcode-select -p >/dev/null 2>&1; then
  echo "--> Xcode command line tools are missing. Run: xcode-select --install"
  read -p "Press Return to close"; exit 1
fi
echo "Developer dir: $(xcode-select -p)"

if ! xcrun clang --version >/dev/null 2>&1; then
  echo "--> Xcode needs its license accepted. Enter your Mac password below (nothing shows while typing)."
  sudo xcodebuild -license accept
fi

echo "--> Configuring..."
rm -rf build
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release || { echo; echo "CONFIGURE FAILED - tell Claude to read build-log.txt"; read -p "Press Return to close"; exit 1; }

echo "--> Building (5-10 minutes the first time)..."
cmake --build build --config Release -j "$(sysctl -n hw.ncpu)" || { echo; echo "BUILD FAILED - tell Claude to read build-log.txt"; read -p "Press Return to close"; exit 1; }

echo "--> Registering the Audio Unit..."
killall -9 AudioComponentRegistrar 2>/dev/null
sleep 2
auval -v aufx Mm2m Mrmu | tail -n 4

APP="$(find build -name Murmur.app -maxdepth 6 | head -1)"
if [ -n "$APP" ]; then rm -rf /Applications/Murmur.app 2>/dev/null; cp -R "$APP" /Applications/ 2>/dev/null && echo "Standalone app copied to /Applications/Murmur.app"; fi

echo
echo "=== DONE ==="
ls -d ~/Library/Audio/Plug-Ins/Components/Murmur.component ~/Library/Audio/Plug-Ins/VST3/Murmur.vst3 2>&1
echo "Quit and reopen Logic, then follow LOGIC-SETUP.md."
read -p "Press Return to close"
