# Gate 6 — reproducible full-engine build gate

Gate 6 does not add another renderer or fallback. It hardens the permanent native build path.

## Permanent changes

- Pins the exact Luanti 5.16.1 commit and the Git blob IDs of every upstream file modified by NavyCraft.
- Rejects dirty, altered, or near-match source trees before applying the overlay.
- Separates client-only mesh and snapshot state from the shared client/server source list.
- Enables Luanti unit tests and benchmarks in the complete engine configuration.
- Refuses to package a build when CTest reports zero runnable tests.
- Captures configuration, compiler, test-inventory, and test logs even on failure.
- Verifies that both Windows executables exist, are plausible release binaries, execute `--version`, and records their SHA-256 values before packaging.
- Preserves the one-time base installation plus later drag-and-drop binary update model.

## Honest build status

The native subsystem compiles and its regression suite passes in the current Linux environment. The complete patched Windows client/server still requires a Windows runner with Visual Studio, the Windows SDK, vcpkg, and access to the exact Luanti source repository. No Windows executable is represented as built until that runner succeeds.
