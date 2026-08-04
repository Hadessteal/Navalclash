# Gate 8 — Full Windows build execution

Gate 8 changes the project from a source-only checkpoint into a reproducible
Windows build candidate. It does not add a fallback renderer.

Permanent changes:

- Corrects every build/package protocol value to native protocol 15.
- Advances the engine package version to 0.8.0.
- Adds a repository-safe source-bundle reconstruction path for GitHub Actions.
- Builds the exact Luanti 5.16.1 commit with the NavyCraft overlay.
- Compiles both `luanti.exe` and `luantiserver.exe`.
- Runs the native regression suite and the configured Luanti CTest suite.
- Rejects zero-test configurations.
- Runs both binaries with `--version`, checks minimum binary sizes, and records
  SHA-256 hashes before an artifact is accepted.
- Produces one full base install and one binary-only drag-and-drop update.

Acceptance rule:

Gate 8 is not a player test build until the Windows workflow completes and the
base/update artifacts pass the binary acceptance step. Compiler or linker
failures are retained as build artifacts and corrected in C++, never bypassed
with Lua motion or temporary entities.
