# Task 16: the smooth dream at the display's refresh rate — where it ended

Done in the session after task 15 (2026-10-07), while the operator was
away ("keep working"); no task doc came before it. Built, measured
headless, not merged into lsd-port `main` or pushed there.

## Why

`smooth = on` draws frames between the dream's ticks, one per 59.94 Hz
blank. psyz's auto VSync uses the driver's VSync only when the display
refreshes at about 59.94, so on a 120 or 144 Hz display the frames went
out at 59.94 through psyz's limiter: smooth, but not as smooth as the
display could show, and with uneven frame pacing against its refresh.

## What changed

- **psyz fork** (pushed; branch `present-rate` from the fork's `main`,
  merged into `main` `ad64361`): `Psyz_VideoPresent(fps)` presents and
  paces the next frame at `fps`, by the driver's VSync when the display
  refreshes at about that rate (or VSync is forced on), else by the
  limiter, without VSync callbacks; `Psyz_VideoVSync` switches the driver
  VSync back to what its own 59.94 pacing wants; `Psyz_VideoGetDisplayRate`
  reports the refresh rate of the window's display. The PSP backend
  presents at its blank. The fork's timing code has diverged from
  upstream's, so this one would need rebasing before an upstream PR.
- **lsd-port** branch `task-16-frame-rate`: `frame_rate = 60` in
  settings.ini (`--frame-rate`, `LSD_FRAME_RATE`): `60` (default, the
  console's blank grid as before), `display`, or 30 to 360. With smooth
  on and anything but 60, `src/pacing.c`'s `TimedPass` runs the dream's
  passes at that rate: each presents, a tick runs when its time is due
  (`pace * 59.94 / 60` a second), and the passes between draw the world
  at the fraction of the tick their time is. A late tick isn't caught up
  on. Menus, the graph and movies are unchanged. README, PLAN, design.md
  ("Frame rate"); the psyz pin moves to `ad64361`.

## Checks

- Headless (RelWithDebInfo x86_64, limiter): `frame_rate = 144` gives
  144.1 frames and 14.00 ticks a second; tick gaps 69 to 77 ms (67 to 84
  on the blank grid). Default: 59.99 frames, 14.00 ticks.
- The camera through a turn at 144 (`draw_trace`): yaw steps of 6 to 7
  a frame, 68 a tick, 9 or 10 frames a tick. A tick's own frame shows the
  scheduled pose up to a frame late, so it can sit a few units off the
  even spacing.
- Lockstep at 144 against `main` (x86_64 Debug, pace 14): days 22 and
  340 and task 14's nw46 and ky47 spots, every STATE and SAVEBLK line the
  same.
- psyz host tests: 319 pass (Vulkan, Debug); GL software 317 pass and the
  same two dither tests fail with and without this change.
- Builds: x86_64 Debug and RelWithDebInfo, i686, Windows x86_64 (MinGW).

## Not tested — for the operator

- **A real high-refresh display.** The driver-VSync path (`display` on a
  120/144 Hz monitor) can't run headless. To try it, on workspace 10:

  ```bash
  SDL_APP_ID=claude-lsd ./build-t15-rwdi/lsd --frame-rate display
  ```

  and watch turning in a dream; the log line `detected ... Hz monitor`
  is psyz's ConfigureVSync for the menus. A variable-refresh display may
  report its maximum.
- `LSD_VSYNC=limitless` (the debug server's fast-forward) doesn't speed
  the dream up in the timed loop: the ticks follow the clock. Lockstep
  doesn't use it.
