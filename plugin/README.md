# Murmur

Murmur converts live monophonic audio into MIDI notes. Sing, play a guitar or bass, and it sends the notes it hears to your MIDI destinations.

## What it is

- Live audio to MIDI conversion for a single pitched source at a time.
- Modes for **Voice**, **Guitar**, and **Bass**, each tuned to that source's pitch range.
- Two MIDI outputs:
  - **Plugin MIDI output** for VST3 hosts such as Ableton Live, Reaper, and Bitwig.
  - A virtual CoreMIDI port named **Murmur Out** that any app on the Mac can receive. This is how Logic Pro gets the notes, because Logic ignores MIDI output from audio-effect plugins.
- Formats: Audio Unit (AU), VST3, and Standalone.

## Requirements

- macOS 11 or later. The build is a universal binary (Apple Silicon and Intel).
- Xcode, or the Xcode Command Line Tools (`xcode-select --install`).
- CMake 3.22 or later (`brew install cmake`, or a download from cmake.org).
- Internet access on the first build, so CMake can fetch JUCE 8.0.4. If you already have a JUCE 8 checkout, point CMake at it with `-DJUCE_DIR=/path/to/JUCE` or place it at `../juce` or `./JUCE`.

## Build

```sh
./build-mac.sh
```

The script configures the project, builds the Release configuration, installs the AU and VST3 bundles, refreshes the AudioComponent registry, and runs `auval`. The Standalone app is left in `build/`. Copy it to `/Applications` to install it.

To run the engine tests, configure with `-DMURMUR_BUILD_TESTS=ON`.

## Parameters

| Parameter | What it does |
|---|---|
| Mode | Voice, Guitar, or Bass. Sets the pitch range and tracking behaviour for the source. |
| Gate | Input level threshold. Below it, Murmur sends no notes. |
| Response | Fast, Balanced, or Stable. Trades how quickly a new note is recognised against how steady the output is. |
| Channel | MIDI channel for the output notes. |
| Transpose | Shifts the output notes by a number of semitones. |
| Velocity | Dynamic follows input loudness. Fixed sends a constant velocity. |
| Pitch bend | Sends pitch bend when the input drifts within a note. |
| Bend range | The bend range, in semitones, that pitch bend messages assume. |
| Audio thru | Passes the input audio through to the plugin output. |
| Virtual port | Turns the **Murmur Out** CoreMIDI port on or off. |
| Panic | Sends all-notes-off on every output. Use it if notes get stuck. |

## Tips

- Murmur tracks one note at a time. Chords and polyphonic input will not convert cleanly.
- Feed it from a direct interface input, not the laptop microphone. The mic picks up room noise and tracks poorly.
- Use a buffer of 64 to 128 samples for low latency.
- For bass, try Transpose +12. Many synths respond better in that register.

## Troubleshooting

- **Plugin does not show up in Logic:** Open Logic Pro > Settings > Plug-in Manager and click Reset & Rescan Selection. You can also run `auval -a` in Terminal to confirm the Audio Unit is registered.
- **"Murmur Out" is not visible to other apps:** The plugin instance must be loaded on a track, and the Virtual port parameter must be on.
- **Notes get stuck:** Press Panic.
- **Notes are doubled in Logic:** The instrument track must not also receive the same notes from another route. Murmur Out should be the only MIDI source for that track.

## Using it in Logic Pro

See LOGIC-SETUP.md.

## Installers

**Mac (.pkg):** double-click `installer/mac/Make Mac Installer.command`. It builds a universal
(Apple Silicon + Intel) release and writes `dist/Murmur-<version>-mac.pkg`, which installs the AU,
VST3 and standalone app (each optional). Uninstall by deleting `Murmur.component`, `Murmur.vst3`
and `/Applications/Murmur.app`.

**Windows (setup.exe):** on a Windows PC with Visual Studio 2022 (C++ workload), CMake, Git and
Inno Setup 6, run `installer\windows\build-windows.bat`. Output: `dist\Murmur-<version>-windows-setup.exe`
(VST3 into `C:\Program Files\Common Files\VST3`, plus the standalone app). Windows has no AU, and no
built-in virtual MIDI ports, so on Windows use Murmur in a DAW that accepts VST3 MIDI output
(Ableton Live, Reaper, Bitwig, Cubase, FL Studio), or point the standalone app's MIDI output at a
loopback driver such as loopMIDI.

**No Windows PC?** Push this folder to a GitHub repository. `.github/workflows/build-installers.yml`
builds both installers on GitHub's machines; download them from the run's page in the Actions tab.

**Unsigned builds:** these installers aren't signed with an Apple Developer ID or a Windows
code-signing certificate, so other computers will warn the first time. On a Mac: System Settings >
Privacy & Security > Open Anyway. On Windows: SmartScreen > More info > Run anyway.
