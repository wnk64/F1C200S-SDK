# Worktree Change Log

Worktree: /home/wnk/Desktop/F1c200S_SOFT/ROOTFS/buildroot-2018.02.11
Component: Buildroot full original source and existing output
Base: 9638c251a6a67ec62b79e63dad960af02b1f1ea3 in original shared repository
Purpose: physically move full Buildroot without directory mappings

## 2026-10-02 / Planned / Physical source relocation
Hypothesis: move the original directory without omitting files or adding aliases.
Files: complete buildroot-2018.02.11 including .config, dl, output.
Git: track the physically moved source in place; preserve historical parent repository.
Verification: directory inode and all existing file metadata must match before and after.
Build: none; no clean/mrproper or replacement toolchain.
Board: no access, transfer, reboot, deployment, or runtime change.
Rollback reference: physical directory can be moved back; historical source remains in parent Git history.

## 2026-10-02 / Completed / Full directory moved
Source commit: 91ff912; 10086 source/config files tracked in the moved directory.
Verification: original inode and all 320977 file-metadata records identical; .config and Makefile hashes passed.
Target: real directory, not a symlink; old source location absent, no compatibility mapping created.
Preserved: dl, output and every original tracked/untracked file; no cleanup of original files.
Build: not run; old absolute references in dependent source/SDK require later explicit relocation fixes.
Board: not contacted or modified in this task.
