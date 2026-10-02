# F1C200S Linux 首阶段适配候选

原源码39695b1，原目录分支candidate/f1c200s-fbdev-20261002，不另开工作树。
只修改LVGL应用，kernel/DTB/AIC/BS/蓝牙/投屏启动链路不改。

UI保留原800x480逻辑布局，RGB565条带直接旋转到fb0原生480x800，不缩放、无整屏中间缓存。
FT6236 evdev按竖屏坐标逆变换；沿用已验证的I2C诊断后备，20ms限频。
字体复用原菜单cjk28.bin，board_font.c按标签字号缩小字形，16字缓存，不加载完整字体到RAM。
工具链为已迁移Buildroot GCC6.4，用实验版string_view和等价clamp兼容，不换工具链。

## 构建

首次在本目录运行 `sh build-curl.sh`，再运行 `make -j2`。SDK默认相对于实体新路径，不用旧路径映射。
程序为build-arm/wiliwili-lvgl；字体cjk28.bin放在可执行文件旁边。
新UI固定board-preview模式，用户界面参数仍可选择详情、搜索、设置等页面。
F1_UI_TEST_MS仅用于直接诊断调用；正式launch-with-menu.sh会清除它，正常运行不会自动退出。
F1_CONFIG_DIR选择私有配置目录；platform_config_path返回该目录下的配置文件路径。

## 网络与当前边界

固定libcurl8.16.0完整源码和COPYING；其官方源码包SHA256为
40c8cddbcb6cc6251c03dea423a472a6cea4037be654ba5cf5dec6eb2d22ff1d。
之前httplib候选要求OpenSSL1.1.1，不适用于现有SDK，已移除。
HTTPS使用SDK现有OpenSSL1.0.2，默认验证CA和域名，不降级验证；旧OpenSSL配置线程锁。
GET接收过程中执行2MiB上限，POST512KiB、响应头32KiB、缩略图128KiB；HTTP解析使用libcurl。
JPEG使用SDK现有libjpeg，限制原图2048x2048，输出最大256x256RGB565。
不引入SDL、FreeType、桌面播放器或新的板端系统服务。

POST已实现但未做账号写操作实机验收；扫码必须由用户手机确认。
我的页Show QR生成二维码，附白色静区；Set-Cookie提取不区分HTTP头大小写。
推荐页先发布元数据，后加载当前可见的4张封面，不等8张图片全部结束。
左侧底部Exit按钮结束应用；launch-with-menu.sh等待退出后返回原菜单。
从原菜单调用时只向父菜单返回，避免启动第二个菜单管理器。
原菜单应用列表新增wiliwili入口，原投屏/触摸/设置行不改。
首阶段不支持容器视频解复用、音频和Cedar视频播放；明确失败，不假装成功。
raw-H264播放器接入与B站DASH/MP4解复用属于下一阶段，不能用投屏FIFO代替B站验证。
实机部署到独立候选目录，不覆盖菜单/播放器，三次冷启动验收尚未执行。

## 本轮实测

ARM926EJ-S/v5TEJ构建rc=0；坐标384000点、越界/RGB565/GCC兼容测试通过ASan/UBSan。
JPEG畸形输入、边界及RGB565测试通过；Cookie头大小写/行界限测试通过。
板端TLS/API GET259字节、MD5和响应上限检查通过，TLS校验开启。
初始配置路径写成目录引发rc134已修复；后续60秒显示试验rc0，用户确认真实触摸切页。
新推荐首轮metadata_ms757、covers_ms3235，不代表所有网络条件的固定耗时。
退出共用路径测试释放应用并恢复菜单；意外继承6秒诊断变量引起用户重进自动退出，
已以d57c4e9无条件清除变量并重启无测试变量的父菜单，不是OOM。

板端软件时钟原为1970年且没有系统CA文件；本轮软件时钟同步电脑，未写RTC。
CA仅在候选目录，通过F1_CA_FILE指定，不修改系统证书配置。
冷启动后时钟恢复、完整登录确认/持久化、屏幕键盘、视频播放仍未验收。
