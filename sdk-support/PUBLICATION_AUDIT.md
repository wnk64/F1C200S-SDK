# 2026-10-13 源码公开检查

仓库：wnk64/F1C200S-SDK。现有SDK根目录 /home/wnk/Desktop/F1c200S_SOFT，无新SDK副本、无新工作树、无重新编译或板端部署。

## 发布内容

- main：SDK脚本、应用、LVGL、外围/菜单脚本、配置、记录及完整Git历史。
- 10个source分支及sdk-lock标签：内核、U-Boot、Buildroot、AIC、Cedar播放器/库、投屏接收器、BlueALSA和wiliwili的锁定完整源码与历史。
- dependencies-20261013.tar.gz：683374177字节；SHA256 ac740360e6321d88e6191a3ccc5bb89293083cbee54ab196dd64473ba71234b0。GitHub返回相同摘要和大小，状态uploaded。
- 依赖包含工具链、下载缓存、8项基线/协议文件；不含SDK编译缓存或用户游戏数据。压缩包只用于传输，不生成新SDK实体目录。

## 凭据检查

扫描发布主仓库与全部锁定组件的可达历史：323142个文本对象；另有1528个二进制或超过4MiB的对象未进行文本模式扫描。检测私钥标记、GitHub令牌、AWS访问密钥、URL内嵌口令、Wi-Fi PSK及B站登录凭据。

未检出真实GitHub令牌、AWS密钥或B站登录凭据。私钥/PFX命中均与公开上游Git对象哈希核验相同：CivetWeb示例TLS证书、curl usercertinmem示例、wiliwili UWP演示签名文件（当前及历史版本）。它们不是本机或板端私人密钥。URL口令命中为上游帮助/测试示例；内核psk命中为指针变量和示例配置。

另查主仓库历史中的带引号PSK/登录常量：仅命中finalize_snapshot.py内的脱敏占位符与检测表达式，没有真实网络密码。源码中含原有本地路径、设备地址和开发账户配置，它们不作为私人凭据保证；公开SDK使用者必须更换默认密码。

媒体扩展名命中为U-Boot链接脚本的.rom后缀和LVGL自带birds.mp4示例，不包含用户ROM或个人媒体。依赖包7683项，未发现.ssh、个人密钥、credentials或.sdk-output路径，22个许可证/版权相关文件保留；8项必需基线/协议内容SHA256逐一核验通过。第三方原源码中的许可证与版权声明保留，汇总不改变其许可条件。

## 验证边界

SDK80项测试及源码还原工具9项测试通过；所有组件远程分支及锁定标签已按提交号核对。按用户禁止复制SDK的要求，没有另建完整SDK下载目录做还原/重编；历史全量构建记录不能冒充最新提交全量验收。完整音视频回归与三次物理冷启动仍待验证。

这是规则扫描和已命中样例核查，不是安全认证、逐行人工审计或完整法律审查。压缩下载包内的所有第三方内容没有递归逐对象文本审查。

2026-10-13 后续结果：wnk64重新登录并取得delete_repo权限后，按用户明确要求，旧8个F1C200S相关仓库已逐个删除；账号仓库列表复核不再包含这些目标。F1C200S相关仅保留统一SDK。旧库Issues/Releases随删除移除，不保证可恢复；现有统一SDK、本地源码及编译缓存未删除，没有创建额外备份或SDK副本。此前等待授权的记录为历史过程，不是当前状态。

确切范围：wnk64/f1c200s-extras、wnk64/licheepi_nano、wnk64/f1c200s-miracast-stable-aic8800d80、wnk64/f1c200s-network-tv、wnk64/f1c200s-casting、wnk64/f1c200s-linux、wnk64/f1c200s-u-boot、wnk64/f1c200s-buildroot。不含树莓派、LT6911、MCU或统一SDK仓库。
