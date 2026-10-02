# Worktree Change Log

Worktree: /home/wnk/tk032f8004-panel
Component: vendor DDNSTO board candidate, binary compatibility observation only
Base: current kernel5.7.1 #250/boot9868a347; existing router tunnel unchanged
Purpose: determine ARMv5 feasibility without changing board kernel or media

## 2026-10-02 planned / vendor ARM soft-float lite compatibility
OfficialHTTPS directory has zig4.2.1 arm32 (~731KiB), lite4.2.2 arm (~737KiB).
ELF32ARM EABI5softfloat/static; packed/noattributes, no provenARMv5 ISA claim.
LitepublishedJSON SHA2567adffae9dae7d6b88329fdd64943fadd2f8835f0ac5027249a8c9e7e51462343
matches extractedbinary. No vendorfullsource available: binary-only runtime
observation, never patch/compile adjacentkernel to support inferredbinary.
Deploy only vendorlite into versioned board/root candidate viaSSH hashcheck.
First --help/--version bounded5sec/ulimitcore0/stack1MiB/virtual24MiB, no token,
no enrollment/cloudlogin/bootservice. Existingcast/player23857 retained.
Any SIGILL/unavailableABI reject; do not mutatekernel or keep repeating.
Credentials/actualenrollment need separateuseraccount authority and compatible
binary/memory evidence. No boardcachedarchive in /tmp.
