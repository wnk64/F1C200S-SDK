$ErrorActionPreference = 'Stop'
$relayRoot = $PSScriptRoot
$relayPython = 'D:\AI工具\Python\python.exe'
$relayProcess = Start-Process -FilePath $relayPython `
    -ArgumentList @((Join-Path $relayRoot 'client.py'), '--port', '2222') `
    -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput (Join-Path $relayRoot 'client.log') `
    -RedirectStandardError (Join-Path $relayRoot 'client-error.log')
$relayProcess.Id | Set-Content -LiteralPath (Join-Path $relayRoot 'client.pid') -Encoding ASCII
Write-Output ('SSH relay process: ' + $relayProcess.Id + '; endpoint 127.0.0.1:2222')
