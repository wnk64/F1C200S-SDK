param(
    [ValidateSet('Launch', 'Click', 'Key', 'Type', 'Scroll', 'Capture', 'Status', 'Close')]
    [string]$Action = 'Status',
    [int]$ProcessId,
    [int]$X,
    [int]$Y,
    [int]$VirtualKey = 27,
    [int]$Delta = -3,
    [string]$Text,
    [string]$Name = 'capture',
    [string]$Executable = 'D:\AITools\build\wiliwili_ascii\wiliwili_lvgl.exe',
    [string]$Arguments = '--board-preview',
    [string]$FixtureDirectory,
    [switch]$VerifyTexture,
    [string]$OutputDirectory = 'D:\AITools\build\wiliwili_ascii\background_audit_20260926',
    [int]$WaitMilliseconds = 400
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
if (!('LvglBackgroundAudit' -as [type])) {
    Add-Type @"
using System;
using System.Runtime.InteropServices;
public class LvglBackgroundAudit {
    public delegate bool Callback(IntPtr handle, IntPtr data);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr handle, out Rect rect);
    [DllImport("user32.dll")] public static extern bool EnumWindows(Callback callback, IntPtr data);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr handle, out uint processId);
    [DllImport("user32.dll", CharSet=CharSet.Ansi)] public static extern IntPtr GetProp(IntPtr handle, string name);
    [DllImport("user32.dll", CharSet=CharSet.Ansi)] public static extern uint RegisterWindowMessage(string name);
    [DllImport("user32.dll", SetLastError=true)] public static extern IntPtr SendMessageTimeout(IntPtr handle, uint message, UIntPtr wparam, IntPtr lparam, uint flags, uint timeout, out UIntPtr result);
    public static IntPtr FindWindow(uint processId) {
        IntPtr found = IntPtr.Zero;
        EnumWindows(delegate(IntPtr handle, IntPtr data) {
            uint owner;
            GetWindowThreadProcessId(handle, out owner);
            if (owner == processId && GetProp(handle, "WiliwiliLvglAudit") == new IntPtr(1)) {
                found = handle;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
"@
}
[void][LvglBackgroundAudit]::SetThreadDpiAwarenessContext([IntPtr](-4))
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

if ($Action -eq 'Launch') {
    if ($Arguments -match '--(cookie-file|qr-login|h264-test)') { throw 'Sensitive startup mode is not allowed in background audit' }
    $directory = Join-Path $OutputDirectory ('session_' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $directory | Out-Null
    $previousMode = $env:WILIWILI_UI_AUDIT
    $previousDirectory = $env:WILIWILI_UI_AUDIT_DIR
    $previousFixtures = $env:WILIWILI_UI_AUDIT_FIXTURES
    try {
        $env:WILIWILI_UI_AUDIT = '1'
        $env:WILIWILI_UI_AUDIT_DIR = $directory
        $env:WILIWILI_UI_AUDIT_FIXTURES = $FixtureDirectory
        $process = Start-Process -FilePath $Executable -ArgumentList $Arguments -WorkingDirectory (Split-Path $Executable) -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $directory 'stdout.log') -RedirectStandardError (Join-Path $directory 'stderr.log')
    } finally {
        $env:WILIWILI_UI_AUDIT = $previousMode
        $env:WILIWILI_UI_AUDIT_DIR = $previousDirectory
        $env:WILIWILI_UI_AUDIT_FIXTURES = $previousFixtures
    }
    $deadline = (Get-Date).AddSeconds(15)
    do {
        Start-Sleep -Milliseconds 100
        $process.Refresh()
        if ($process.HasExited) { throw "Application exited: $($process.ExitCode)" }
        $handle = [LvglBackgroundAudit]::FindWindow($process.Id)
    } while ($handle -eq [IntPtr]::Zero -and (Get-Date) -lt $deadline)
    if ($handle -eq [IntPtr]::Zero) { throw 'Background audit channel did not appear within 15 seconds' }
    $session = [ordered]@{ ProcessId = $process.Id; StartTime = $process.StartTime.ToUniversalTime().ToString('o'); Directory = $directory; FixtureDirectory = $FixtureDirectory }
    $session | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDirectory "session_$($process.Id).json") -Encoding UTF8
} else {
    if (!$ProcessId) { throw 'ProcessId is required' }
    $process = Get-Process -Id $ProcessId
    if ($process.ProcessName -ne 'wiliwili_lvgl') { throw 'Target is not wiliwili_lvgl' }
    $session = Get-Content -Raw -LiteralPath (Join-Path $OutputDirectory "session_$ProcessId.json") | ConvertFrom-Json
    if ($process.StartTime.ToUniversalTime().ToString('o') -ne $session.StartTime) { throw 'Process ID has been reused' }
    $directory = $session.Directory
    $FixtureDirectory = $session.FixtureDirectory
    $handle = [LvglBackgroundAudit]::FindWindow($process.Id)
    if ($handle -eq [IntPtr]::Zero) { throw 'Target is not a background audit instance; no input sent' }
}
$message = [LvglBackgroundAudit]::RegisterWindowMessage('Wiliwili.Lvgl.BackgroundAudit.v1')
function Send-AuditCommand([uint32]$operation, [long]$value = 0) {
    $reply = [UIntPtr]::Zero
    $sent = [LvglBackgroundAudit]::SendMessageTimeout($handle, $message, [UIntPtr]$operation, [IntPtr]$value, 2, 3000, [ref]$reply)
    if ($sent -eq [IntPtr]::Zero) { throw 'Background UI did not respond within 3 seconds' }
    return $reply.ToUInt64()
}
if ((Send-AuditCommand 0) -ne 0x57494c49) { throw 'Background protocol mismatch' }
$client = New-Object LvglBackgroundAudit+Rect
if (![LvglBackgroundAudit]::GetClientRect($handle, [ref]$client)) { throw 'Cannot obtain client size' }
if ($Action -in @('Click', 'Scroll')) {
    if ($X -lt 0 -or $X -ge $client.Right -or $Y -lt 0 -or $Y -ge $client.Bottom) { throw 'Pointer is outside client area' }
    $point = [long]($X -bor ($Y -shl 16))
    if ((Send-AuditCommand 1 $point) -ne 1) { throw 'Pointer motion rejected' }
    if ($Action -eq 'Click') {
        if ((Send-AuditCommand 2 $point) -ne 1) { throw 'Pointer press rejected' }
        Start-Sleep -Milliseconds 180
        if ((Send-AuditCommand 3 $point) -ne 1) { throw 'Pointer release rejected' }
    } elseif ((Send-AuditCommand 8 $Delta) -ne 1) { throw 'Wheel input rejected' }
}
if ($Action -eq 'Key' -and (Send-AuditCommand 4 $VirtualKey) -ne 1) { throw 'Key input rejected' }
if ($Action -eq 'Type') {
    foreach ($character in $Text.ToCharArray()) {
        if ((Send-AuditCommand 5 ([int]$character)) -ne 1) { throw 'Text character rejected' }
        Start-Sleep -Milliseconds 60
    }
}
if ($Action -eq 'Close') {
    if ((Send-AuditCommand 7) -ne 1) { throw 'Close request rejected' }
    if (!$process.WaitForExit(5000)) { throw 'Application did not close within 5 seconds' }
    [pscustomobject]@{ ProcessId = $process.Id; Closed = $true; ExitCode = $process.ExitCode } | ConvertTo-Json
    return
}
Start-Sleep -Milliseconds $WaitMilliseconds
$process.Refresh()
if ($process.HasExited) { throw 'Application exited during UI operation' }
$result = [ordered]@{
    Action = $Action
    ProcessId = $process.Id
    BackgroundOnly = $true
    FixtureMode = ![string]::IsNullOrEmpty($FixtureDirectory)
    Responding = $process.Responding
    ClientWidth = $client.Right
    ClientHeight = $client.Bottom
    WorkingSetBytes = $process.WorkingSet64
    PrivateBytes = $process.PrivateMemorySize64
    LvglHeapUsedBytes = Send-AuditCommand 9 0
    LvglHeapSampledPeakBytes = Send-AuditCommand 9 1
    LvglHeapFragmentationPct = Send-AuditCommand 9 2
    LvglHeapTotalBytes = Send-AuditCommand 9 3
    DisplayPresentCount = Send-AuditCommand 9 4
    CpuSeconds = $process.CPU
    AuditDirectory = $directory
}
if ($Action -eq 'Capture') {
    if ($VerifyTexture) {
        if ((Send-AuditCommand 10) -ne 1) { throw 'SDL rendered texture does not match the LVGL audit pixels' }
        $result['TextureMatchesAuditFrame'] = $true
    }
    if ($Name -notmatch '^[a-zA-Z0-9_-]+$') { throw 'Invalid snapshot name' }
    $nonce = [long]([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds() % 1000000000)
    if ((Send-AuditCommand 6 $nonce) -ne 1) { throw 'Frame capture rejected' }
    $bitmapPath = Join-Path $directory "frame_$nonce.bmp"
    $path = Join-Path $OutputDirectory "$Name.png"
    $image = [Drawing.Image]::FromFile($bitmapPath)
    try { $image.Save($path, [Drawing.Imaging.ImageFormat]::Png) } finally { $image.Dispose(); Remove-Item -LiteralPath $bitmapPath }
    $result['Screenshot'] = $path
    $result['Sha256'] = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    $result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDirectory "$Name.json") -Encoding UTF8
}
$result | ConvertTo-Json
