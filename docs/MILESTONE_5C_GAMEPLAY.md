# Milestone 5C — resources, industry, equipment and dynamic markets

## Goal

M5C adds a production-and-trade loop to the M5A/M5B career campaign:

1. locate a registered resource site;
2. extract physical raw cargo;
3. refine it at an industrial port;
4. manufacture components and refit kits;
5. install the kits on an active vessel;
6. trade surplus commodities between ports whose prices respond to stock.

The moving-construct protocol remains at capability 11. This milestone is a server-authoritative Lua gameplay layer.

## Resource sites

Administrators can register persistent iron, coal, copper, oil and salvage fields. Each site records its position, extraction radius, remaining reserve, total extracted material and last operator.

Extraction creates physical inventory items and reduces the authoritative reserve. It cannot be completed from outside the site radius.

```text
/resource create iron_bank iron 900
/resource list
/resource extract iron_bank 4
```

M5C does not procedurally place deposits in world generation. Resource-site placement is an administration/content-authoring step.

## Port inventories and commodity markets

Every port gains a persistent warehouse with finite capacity and stock for eleven commodities:

- iron ore;
- coal;
- copper ore;
- crude oil;
- recovered salvage;
- steel ingots;
- copper ingots;
- fuel drums;
- hull plates;
- machinery parts;
- marine electronics.

Buy and sell prices are calculated from current stock, target stock, commodity base value and the port faction. Scarce goods become expensive; oversupplied goods become cheaper. Market ticks add small faction-specific production and consumption drift.

```text
/market list
/market buy iron_ore 10
/market sell fuel_drum 4
/market routes
/warehouse status outpost
```

Goods remain ordinary physical inventory items during transport. A player cannot sell cargo they do not possess.

## Refining and manufacturing

Industrial jobs consume physical inputs immediately, run on a persistent timer and place completed output in a player-specific locker at the originating port. Completed goods must be collected at that port.

The production chain includes:

- ore and coal to steel;
- copper ore and coal to copper ingots;
- crude oil and coal to fuel;
- steel to hull plates;
- steel and copper to machinery parts;
- copper and salvage to electronics;
- components to propulsion, pump, sensor and fire-control refit kits.

```text
/industry list
/industry queue steel_ingot 4
/industry status
/industry collect
```

Jobs, remaining time and completed lockers persist through server restarts.

## Vessel equipment tiers

Manufactured kits can be installed only while the player is inside a port with shipyard service and commands an active vessel.

Implemented slots and effects:

- **propulsion** — increases calculated vessel top speed;
- **damage-control pumps** — increases flooding-removal rate;
- **sensor suite** — increases radar, detector and sonar range;
- **fire control** — reduces weapon reload delay.

Tier II packages replace Tier I packages and require higher career ranks. Equal or lower-tier installations are rejected before consuming the kit.

```text
/equipment list
/equipment status
/equipment install engine_t1
```

Installed equipment and derived modifiers are stored in the vessel systems snapshot.

## Authority and persistence

The server owns:

- resource reserves;
- port warehouse stock;
- market prices;
- player credits;
- manufacturing queues and lockers;
- installed vessel equipment.

Clients cannot report extraction, manufacture completion, cargo sales or refit installation as completed.

## Deliberate boundaries

M5C does not yet include:

- procedural mineral or oil-field generation;
- graphical cargo holds (freight mass and class capacity are implemented in M5D);
- automated NPC freighters moving stock between ports;
- player-owned factories or corporations;
- regional supply shocks from destroyed convoys;
- final commodity values, production times or balance;
- a polished market or factory formspec.

Those are content, balancing and presentation work for later gameplay milestones.
