# Theatre — by Sound Brewery

**See how your panning covers a real cinema hall.** Theatre is an analysis plug-in (VST3 + Audio Unit, Mac) that shows where a sound plays in a modelled cinema and how well it reaches every seat. It adds no latency and never changes your audio.

## What it shows

- **3D Hall:** a 20 × 26 × 9 m cinema with 308 tiered seats, a screen and 33 speakers. Every speaker that plays gets a see-through **coverage sphere**: bigger and brighter means louder and reaching further. Spheres are blue for the screen, green for the surrounds and amber for the ceiling.
- **Plan view:** a top-down hall plan with seat colours (level compared with the mix seat), plus a side section for height.
- **Monitoring:** 7.1.4 · 7.1 · 5.1 · Stereo. Only the speakers in that format play.
- **Render as:** Bed (whole speaker arrays) or Object (individual speakers, blended between the wall layer and the ceiling layer by height).
- **Source:**
  - **Panner** — move the sound with Theatre's own controls (left–right, screen–rear, height, size). These can be automated.
  - **Track signal** — Theatre reads the real level of each channel on the track or bus it is inserted on (5.1, 7.1 or 7.1.4) and shows that sound's coverage.
- **Seat check:** pick any seat to see its level compared with the mix seat, how far the sound has moved away from where you panned it, which speaker reaches it first, and a plain-language verdict.
- **Channel meters:** peak meters with hold, and Momentary / Short-term / Integrated LUFS (ITU-R BS.1770 K-weighting, gated integrated).

## Install (no Terminal needed)

1. Open the **Releases** page of this repository (right-hand side on GitHub) and download the newest **Theatre-Installer-….pkg**.
2. Double-click the .pkg. If macOS says it can't check the developer:
   - Open **System Settings → Privacy & Security**.
   - Scroll down, find the message about *Theatre-Installer* and click **Open Anyway**. Confirm with your password.
3. Click **Continue → Install**. The VST3 and Audio Unit are installed for all your audio apps.
4. Open your DAW and rescan plug-ins if it asks. Theatre appears under **Sound Brewery**.

## Using it

- Insert Theatre on a **5.1, 7.1 or 7.1.4** track or bus (or on stereo).
- Choose **Track signal** to follow the real audio, or **Panner** to explore a position by hand.
- Switch between **Plan view** and **3D Hall** with the tabs at the top. Drag the 3D hall to orbit around it and scroll to zoom.

## Building from source

The Mac installer is built automatically by GitHub Actions on every push to `main` (see `.github/workflows/build-mac.yml`). For a local build: `cmake -B build -G Xcode && cmake --build build --config Release`.

Built with [JUCE](https://juce.com) (AGPLv3 / commercial licence).
