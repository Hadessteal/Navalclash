# Milestone 2 — source-derived NavyCraft systems

This milestone changes the project from a generic vessel experiment into a NavyCraft-derived game foundation using the supplied Java source as authority.

## Implemented in this package

- Eight exact default craft profiles from `CraftType.java`.
- Twenty exact engine profiles from `CraftMover.java`, including power, maximum speed, vehicle class and original economy cost.
- Helm craft-type selection and validation.
- Smooth throttle, gears, rudder and vertical-plane simulation.
- Surface/submerged engine rules and reduced submerged diesel output.
- Ballast closed/flood/blow/auto modes.
- Source-style ±10% submarine neutral-buoyancy window.
- Physical moving controls through the temporary preview entities.
- Engine on/off controls before and after launch.
- Periscope, radar, sonar-family, launcher, radio and hyperdrive state.
- Basic construct-to-construct radar and sonar contacts.
- Owner/captain/crew state and open/crew/closed boarding filters.
- Component blocks for fire control, TDC, AA guns, pumps and the remaining source systems.
- Source audit documenting what is exact, adapted or blocked by missing source.

## Not yet complete

- Native section-mesh renderer and true moving-platform collision.
- Sealed-space/enclosed-air volume solving.
- Water ingress based on actual hull breaches.
- Pump lifespan and world-water removal.
- Full periscope camera implementation.
- Source-accurate sensor probability/noise models.
- Cannons, torpedo construction, tube doors, arming distance, depth charges and bombs.
- Autocraft routes, merchants and battle mode.
- Shipyard claim/lot/economy internals, because the supplied Shipyard JAR does not contain its declared implementation class.

## Test controls

Build a connected vessel using `nc_core:frame`, one `nc_navycraft:helm`, a navigation control and compatible engines. Sneak-right-click the helm to cycle its craft type; normal right-click launches.

While moving, right-click or punch physical component blocks, or use:

```text
/nc_types
/nc_throttle 0..100
/nc_gear -2..3
/nc_rudder -1..1
/nc_planes -1..1
/nc_ballast closed|flood|blow|auto
/nc_subdrive surface|submerged
/nc_engine all on|off
/nc_sensors
/nc_systems
/nc_boarding open|crew|closed
/nc_crew add|remove|list
/nc_dock
```
