# Milestone 5B — factions, operations and NPC fleets

## Goal

Expand the M5A contract loop into a persistent PvE campaign:

1. enlist with a playable faction;
2. operate from faction-aligned ports;
3. enter contested waters;
4. encounter server-controlled hostile vessels;
5. complete connected objectives rather than isolated jobs;
6. change territorial control through presence and combat;
7. receive career, reputation and operation rewards.

## Factions and diplomacy

M5B defines four factions:

- **Commonwealth Navy** — patrol, defence and anti-piracy service;
- **Merchant Marine** — trade, convoy and logistics interests;
- **Independent Salvage Guild** — recovery and wreck reclamation;
- **Corsair Coalition** — hostile NPC raiders and privateers.

Player allegiance is persistent. The career reputation table remains the canonical standing record, while `factions.lua` stores membership and diplomatic relationships. Hostility is evaluated from both factions' relationship tables.

## Territories

Every registered port can act as the centre of a territorial zone. Territory state includes:

- stable port/territory ID;
- centre and radius;
- controlling faction;
- current challenger;
- capture progress;
- last control-change time and reason.

The controller samples active vessels and derives pressure from surviving hull size and weapon count. Friendly presence reduces hostile capture progress. Hostile presence advances a challenge when it exceeds the defending strength. Authoritative vessel victories can also contribute influence.

Territory data persists in mod storage and updates the associated port faction when control changes.

## NPC fleets

`fleets.lua` generates small patrol and gunboat snapshots from normal NavyCraft nodes. Each NPC vessel has:

- hull, helm, engine, radar, pump and weapon nodes;
- a normal NavyCraft profile and systems record;
- a unique runtime owner;
- faction and encounter metadata;
- normal damage, flooding and sinking state.

The AI controller supplies rudder, throttle, target and fire-control choices. Weapons are fired through `navycraft.weapons`, so projectiles, hit detection, armour, breaches, fragments and effects remain on the existing authoritative paths.

Encounter records track active, destroyed, lost and failed units. An encounter completes only after no active units remain.

## Connected operation: Frontier Watch

### Stage 1 — Chart the Frontier

Enter the destination territory, record its controlling faction, then return to the origin port for debrief.

### Stage 2 — Break the Raider Screen

A persistent Corsair encounter is created near the frontier. The mission completes after every encounter vessel is defeated.

### Stage 3 — Secure the Anchorage

Establish the player's faction as territorial owner and hold the waters for twenty seconds. Leaving the zone or losing control interrupts the hold timer.

Each stage has a rank gate and its own reward. Completing all three stages grants an additional operation bonus and permanently records completion.

## Commands

```text
/faction list
/faction status
/faction join navy|merchant|salvage

/territory list
/territory status [port]
/territory set <port> <faction>
/territory contest <port> <faction> <amount>

/operation list
/operation accept [offer-id]
/operation status

/fleet list
/fleet status <id>
/fleet spawn <faction> [difficulty]
/fleet clear <id>
```

Territory and fleet mutation commands require the server privilege.

## Persistence and authority

Faction membership, territories, encounter metadata and operation progress use Luanti mod storage. Active NPC constructs remain covered by the existing construct persistence path.

Mission completion is derived from server-side construct positions, encounter state, territorial state and authoritative damage hooks. Clients cannot claim a defeated fleet or captured port directly.

## Deliberate boundaries

M5B provides repeatable PvE encounters and one complete operation chain. It does not yet include:

- authored dialogue or cinematic presentation;
- global strategic pathfinding between arbitrary islands;
- a production economy or commodity simulation;
- faction-owned shipyards and reinforcement budgets;
- sophisticated fleet formations or retreat doctrine;
- procedural world-region generation;
- final difficulty and reward balancing.

Those belong to later gameplay and content milestones.
