# Reverse Swell

A VST3 plugin (plus a standalone app) that turns any audio file into a reverse-reverb intro: the sound "sucks in" and swells up into the first hit or word.

Drop a file in → select the hit → drag the result straight onto a DAW track.

---

## Building

You need **CMake 3.22+** and a C++17 compiler. JUCE 8 is downloaded automatically the first time you configure (internet access required for that step).

### Windows (Visual Studio 2022)

```bat
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

Output: `build\ReverseSwell_artefacts\Release\VST3\Reverse Swell.vst3`

### macOS (Xcode command line tools)

```bash
cmake -B build -G Xcode          # or: cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Output: `build/ReverseSwell_artefacts/Release/VST3/Reverse Swell.vst3` (universal: Apple Silicon + Intel)

### Options

| Option | What it does |
|---|---|
| `-DJUCE_DIR=/path/to/JUCE` | Use a local JUCE checkout instead of downloading |
| `-DREVERSE_SWELL_COPY_AFTER_BUILD=ON` | Copy the VST3 into your system plug-in folder automatically |

Want Audio Unit too (Logic, GarageBand)? Change `FORMATS VST3 Standalone` to `FORMATS VST3 AU Standalone` in `CMakeLists.txt`.

---

## Installing

Copy the whole `Reverse Swell.vst3` folder/bundle to:

| OS | Folder |
|---|---|
| Windows | `C:\Program Files\Common Files\VST3\` |
| macOS | `~/Library/Audio/Plug-Ins/VST3/` (just you) or `/Library/Audio/Plug-Ins/VST3/` (all users) |

Then rescan plug-ins in your DAW. It shows up under **Fx → Reverb**.

On macOS, if a locally built plug-in is blocked: `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Reverse\ Swell.vst3`

The **Standalone** app (`.../Release/Standalone/`) works without a DAW: drop a file in, then export or drag out.

---

## Using it

1. **Load**: drag a WAV / AIFF / FLAC / MP3 onto the top waveform (from your desktop or your DAW), or click **Load**.
   The plugin guesses the first hit and selects 1 second from there.
2. **Select**: click-drag on the input waveform to choose the part to reverse-reverb (usually the first hit or word).
   - Drag the purple handles to adjust the edges
   - Mouse wheel zooms, Shift+wheel or Alt-drag scrolls, double-click zooms back out
   - A single click moves the selection and keeps its length
   - **Snap to zero** keeps the edges on zero crossings so they don't click
3. **Shape**: every change re-renders in the background after a short pause.
4. **Listen**: press **Play**, or send a MIDI note to the plugin's track to trigger the preview from your DAW.
5. **Get it out**: drag the bottom waveform onto a DAW track, or click **Export WAV** (24-bit, original sample rate).

### Controls

| Control | Meaning |
|---|---|
| Swell Length | How long before the hit the swell starts (0.5–8 s) |
| Room Size | Size of the virtual room (density and character) |
| Decay | Reverb tail length (RT60). Longer = fuller swell |
| Damping | Darkens the tail (high-frequency absorption) |
| Pre-delay | Leaves a gap of silence between the swell and the hit |
| Width | Stereo width of the swell (0 = mono) |
| Swell Level / Dry Level | Mix of the swell and the original audio. All the way down = off |
| Curve | Fade-in shape: Linear, Exponential (slow then sudden) or Log (quick then steady) |
| Include dry | Off = export only the swell, to layer it yourself |
| Normalize | Brings the output peak to −0.3 dBFS |
| Selection only | On = reverb only the selection. Off = reverb everything from the selection to the end of the file |

### How it works

The selection is reversed, run through a reverb at 100 % wet with a long tail, then reversed again. The reverb tail turns into a rising swell. That swell is placed so its end lands exactly on the start of your selection, with a 30 ms crossfade into the dry hit. The result is longer than your file by the swell length, so when you drag it into the DAW, place it so the hit lines up.

The reverb is a custom 8-line feedback delay network with input diffusion, a real RT60 decay control and in-loop damping. All rendering happens on a background thread. The audio thread only plays back the finished result (no allocation, no file I/O).

### Notes

- The plugin saves the **path** of the loaded file with your project, not the audio itself. If you move or delete the file, it won't reload.
- Drag-out files are written to your temp folder under `ReverseSwell/`. Your DAW normally copies them into the project on import.
- Audio on the plugin's input passes through untouched; the preview is mixed on top.

---

## Project layout

```
CMakeLists.txt
Source/
  PluginProcessor.*   parameters, state, file loading, background render thread, preview playback
  PluginEditor.*      layout, buttons, knobs, drag-out
  WaveformView.*      waveform drawing, selection, zoom, file drop target
  SwellRenderer.*     offline DSP (reverse → reverb → reverse → align/crossfade)
  SwellLookAndFeel.*  black & purple styling
  Params.h            parameter IDs
```
