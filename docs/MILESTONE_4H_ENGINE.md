# Milestone 4H — construct-attached sounds and visual effects

Milestone 4H adds an engine-native effects channel for sounds and particles that originate in a moving construct's local coordinate system. The server sends a compact effect event; each client samples its current vessel transform so the effect follows smooth translation and yaw rather than being frozen at the world position where it was created.

## Effect protocol

`TOCLIENT_NAVYCRAFT_CONSTRUCT_EFFECT` is assigned command `0x69`. The NavyCraft client-command range now ends at `0x6A`.

Every packet contains:

- a monotonically increasing per-construct sequence;
- construct and effect IDs;
- an effect kind and preset;
- ship-local position and direction;
- optional inherited velocity;
- sound or texture name;
- gain, pitch and hearing distance;
- particle count, size, lifetime, glow and collision flags.

The codec is versioned, rejects malformed or oversized packets, bounds strings and numeric ranges, and ignores stale or duplicate sequences.

## Supported effect kinds

- one-shot positional sound;
- start and stop positional looping sound;
- transient particle burst;
- start and stop continuous particle emitter;
- short glowing particle flash.

Long-lived sounds and emitters use a stable `effect_id`. Starting a replacement with the same logical key first stops the previous instance. Construct removal and scene reset stop all associated sounds and clear emitter state.

## Client integration

`ClientConstructEffects` is owned beside `ClientConstructScene` and shares the reconstructed `ClientConstructManager` state.

On each client step it:

1. drains transient effect events;
2. samples the current construct transform;
3. converts local position and direction to world space;
4. adds construct surface velocity to the event velocity;
5. updates active looped-sound position and velocity;
6. emits moving particle bursts for persistent emitters;
7. removes effects whose vessel no longer exists.

Looping sounds use Luanti's positive sound handles so their position and velocity can be updated. One-shot sounds use unmanaged handle zero. Particles are submitted through Luanti's existing `ParticleManager` event path, retaining its texture, blend, glow, collision and lifetime handling.

## NavyCraft gameplay hooks

The gameplay adapter now produces native events for:

- engine startup and throttle-dependent engine loops;
- exhaust smoke and surface wakes;
- flooding jets and bubbles;
- hull damage sparks and impact sounds;
- heavy-damage smoke and fire;
- sinking alarm and splash burst;
- cannon and AA muzzle flashes;
- torpedo and depth-charge launch effects.

Effects are defined in `nc_navycraft/effects.lua`. Stock Luanti uses a reduced `sound_play` and `add_particlespawner` fallback. The custom engine uses `core.emit_dynamic_construct_effect`.

## Included original assets

The project includes five original 32×32 RGBA particle textures and six original mono Ogg Vorbis sounds generated specifically for this prototype. They do not come from the supplied NavyCraft JARs or an external asset pack.

## Verification boundary

Completed:

- packet round-trip and stale-event tests;
- local-to-world position, direction and surface-velocity sampling tests;
- active loop/emitter lifecycle tests;
- client sound/particle integration compatibility compilation;
- gameplay smoke test proving a weapon event emits sound and muzzle flash;
- release, AddressSanitizer and UndefinedBehaviorSanitizer builds;
- overlay idempotency and archive checks.

Not completed:

- complete Luanti 5.16.1 executable link;
- live OpenAL playback test;
- real GPU particle test;
- effect interest management and large multiplayer load measurement;
- dynamic point-light injection or camera shake.
