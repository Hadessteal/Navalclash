# Storefront plan

The base game must run without any storefront client or SDK. Store services are enhancements, not dependencies.

| Store | Initial release path | Later optional integration |
|---|---|---|
| Steam | ordinary Windows/Linux depots; launch executable without Steamworks linkage | achievements, Cloud, invites, rich presence |
| GOG | DRM-free offline installer/package | Galaxy achievements, cloud saves, invites |
| Epic Games Store | self-published Windows package | EOS achievements, lobbies, cross-store multiplayer |
| itch.io | zip and Butler channels | channel-based patch delivery |
| Humble | DRM-free download or key fulfilment | store/key fulfilment selected during commercial onboarding |

## Cross-store multiplayer

Use the NavyCraft master-server/account layer or direct-IP connections as the common path. Never make Steam identity the canonical account identifier. Platform identities should be optional linked identities attached to an internal account UUID.

## Build outputs

```text
artifacts/
  windows-x86_64/
  linux-x86_64/
  dedicated-server-windows-x86_64/
  dedicated-server-linux-x86_64/
  engine-source/
  notices/
```

Partner-specific manifest files and credentials are generated outside source control because portal formats and identifiers are account-specific.
