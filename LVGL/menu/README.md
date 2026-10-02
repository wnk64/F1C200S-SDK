# F1C200S 应用菜单候选

LVGL 8.2.0，与用户指定 Desktop/LVGL 的311个库源码/头文件一致。
480×800、3.2寸竖屏；首页三枚432×156触摸按钮，无SDL/桌面/CMA新帧。
复用已验证fbdev32位扫描输出，RGB565两块16行条带共30KiB、LVGL堆128KiB。
中文采用28px/4bpp、95字界面子集；generate_menu_font.py只在构建主机运行，
板端不安装字体引擎/完整字体文件。新增中文应用名时须扩展该脚本的TEXT并重生成。

## 启动

板端先注册现有FT6236模块，再运行：

```sh
sh /root/aic_miracast/candidates/lvgl_connections_20261002/menu-supervisor.sh
```

不要与原touch-test或播放器并行启动，只有一个进程拥有显示。
首次部署不添加开机服务。触摸保留现有诊断I2C轮询后备，不代表IRQ已修复。

## 应用接入

apps.conf每行五列：id|中文标题|副标题|图标类型|绝对可执行文件或内部页面。
图标类型cast/touch/settings；最多12个入口，每页3个，超过3个自动显示翻页按钮。
外部应用应前台运行，退出前关闭全部子进程和显示资源；不得启动后台任务后立即返回。
菜单退出码20～31选择对应应用，supervisor先wait回收GUI再执行应用；结束后重新启动GUI。
@touch/@settings为内部页面，设置亮度1～8，防止误操作全黑；当前亮度不自动改变。
启动失败返回首页错误提示，详细信息记录application.log，不无限重启故障GUI。

## 投屏

cast-app.sh调用本目录start-cast-default-audio.sh的cold/session，
该候选从已有audio_reopen启动器派生，只去掉强制蓝牙在线的启动条件；
使用ALSA default由S35audio-route自动选择蓝牙/耳机，不改原启动器、
音频守护、驱动、播放器或协议。无手机连接180秒返回菜单。
投屏期间只留轻量shell管理；断开/失败后结束自己启动的watch/player/sink，
按PID及启动时间防止误杀复用PID，再创建菜单。独立投屏已运行时拒绝接管。

## 构建与验证

在原panel仓库lvgl_menu_20261002中make，使用原Buildroot GCC6.4用户态工具链。
--validate仅检查应用表；--smoke依次显示首页/设置/触摸/首页并退出，不启动投屏。
新屏参DTB7018246d保持不动。当前候选需真实触摸、投屏生命周期和三次冷启动验收。

## Wi-Fi 与蓝牙

系统设置包含Wi-Fi/蓝牙入口。设备列表每页3条，选择设备后连接/断开；
忘记操作有独立确认页。Wi-Fi新网络输入掩码密码；键盘支持大小写、数字、
空格和ASCII符号。蓝牙支持PIN/Passkey输入、数字确认和配对码显示。
扫描/连接在短时helper里执行，GUI不阻塞；任务结束回收helper，投屏前没有
网络页面进程残留。GUI继续128KiB堆/30KiB条带，无新增CMA帧。

普通Wi-Fi仅操作BS的rtlwlan0现有wpa控制接口，支持开放/WPA-PSK；
不支持WEP、企业EAP和纯WPA3，明确提示，不降级安全认证。成功关联后
SAVE_CONFIG并通知BS udhcpc重取租约；新网络失败恢复原网络选择。
断开是本次运行断开，忘记才删除持久配置；断开/忘记当前网络会影响SSH。
密码只经stdin管道，不出现在进程命令行/日志；不调用shell拼接SSID或密码。
Wi-Fi列表显示当前连接和已保存项（含未扫描到的已保存网络）。

蓝牙使用BlueZ DBus，不依赖解析bluetoothctl控制台输出；只授权用户选中
设备的配对请求。保留原BlueALSA/S35audio-route默认声卡策略。忘记会移除
BlueZ配对信息，但设备仍在附近时下次扫描仍可能出现。

未知中文设备名用开放Takao CJK后备：cjk28.bin存SD/ext4约10.57MiB，
只读取需要的字形，缓存16个字约8.3KiB，不加载完整字体到RAM。原字体、
许可证TAKAO_LICENSE.txt、生成脚本保留于源码仓库；板端仅需字库bin。
字库FD在GUI退出时关闭。支持基本CJK范围，其他Unicode字形可能显示占位。

--network-smoke仅用模拟设备依次渲染列表/详情/键盘/配对确认，不切换网络。
真实只读扫描、界面smoke和协议ASan/UBSan通过；真实换网/DHCP/重启保持、
设备配对/连接/忘记、声音及投屏回归仍须用户操作验收，不以扫描成功代替。
旧菜单/root/aic_miracast/candidates/lvgl_menu_20261002保留作回退。
