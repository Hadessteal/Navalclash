# Milestone 5A — playable career loop

## Goal

Turn the completed moving-vessel backend into a repeatable playable loop:

1. enlist and receive a small starting grant;
2. accept a contract at a naval port;
3. crew and operate a vessel using the existing NavyCraft systems;
4. complete travel, cargo, patrol or combat objectives;
5. receive credits, experience, reputation and crew shares;
6. spend rewards on ammunition, repair parts and port services;
7. unlock harder contracts and vessel categories through rank progression.

## Career state

`nc_campaign` stores a versioned career record for each player containing:

- enlistment state;
- navy, merchant and salvage reputation;
- distance, mission, combat, cargo and economy statistics;
- completion counts by mission family;
- rank-derived unlock reporting.

Credits and experience remain canonical in `nc_shipyard`. M5A extends the Shipyard account API with atomic credit/debit helpers, balances, rank access and a bounded transaction ledger. This avoids maintaining two competing money balances.

## Ports

Administrators register ports at their current position with `/port create`. A port has:

- stable ID and display name;
- centre and service radius;
- faction;
- mission, repair and ammunition service flags.

Mission boards and quartermaster terminals only work inside a registered port radius.

## Contract types

### Commissioning sea trial

Travel 250 metres in an active vessel and return to any port. This is the starter contract and proves basic launch, movement and docking functionality.

### Sealed naval dispatch

Take a physical sealed-dispatch item from the origin port to another registered port. Completion requires both the vessel and the cargo to arrive.

### Maritime patrol

Travel from the origin to a destination port, then return to the origin. Progress is stage-based and survives server shutdown.

### Combat-readiness patrol

Inflict a required amount of authoritative construct block damage. Progress is fed from the existing native projectile and vessel-damage path rather than client claims.

## Crew stations

M5A adds gameplay station assignments without replacing the source-derived NavyCraft ownership and crew authorization system:

- captain;
- executive officer;
- helm;
- engineer;
- gunner;
- sensor operator;
- quartermaster;
- deck crew;
- passenger.

Travel and explicit station activity build contribution points. Mission completion grants the mission owner the full contract reward and creates an additional contribution-weighted crew pool worth 25 percent of the base credits and experience.

## Economy

Quartermaster services include:

- cannon shells;
- AA ammunition;
- torpedoes;
- depth charges;
- bombs;
- repair parts;
- paid active-vessel repair;
- pump-charge replenishment.

Rank requirements can restrict advanced stock. Purchases debit the canonical Shipyard balance. Failed inventory insertion or repair automatically refunds the charge.

## Persistence and authority

Career, port and mission state use Luanti mod storage. Progress is updated from server-side vessel transforms, docking hooks and damage hooks. Client input cannot directly award mission completion.

## Deliberate boundaries

M5A is the first playable loop, not the complete game economy. It does not yet include:

- factions and territorial control;
- NPC fleets and authored campaign chains;
- dynamic commodity markets;
- insurance, ship auctions or player trading;
- production chains and resource extraction;
- matchmaking or competitive seasons;
- final UI, tutorial presentation or localisation.

Those are gameplay/content milestones, not missing moving-construct backend families.
