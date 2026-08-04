# Milestone 5E — NPC logistics, convoys, piracy and strategic supply

## Goal

M5E makes the economy and faction map depend on physical shipping. Ports no longer behave as isolated infinite service menus. Commodities must leave one warehouse, survive a voyage aboard an NPC freighter, and arrive before the destination receives the stock or strategic benefit.

The construct protocol remains at capability 11. M5E is a server-authoritative gameplay layer using the existing native vessel, navigation, combat, damage and persistence systems.

## Merchant convoys

A shipment records:

- origin and destination ports;
- faction ownership;
- physical commodity manifest;
- cargo value and route distance;
- calculated piracy risk;
- convoy encounter and freighter identity;
- escort contract holder and proximity coverage;
- pirate encounter and defeated-raider count;
- scheduled, en-route, delivered or lost state.

Stock is debited from the origin warehouse at dispatch. A convoy contains a freighter and one or more armed escorts. These are normal NavyCraft moving constructs with engines, navigation, weapons, hull damage, flooding and structural destruction.

The destination warehouse is credited only after the freighter physically reaches the destination port. Destroying the freighter resolves the shipment as lost.

## Escort contracts

Players can accept an open convoy at its origin port. Escort credit accumulates only while the player's commanded vessel remains within 120 nodes of the freighter. Accepting a contract and then leaving does not earn the reward.

Successful escorts receive:

- credits based on route distance and cargo value;
- career experience;
- Merchant Marine and Navy reputation;
- an additional anti-piracy bonus when raiders are defeated.

Insufficient proximity coverage produces no escort payment even if another force completes the route.

## Piracy

Routes have a risk value based on distance and territorial conditions. Convoys can automatically attract Corsair raiders, and administrators can trigger a controlled attack for events or testing.

Pirate groups track the convoy encounter and use the normal NPC combat path. Raider destruction is recorded for escort bonuses. A surviving pirate force does not magically transfer cargo; the strategic loss is represented by the destroyed freighter and missing destination delivery.

## Strategic port supply

Each port persists five supply levels from 0 to 100:

- fuel;
- provisions;
- munitions;
- repair capacity;
- industrial capacity.

Delivered commodities replenish one or more categories. Examples:

- fuel drums replenish fuel;
- provisions replenish provisions;
- munitions crates replenish munitions;
- hull plates and machinery replenish repair capacity;
- machinery and electronics replenish industrial capacity.

Ports consume supply gradually. Contested ports consume it faster.

## Gameplay effects

Strategic supply is connected to existing systems:

- low relevant supply raises commodity demand and prices;
- ammunition sales consume munitions stores;
- repairs, pump recharge and maintenance consume repair capacity;
- low industrial stores slow persistent factory jobs;
- critically depleted services refuse new work;
- port defenders receive a supply-derived territorial strength multiplier.

These effects are deterministic and visible through `/supply status`.

## Automatic shipping

The server periodically compares port warehouses. It selects routes where an origin has surplus and a friendly destination has a shortage, then constructs a mixed manifest. A small active-convoy cap prevents unattended fleet growth.

Administrators can dispatch explicit manifests for scenarios:

```text
/convoy dispatch home outpost fuel_drum 8 provisions 12 munitions_crate 4
```

## Commands

```text
/supply list
/supply status [port]
/supply set <port> <category> <0-100>

/convoy list
/convoy status <id>
/convoy accept <id>
/convoy dispatch <origin> <destination> [commodity amount...]
/convoy raid <id> [difficulty]
```

Supply mutation, convoy dispatch and forced raids require server privilege.

## New commodities and production

M5E adds physical provisions and naval munitions crates. Munitions crates can be manufactured through the existing industry queue. Both commodities can be bought, sold, carried in vessel cargo and shipped by NPC convoys.

## Authority and persistence

The server owns shipment creation, origin-stock debit, NPC fleet state, arrival checks, escort coverage, losses, rewards, destination-stock credit and strategic supply. All shipping and supply records persist in mod storage.

## Deliberate boundaries

M5E does not yet include:

- visible cargo-container meshes loaded onto freighter decks;
- boarding or capturing merchant ships;
- pirate resale markets for stolen cargo;
- a world-scale ocean route graph around islands;
- crew wages, ration consumption aboard individual player vessels;
- final commercial economy balance values.
