# F1C200S Linux 首阶段适配候选

原源码39695b1，原目录分支candidate/f1c200s-fbdev-20261002，不另开工作树。
只修改LVGL应用，kernel/DTB/AIC/BS/蓝牙/投屏启动链路不改。

UI保留原800x480逻辑布局，RGB565条带直接旋转到fb0原生480x800，不缩放、无整屏中间缓存。
FT6236 evdev按竖屏坐标逆变换；沿用已验证的I2C诊断后备，20ms限频。
字体复用原菜单menu_font_stream.c和cjk28.bin，16字缓存，不加载完整字体到RAM。
工具链为已迁移Buildroot GCC6.4，用实验版string_view和等价clamp兼容，不换工具链。

## 构建

在本目录运行 `make -j2`。SDK默认相对于实体新路径，不用旧路径映射。
程序为build-arm/wiliwili-lvgl；字体cjk28.bin放在可执行文件旁边。
新UI固定board-preview模式，用户界面参数仍可选择详情、搜索、设置等页面。
F1_UI_TEST_MS=15000用于有限时显示试验；F1_CONFIG_DIR选择私有配置目录。

## 网络与当前边界

固定cpp-httplib v0.11.4及MIT许可证：
httplib.h SHA256 c324a892f254f3ff2cef7d927757c3afd2150e59792e008689f071b7a078d954。
HTTPS使用SDK现有OpenSSL，默认验证CA和域名，不降级验证。
GET接收过程中执行2MiB上限，缩略图沿用应用128KiB上限，响应头交给库解析。
JPEG使用SDK现有libjpeg，限制原图2048x2048，输出最大256x256RGB565。
不引入SDL、FreeType、桌面播放器或新的板端系统服务。

首阶段不支持POST写操作、容器视频解复用、音频和Cedar视频播放；明确失败，不假装成功。
raw-H264播放器接入与B站DASH/MP4解复用属于下一阶段，不能用投屏FIFO代替B站验证。
实机部署到独立候选目录，不覆盖菜单/播放器，三次冷启动验收尚未执行。
