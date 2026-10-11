# 2026-10-11 Source Cleanup

Worktree: /home/wnk/Desktop/F1c200S_SOFT
Baseline: 4653ec6; kernel b3d8ed3, complete tracked source and generated baseline retained.
Request: pause SDK integration, remove redundant source archives before continuing.
User specifically approved deleting source bundles after verifying their content is retained in the current complete Git repository.

## Planned

Inventory: unified directory about23GiB, oldarchives7.8GiB, legacyproject3.1GiB, Buildrootoutput6.4GiB. Buildrootoutput/build includes actual package sources and active build dependencies; not a disposable cache yet. Do not delete it, active kernel/U-Boot outputs, .config, toolchain, user changes, unique sources, logs or recovery images.

Candidates: exactly23 files larger than100000000bytes with suffix.bundle under SOFTWARE/existing-archives. Each bundle reference resolves to a commit in KERNEL/linux-5.7.1. `git fsck --full --no-reflogs` passed; only dangling objects reported. Old reflogs contain invalid historical entries; no reflogs or objects will be deleted.

Before deletion: record SHA256/size/path and complete original reference lists, pin each referenced object under refs/archive-cleanup/20261011/<object-id>, recheck full repository integrity. Skip any tracked or nonregular file, any symlink, or any path escaping the exact archive root. Delete only after all candidates validate. No source tree, config, current binary or unique archive deletion.

Recovery: each deleted bundle's historical commits and tags remain in the complete kernel Git; original reference map is recorded. New bundles can be exported with git bundle create if needed, though byte-for-byte container reconstruction is not promised. No extra backup copy created.

## 中文说明

先暂停 SDK 工作。统一目录约 23 GiB，旧归档约 7.8 GiB、旧项目约 3.1 GiB，Buildroot 输出约 6.4 GiB；后者包含实际依赖源码与工具链，暂不整目录清除。

用户已明确允许删除验证重复的源码包。只处理旧归档里的23份大型内核 `.bundle`，所有引用提交已在当前完整内核 Git 中找到，完整对象校验通过。删除前固定历史对象引用并保存原包 SHA256、字节数、路径和引用映射；源码、配置、日志、独有恢复镜像和当前构建产物全部保留。不新建备份，不清 Git 历史，不动板子。

## 完成结果 / Result

已删除23份验证重复的内核源码 bundle，共4,942,070,964字节（4.603 GiB）。删除清单与引用映射提交：572b42b，目录 records/source-cleanup-20261011。历史对象固定于内核仓库 refs/archive-cleanup/20261011/ 下，未清除任何 Git 对象。

删除后 git fsck --full --no-reflogs 返回0；大型重复 bundle 剩余0份。当前内核 .config、arch/arm/boot/zImage 与 Buildroot output/host 存在。统一目录约18 GiB，虚拟磁盘可用约37 GiB。原有用户修改及未跟踪文件保留，板子未改动。只完成主机重复归档清理，尚未完成一键 SDK。

Deleted 23 redundant bundles, reclaiming 4,942,070,964 bytes. Post-removal full Git object verification passed. Active source, configurations, build artifacts, toolchain and unrelated user changes were preserved. Historical source content remains recoverable from pinned Git objects and recorded reference mappings; original bundle container bytes are not retained.
