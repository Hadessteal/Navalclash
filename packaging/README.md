# Packaging

These folders describe canonical package contents. Actual partner manifests contain account-specific identifiers and should be generated from private CI variables.

- `common/`: files every client build must include.
- `steam/`: depot-oriented staging notes.
- `gog/`: DRM-free installer/package staging notes.
- `epic/`: Epic Games Store staging notes.
- `itch/`: zip/Butler channel staging notes.
- `humble/`: DRM-free/key-fulfilment staging notes.

No restricted SDKs or partner credentials belong in this repository.
