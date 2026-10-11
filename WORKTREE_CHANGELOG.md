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

### Source completion result
Source import commit931c3e9a91ff152db92ae6f82108bca838d41ba8 adds exactly9 files/1237 lines; no existing source file overwritten. Re-audit:10067 identical stock files,0 missing, same13 custom files retained. Original.config hash unchanged; original output untouched; old overlay submodule remains dirty and unstaged.
Build: SDK fresh configuration/source-acquisition retry pending after source-lock update. No board deployment or cold-boot test. This establishes source completeness, not final image acceptance.

## 2026-10-11 / Planned / Repair overwritten target package index
Evidence: package/Config.in is13 lines containing onlyRTL8723DS/libnl/hostapd fragments, replacing the official2017-line target package index. Original.config still contains BusyBox/skeleton/ALSA/SSH package selections; olddefconfig drops them because their symbols are absent. The5MiB target lacks passwd/shadow; stock skeleton source files exist and are tracked/exported correctly.
Files: package/Config.in restored from the exact verified2018.02.11 upstream source, plus explicit libcedarc/libcedarx Config.in includes. Preserve all other customized files and current.config/output/overlay changes.
Hypothesis: full package-index restoration preserves intended original package selections and enables skeleton installation/rootfs image generation.
Build: isolated SDK only. Reuse already built identical toolchain after verifying toolchain-related Kconfig values remain unchanged; rerun original9b7f1224 config with the repaired index. No original output cleanup.
Deploy:none. Rollback: previous source commit9465767 retains the broken13-line index for provenance; revert only the index repair if necessary. Decision: pending configuration and full-build verification.

### Index repair and configuration verification
Source index commit30bdc8ef08c4c196fee4892b2f2b450ee58ef9f4 restores the official index plus two local Cedar includes. Original.config hash unchanged; old overlay dirty state retained. SDK repair reapplies the original config, verifies all toolchain/architecture/ABI settings unchanged, and restores BusyBox, both skeleton packages, ALSA, Dropbear, BlueZ5 and libcedarc selections. Effective isolated config SHA2560722011450c1ebb88e4768187c7e91a932e44ae3843223051368dbbaf36f1a9f.
Initial full build completed the toolchain but not a usable system: only5MiB target and failed rootfs.tar due to missing skeleton accounts. Stock skeleton source files were present; package-index corruption caused the omission. This failed target is not an accepted image.
Next: continue all restored package builds using the verified compatible toolchain, then validate rootfs contents and integrate current runtime components. No board deployment/cold boot yet.
