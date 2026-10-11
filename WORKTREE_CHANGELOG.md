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

## 2026-10-11 / Planned / Restore missing upstream integration sources
Base:1837d25; active.config SHA2569b7f12245973997e63e4d18f2582e81de6291de771c03864c04a106ebe8c0455. Existing output and dirty board/licheepi/nano/rootfs_overlay submodule preserved.
Evidence: exact official2018.02.11 archive SHA2564d9cb4b862877eb2493bd857e0e3d49427b00fdeecd03bf5c24bfc952b586e5a;10058 identical source files,13 customized files preserved, only9 missing files all underlinux/.
Files: linux/{Config.in,Config.ext.in,linux.mk,linux-ext-aufs.mk,linux-ext-ev3dev-linux-drivers.mk,linux-ext-fbtft.mk,linux-ext-rtai.mk,linux-ext-xenomai.mk,0001-timeconst.pl-Eliminate-Perl-warning.patch.conditional}.
Hypothesis: restoring only the exact missing stock integration files will repair Kconfig traversal without changing custom packages, current.config, overlay or old build artifacts.
Build: planned isolated SDK olddefconfig/source acquisition; no in-place rebuild or clean.
Deploy:none. Rollback: revert the source-import commit; no existing source file is overwritten.
Decision: pending repair verification; full SD image integration remains active.
