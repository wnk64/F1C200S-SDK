# DDNSTO 原生 SSH / SCP 转接候选

已验证电脑→DDNSTO真实HTTPS/WSS→OpenWrt→板子的原生SSH与SCP。
不是浏览器终端，不是直接公开板子的TCP22；使用已有域名的独立/f1-ssh路径。
无需继续改DDNSTO映射，原LuCI网页保留。另有18089专用LAN服务作候选入口。

## 现在连接

电脑端隐藏转接已启动，PID记录于client.pid（当前9324），监听仅127.0.0.1:2222。

```powershell
ssh -p 2222 root@127.0.0.1
scp -O -P 2222 .\file root@127.0.0.1:/root/
```

登录的是板子，不是路由器，密码仍为板端既有密码1。PuTTY连接类型SSH、
主机127.0.0.1、端口2222；PSCP采用-scp。OpenSSH SCP加-O因为板端仅有
传统SCP支持，未确认SFTP子系统可用。
SSH主机ED25519指纹必须是：

```text
SHA256:I1li/9+0wRwOzVnASuXFl8VwY6uZTMTKaE13b84S/Dc
```

停止电脑代理会使上述端口不可用，路由器也必须在线且能访问板子10.0.0.234。
当前电脑重启后可用以下命令手动恢复临时验证代理（不更改PowerShell脚本策略）：

```powershell
& 'D:\AI工具\Python\python.exe' 'C:\Users\26301\Desktop\F1C200S\ddnsto_ssh_20261002\start_client.py'
```

## 安装状态和边界

路由器/root/f1-ddnsto-ssh/websocat为SHA256验证的官方ARM64依赖，
/etc/init.d/f1-ddnsto-ssh由procd守护并启用启动，固定目标10.0.0.234:22。
nginx只新增conf.d/f1-ssh.conf及f1-ssh.locations，不改原LuCI/UCI/防火墙。
未实际重启路由器验收；可重启恢复仅是已配置，不是已测试结论。

电脑原生Websocat TLS遭Schannel无凭证错误，使用Python标准SSL建立校验
证书的TLS流，再交成熟websocat处理WebSocket协议；不使用-k/不跳过证书。
电脑工具仅临时下载为_tmp_websocat.exe、用于当前运行/后续立即外网测试，
并非永久安装；D:\AI工具安装写入受沙箱拒绝，须在权限允许后迁入该目录。
不能将项目目录中的临时exe视为违反指定工具位置的永久安装。
start_client.py无开机计划任务，重复启动拒绝占用已有2222端口。

private/basic-auth.txt为256位随机隧道密码，由基本认证HTTP头传递，不放URL、
命令行、Git或访问日志。private中的生成nginx配置也含密钥，不得分享。
SSH密码仍由SSH端到端认证处理，未交给DDNSTO网页应用。TLS终止于DDNSTO，
DDNSTO转发服务属于信任边界；SSH正文仍端到端加密。认证仅限固定板子SSH，
不能作为任意TCP代理。路由器后台18090只监听回环，最多4条同时转接连接。
LAN测试临时明文ws通道2223已停止；正常客户端只用WSS域名。

## 验证结果

- 局域网原生SSH通过，板端5.7.1/启动9868a347指纹匹配。
- 云端/f1-ssh未认证401，路由器原网页首页200。
- 两次云端原生SSH通过，持久服务切换后复测也通过。
- 两轮256KiB二进制（全部字节值/NUL）双向SCP，主机/板端/回传SHA256一致：
  2312394bd99545d9de131c24efb781e765ac1aec243f2ed9347597a793a415e9。
- 路由器relay空闲RSS2080KiB，不在板端部署服务，板端投屏未停止。
- 当前测试电脑仍在原网络，虽然路径真实经过云端，但另一Wi-Fi/热点切换、
  服务长时/重启、域名映射长期稳定性尚未验收，不能保证所有外网均可达。

源码位于原panel仓库ddnsto_ssh_20261002，Git跟踪，不新工作树。
公开脚本/文档与secret文件严格分开，依赖二进制验证过发布摘要。

## 停用与回退

先停止本电脑client.pid对应的自有Python及子websocat，不误杀其他Python。
路由器运行/etc/init.d/f1-ddnsto-ssh stop并disable，再删除仅本候选新增的
两个nginx配置文件，nginx -t -c /etc/nginx/uci.conf检查后reload。
原DDNSTO映射/服务与LuCI无需恢复，因为没有覆盖；不得删除原配置或重启网络。
若要彻底卸载，再核对后删除本轮/root/f1-ddnsto-ssh与init服务，当前未执行卸载。
