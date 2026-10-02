# Worktree Change Log

Worktree: /home/wnk/tk032f8004-panel
Component: router DDNSTO SSH relay, separate from board applications
Base: 46712d5; board kernel/player/source archives retained
Purpose: reuse paid DDNSTO HTTPS mapping for native SSH/SCP

## 2026-10-02 planned / authenticated WebSocket relay
Both domains still return routerLuCI; use existingHTTPmapping /f1-ssh path
and additionally scoped10.0.0.1:18089 server. Add two nginxfiles only,
nginxsyntaxtest before reload, remove additions and reload on failedgate.
OriginalLuCI/nginx/UCI/DDNSTO mapping untouched. No WAN firewall rule.
Websocat upstream1.14.1 verifiedpublishedSHA256 for ARM64musl/Windowsx64.
Use transient /tmp routerbinary and workspace temporaryWindowsbinary only;
permanentWindows install D:/AItools denied by sandbox, no installationelsewhere.
Random256bit Basiccredential file (not Git/URL/log/argv), nginxcheck before
websocketupgrade, fixed10.0.0.234:22 destination, relaylisten127.0.0.1:18090,
clientlisten127.0.0.1:2222, bounded64KiB/messages/4connectionlimit. TLSverification
and pinnedSSHhostkey intact. No boardmemory/network/player changes.
Tests planned: unauthorized401, localSSH/pinnedhostkey, HTTPS/WSSSSH, SCP
hashroundtrip, LuCIstillavailable. No bootautoload or stablepromotion.
