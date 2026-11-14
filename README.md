# Theatre — by Sound Brewery

**See how your panning covers a real cinema hall.** Theatre is an analysis plug-in (VST3 + Audio Unit, Mac) that shows where a sound plays in a modelled cinema and how well it reaches every seat. It adds no latency and never changes your audio.

## What it shows

- **3D Hall (live):** a 20 × 26 × 9 m cinema with 308 tiered seats, a screen and 33 speakers. Every speaker's **coverage sphere** follows its channel meter: it grows, pulses, sends out wavefronts and fades with the audio. The **SOUND** marker shows where the mix position hears the sound (L + R equal = screen centre). Sphere colours: blue for the screen, green for the surrounds, amber for the ceiling, violet for the LFE.
- **Plan view:** a top-down plan with seat colours (level compared with the mix position) and a side section for height.
- **Mix position:** the direction and height of the sound as heard from the mix position (row I, centre), and where it comes from.
- **Channel meters:** always show the track's real channels. True-peak bars (4× oversampled, ITU-R BS.1770-4 method), fall-back of 20 dB in 1.7 s, 2 s peak hold, a max true-peak number per channel (red at −1 dBTP and above), and Momentary / Short-term / Integrated LUFS.
- **Channel order:** **Pro Tools / Film** (L C R Ls Rs Lrs Rrs LFE Ltf Rtf Ltr Rtr) is the default, which is right for Pro Tools, including through Blue Cat PatchWork. Choose **SMPTE / ITU** (L R C LFE Ls Rs …) for Nuendo, Logic, Reaper and similar, or **Use the host's channel labels**. The header shows which channel number Theatre reads as which speaker.
- **Monitoring:** **Track** (the track's own format) or 7.1.4 · 7.1 · 5.1 · Stereo. Smaller formats fold the sound down: heights go to the wall surrounds, rears join the sides, and stereo takes everything into L and R.
- Opens at zero: nothing lights up and no values are shown until audio plays.

## Install (no Terminal needed)

1. Open the **Releases** page of this repository (right-hand side on GitHub) and download the newest **Theatre-Installer-….pkg**.
2. Double-click the .pkg. If macOS says it can't check the developer:
   - Open **System Settings → Privacy & Security**.
   - Scroll down, find the message about *Theatre-Installer* and click **Open Anyway**. Confirm with your password.
3. Click **Continue → Install**. The VST3 and Audio Unit are installed for all your audio apps.
4. Open your DAW and rescan plug-ins if it asks. Theatre appears under **Sound Brewery**.

## Using it

- Insert Theatre on a 5.1, 7.1 or 7.1.4 track or bus. In Pro Tools, host the AU in Blue Cat PatchWork.
- Check the **Track:** line in the header. Send a test signal to one channel and make sure the right meter lights. If it doesn't, change **Channel order**.
- Switch between **Plan view** and **3D Hall** with the tabs at the top. Drag the 3D hall to orbit it and scroll to zoom.

## Building from source

The Mac installer is built automatically by GitHub Actions on every push to `main` (see `.github/workflows/build-mac.yml`). For a local build: `cmake -B build -G Xcode && cmake --build build --config Release`.

Built with [JUCE](https://juce.com) (AGPLv3 / commercial licence).
