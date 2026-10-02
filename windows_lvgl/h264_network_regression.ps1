param(
    [string]$Executable = 'D:\AITools\build\wiliwili_ascii\wiliwili_h264_output_test.exe',
    [string]$Ffmpeg = 'D:\AITools\ffmpeg\ffmpeg-9.0.2-essentials_build\bin\ffmpeg.exe',
    [string]$Python = 'D:\AITools\msys64\mingw64\bin\python.exe',
    [string]$Source = '',
    [string]$OutputDirectory = 'D:\AITools\build\wiliwili_ascii\h264_network_20260926'
)

$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$OutputDirectory = (Resolve-Path $OutputDirectory).Path
$results = New-Object 'Collections.Generic.List[object]'
$server = $null
function Start-Hidden([string]$program, [string[]]$arguments) {
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $program
    foreach ($argument in $arguments) { if ($argument.Contains('"') -or $argument.EndsWith('\')) { throw 'Unsupported fixture argument' } }
    $info.Arguments = ($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $info
    [void]$process.Start()
    return [pscustomobject]@{ Process = $process; ErrorTask = $process.StandardError.ReadToEndAsync(); OutputTask = $process.StandardOutput.ReadToEndAsync() }
}
function Run-Hidden([string]$program, [string[]]$arguments, [string]$name, [bool]$expectFailure = $false) {
    $child = Start-Hidden $program $arguments
    $clock = [Diagnostics.Stopwatch]::StartNew()
    try {
        if (-not $child.Process.WaitForExit(20000)) { throw "$name timed out" }
        $child.Process.WaitForExit()
        $log = $child.ErrorTask.GetAwaiter().GetResult()
        [IO.File]::WriteAllText((Join-Path $OutputDirectory "$name.log"), $log)
        if (($expectFailure -and $child.Process.ExitCode -eq 0) -or (-not $expectFailure -and $child.Process.ExitCode -ne 0)) { throw "$name failed (exit $($child.Process.ExitCode)): $log" }
        $results.Add([ordered]@{ Test = $name; ExitCode = $child.Process.ExitCode; ExpectedFailure = $expectFailure; ElapsedMs = $clock.ElapsedMilliseconds })
        Write-Host "PASS $name ($($clock.ElapsedMilliseconds) ms)"
        return $log
    } finally {
        if (-not $child.Process.HasExited) { $child.Process.Kill(); $child.Process.WaitForExit() }
        $child.Process.Dispose()
    }
}

try {
    $sourceMp4 = Join-Path $OutputDirectory 'faststart.mp4'
    if ($Source) {
        Run-Hidden $Ffmpeg @('-hide_banner', '-loglevel', 'error', '-nostdin', '-y', '-i', $Source, '-map', '0:v:0', '-an', '-c:v', 'copy', '-movflags', '+faststart', $sourceMp4) 'prepare-source' | Out-Null
    } else {
        Run-Hidden $Ffmpeg @('-hide_banner', '-loglevel', 'error', '-nostdin', '-y', '-f', 'lavfi', '-i', 'testsrc2=size=800x480:rate=25', '-frames:v', '50', '-c:v', 'libx264', '-preset', 'ultrafast', '-pix_fmt', 'yuv420p', '-g', '25', '-bf', '0', '-movflags', '+faststart', $sourceMp4) 'generate-source' | Out-Null
    }
    $reference = Join-Path $OutputDirectory 'reference.h264'
    Run-Hidden $Executable @('--source', $sourceMp4, '--ffmpeg', $Ffmpeg, '--output', $reference) 'local-reference' | Out-Null
    $ready = Join-Path $OutputDirectory ('ready_' + [Guid]::NewGuid().ToString('N') + '.json')
    $requestLog = Join-Path $OutputDirectory ('requests_' + [Guid]::NewGuid().ToString('N') + '.jsonl')
    $server = Start-Hidden $Python @((Join-Path $PSScriptRoot 'h264_http_fixture.py'), '--source', $sourceMp4, '--ready', $ready, '--log', $requestLog)
    $deadline = (Get-Date).AddSeconds(10)
    while (-not (Test-Path -LiteralPath $ready)) {
        if ($server.Process.HasExited -or (Get-Date) -gt $deadline) { throw 'Loopback fixture server did not start' }
        Start-Sleep -Milliseconds 50
    }
    $base = 'http://127.0.0.1:' + (Get-Content -Raw -LiteralPath $ready | ConvertFrom-Json).port
    foreach ($endpoint in @('video', 'slow')) {
        $output = Join-Path $OutputDirectory "$endpoint.h264"
        Run-Hidden $Executable @('--source', "$base/$endpoint.mp4", '--ffmpeg', $Ffmpeg, '--output', $output) "http-$endpoint" | Out-Null
        if ((Get-FileHash -LiteralPath $reference).Hash -ne (Get-FileHash -LiteralPath $output).Hash) { throw "$endpoint HTTP output differs from the complete source" }
    }
    $slowConsumer = Join-Path $OutputDirectory 'slow-consumer.h264'
    Run-Hidden $Executable @('--source', "$base/video.mp4", '--ffmpeg', $Ffmpeg, '--output', $slowConsumer, '--consumer-delay', '25', '--read-size', '4096') 'http-slow-consumer' | Out-Null
    if ((Get-FileHash -LiteralPath $reference).Hash -ne (Get-FileHash -LiteralPath $slowConsumer).Hash) { throw 'Backpressure lost H264 bytes' }
    Run-Hidden $Executable @('--source', "$base/truncated.mp4", '--ffmpeg', $Ffmpeg, '--output', (Join-Path $OutputDirectory 'truncated.h264')) 'http-truncation' $true | Out-Null
    $timeoutClock = [Diagnostics.Stopwatch]::StartNew()
    Run-Hidden $Executable @('--source', "$base/stall.mp4", '--ffmpeg', $Ffmpeg, '--output', (Join-Path $OutputDirectory 'timeout.h264'), '--io-timeout', '1000') 'http-io-timeout' $true | Out-Null
    if ($timeoutClock.ElapsedMilliseconds -gt 5000) { throw 'Network I/O timeout did not terminate promptly' }
    $cancelLog = Run-Hidden $Executable @('--source', "$base/stall.mp4", '--ffmpeg', $Ffmpeg, '--output', (Join-Path $OutputDirectory 'cancelled.h264'), '--cancel-after', '300') 'http-cancel-waiting'
    if ($cancelLog -notmatch 'H264_CANCEL_MS=(\d+)' -or [int]$Matches[1] -gt 1500) { throw 'Cancellation failed to stop the waiting producer promptly' }
    $requests = @(Get-Content -LiteralPath $requestLog | ForEach-Object { $_ | ConvertFrom-Json })
    if ($requests.Count -lt 6 -or @($requests | Where-Object { -not $_.headers_ok }).Count) { throw 'HTTP headers were missing or the fixture requests did not execute' }
    $originalHash = (Get-FileHash -LiteralPath $sourceMp4).Hash
    Run-Hidden $Executable @('--source', $sourceMp4, '--ffmpeg', $Ffmpeg, '--output', $sourceMp4) 'prevent-source-overwrite' $true | Out-Null
    if ((Get-FileHash -LiteralPath $sourceMp4).Hash -ne $originalHash) { throw 'Source file was overwritten' }
    foreach ($badOption in @(@('--io-timeout', '4294968296'), @('--read-size', '7oops'), @('--cancel-after', '-1'))) {
        $invalidOutput = Join-Path $OutputDirectory ('invalid_' + [Guid]::NewGuid().ToString('N') + '.h264')
        Run-Hidden $Executable (@('--source', $sourceMp4, '--ffmpeg', $Ffmpeg, '--output', $invalidOutput) + $badOption) ("reject-invalid-" + $badOption[0].Substring(2)) $true | Out-Null
        if (Test-Path -LiteralPath $invalidOutput) { throw 'Invalid arguments created an output file' }
    }
} finally {
    if ($null -ne $server) {
        try { if (-not $server.Process.HasExited) { $server.Process.Kill(); $server.Process.WaitForExit() } } finally { $server.Process.Dispose() }
    }
    $results | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'results.json') -Encoding UTF8
}
