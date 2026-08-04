# Milestone 5D — vessel classes, blueprints, cargo and maintenance

## Goal

M5D turns the free-form moving constructs into a progression system without removing NavyCraft's block-built identity. A hull is still constructed node by node, but its size, structural weight, engines and weapon mounts determine which campaign vessel class it can be certified as.

The milestone adds four linked loops:

1. build a hull that fits a class envelope;
2. unlock and certify its blueprint at a naval port;
3. load cargo within the class capacity and accept the resulting mass penalty;
4. operate, maintain and refit the vessel over its service life.

The moving-construct protocol remains at capability 11. M5D is a server-authoritative gameplay layer.

## Vessel classes

The broad source-derived craft types remain unchanged: boat, ship, freeship, halfship, aircraft, airship, submarine and tank. M5D adds campaign classifications within those categories.

Surface classes include motor launch, patrol boat, patrol cutter, corvette, frigate, destroyer, cruiser and fleet carrier. Submarine, aircraft, airship and armoured-vehicle class ladders are included as well.

Each class defines:

- compatible NavyCraft craft type;
- minimum and maximum hull blocks;
- maximum structural weight;
- engine and weapon-mount limits;
- base cargo capacity;
- recommended crew complement;
- Shipyard lot association;
- career-rank requirement;
- maximum equipment tier by slot;
- maintenance cost scale.

A damaged vessel is not reclassified merely because it has temporarily lost blocks. Classification uses the commissioned hull size, while repair and combat systems continue to use live block counts.

New vessels receive a provisional best-fit class. Certification at a port makes the classification persistent and checks the complete class envelope.

```text
/vesselclass list
/vesselclass status
/vesselclass certify patrol_cutter
```

## Blueprint progression

Every career receives starter blueprints for small launches, patrol boats and patrol cutters. Higher classes require persistent research at a port with shipyard service.

Research consumes:

- career credits;
- the required rank;
- physical manufactured components such as hull plates, machinery and electronics.

Unlocks persist per player. A blueprint unlock does not automatically create a vessel: the player must still build a compatible hull and have it certified.

```text
/blueprint list
/blueprint status
/blueprint research corvette
```

## Cargo holds and operating mass

Commodities can now be transferred from a player's physical inventory into the commanded vessel's persistent cargo manifest.

Each commodity has cargo-space and mass values. The server calculates:

- occupied cargo units;
- class and equipment-adjusted capacity;
- cargo mass;
- equipment mass;
- total operating mass;
- overload state.

Operating mass feeds the existing propulsion and buoyancy calculations. An overloaded vessel receives an additional speed penalty.

Cargo can be loaded or unloaded only when both the player and vessel are inside the same registered port.

```text
/cargo status
/cargo load iron_ore 20
/cargo unload iron_ore 5
```

## Maintenance and operating cost

Every active vessel now records persistent condition, operating hours, accrued service cost and completed service count.

Wear accrues while engines are operating. High speed, overload and demanding equipment increase strain. Poor condition reduces:

- top speed;
- pump output;
- sensor range;
- weapon reload readiness.

Scheduled service requires the vessel to be inside the player's port, consumes credits and—when condition is low—repair parts. It restores condition and clears accrued service cost.

```text
/maintenance status
/maintenance service
```

M5D does not use random catastrophic failures. Condition creates predictable performance degradation so players can plan maintenance rather than lose a vessel to an invisible dice roll.

## Equipment balance pass

Existing refits now have mass and maintenance trade-offs. M5D also adds:

- Tier I and II cargo-handling refits;
- Tier I and II compartment-armour refits.

Cargo handling expands capacity but adds machinery mass. Compartment armour reduces additional flooding from severe hits, but increases vessel mass and reduces speed. Class certification limits which slots and tiers can be installed.

This does not reduce native block destruction from M4I; the armour refit affects campaign flooding and survivability state. Structural armour remains determined by the actual registered node materials in the native projectile engine.

## Authority and persistence

The server owns blueprint unlocks, certifications, cargo manifests, equipment, operating mass, condition, operating hours and service payments. These values are stored in career or vessel persistence and cannot be completed by a client report.

## Deliberate boundaries

M5D does not yet include:

- graphical cargo containers or visible deck loading;
- dry-dock construction templates that automatically place a blueprint;
- insurance, vessel resale or permanent-loss rules;
- crew wages and food consumption;
- final commercial balance values;
- automated NPC freight loading;
- a polished shipyard design formspec.
