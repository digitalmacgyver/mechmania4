# Sound System Overview

This document explains how the MechMania IV observer drives audio, how to add or
override sounds, and which configuration knobs are available.

## Runtime Architecture

- The server (`mm4serv`) or replay driver emits gameplay events into the shared
  `CWorld`.
- `ObserverSDL` polls that world every frame, turns world events into logical
  audio identifiers (e.g. `team.launch.default`), and hands them to
  `mm4::audio::AudioSystem`.
- `AudioSystem` looks up each logical identifier in the loaded sound catalog,
  schedules the effect according to its behavior rules, and dispatches it
  through SDL_mixer. This same path runs in the normal observer UI or in
  headless diagnostics.
- Manual cues (menu clicks, diagnostics beeps) skip the world and queue their
  logical identifiers directly.

## Configuration Files

The default catalog lives at `sound/defaults.txt`. The parser expects two-space
indented YAML-like syntax (no tabs). When the observer starts, it searches for
this file relative to the current working directory; provide `--assets-root` to
point at an alternate asset tree when running from another location.

The file is organised into a few high-level namespaces:

- `game.soundtrack.songs`: ordered list of music files that seed the playlist.
- `teams.<namespace>`: per-team effect trees (`team` for world index 0, `team2`
  for index 1, etc.). Each leaf becomes a logical identifier.
- `manual.*`: non-gameplay events (menu interactions, diagnostics, scripted
  cues).
- `volume.*`: optional global volume controls (`soundtrack`, `effects`). Values
  are percentages between 0–100.

Each effect leaf supports the following keys:

```yaml
teams:
  team:
    launch:
      default:
        file: ../assets/star_control/effects/orz-go.wav
        inherit: optional.base.logical.id     # optional; copies another entry
        behavior:
          mode: queue                         # "queue" or "simultaneous"
          delay_ticks: 0                      # frames to wait before playback
          duration_ticks: 12                  # expected length per loop
          scale:                              # optional automatic loop scaling
            per_quantity: 256.0               # event quantity that triggers 1 loop
            min_loops: 1
            max_loops: 8
```

- `file`: absolute path or relative path (relative paths are resolved starting
  from the config file's directory; `--assets-root` overrides that base).
- `inherit`: copies another logical identifier before applying the local
  overrides. Useful for reusing the same behavior with a new asset.
- `behavior.mode`:
  - `simultaneous` (default) plays all requests immediately.
  - `queue` enforces serialized playback so repeated events run back-to-back.
  - `truncate` (alias `cutoff`) halts any currently playing instance of the same
    logical event before starting the new one; other sounds continue normally.
- `behavior.delay_ticks`: adds a fixed frame delay before the effect can start.
- `behavior.duration_ticks`: expected length of one loop; the mixer uses it to
  track channel lifetime when SDL_mixer does not provide it.
- `behavior.scale`: enables automatic loop expansion. When the event's quantity
  is supplied (for example, multiple missiles launching in the same tick), the
  system computes the number of loops as
  `ceil(quantity / per_quantity)` clamped between `min_loops` and `max_loops`.

If no `behavior` block is present, defaults are `simultaneous`, zero delay, and
one loop.

### Global Volume

Set the `volume` block to scale music/effects output without touching the
system mixer. Example:

```yaml
volume:
  soundtrack: 100  # percent (0 disables music)
  effects: 75      # play effects at 75% of the configured volume
```

Values outside 0–100 are clamped. Muting still takes precedence over these
percentages.

### Music Configuration

`game.soundtrack.songs` is the authoritative playlist. The first entry becomes
the default track when the observer starts unless another track is selected
manually. Paths obey the same resolution rules as effects. The observer exposes
`NextTrack(true)` (keyboard shortcut `X`) to move through this list.

### Manual Channels

Entries under `manual.*` exist for UI and tooling cues. The observer currently
emits:

- `manual.menu.toggle_enabled[_alt]` and `manual.menu.toggle_disabled[_alt]`
  when music/effects mute toggles flip.
- `manual.audio.ping` when the optional diagnostics heartbeat is active.

Add additional manual cues by calling `AudioSystem::QueueEffect()` with the new
logical identifier; declare its asset and behavior in the `manual` block.

## Providing New Assets

1. Add or update the relevant node in `sound/defaults.txt`, or ship an alternate
   config file and pass its path to `AudioSystem::Initialize`.
2. Use either relative or absolute paths:
   - Relative paths are resolved from the configuration file’s directory; if you
     launch the observer with `--assets-root` the paths remain relative to that
     override.
   - Absolute paths (e.g., `/opt/mm4/audio/some.wav`) are used verbatim and can
     live anywhere on the filesystem.
3. Restart the observer; the library reloads assets during initialization only.

If you need to package multiple configurations, commit a bespoke catalog (for
example, `sound/tournament_defaults_tbd.txt`) and swap it into place as
`sound/defaults.txt` before launching (or supply a different `sound/` directory
through your deployment tooling). When the observer runs inside Docker, mount
the asset directory and pass `--assets-root` so the catalog resolves paths
correctly.

## Runtime Controls

- `--mute`: start the observer with both music and effects muted. The new menu
  toggle state machine resets when this flag is active so the first unmute plays
  the standard confirmation cue.
- `M`: toggle music mute at runtime (`manual.menu.toggle_enabled*` cues vary for
  enabled/disabled states).
- `E`: toggle effects mute.
- `X`: advance to the next soundtrack entry (even while muted; playback resumes
  on the next unmute).
- `--verbose`: emit extra `[audio]` logging whenever effects are queued.
- `--enable-audio-test-ping`: enable the diagnostics heartbeat (still gated by
  `--verbose`).
- `--playlist-seed <uint32>`: override the soundtrack shuffle seed to reproduce
  deterministic playlists (logged together with the catalog order).

Headless smoke tests (`scripts/audio_headless_smoke.sh`) use the same
configuration but run beneath `SDL_VIDEODRIVER=dummy`. Inspect the captured logs
to confirm playlist initialization (`[audio] playlist context=…`) and effect
dispatch (`[audio] tick=… schedule event=…`) when audio hardware is unavailable.

## Effect Requests From Code

- Gameplay code should call `CWorld::LogAudioEvent()` with a populated
  `EffectRequest`. The observer drains that queue every frame.
- The request structure can specify `requestedLoops`, `requestedDelayTicks`, and
  `quantity`; the catalog provides defaults when these fields are zero.
- Manual menus and other UI elements call `AudioSystem::QueueEffect()` directly.

When introducing a new logical identifier, prefer namespaced identifiers such as
`teamX.some_event.variant` or `manual.menu.action` so that inheritence and
fallback rules remain predictable. The loader automatically falls back to the
`team` namespace when `teamN.*` is missing, reducing duplication.

## Event Reference

The observer only plays effects that the game code explicitly queues via
`CWorld::LogAudioEvent()`. If you remove an entry from `sound/defaults.txt` you
can re-add it later using the logical identifiers listed below.

### Team-scoped events

Unless noted otherwise these are queued as `teamN.<suffix>` where `team` is the
baseline namespace and `team2`, `team3`, … fall back to those entries when a
more specific block is absent.

- `launch.default` – Ship undocks from any station.
- `launch.launch_from_friendly` / `.launch_from_enemy` – Play alongside
  `launch.default` when the launch came from a friendly or enemy station.
- `dock.default` – Ship begins docking animation.
- `dock.dock_at_friendly` – Friendly dock confirmation.
- `dock.dock_at_enemy` – Enemy dock confirmation.
- `dock.dock_at_enemy_alert` – Additional alert that fires with
  `dock_at_enemy`.
- `deliver_vinyl.default` – Vinyl delivered to any station.
- `deliver_vinyl.deliver_vinyl_friendly` – Bonus cue when the delivery was to a
  friendly station.
- `deliver_vinyl.deliver_vinyl_enemy` – Bonus cue when delivering to an enemy
  station.
- `raise_shields.default` – Ship raises shields.
- `jettison.default` – Any cargo jettison.
- `jettison.jettison_vinyl` / `jettison.jettison_uranium` – Material-specific
  cues emitted in addition to `jettison.default`.
- `laser.default` – Ship fires its laser.
- `laser.laser_asteroid.laser_asteroid_break` – Laser shot shatters an asteroid.
- `laser.laser_asteroid.laser_asteroid_nobreak` – Laser shot glances off an
  asteroid without breaking it.
- `col_ship_laser.default` – Ship hit by an incoming laser.
- `col_ship_asteroid.default` – Ship collides with an asteroid.
- `col_ship_asteroid.col_ship_asteroid_eat` – Ship successfully ingests the
  asteroid (queued alongside the default collision cue).
- `col_ship_asteroid.col_ship_asteroid_destroy` – Ship destroys an asteroid on
  impact (shatter events).
- `col_ship_ship.default` – Ship-to-ship collision.
- `col_ship_ship.col_ship_friendly_ship` – Friendly ship collision variant.
- `col_station_asteroid.default` – Asteroid hits a station.
- `damage.shield` – Shield drop detected by `AudioEventTracker` (queued as
  `teamN.damage.shield`).
- `ship_out_of_fuel.default` – Ship expends its last fuel.
- `ship_destroyed.default` – Ship explodes.
- `game_won.default` – Winning team celebration (only the winning team queues
  this event).

### Manual / UI events

- `manual.menu.toggle_enabled` / `_alt` – Menu toggle confirmation when turning
  music or effects back on.
- `manual.menu.toggle_disabled` / `_alt` – Menu toggle when muting.
- `manual.audio.ping` – Optional diagnostics heartbeat (`--enable-audio-test-ping`).

### Station tracker

The embedded tracker keeps an eye on station cargo and shield changes:

- `team.damage.shield` – Queued when a ship takes appreciable shield damage in
  a single turn. Configure `team.damage.shield` (without a numeric suffix) to
  cover every team.
