param(
    [string]$Executable = 'D:\AITools\build\wiliwili_ascii\wiliwili_h264_output_test.exe',
    [string]$UiExecutable = 'D:\AITools\build\wiliwili_ascii\wiliwili_lvgl.exe',
    [string]$MediaDirectory = 'D:\AITools\ffmpeg\ffmpeg-9.0.2-essentials_build\bin',
    [string]$FixtureDirectory = 'D:\AITools\build\wiliwili_ascii\h264_network_ui_20260926\fixtures',
    [string]$OutputDirectory = 'D:\AITools\build\wiliwili_ascii\h264_memory_baseline_20260926',
    [int]$Frames = 300,
    [int]$UiIterations = 6,
    [switch]$SkipUi,
    [switch]$SoftwareUi,
    [switch]$UiOnly
)

$ErrorActionPreference = 'Stop'
if ($Frames -lt 50 -or $Frames -gt 1500 -or $UiIterations -lt 1 -or $UiIterations -gt 100) { throw 'Invalid audit workload' }
if ($SkipUi -and $UiOnly) { throw 'SkipUi and UiOnly cannot be combined' }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$OutputDirectory = (Resolve-Path $OutputDirectory).Path
$ffmpeg = Join-Path $MediaDirectory 'ffmpeg.exe'
$ffplay = Join-Path $MediaDirectory 'ffplay.exe'
if (-not ('LvglMemoryAudit' -as [type])) {
    Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class LvglMemoryAudit {
    [StructLayout(LayoutKind.Sequential)]
    public struct Counters {
        public uint Size;
        public uint PageFaults;
        public UIntPtr PeakWorkingSet;
        public UIntPtr WorkingSet;
        public UIntPtr QuotaPeakPagedPool;
        public UIntPtr QuotaPagedPool;
        public UIntPtr QuotaPeakNonPagedPool;
        public UIntPtr QuotaNonPagedPool;
        public UIntPtr PagefileUsage;
        public UIntPtr PeakPagefileUsage;
        public UIntPtr PrivateUsage;
    }
    [DllImport("psapi.dll", SetLastError = true)]
    public static extern bool GetProcessMemoryInfo(IntPtr handle, ref Counters counters, uint size);
    public static ulong[] Read(IntPtr handle) {
        var counters = new Counters();
        counters.Size = (uint)Marshal.SizeOf(typeof(Counters));
        if (!GetProcessMemoryInfo(handle, ref counters, counters.Size)) return null;
        return new ulong[] { counters.WorkingSet.ToUInt64(), counters.PeakWorkingSet.ToUInt64(),
                             counters.PrivateUsage.ToUInt64(), counters.PeakPagefileUsage.ToUInt64() };
    }
}
"@
}
$samples = New-Object 'Collections.Generic.List[object]'
$summaries = @{}
$uiStages = New-Object 'Collections.Generic.List[object]'
$auditStarted = [Diagnostics.Stopwatch]::StartNew()
function Record-Memory($process, [string]$caseName, [string]$role, [string]$stage = '') {
    $values = [LvglMemoryAudit]::Read($process.Handle)
    if ($null -eq $values) { return }
    $key = "$caseName/$role/$($process.Id)"
    if (-not $summaries.ContainsKey($key)) {
        $summaries[$key] = [ordered]@{ Case = $caseName; Role = $role; ProcessId = $process.Id; SampleCount = 0; PeakWorkingSetBytes = 0L; PeakPrivateCommitBytes = 0L; MaxSampledPrivateBytes = 0L }
    }
    $record = $summaries[$key]
    $record.SampleCount++
    $record.PeakWorkingSetBytes = [Math]::Max([long]$record.PeakWorkingSetBytes, [long]$values[1])
    $record.PeakPrivateCommitBytes = [Math]::Max([long]$record.PeakPrivateCommitBytes, [long]$values[3])
    $record.MaxSampledPrivateBytes = [Math]::Max([long]$record.MaxSampledPrivateBytes, [long]$values[2])
    $sample = [ordered]@{ ElapsedMs = $auditStarted.ElapsedMilliseconds; Case = $caseName; Role = $role; Stage = $stage; ProcessId = $process.Id; WorkingSetBytes = $values[0]; PrivateBytes = $values[2] }
    $samples.Add($sample)
    return $sample
}
function Start-Hidden([string]$program, [string[]]$arguments, [bool]$pipeInput = $false, [bool]$binaryOutput = $false, [bool]$dummySdl = $false) {
    foreach ($argument in $arguments) { if ($argument.Contains('"') -or $argument.EndsWith('\')) { throw 'Unsupported audit argument' } }
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $program
    $info.Arguments = ($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardInput = $pipeInput
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    if ($dummySdl) {
        $info.EnvironmentVariables['SDL_VIDEODRIVER'] = 'dummy'
        $info.EnvironmentVariables['SDL_AUDIODRIVER'] = 'dummy'
        $info.EnvironmentVariables['SDL_RENDER_DRIVER'] = 'software'
    }
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $info
    [void]$process.Start()
    $outputTask = if ($binaryOutput) { $null } else { $process.StandardOutput.ReadToEndAsync() }
    return [pscustomobject]@{ Process = $process; ErrorTask = $process.StandardError.ReadToEndAsync(); OutputTask = $outputTask }
}
function Close-Owned($child) {
    if ($null -eq $child) { return }
    try { if (-not $child.Process.HasExited) { $child.Process.Kill(); $child.Process.WaitForExit() } } finally { $child.Process.Dispose() }
}
function Finish-Owned($child, [string]$name) {
    if (-not $child.Process.WaitForExit(10000)) { throw "$name did not exit" }
    $child.Process.WaitForExit()
    $log = $child.ErrorTask.GetAwaiter().GetResult()
    [IO.File]::WriteAllText((Join-Path $OutputDirectory "$name.log"), $log)
    if ($child.Process.ExitCode -ne 0) { throw "$name exit $($child.Process.ExitCode): $log" }
}
function Find-ProducerChild($parent) {
    $item = Get-CimInstance Win32_Process -Filter "ParentProcessId=$($parent.Id)" | Where-Object { $_.Name -eq 'ffmpeg.exe' } | Select-Object -First 1
    if ($item) {
        try { $process = Get-Process -Id $item.ProcessId; [void]$process.Handle; return $process } catch { return $null }
    }
    return $null
}
function Sample-Ui($process, [string]$stage, [int]$durationMs = 1000) {
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $latest = $null
    do {
        $latest = Record-Memory $process 'ui' 'lvgl' $stage
        Start-Sleep -Milliseconds 100
    } while ($clock.ElapsedMilliseconds -lt $durationMs)
    if ($null -eq $latest) { throw 'UI memory counter unavailable' }
    $lvgl = & $uiTool -Action Status -ProcessId $process.Id -OutputDirectory $OutputDirectory -WaitMilliseconds 0 | ConvertFrom-Json
    $latest['LvglHeapUsedBytes'] = $lvgl.LvglHeapUsedBytes
    $latest['LvglHeapSampledPeakBytes'] = $lvgl.LvglHeapSampledPeakBytes
    $latest['LvglHeapFragmentationPct'] = $lvgl.LvglHeapFragmentationPct
    $latest['SoftwareRenderer'] = [bool]$SoftwareUi
    $uiStages.Add($latest)
}
$ui = $null
$uiId = 0
$uiTool = Join-Path $PSScriptRoot 'ui_audit.ps1'
$setup = $null
try {
    $source = Join-Path $OutputDirectory 'memory_sample.mp4'
    if (-not $UiOnly) {
        $setup = Start-Hidden $ffmpeg @('-hide_banner', '-loglevel', 'error', '-nostdin', '-y', '-f', 'lavfi', '-i', 'testsrc2=size=800x480:rate=25', '-frames:v', "$Frames", '-c:v', 'libx264', '-threads', '1', '-preset', 'ultrafast', '-pix_fmt', 'yuv420p', '-g', '25', '-bf', '0', '-movflags', '+faststart', $source)
        Finish-Owned $setup 'generate-sample'
        Close-Owned $setup
        $setup = $null
    }
    if (-not $SkipUi) {
        if (-not (Test-Path -LiteralPath $FixtureDirectory)) { throw 'UI fixtures missing; run ui_regression.ps1 first or use -SkipUi' }
        $previousSoftware = $env:WILIWILI_UI_AUDIT_SOFTWARE
        try {
            $env:WILIWILI_UI_AUDIT_SOFTWARE = if ($SoftwareUi) { '1' } else { '0' }
            $launch = & $uiTool -Action Launch -Executable $UiExecutable -FixtureDirectory $FixtureDirectory -OutputDirectory $OutputDirectory -WaitMilliseconds 2000 | ConvertFrom-Json
        } finally { $env:WILIWILI_UI_AUDIT_SOFTWARE = $previousSoftware }
        $uiId = $launch.ProcessId
        $ui = Get-Process -Id $uiId
        [void]$ui.Handle
        Sample-Ui $ui 'home-baseline' 2000
        foreach ($iteration in 1..$UiIterations) {
            & $uiTool -Action Click -ProcessId $uiId -X 150 -Y 180 -OutputDirectory $OutputDirectory -WaitMilliseconds 500 | Out-Null
            Sample-Ui $ui "detail-$iteration" 300
            & $uiTool -Action Click -ProcessId $uiId -X 250 -Y 390 -OutputDirectory $OutputDirectory -WaitMilliseconds 350 | Out-Null
            Sample-Ui $ui "player-$iteration" 1000
            & $uiTool -Action Key -ProcessId $uiId -VirtualKey 27 -OutputDirectory $OutputDirectory -WaitMilliseconds 400 | Out-Null
            Sample-Ui $ui "returned-$iteration" 1000
        }
        & $uiTool -Action Close -ProcessId $uiId -OutputDirectory $OutputDirectory | Out-Null
        $uiId = 0
        $ui.Dispose()
        $ui = $null
    }
    $caseNames = if ($UiOnly) { @() } else { @('copy-bounded', 'player-default', 'player-single-thread') }
    foreach ($caseName in $caseNames) {
        $producer = $null
        $player = $null
        $worker = $null
        try {
            if ($caseName -eq 'copy-bounded') {
                $producer = Start-Hidden $Executable @('--source', $source, '--ffmpeg', $ffmpeg, '--output', (Join-Path $OutputDirectory 'memory_output.h264'), '--read-size', '4096', '--consumer-delay', '10')
                $copyTask = $null
            } else {
                $playerArguments = @('-hide_banner', '-loglevel', 'error', '-autoexit', '-an', '-f', 'h264')
                if ($caseName -eq 'player-single-thread') { $playerArguments += @('-threads', '1') }
                $playerArguments += @('-i', 'pipe:0')
                $player = Start-Hidden $ffplay $playerArguments $true $false $true
                $producer = Start-Hidden $Executable @('--source', $source, '--ffmpeg', $ffmpeg, '--stdout') $false $true
                $copyTask = $producer.Process.StandardOutput.BaseStream.CopyToAsync($player.Process.StandardInput.BaseStream)
            }
            $clock = [Diagnostics.Stopwatch]::StartNew()
            $inputClosed = $false
            do {
                Record-Memory $producer.Process $caseName 'api-host' | Out-Null
                if ($null -eq $worker -and -not $producer.Process.HasExited) { $worker = Find-ProducerChild $producer.Process }
                if ($null -ne $worker) { Record-Memory $worker $caseName 'ffmpeg-demux' | Out-Null }
                if ($null -ne $player) {
                    Record-Memory $player.Process $caseName 'ffplay-decoder' | Out-Null
                    if ($copyTask.IsCompleted -and -not $inputClosed) {
                        [void]$copyTask.GetAwaiter().GetResult()
                        $player.Process.StandardInput.Close()
                        $inputClosed = $true
                    }
                }
                if ($clock.ElapsedMilliseconds -gt 120000) { throw "$caseName timed out" }
                Start-Sleep -Milliseconds 100
            } while (-not $producer.Process.HasExited -or ($null -ne $player -and -not $player.Process.HasExited))
            Finish-Owned $producer $caseName
            if ($null -ne $player) { Finish-Owned $player "$caseName-consumer" }
            Write-Host "PASS memory case $caseName"
        } finally {
            Close-Owned $producer
            Close-Owned $player
            if ($null -ne $worker) { $worker.Dispose() }
        }
    }
} finally {
    Close-Owned $setup
    if ($uiId) { & $uiTool -Action Close -ProcessId $uiId -OutputDirectory $OutputDirectory | Out-Null }
    if ($null -ne $ui) { $ui.Dispose() }
    $summaries.Values | Sort-Object Case, Role | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'memory_summary.json') -Encoding UTF8
    $samples | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'memory_samples.json') -Encoding UTF8
    $uiStages | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'ui_stages.json') -Encoding UTF8
}
$summaries.Values | Sort-Object Case, Role | ForEach-Object {
    Write-Host ('{0} / {1}: peak WS {2:N2} MiB, peak private commit {3:N2} MiB, samples {4}' -f $_.Case, $_.Role, ($_.PeakWorkingSetBytes / 1MB), ($_.PeakPrivateCommitBytes / 1MB), $_.SampleCount)
}
