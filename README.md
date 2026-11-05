# RiseKit

A light-on-CPU build-up effect in the spirit of Endless Smile. One big **Smile** control
pushes a whole chain of effects at once: high-pass filter, phaser, stereo widening,
reverb wash, drive, a tempo-synced chop and a level ride. Pull the smile back to 0 and the
plugin is fully dry again and uses no CPU at all.

## Using it in FL Studio

1. Put RiseKit on a mixer insert (or on a bus that your synths/drums/vocals go through).
2. Either:
   - **Automate Smile** (right-click the dial → *Create automation clip*), or
   - **Automate Rise**: turn it **on** where the build-up starts and **off** at the drop.
     The smile then climbs by itself over the chosen **Length** (1–32 bars), in time with
     your project. **Curve** sets the shape (slow start → big push at the end).
3. **Auto drop at the end** snaps back to dry the moment the length is reached, so you
   don't need to automate Rise off.
4. Pick a starting sound from the preset menu, then tweak the **Character** knobs.
   They set how much of each effect you get at full smile.

| Control | What it does |
|---|---|
| Filter / Reso | How high the high-pass climbs (up to ~3 kHz) and how much it peaks |
| Phaser | Sweeping phaser that speeds up as the smile rises |
| Reverb | Washy reverb that grows longer and louder |
| Drive | Saturation that gets dirtier |
| Width | Widens the stereo image |
| Chop | Tempo-synced gate: 1/4 → 1/8 → 1/16 → 1/32 near the top |
| Ride | Level change at full smile (±12 dB) |
| Mix / Output | Global dry/wet and output level |
| Safety clip | Soft-catches peaks above about −1 dBFS |

## Building (GitHub Actions)

1. Make a new GitHub repo (e.g. `RiseKit`) and upload everything in this folder,
   keeping the folder structure (`Source/`, `tests/`, `.github/workflows/`, `CMakeLists.txt`).
2. Every push starts both builds. Open the **Actions** tab, wait for the green tick,
   and download the artifact:
   - **RiseKit-mac-VST3**: unzip twice, you get `RiseKit.vst3`
   - **RiseKit-windows-VST3**: unzip, you get the `RiseKit.vst3` folder
3. The builds also run the DSP tests (`tests/dsp_test.cpp`). If those fail, the build fails.

## Installing

**Mac**: copy `RiseKit.vst3` to `~/Library/Audio/Plug-Ins/VST3/`, then run this once in
Terminal so macOS lets the unsigned plugin load:

```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/RiseKit.vst3
```

**Windows**: copy the `RiseKit.vst3` folder to `C:\Program Files\Common Files\VST3\`.

Then in FL Studio: *Options → Manage plugins → Find installed plugins*.

## Why it's light on CPU

- Smile at 0 = the audio is passed straight through, nothing is calculated.
- Filter, phaser and reverb coefficients are updated every 32 samples, not every sample.
- Effects that are turned down are skipped completely.
- The reverb is a small 4-line delay network instead of a big algorithmic reverb.
- No oversampling, no FFT.

Measured with everything maxed out: about 0.25 % of one CPU core.
