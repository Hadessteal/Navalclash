# Next permanent implementation stage

Gate 9 is the complete exact-source Luanti 5.16.1 compile, link and runtime acceptance stage.

1. Run the Windows native workflow against commit `5ebd9b57984d5854e0a37fd0125da48e9e59e190`.
2. Resolve every compiler error against the real `Client`, `Server`, `LocalPlayer`, network and renderer APIs.
3. Resolve every linker error until both `luanti.exe` and `luantiserver.exe` are produced.
4. Add process-level startup tests requiring construct protocol 16 from both binaries.
5. Launch a native nine-block vessel and prove that no `lua-*` construct path exists.
6. Run local collision acceptance: stand, walk, jump, cross seams, leave deck, turn and stop.
7. Record correction counts and reject the build if repeated rider snaps occur.
8. Run a two-client synchronization test with server-authoritative rider state.
9. Publish the first versioned native base archive only after those tests pass.
10. Publish later compatible iterations as binary-only drag-and-drop updates.

A source checkpoint is not sent to the player for gameplay testing. The next player-facing artifact must contain compiled native binaries and automated runtime results.
