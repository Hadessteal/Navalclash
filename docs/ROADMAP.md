# Engineering roadmap

## M0 — foundation

- [x] Pin Luanti 5.16.1.
- [x] Construct IDs, transforms and registry.
- [x] Local/world transform tests.
- [x] Game scaffold and hull scanner.
- [x] Storefront-neutral adapter ABI.

## M1A — lifecycle and API prototype

- [x] Local voxel records.
- [x] Linear and yaw motion.
- [x] Bounds, snapshots and persistence codec.
- [x] Initial native Lua API.
- [x] Temporary stock-Luanti renderer.

## M1B — native rendered construct

- [x] 16x16x16 section partitioning.
- [x] Incremental dirty-section tracking.
- [x] Exposed-face section geometry generation.
- [x] Transform and section packet codecs.
- [x] Client packet-to-voxel and packet-to-mesh state manager.
- [x] Snapshot interpolation and capped extrapolation.
- [x] Irrlicht section mesh adapter.
- [x] Luanti MapBlockMesh material/drawtype renderer for construct sections.
- [x] Animated tile stepping and scene-node transparency bridge.
- [x] Construct-local positional sounds, particles and gameplay effect events.
- [x] Server-authoritative projectile motion, guidance, replication and construct damage.
- [x] Native target tracks, lead prediction, ballistic/TDC solutions and automatic batteries.
- [x] Native route following, arrival braking, obstacle avoidance, formation control and stuck recovery.
- [x] Luanti client packet handlers.
- [x] Luanti server peer broadcasts.
- [x] SQLite native world-save integration and construct-local action journal.
- [ ] Full custom Luanti client/server compile and runtime pass.

Acceptance test: a 10x4 raft launches, moves smoothly for two clients, rotates and docks without entity-per-block rendering. **Not yet reached.**

## M2 — walkable deck

- [x] Yaw-aware construct broad phase.
- [x] Local voxel collision queries.
- [x] Swept AABB collision core.
- [x] Platform local-anchor tracking.
- [x] Translation and yaw displacement inheritance.
- [x] Contact detection in Luanti player physics overlay.
- [x] Walking, jumping and edge transition integration foundation.
- [x] Client rider prediction and server reconciliation foundation.
- [x] Rotated side, floor and ceiling collision response.
- [x] Remote-player, creature and dropped-item platform inheritance.

## M3 — moving interaction

- [x] Construct-local DDA raycast core.
- [x] Native local node edit API.
- [x] Merge construct hits with Luanti PointedThing distance selection.
- [x] Dig/place protocol with construct ID and local coordinate.
- [x] Construct-local metadata, inventory-list storage, node timers and explicit callbacks.
- [x] Transactional digging/placement, formspec inventories, protection and rollback.
- [x] Dynamic construct-local liquid simulation and specialised moving-node behaviours.
- [x] Hierarchical revolute/prismatic machinery and articulated rendering.

## M4 — NavyCraft gameplay alpha

- [x] Source-derived craft, engine, combat, sensor, storage, route, radio and Shipyard prototype systems.
- [x] Native construct gameplay path implemented; stock Luanti retains a fallback renderer.
- [x] Projectile breaches, flooding and damage tied to native local-node edits.
- [x] Structural connectivity, detached construct fragments and independent wreck buoyancy.
- [x] Fire-control/TDC command and moving-control integration.
- [x] Native waypoint, hold, formation and local collision-avoidance integration.
- [x] Turret traverse animation and line-of-fire obstruction.
- [ ] Dedicated server runtime testing.

## M4N — backend feature-complete gate

- [x] Articulated platform collision and rider inheritance.
- [x] Persistent construct-local liquids, compartments, pumps, drains and breaches.
- [x] Liquid transfer to detached wreck fragments.
- [x] Specialised climbable, breathable, damaging, permeable and conveyor node definitions.
- [ ] Complete patched Luanti executable compile and live multiplayer runtime pass.
- [ ] Stress, soak and performance optimisation.

After the executable integration pass, development shifts primarily to gameplay systems, content, balance and presentation.

## M5 — playable gameplay phase

### M5A — career and contract loop

- [x] Persistent career, credits, experience, ranks and reputation.
- [x] Registered naval ports and service radii.
- [x] Sea-trial, courier, patrol and combat contracts.
- [x] Crew station assignments and contribution-weighted reward sharing.
- [x] Mission boards, quartermaster terminals and port purchases.
- [x] Paid repair and pump recharge services.
- [x] Gameplay persistence and authoritative travel/damage progress hooks.

### M5B — factions and fleet operations

- [x] Persistent faction allegiance, diplomacy and reputation integration.
- [x] Port-centred territorial objectives and capture progress.
- [x] Generated NPC patrol and gunboat fleets using normal vessel systems.
- [x] Persistent encounter tracking and authoritative defeat handling.
- [x] Connected three-stage Frontier Watch operation.
- [x] PvE mission rewards, rank gates and operation completion bonuses.

### M5C — resources, industry and markets

- [x] Persistent finite resource sites and physical extracted cargo.
- [x] Port commodity warehouses with finite stock.
- [x] Scarcity, oversupply and faction-sensitive dynamic prices.
- [x] Persistent refining and manufacturing queues.
- [x] Player-specific completed-goods lockers at ports.
- [x] Tiered propulsion, pump, sensor and fire-control refits.
- [x] Live vessel-system modifiers from installed equipment.

### M5D — vessel progression, logistics and upkeep

- [x] Campaign vessel classes layered over source-derived craft categories.
- [x] Hull, structural-weight, engine and weapon-mount class validation.
- [x] Persistent starter and researchable blueprints.
- [x] Port vessel certification and class-limited equipment tiers.
- [x] Persistent cargo manifests, class capacity and commodity mass.
- [x] Cargo/equipment operating mass connected to propulsion and buoyancy.
- [x] Persistent condition, operating hours and service costs.
- [x] Maintenance penalties for propulsion, pumps, sensors and reload timing.
- [x] Cargo-handling and compartment-armour refit trade-offs.

### M5E — NPC logistics and strategic supply

- [x] Persistent inter-port shipments and physical warehouse debit/credit.
- [x] NPC merchant freighters and armed escort compositions.
- [x] Proximity-verified player escort contracts and rewards.
- [x] Corsair convoy raids and pirate-defeat accounting.
- [x] Persistent fuel, provisions, munitions, repair and industry supply.
- [x] Supply-sensitive prices, services, factory speed and territorial defence.
- [x] Automatic surplus-to-shortage convoy scheduling.

### M5F — ownership, insurance and salvage

- [x] Persistent vessel titles and ownership history.
- [x] Accepted direct ownership transfers with optional payment.
- [x] Server-owned vessel resale escrow and stored-title delivery.
- [x] Hull and comprehensive insurance policies, waiting periods and deductibles.
- [x] Covered destruction, sinking and capture claims.
- [x] Disabled-vessel boarding, contested capture and live title transfer.
- [x] Persistent wreck records and exclusive salvage rights.
- [x] Material salvage and standard-mode hulk restoration.
- [x] Casual, standard and strict permanent-loss configurations.

### M5G — onboarding and presentation

- [x] Persistent guided basic training.
- [x] Role-aware career, mission and vessel HUD.
- [x] Full, minimal and disabled HUD modes.
- [x] Integrated command, helm, engineering, weapons and sensors formspecs.
- [x] High-contrast, large-text and colour-vision presentation settings.
- [x] Subtitle, reduced-motion and alert-filter preferences.
- [x] Deduplicated alert queue and notification history.
- [x] Bridge console, training console and service handbook.

The planned gameplay-system milestones are now complete. Content expansion, balancing and live-runtime validation continue after the executable integration gate.

## M6 — storefront release pipeline

- [ ] Complete patched Luanti executable build and runtime pass.
- [ ] Multiplayer stress, soak and performance optimisation.
- [ ] Signed Windows and Linux builds.
- [ ] Crash reporting/privacy decision.
- [ ] Steam depot upload adapter.
- [ ] itch Butler channels.
- [ ] GOG, Epic and Humble package validation.
- [ ] Automated source offer and notices.
