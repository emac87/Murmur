# Using Murmur in Logic Pro

Logic ignores MIDI that comes out of audio-effect plug-ins. So Murmur also sends
its notes to a virtual MIDI port called **Murmur Out**, and a Software Instrument
track in Logic plays from that port. The setup is two tracks: an audio track
that listens, and an instrument track that plays.

```
guitar / bass / mic ─▶ Audio track [Murmur] ──(Murmur Out port)──▶ Instrument track [synth]
```

## 1. Build and install (once)

```bash
xcode-select --install        # if you don't have the command line tools yet
brew install cmake            # if you don't have CMake yet
cd murmur-plugin
./build-mac.sh
```

The first build downloads JUCE and takes a few minutes. When it's done:
- `Murmur.component` (AU) is in `~/Library/Audio/Plug-Ins/Components/`
- `Murmur.vst3` is in `~/Library/Audio/Plug-Ins/VST3/` (Logic doesn't use VST3; it's for other DAWs)
- `Murmur.app` (standalone) is in `build/`

Quit Logic before building, then reopen it. On launch Logic validates new AUs. If
Murmur is missing, open **Logic Pro > Settings > Plug-in Manager**, find
Murmur Audio > Murmur, and click **Reset & Rescan Selection**.

## 2. Logic settings (once)

1. **Logic Pro > Settings > Audio > Devices**: set **I/O Buffer Size** to **64** or
   **128**. A smaller buffer gives lower latency.
2. Make sure **Software Monitoring** is turned on in the Audio settings.

## 3. The two tracks

**Track 1: Audio (the listener)**
1. Create a new **Audio** track. Set its **Input** to the interface input your
   guitar, bass or mic is plugged into (mono is fine).
2. In an **Audio FX** slot, choose **Audio Units > Murmur Audio > Murmur**.
3. Click the track's **Input Monitoring** button (the "I" button). Logic only runs
   plug-ins on live input while the track is monitoring or record-armed.
4. In Murmur, choose **Voice**, **Guitar** or **Bass**. The note display should
   follow what you play. The **MIDI port** chip in Murmur's header should be green.

**Track 2: Software Instrument (the player)**
1. Create a **Software Instrument** track and load any instrument (Retro Synth,
   Alchemy, Studio Piano, Sampler, a third-party synth).
2. **Record-arm** it (the R button). An armed instrument track plays incoming MIDI
   even when you have the audio track selected to tweak Murmur.
3. Optional but tidy: in this track's **Track inspector**, set **MIDI In Port** to
   **Murmur Out**. Then your MIDI keyboard won't also play this track, and Murmur
   won't play your other tracks. If Murmur Out isn't in the menu, turn on
   **Settings > Advanced > Enable Complete Features**, then check that Murmur Out
   is ticked in **Settings > MIDI > Inputs**.

Play, and the synth follows you.

## 4. Recording

- Press **Record**. The armed instrument track records the MIDI that Murmur
  generates.
- To keep the dry guitar or vocal too, also record-arm the audio track.
- To hear only the synth, turn off **Audio thru** in Murmur, or lower the audio
  track's fader.

## 5. Settings for a first session

| Situation | Try |
|---|---|
| Notes flicker or stray notes appear | Raise **Gate** (e.g. −45 dB), or set **Response** to Stable |
| Feels laggy | Set **Response** to Fast, use a buffer of 64 |
| Bass sounds muddy on the synth | **Transpose** +12 |
| Sliding or vibrato should be smooth | Turn on **Pitch bend**. Set **Bend range** to match the synth's pitch-bend range (often 2) |
| Even dynamics wanted | Set **Velocity** to Fixed |
| A note is stuck | Click **Panic** |

## Gotchas

- **One note at a time.** Murmur is monophonic, so chords give unpredictable notes.
  On guitar, mute the strings you aren't playing.
- **Use the interface's instrument input** (Hi-Z / DI), not the laptop mic.
- **Doubled notes**: something else is also sending to the same instrument track,
  for example the standalone Murmur app running at the same time. Set the track's
  MIDI In Port to Murmur Out and quit the standalone app.
- **Murmur Out only exists while Murmur is loaded** (in Logic or in the
  standalone app). If you open a project and the port seems missing, make sure
  the audio track with Murmur is loaded and monitoring.
- **Several Murmur instances** all share the one Murmur Out port. Give each a
  different **Channel**, and set each instrument track's **MIDI In Channel** to
  match.

## Fallback: standalone app

If the AU gives you trouble, open `Murmur.app` and choose your interface under
its **Options > Audio/MIDI Settings**. It creates the same Murmur Out port, and
the Logic instrument track (step 3, track 2) plays from it as before. Don't also
select Murmur Out as a MIDI output inside the standalone settings, or you'll get
doubled notes.
