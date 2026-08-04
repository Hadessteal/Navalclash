# Milestone 5F — ownership, insurance, capture and salvage

M5F gives every active campaign vessel a persistent legal identity. A vessel is no longer only a transient construct owned by whichever player launched it. It carries a title number, ownership history and loss status that survive storage, resale, capture and recovery.

The construct protocol remains at capability 11. M5F is a server-authoritative gameplay layer using the existing construct, storage, damage, faction, career and port systems.

## Vessel titles

The first campaign launch assigns a title such as `NC-000001`. The registry stores:

- current owner;
- vessel name and certified class;
- active, stored, listed, captured or wrecked status;
- active construct or stored-record identity;
- acquisition and transfer history;
- loss kind and time.

A new `preview.transfer_owner` helper updates the construct owner, owner-active lookup, command crew and abandonment state atomically. This also corrects the older takeover path, which changed fields without updating the owner lookup.

## Direct transfers

An owner can create a port-bound offer addressed to one player. The offer may be a gift or have a credit price. The buyer must explicitly accept it before expiry while at the offering port.

Payment and title transfer are atomic. A failed transfer refunds the buyer. The previous owner receives payment only after the live owner mapping changes successfully.

## Brokered resale

The ship broker does not leave the sale vessel active in the world. It:

1. validates ownership, port position and zero motion;
2. snapshots the vessel;
3. places its title and snapshot in server-owned escrow;
4. removes the active construct with the non-loss reason `sale_escrow`;
5. advertises a persistent listing.

A successful buyer payment imports the same title into the buyer's stored-vessel inventory. Cancellation returns the escrow snapshot to the seller. The escrow path prevents the common duplication bug where both seller and buyer retain a copy.

## Insurance

Two plans are provided.

### Hull Protection

- lower premium;
- higher deductible;
- covers destruction and sinking;
- excludes capture;
- replacement strips cargo and installed equipment.

### Comprehensive Protection

- higher premium;
- lower deductible;
- covers destruction, sinking and hostile capture;
- preserves installed equipment and cargo state in the policy snapshot.

Policies include a waiting period and are consumed by the first covered loss. Title transfer cancels an unused policy. A policy that loses the vessel during its waiting period is void rather than immediately profitable.

A settled claim creates a separate stored replacement vessel. The lost or captured title is never cloned. The replacement receives a new title when launched.

## Capture

Capture is a timed boarding action. The target must be:

- within twelve nodes of the boarder;
- stationary;
- abandoned, helm-disabled or at or below fifty percent hull;
- hostile to the boarder's faction unless already abandoned.

Progress pauses and decays if the boarder leaves range, the vessel becomes mobile or defending owner/crew members contest the vessel. On completion, the existing live construct and title transfer to the captor.

For an insured capture, the former owner's loss is recorded before the transfer. This preserves the comprehensive policy claim while still ensuring only one live prize exists.

## Wrecks and salvage rights

A destroyed player title creates a persistent wreck record containing:

- title and former owner;
- world position;
- recoverable snapshot;
- estimated surviving block count;
- exclusive-rights expiry;
- claimant and recovery status.

The original owner receives an exclusive period. After expiry, unclaimed wrecks become open, with the Salvage Guild receiving earlier professional access. A settled insurance claim assigns wreck rights to the insurer, preventing the former owner from receiving both a replacement and the wreck value.

Material recovery awards salvage crates, some hull plate and a small credit payment. Standard-mode hulk restoration imports a heavily damaged, empty and poorly maintained vessel into storage after a recovery fee.

## Permanent-loss rules

The server stores one of three modes.

### Casual

Destruction creates an automatic emergency recovery record with reduced hull condition and no cargo or ammunition.

### Standard

The title is wrecked. A valid salvage-rights holder can pay to restore a damaged hulk or recover raw materials.

### Strict

The title is permanently lost. Only material salvage remains unless a covered insurance policy creates a replacement.

Administrative removal, ship storage, repair replacement, sale escrow and salvage cleanup are explicitly excluded from loss detection.

## Commands

```text
/title status
/title offer <player> [price]
/title accept <offer>
/title cancel <offer>

/shipmarket list
/shipmarket sell <price>
/shipmarket buy <listing>
/shipmarket cancel <listing>

/insurance plans
/insurance buy <hull|comprehensive>
/insurance status
/insurance claim <claim-id>

/capture start
/capture status
/capture abort

/salvage list
/salvage claim <wreck>
/salvage recover <wreck>
/salvage restore <wreck>

/lossmode status
/lossmode set <casual|standard|strict>
```

## Verification coverage

The M5F smoke flow verifies:

- title issuance;
- comprehensive policy purchase and deterministic maturity;
- hostile capture of a disabled vessel;
- live owner and owner-active mapping transfer;
- approval and settlement of the former owner's capture claim;
- accepted direct title transfer with payment;
- broker escrow removal and buyer delivery;
- destruction creating a rights-controlled wreck;
- strict mode refusing hulk restoration;
- standard mode restoring a damaged stored hulk;
- registration of all M5F commands and service nodes.

## Remaining boundaries

M5F does not yet include legal organisations, multi-owner corporations, mortgages, taxes, auctions or offline boarding defence. Insurance pricing is campaign balance data, not a real actuarial model.

The full patched Luanti executable still needs the complete source build and multiplayer runtime validation.
