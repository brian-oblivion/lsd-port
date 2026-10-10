# Task 25: sound settings (volumes and interpolation)

*Done in the overview session, 2026-10-10 (no handover): the settings in
`src/settings.c` and the menu's SOUND section, psyz `30200d3`; what was
found and measured is in `docs/design.md`, "Sound settings".*

The operator wants a sound section in the settings menu:

1. **Volume sliders**: master, music, sound effects and movies, each
   0 to 100 %, changing what is heard at once.
2. **A higher-quality option**: the SPU's sample interpolation (the
   console's 4-tap gaussian against a cleaner one), suggested by the
   overview session on 2026-10-10 as feature idea 6.

Both are port-only, default to the console's sound (100 % and
`console`), and change only what reaches the speakers: nothing the game
can read back (SPU registers, ENVX, libsnd's state) may change.

Read first: `docs/design.md` ("Settings menu", and the music clock in
"Pace"), `docs/tasks/21-settings-menu-handover.md`, `src/settings.c`,
`src/menu.cpp`, `src/ini.c`, `decomp/src/sound/wbgm.c` (the music:
SEQ through libsnd, `SsSeqSetVol`), `decomp/src/sound/vab_stream_obj.c`
(sound effects: `SsUtKeyOn` and `SsUtAutoVol`; `SsSetMVol`),
`decomp/src/cd/cd_stream.c` (`SetupCdStreamAudio`: XA for the movies,
main and CD volume), `psyz/psyz/src/psyz/psyz_spu.c` (`spu_tick`, the
mix), `psyz/psyz/src/psyz/spu_voice.h` and `spu_gauss.h`,
`psyz/psyz/src/platform/sdl3_audio.c`, psyz's libsnd (`SpuVm*`: how a
voice is allocated, and what tells a SEQ note from an `SsUtKeyOn` one),
`docs/upstream/psyz-prs.md` ("spu-end-mute-envx", "xa-zigzag"), the
docstrings of `tools/snd_compare.py` and `tools/lockstep.py`. Task 01's
Rules apply.

## Where things stand (2026-10-10)

- lsd-port `main` `6f81a4c`; `decomp/` `95f67e0fb`; `psyz/` the fork's
  `b431599`.
- psyz mixes at 44100 Hz in `spu_tick`: each voice is decoded and
  resampled by its pitch through the gaussian table, scaled by its
  envelope and its volume registers, summed with the reverb's output
  and the CD/XA ring (by `cd_vol`), then scaled by `main_vol`. SDL
  converts to the device's rate.
- The game sets the volumes itself: `SsSetMVol(120, 120)` once,
  `SsSeqSetVol` for the music (and fades it through `WBgm`),
  `SsUtKeyOn`/`SsUtAutoVol` per effect, and `SetupCdStreamAudio`'s
  main and CD volume for movies. Scaling those calls would change
  libsnd's state, so the volumes belong in the mix instead.
- The settings menu (task 21) has no sliders yet; every setting is a
  choice from a list.

## Questions, in order

1. **Which voice is which.** Find how psyz's libsnd can tell a voice
   keyed by the SEQ player from one keyed by `SsUtKeyOn` (and whether
   LSD plays any effect through a SEQ, or music through `SsUtKeyOn`).
   Record each voice's group at key-on in psyz without changing any
   register or libsnd field the game reads. Count, headless over a day
   or two, how many notes fall in each group, and list anything that
   doesn't fit (the title menu's sounds, the graph, flashback, the
   linking sounds, the ghost-face jingle). If music and effects can't be
   told apart cleanly, stop and report.
2. **The volumes.** Per group, a gain applied in `spu_tick` after the
   voice's own volume, and so before the reverb send: say whether a
   music voice's reverb should follow the music slider (probably yes,
   by sending it already scaled), and the CD/XA ring takes the movies
   slider. Master scales the final mix (in psyz, or the SDL stream's
   gain: say which and why; clipping must not change with master below
   100 %). Behind one small psyz API (for example
   `Psyz_SpuSetGain(group, gain)`), switched off at 100 %, where the
   mix must stay bit-identical. Changes take effect at the next
   pulled buffer; check that dragging a slider doesn't click (ramp
   over a few ms if it does).
3. **The interpolation.** psyz option, `console` (the gaussian table,
   as now) and one cleaner filter. Try at least a 4-point cubic
   (Hermite or Catmull-Rom) and a windowed sinc (8 to 16 taps), at
   LSD's common pitches. The gaussian is a low-pass: many PS1 samples
   were made to sound right through it, so a flatter filter can sound
   brighter or harsher, not only cleaner. Measure with
   `snd_compare.py` (spectrum against `console` and against
   DuckStation, which uses the gaussian) and give the operator short
   WAV clips (scratchpad, `SDL_AUDIO_DRIVER=disk`) of the same music
   and effects with each filter, so they can choose. Pick at most two
   non-console choices to offer; CPU time per second of audio for each.
4. **Settings.** In `settings.ini`, the command line and the
   environment, as the others: `volume`, `music_volume`,
   `effects_volume`, `movie_volume` (0 to 100) and
   `interpolation = console | <chosen>`; propose better names if these
   read badly. In the menu, a "Sound" section with four sliders and the
   interpolation choice, working with keyboard, gamepad (left and right
   on the d-pad move a slider) and mouse; a value given on the command
   line is greyed as the others are. Everything changes at once.
5. **Check**:
   - lockstep STATE and SAVEBLK lines identical with every slider at 0,
     50 and 100 and each interpolation, days 1, 22 and 340;
   - at the defaults, the disk-audio output byte-identical to `main`
     over the same run;
   - with music at 0, no SEQ note heard, and the effects unchanged
     (and the other way around); movies at 0 silences a movie's sound
     only; master scales all;
   - nothing changes with the menu open but what was dragged;
   - builds: x86_64 and i686, Windows (MinGW); psyz's host tests (add
     one for the gain path only if the fork's test rules allow it, see
     the psyz PR notes).
6. README ("The settings menu", and a "Sound" section), `docs/design.md`
   section, and handover `docs/tasks/25-sound-settings-handover.md`.

## Rules (in addition to task 01's)

- lsd-port: work on a branch; `main` is merged and pushed with the
  operator's yes.
- lsddecomp: no changes.
- psyz fork: push and merge freely, one topic branch from the fork's
  `main`; keep the delta small, off by default, and shaped so the
  volume and interpolation parts could each go upstream as a PR;
  never push to or open anything on the original psyz.
- Headless only (offscreen, `SDL_AUDIO_DRIVER=disk`, the debug server;
  DuckStation on Xvfb as its memory notes say); kill what you start, by
  pid. At most three lockstep runs at once.
- No game data, recordings or clips in any repository.

## Report back with

How voices are grouped and what didn't fit; where each gain is applied
and why; the filters tried, their measurements and CPU cost, and the
clips' paths for the operator to listen to; the setting names; the
lockstep, byte-identity and silence checks; anything the menu needed to
grow for sliders.
