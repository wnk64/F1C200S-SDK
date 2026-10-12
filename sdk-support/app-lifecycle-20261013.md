# 统一应用退出资源回收

计划基线：SDK 5a3088c，完整源码与生成器已保留；板端内核5.7.1不变。用户要求任何独立应用退出到主菜单前立即释放其专用资源。原菜单管理器只wait入口脚本，不追踪脱离进程组/双fork的后代；投屏AIC/WPA/PBC/DHCP此前需等进入DLNA才释放，这是生命周期设计遗漏。

范围：SDK runtime-menu/f1_app_runner.c、menu-supervisor.sh、app-release.sh、应用临时目录与wiliwili父进程识别的生成映射，runtime-tools构建/安装清单与测试。监管器作为Linux subreaper，为每个应用设置独立session和私有TMPDIR；入口退出/崩溃/取消后，先TERM再有限KILL所有被它收养的后代，核验waitpid无子进程，清理自己创建的私有目录后才返回菜单。即使应用子进程setsid或双fork，也不会逃过回收。显示、声音、CMA通过进程退出关闭句柄释放；不能回收时不启动第二个界面。

统一出口调用资源hook，释放闲置投屏AIC模块与已知临时会话；不关闭共享BS联网、SSH、蓝牙后台、PCF8563/IP5209/电源、触摸和显示基础驱动，不删除登录、记忆路径、录音、上传、ROM等持久数据。文件页缓存仍由内核按需回收，不全局drop_caches冒充应用清理。

保护：所有菜单应用（投屏、wiliwili、ONS、本地媒体、阅读、DLNA、网页传输、录音、SFC/GBA/PS1/MD）、显示独占和蓝牙联网。只修改SDK生成层，不改内核/播放器/模拟器源。wiliwili原父进程白名单仅允许menu-supervisor，需兼容新runner，避免误判菜单仍在运行。

验证：host正常退出/主进程SIGKILL/监管器SIGTERM/无法exec、跨session及嵌套后代、私有临时文件删除、外部不相关进程/文件保留；板端真实投屏进入退出再进入、浏览器/传输/录音/媒体/阅读等限时启动退出，逐个记录PID与CMA/Slab回落。ROM/手机音视频真实操作和三次物理冷启动未完成前不标稳定验收。

回滚：保留的完整portable-sdk旧生成菜单管理器与启动器须先hash匹配板端；新增runner/hook可移除，源码回滚至5a3088c。部署前核验已有回滚产物，不额外创建无授权重复包。原未提交驱动DTB和Cedar日志保持不动。

15a0827初步构建通过，75项测试包含正常、SIGKILL主应用、SIGTERM监管器、exec失败、跨session/嵌套后代，均被回收且私有目录删除、不相关进程与文件保留。生成runner SHA2569b420bd4...，菜单初始候选3bd42b67...。旧完整portable-sdk菜单SHA256c1446fda...与板端一致，已有回滚证据。完善统一hook同时卸载仅投屏使用的aic_load_fw；wiliwili两个/run私有临时日志默认删除（F1_KEEP_APP_LOGS=1仅用于诊断保留），保留其账号/路径等持久配置。全部变更仍只在SDK生成层，真实板端多应用验证待进行。
