# Milestone 4K — native navigation, avoidance and formations

Milestone 4K replaces the gameplay layer's simple “turn toward the next point” autotravel loop with a server-side navigation controller for continuously translated and rotated constructs. It preserves NavyCraft's existing route records, craft types, engine output, throttle, gear, buoyancy and vertical-control rules. The native layer calculates a safe movement command; the gameplay layer applies that command through the vessel's ordinary systems.

## Source-derived behaviour

The supplied Bukkit source remains authoritative for these concepts:

- each craft may hold a `WayPoints` list and a `currentWayPoint` index;
- routes use `routeID` and `routeStage` state;
- automatic craft use `isAutoCraft` and `WayPointTravel` state;
- the automatic movement task scans a volume ahead of the craft and steers away from an occupied side;
- an automatic craft can become stuck and eventually stop or recover through `stuckAutoTimer` handling;
- route and waypoint commands remain part of the NavyCraft control surface.

Those concepts are visible in `Craft.java`, `MoveCraft_Timer.java` and `CraftMover.java`. The source implementation assumes grid-aligned Bukkit craft movement and does not contain a continuous velocity-obstacle solver, smooth braking model or moving formation controller.

## Luanti engine adaptations

`ConstructNavigationEngine` adds the calculations needed by native moving constructs:

- manual, position-hold, route and formation modes;
- surface, submersible, air and ground movement domains;
- speed, acceleration, braking, yaw-rate and vertical-speed limits;
- waypoint arrival radii and optional per-waypoint target speeds;
- braking-distance based arrival control;
- corner lookahead and direction blending;
- observer-local terrain hazard records;
- automatic moving-construct separation;
- predicted closest-approach and route-corridor avoidance;
- deterministic port/starboard decisions for hazards directly ahead;
- formation slots transformed from leader-local to world space;
- leader translation and rotational velocity inheritance;
- stuck detection with timed reverse-and-turn recovery;
- optional direct application to the native construct, or advisory commands for the gameplay systems.

The gains, continuous kinematics, formation controller and obstacle scoring are M4K adaptations. They are not presented as recovered Bukkit constants.

## Authority and control flow

1. The NavyCraft route or navigation command selects manual, hold, route or formation mode.
2. Lua configures the native controller from the craft type, dimensions and current top speed.
3. Terrain raycasts submit short-lived observer-local hazard records.
4. The native controller also considers every other active construct as a moving obstacle.
5. The controller emits forward speed, vertical speed and yaw-rate commands.
6. Lua applies those commands through the normal construct movement fields.
7. Existing engine, gear, flooding, buoyancy and craft-type systems remain responsible for the vessel's actual capabilities.

The native controller does not create engine power or bypass a disabled vessel. It is a helmsman, not a second propulsion simulation.

## Movement domains

### Surface and ground

Navigation is solved in the horizontal plane. Vertical motion remains controlled by water support, terrain and the craft's existing gameplay systems.

### Air

The controller solves horizontal steering and a bounded vertical command independently. A craft directly above or below a waypoint climbs or descends without drifting along its previous heading.

### Submersible

Submarine routes use the same independent horizontal and vertical control, while the existing ballast and depth systems continue to determine whether the requested depth change is physically available.

## Obstacle avoidance

Terrain observations are keyed by both observer construct and local observation ID. Multiple vessels can therefore use the same small ray index range without overwriting one another.

For each obstacle, M4K examines:

- predicted closest approach using relative velocity;
- distance from the intended lookahead corridor;
- combined vessel and safety radii;
- time horizon derived from speed and lookahead distance;
- urgency and preferred avoidance side.

Dynamic construct-to-construct separation is entirely native. Static terrain currently enters through the server-side Lua `core.raycast` bridge.

## Formation movement

A follower stores a leader construct ID and a leader-local offset. The engine transforms that slot through the leader's current yaw and adds the velocity created by leader rotation. This prevents the follower from chasing a stale world coordinate while the formation turns.

## Lua controls

```text
/ship navigation status
/ship navigation manual
/ship navigation hold
/ship navigation route
/ship navigation loop on|off
/ship navigation avoidance on|off
/ship navigation formation <craft-id> [x y z]
/nc_navigation
```

`/ship nav` is an alias for `/ship navigation`. Existing route creation and binding commands remain valid; binding a route enables native route mode when protocol capability version 8 is present.

## Native Lua API

```lua
core.configure_dynamic_construct_navigation(construct_id, definition)
core.set_dynamic_construct_route(construct_id, waypoints, loop)
core.observe_dynamic_construct_obstacle(construct_id, obstacle)
core.step_dynamic_construct_navigation(delta_seconds, current_time)
core.get_dynamic_construct_navigation(construct_id_or_nil)
core.set_dynamic_construct_navigation_enabled(construct_id, enabled)
core.clear_dynamic_construct_navigation(construct_id)
```

Protocol capability version 8 enables this API. M4K is server-only and adds no new client packet opcode. It controls constructs through the existing transform replication path.

## Verification completed

- straight route following;
- waypoint advancement and route completion;
- braking and corner lookahead;
- terrain-obstacle avoidance;
- observer-local obstacle ID isolation;
- dynamic construct separation;
- formation slot transform and velocity inheritance;
- air-domain vertical-only waypoint control;
- stuck detection and reverse/turn recovery;
- native Lua configuration, route and command APIs;
- route binding and navigation commands in the gameplay smoke test;
- release, compatibility, platform and sanitizer builds.

## Runtime boundary

Still not completed:

- linking and running the complete patched Luanti 5.16.1 executable;
- long-range global path planning around islands or enclosed terrain;
- maritime right-of-way, team and friendly-formation policy;
- current, wind and sea-state compensation;
- real multiplayer latency and large-fleet tuning;
- stress testing of hundreds of simultaneously navigating vessels.
