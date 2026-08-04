# Milestone 4J — native fire control and automatic gunnery

Milestone 4J adds server-side target tracks and firing solutions on top of the M4I projectile runtime. It does not replace NavyCraft's weapon catalogue or ammunition rules. The native layer calculates where and when to aim; the Lua gameplay layer still decides whether a weapon exists, is armed, has ammunition and has completed its source-derived reload delay.

## Source-derived behaviour

The supplied Bukkit source remains authoritative for the control surface and weapon distinctions:

- `firecontrol` and `tdc` are separate vessel systems;
- cannon fire uses a selected range and a parabolic flight path;
- torpedo controls expose heading, rudder, running depth, arming and automatic/manual state;
- source TDC modes distinguish straight-running, periscope-directed and automatic/targeted operation;
- torpedo depth controls are constrained to 0–60;
- the supplied ten-entry weapon catalogue continues to define names, salvo counts, ammunition, explosive strength, speed and range;
- the source weapon timeout remains the normal NavyCraft battery reload gate.

Those points are derived from `NavyCraft_BlockListener.java`, `Craft.java`, `OneCannon.java` and the supplied torpedo classes. The original plugin did not contain a continuous smooth-motion interception engine or autonomous battery scheduler.

## Luanti engine adaptations

`ConstructFireControlEngine` adds the calculations required by moving native constructs:

- observer-specific target tracks;
- filtered position, velocity and acceleration estimates;
- confidence decay and stale-track expiry;
- constant-speed direct interception;
- low- and high-arc ballistic solutions with gravity and drag;
- horizontal torpedo interception with separate selected depth;
- shooter translation inherited by the launch velocity;
- numerical miss-distance estimation;
- weapon elevation, range and traverse limits;
- manual, automatic and defensive battery modes;
- target-class filtering and defensive air-target priority;
- native battery reload scheduling.

The tracking filter, physical gravity value, target scoring and automatic batteries are M4J adaptations. They are not presented as recovered Bukkit values.

## Server authority flow

1. A NavyCraft radar, detector, sonar or visual contact becomes an observation.
2. The native engine updates the track owned by that observing vessel.
3. A manual request or automatic battery asks for a solution.
4. The solver returns aim point, yaw, pitch, intercept time and world launch velocity.
5. Lua re-checks crew permission, mount presence, launcher safety, ammunition and reload state.
6. The accepted launch enters the M4I server-authoritative projectile runtime.

Automatic gunnery therefore cannot create ammunition or bypass the existing gameplay rules. It consumes the same inventory counters and invokes the same firing functions as a player.

## Solution families

### Direct interception

For zero-gravity projectiles, M4J solves the relative-motion quadratic for the earliest positive intercept within the weapon's lead-time and range limits.

### Ballistic interception

Shells, fireballs and AA rounds use an iterative gravity/drag solution. This is the native equivalent of the original cannon's scripted parabolic path. Low and high arcs are selectable.

### Torpedo interception

Torpedoes solve the horizontal intercept separately and use the selected TDC depth as the vertical control target. Guided torpedoes retain the M4I course and depth correction after launch.

## Automatic batteries

A battery records its vessel, local muzzle position, weapon specification, mode, permitted target classes, selected target, confidence threshold, reload schedule and traverse arc.

- `manual` never emits autonomous fire orders;
- `automatic` uses the vessel's designated tracked target;
- `defensive` searches allowed tracks and prioritises approaching air targets.

The native scheduler emits a fire order only. Lua performs the final gameplay validation and launch.

## Lua controls

```text
/ship firecontrol status
/ship firecontrol manual|auto|defensive
/ship firecontrol arc low|high
/ship firecontrol solution [weapon]
/ship firecontrol fire [weapon]
/ship tdc straight|periscope|auto
/ship tdc heading <degrees>
/ship tdc depth <0-60>
/ship tdc solution [weapon]
/ship tdc fire [weapon]
/ship fire solution [weapon]
/nc_firecontrol
```

A moving fire-control block cycles the selected weapon with right-click and cycles manual/automatic/defensive mode with its alternate interaction. A moving TDC cycles source-style modes and can display an automatic solution. The AA control can toggle defensive mode.

## Native Lua API

```lua
core.observe_dynamic_construct_target(observer_id, observation)
core.solve_dynamic_construct_fire_control(shooter_id, target_id, weapon_definition)
core.configure_dynamic_construct_battery(shooter_id, battery_definition)
core.remove_dynamic_construct_battery(battery_id)
core.step_dynamic_construct_fire_control(current_time, maximum_track_age)
core.get_dynamic_construct_fire_control_tracks(observer_id_or_nil)
core.get_dynamic_construct_fire_control_batteries(shooter_id_or_nil)
```

Protocol capability version 7 enables this API. M4J does not add another network opcode: target tracking and automatic-battery decisions are server-only, while their projectiles use the existing M4I replication packet.

## Verification completed

- filtered moving-target tracks;
- direct lead against a translating target;
- low/high gravity arc solving;
- torpedo horizontal intercept and selected depth;
- estimated miss distance;
- air-target classification and defensive selection;
- traverse, confidence, range and reload rejection;
- native Lua solution and battery APIs;
- solved-fire launch velocity reaching M4I unchanged;
- automatic orders passing through ammunition and weapon rules;
- release, compatibility and gameplay smoke builds.

## Runtime boundary

Still not completed:

- linking and running the complete patched Luanti 5.16.1 executable;
- real multiplayer sensor latency and packet-loss tuning;
- turret meshes, traverse animation and independent mount rotation;
- line-of-fire obstruction by the firing vessel before launch;
- friendly-fire policy and team/organization identification;
- large-battle target-track and battery-load measurements.
