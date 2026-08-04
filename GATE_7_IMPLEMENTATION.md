# Gate 7 — mandatory native protocol negotiation

Gate 7 adds a permanent client/server capability handshake before any NavyCraft
construct traffic is permitted. It prevents a mismatched client, server, or
partial engine build from silently accepting ship transforms and rider packets.

## Wire protocol

The native construct protocol is now **15** and the engine build is **0.7.0**.
The handshake uses a bounded binary packet with:

- `NCHS` magic and an independently versioned wire format;
- construct protocol number;
- engine semantic version and build identifier;
- supported and required 64-bit feature masks;
- a non-zero connection nonce;
- an explicit server accept or reject result;
- bounded build and rejection strings;
- rejection of truncation, trailing data, invalid kinds and oversized packets.

The required feature set is native scene rendering, moving-hull collision,
server rider authority, native interactions, fixed-step simulation and
clock-synchronised snapshots. Articulation is negotiated as an additional
supported feature.

## Client authority gate

The client sends one hello after reaching Luanti's ready state. Until the server
accepts the exact protocol and required feature set, the client:

- does not create or step the construct scene;
- does not apply construct sections, transforms, effects or articulation;
- does not apply moving-platform motion or moving-hull collision;
- does not transmit rider state;
- does not transmit construct interactions.

Connection resets clear the nonce and negotiation state. A construct reset clears
only construct state and does not incorrectly tear down an accepted connection.

## Server authority gate

The server validates the client hello and records accepted peers. Until a peer is
accepted, the server:

- does not send construct state to that peer;
- does not accept rider packets;
- does not exempt movement from normal checks;
- does not accept construct interactions;
- does not include the peer in carried-rider processing.

The full authoritative construct snapshot is sent only after acceptance. Client
ready/reconnect clears all handshake, rider and interaction state before a new
hello is required.

## Network integration

Gate 7 reserves one reliable command in each direction:

- `TOCLIENT_NAVYCRAFT_HANDSHAKE = 0x6C`;
- `TOSERVER_NAVYCRAFT_HANDSHAKE = 0x56`.

The command table limits are now `0x6D` and `0x57` respectively.
