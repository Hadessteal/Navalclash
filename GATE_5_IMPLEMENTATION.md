# Gate 5 — Swept construct broad-phase and predictive rider authority

Gate 5 permanently replaces all-construct collision scans and exact rider-position correction with a bounded native collision index and motion-aware client/server rider state.

## Swept construct collision world

`ConstructCollisionWorld` is rebuilt from the sampled native construct transforms. Each entry stores:

- the current world-space construct bounds;
- conservative predicted bounds using linear velocity;
- conservative yaw travel expansion;
- deterministic construct-ID ordering.

Local-player movement queries a swept player AABB and receives only relevant constructs. A currently supporting construct is always retained, even when interpolation or packet delay temporarily places its broad-phase bounds outside the query. This removes the need to test player geometry against every active vessel each frame without risking loss of an established deck contact.

## Stable deck support

Support selection now accepts the previous support node as a preferred candidate. A small deterministic bias keeps that node selected at adjacent deck seams unless another surface is clearly better. Rider state records the preferred support node, uses tighter contact tolerances, ignores sub-slop height errors, and clamps larger vertical corrections.

This prevents the support solver from alternating between neighbouring hull blocks or repeatedly detaching and reacquiring contact because of tiny floating-point gaps.

## Predictive rider packets

Rider packet codec version 3 adds construct-local rider velocity. The client transmits rider state when contact changes, anchor or local velocity changes materially, jump is pressed, or the periodic authority interval expires.

For ordinary hull riders the client derives local velocity after subtracting construct surface motion. Articulated riders send zero local velocity because their joint motion is already represented by the articulated transform.

## Server rider authority

The server now separates two responsibilities:

1. carry the authoritative player by the actual change in the platform anchor;
2. reconcile gradually toward the predicted local rider path.

The server validates local rider speed, limits prediction age, applies bounded corrections, and reserves hard snaps for large invalid divergence. It no longer writes the exact predicted rider position every simulation tick.

## Protocol change

The native construct protocol is now **14**. Gate 5 clients and servers must agree on protocol 14 because rider packets now contain local velocity and use the revised authority semantics.

## Removed temporary paths

No Lua hull renderer, Lua collision entity, Lua player teleport, or manual Lua construct simulation exists in the loaded game. Gate 5 extends only the permanent native implementation.

## Release status

Gate 5 is a verified source checkpoint. A complete exact-source Windows client/server compile and real runtime collision and multiplayer pass are still required before a player-facing build is released.
