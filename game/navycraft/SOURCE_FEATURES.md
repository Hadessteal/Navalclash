# NavyCraft authoritative-source audit — Milestone 3

## Source inputs

This implementation was built from the two JAR files supplied by Brett Martinelli. The JARs are treated as the functional specification; no replacement feature list was invented.

| Input | SHA-256 | Finding |
|---|---|---|
| `Navycraft-iizdev-stained_glass-0.0.1(1).jar` | `92cce764895f533c6ef889afd8bd45094469b2707c272cb8d5c414a428c3142b` | Contains 38 Java source files plus compiled classes for NavyCraft, AimCannon, torpedoes, pumps, periscopes, movement and commands. |
| `NavyCraft-Shipyard-3.1.5 (1).jar` | `f6bccc1bd70a688c4b216065487ddee2aaf0387ee536f61100e2c22920c8f1be` | Contains `com/maximuspayne/navycraft/shipyard/Shipyard.class` (69,480 bytes), `plugin.yml` and manifest. |

The earlier M2 statement that the Shipyard class was absent was incorrect and is superseded by this audit.

## NavyCraft features found and represented

### Craft system

- Boat, Ship, Freeship, Halfship, Aircraft, Airship, Submarine and Tank.
- Source minimum/maximum block limits, maximum speeds, gears, cruise capability, flight/diving/terrestrial flags and turn radius.
- Face-connected construction scanning.
- Metadata/inventory preservation.
- Smooth transform state with translation, yaw and vertical velocity.
- Docking, disabling, removing, destroying, scuttling and restart persistence.
- Captain, driver, crew history, abandonment, delayed release and takeover.
- Remote control and universal remote item.
- Waypoints, autotravel, route IDs/stages and automatic craft.
- Hyperdrive/hyperspace movement multiplier.

### Engines and movement

All 20 source engine definitions are included with their IDs, class, power, maximum speed and cost data:

- Diesel 1–3
- Motor 1
- Boiler 1–3
- Gasoline 1–2
- Nuclear
- Airplane 1–8 in source order
- Tank 1–2

Engine compatibility, set-on/actual-on state, surface/submerged operation, throttle, gear, rudder, dive planes and engine telegraph controls are implemented.

### Buoyancy, damage and recovery

- Source displacement constants: block 1.0, air 15.0, minimum 0.33 and weight multiplier 1.0.
- Enclosed-air flood-fill detection within a bounded craft volume.
- Ballast closed/flood/blow/automatic modes.
- Surface/submerged drive selection.
- Flooding, abstract pump charge, hull integrity, helm destruction and sinking.
- Three-minute scuttle/disable timer.
- Launch-time repair snapshot and Shipyard repair restoration.
- Per-block construct damage and armour groups.

### Sensors and communications

- Periscope.
- Radar and short-range detector.
- Hydrophone/passive sonar.
- Active sonar.
- High-frequency sonar.
- Contact range, bearing and signal strength.
- Target selection.
- Four stored radio channels, channel selector, radio on/off and crew chat.

### Weapons

The source `cannonType` mapping is preserved:

0. Single Cannon
1. Double Cannon
2. Fireball Cannon
3. Torpedo Mk II
4. Depth-Charge Dropper
5. Depth-Charge Launcher Mk II
6. Triple Cannon
7. Torpedo Mk III
8. Torpedo Mk I
9. Bomb Dropper

Also implemented:

- Source four-second weapon timeout.
- Source torpedo explosive powers: Mk I 7, Mk II 10 and Mk III 14.
- Source depth-charge explosive power 10.
- Cannon range setting 10–200.
- Launcher safe/armed state.
- Fire-control selection.
- TDC relative heading and depth.
- Torpedo tube state and firing.
- Guided Mk III target correction.
- AA gun and AA ammunition.
- World-node, player and moving-construct damage.
- `/explode` and diagnostic `/explodesigns` equivalents.

Projectile speeds and ranges are explicitly a Luanti gameplay adaptation because Bukkit tick displacement cannot be transferred directly as physical units.

### Vehicle storage and source sign equivalents

- `*select*` control.
- `*claim*` control.
- `*recall*` control.
- `*spawn*` control.
- `/ship store`, stored-vehicle listing, selection, recall, spawn, deletion and repair.
- Ten-minute source teleport/recall cooldown.
- Persistent blueprints containing nodes, metadata, systems and launch repair snapshots.

### Shipyard 3.1.5

The implementation class was inspected directly. The following source command surface is represented:

- `reward`
- `list`
- `tp`
- `open`
- `info`
- `addmember`
- `remmember`
- `clear`
- `rename`
- `public`
- `private`
- `player`
- `plist` / `playerlist`
- `ptp`

Source lot mapping is preserved:

- `DD` → `SHIP1`
- `SUB1` → `SHIP2`
- `SUB2` → `SHIP3`
- `CL` → `SHIP4`
- `CA` → `SHIP5`
- `HANGAR1`
- `HANGAR2`
- `TANK1`
- `TANK2`

Also represented:

- Claim controllers and plot rewards.
- Open-plot discovery.
- Ownership and members.
- Public/private state.
- Plot teleporting, clearing and renaming.
- Player reward counts.
- Experience, rank, payday and balances.
- Craft experience rewards.

## Necessary platform adaptations

These are not claimed to be literal source behaviour:

1. The original Shipyard class expects a prebuilt `shipyard` world, WorldGuard regions, WorldEdit clearing and PermissionsEx rank/pay nodes. The Luanti implementation uses native persistent axis-aligned plot boxes, a plot wand, native node clearing and configurable rank defaults.
2. Essentials economy and paid sign purchases do not yet have an external economy backend. `/sign undo` exists but reports that no paid transaction is pending.
3. Bukkit material IDs are represented by original NavyCraft hull, control, engine, ammunition and weapon nodes rather than copying Minecraft assets.
4. Projectile speed/range values are Luanti balancing values; weapon identities, salvo counts, cooldowns and verified explosive strengths come from the source.
5. Moving vessels use only the patched native Luanti construct renderer and collision pipeline.

## Smooth-engine work included

The LGPL engine overlay now contains:

- Dynamic construct registry and local voxel storage.
- Transform/velocity state.
- Versioned binary serialization.
- Lua lifecycle API.
- 16×16×16 construct section partitioning, including correct negative-coordinate floor division.
- Deterministic section node order and revision tracking.
- Network transform snapshot interpolation using the shortest yaw path.
- Capped transform extrapolation for client prediction.

The remaining production work is connecting sections to Luanti client meshing, packet replication, rotatable collision, ship-local raycasting and moving-platform player physics.

## Milestone 5A original gameplay layer

The career, port-contract, crew-station and quartermaster systems in `nc_campaign`
are original Luanti gameplay work. They are not claimed as recovered behavior from
the supplied Bukkit JARs. They use the source-derived craft, rank, Shipyard,
weapon and vessel-control systems as their foundation.

## Milestone 5B original gameplay layer

Faction diplomacy, territorial control, generated NPC fleets and the Frontier
Watch operation chain are original Luanti gameplay systems. They are not claimed
as recovered Bukkit NavyCraft behavior. NPC vessels deliberately reuse the
source-derived craft, weapon, engine and control definitions plus the native
moving-construct backend rather than introducing a separate arcade simulation.

## Milestone 5C original gameplay layer

Resource sites, persistent port warehouses, stock-sensitive commodity pricing,
industrial queues, collection lockers and tiered vessel refits are original
Luanti gameplay systems. They are not claimed as recovered Bukkit NavyCraft
behavior. The refits intentionally modify the source-derived propulsion, pump,
sensor and weapon-reload calculations rather than replacing those systems.

## Milestone 5D original gameplay layer

Campaign vessel classes, persistent blueprint research, class certification,
cargo-capacity and cargo-mass rules, operating condition, scheduled maintenance,
and cargo/armour refit trade-offs are original Luanti gameplay systems. They are
not claimed as recovered Bukkit NavyCraft behavior. The broad craft types,
Shipyard lot names, engine definitions, weapon identities and rank ladder remain
source-derived foundations; the finer class envelopes and balance values are
explicit gameplay adaptations.


## Milestone 5E original gameplay layer

Persistent NPC shipping, merchant freighter composition, escort-coverage
contracts, Corsair convoy raids, strategic port supplies, service shortages and
warehouse-linked deliveries are original Luanti gameplay systems. They are not
claimed as recovered Bukkit NavyCraft behavior. Freighters, escorts and raiders
reuse the source-derived craft, propulsion, weapon and damage systems plus the
native moving-construct backend.

## Milestone 5F original gameplay layer

Persistent vessel titles, accepted ownership transfers, broker escrow, marine
insurance, hostile capture, wreck rights, salvage recovery and permanent-loss
modes are original Luanti gameplay systems. They are not claimed as recovered
Bukkit NavyCraft behavior. The older source-derived abandonment and takeover
concepts remain part of the vessel-control foundation, while M5F adds the
server-authoritative economic and legal lifecycle required for a persistent game.

## Milestone 5G original gameplay layer

Guided onboarding, role-aware HUDs, integrated station formspecs, accessibility
preferences, colour-vision palettes, subtitles, reduced-motion preferences and
the server-owned alert queue are original Luanti gameplay systems. They are not
claimed as recovered Bukkit NavyCraft behavior. The station interface controls
the same source-derived and native vessel systems used by the command layer.
