param(
    [string]$Executable = 'D:\AITools\build\wiliwili_ascii\wiliwili_h264_output_test.exe',
    [string]$MediaDirectory = 'D:\AITools\ffmpeg\ffmpeg-9.0.2-essentials_build\bin',
    [string]$OutputDirectory = 'D:\AITools\build\wiliwili_ascii\h264_regression_20260926'
)

$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$OutputDirectory = (Resolve-Path $OutputDirectory).Path
$ffmpeg = Join-Path $MediaDirectory 'ffmpeg.exe'
$ffprobe = Join-Path $MediaDirectory 'ffprobe.exe'
$ffplay = Join-Path $MediaDirectory 'ffplay.exe'
foreach ($required in @($Executable, $ffmpeg, $ffprobe, $ffplay)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Missing executable: $required" }
}
$results = New-Object 'Collections.Generic.List[object]'
function Quote-Argument([string]$value) {
    $builder = New-Object Text.StringBuilder
    [void]$builder.Append('"')
    $slashes = 0
    foreach ($character in $value.ToCharArray()) {
        if ($character -eq '\') { $slashes++; continue }
        $count = if ($character -eq '"') { $slashes * 2 + 1 } else { $slashes }
        [void]$builder.Append('\', $count)
        [void]$builder.Append($character)
        $slashes = 0
    }
    [void]$builder.Append('\', $slashes * 2)
    [void]$builder.Append('"')
    return $builder.ToString()
}
function Start-Hidden([string]$program, [string[]]$arguments, [bool]$inputPipe = $false, [bool]$outputPipe = $false, [bool]$dummySdl = $false) {
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $program
    $info.Arguments = ($arguments | ForEach-Object { Quote-Argument $_ }) -join ' '
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    $info.RedirectStandardInput = $inputPipe
    $info.RedirectStandardOutput = $outputPipe
    $info.RedirectStandardError = $true
    if ($dummySdl) {
        $info.EnvironmentVariables['SDL_VIDEODRIVER'] = 'dummy'
        $info.EnvironmentVariables['SDL_AUDIODRIVER'] = 'dummy'
        $info.EnvironmentVariables['SDL_RENDER_DRIVER'] = 'software'
    }
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $info
    [void]$process.Start()
    return [pscustomobject]@{ Process = $process; ErrorTask = $process.StandardError.ReadToEndAsync() }
}
function Finish-Hidden($child, [string]$name, [int]$expectedExit = 0, [bool]$expectFailure = $false) {
    try {
        if (-not $child.Process.WaitForExit(30000)) { throw "$name timed out" }
        $child.Process.WaitForExit()
        $log = $child.ErrorTask.GetAwaiter().GetResult()
        [IO.File]::WriteAllText((Join-Path $OutputDirectory "$name.log"), $log)
        $exitCode = $child.Process.ExitCode
        if (($expectFailure -and $exitCode -eq 0) -or (-not $expectFailure -and $exitCode -ne $expectedExit)) {
            throw "$name exit $exitCode : $log"
        }
        $results.Add([ordered]@{ Test = $name; ExitCode = $exitCode; ExpectedFailure = $expectFailure })
        Write-Host "PASS $name (exit $exitCode)"
    } finally {
        if (-not $child.Process.HasExited) { $child.Process.Kill(); $child.Process.WaitForExit() }
        $child.Process.Dispose()
    }
}
function Run-Hidden([string]$program, [string[]]$arguments, [string]$name, [bool]$expectFailure = $false) {
    Finish-Hidden (Start-Hidden $program $arguments) $name 0 $expectFailure
}
function Frame-Hashes([string]$source, [string]$name) {
    $hashFile = Join-Path $OutputDirectory "$name.framemd5"
    Run-Hidden $ffmpeg @('-hide_banner', '-loglevel', 'error', '-nostdin', '-y', '-i', $source, '-map', '0:v:0', '-an', '-f', 'framemd5', $hashFile) $name
    return @((Get-Content -LiteralPath $hashFile | Where-Object { $_ -match '^\d+\s*,' }) | ForEach-Object { ($_ -split ',')[-1].Trim() })
}

try {
    $source = Join-Path $OutputDirectory '中文 空格源.mp4'
    Run-Hidden $ffmpeg @('-hide_banner', '-loglevel', 'error', '-nostdin', '-y', '-f', 'lavfi', '-i', 'testsrc2=size=800x480:rate=25', '-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=48000', '-frames:v', '50', '-shortest', '-c:v', 'libx264', '-preset', 'ultrafast', '-pix_fmt', 'yuv420p', '-profile:v', 'baseline', '-g', '25', '-bf', '0', '-c:a', 'aac', $source) 'generate-h264'
    $output = Join-Path $OutputDirectory '输出 视频.h264'
    Run-Hidden $Executable @('--source', $source, '--ffmpeg', $ffmpeg, '--output', $output, '--lifecycle-test') 'api-mp4-lifecycle'
    $sourceHashes = @(Frame-Hashes $source 'decode-source')
    $outputHashes = @(Frame-Hashes $output 'decode-output')
    if ($sourceHashes.Count -ne 50 -or ($sourceHashes -join ',') -ne ($outputHashes -join ',')) { throw 'Decoded frames changed or were lost' }
    $results.Add([ordered]@{ Test = 'decoded-frame-equality'; Frames = 50; Passed = $true })
    Write-Host 'PASS decoded-frame-equality (50 frames, 800x480 source)'
    $shortSource = Join-Path $OutputDirectory 'single-frame.mp4'
    Run-Hidden $ffmpeg @('-hide_banner', '-loglevel', 'error', '-nostdin', '-y', '-i', $source, '-map', '0:v:0', '-frames:v', '1', '-an', '-c:v', 'copy', $shortSource) 'generate-single-frame'
    $shortOutput = Join-Path $OutputDirectory 'single-frame.h264'
    Run-Hidden $Executable @('--source', $shortSource, '--ffmpeg', $ffmpeg, '--output', $shortOutput, '--lifecycle-test') 'api-single-frame-lifecycle'
    if (@(Frame-Hashes $shortOutput 'decode-single-frame').Count -ne 1) { throw 'Single-frame source was lost' }
    $probe = Join-Path $OutputDirectory 'output_probe.json'
    $probeChild = Start-Hidden $ffprobe @('-v', 'error', '-show_entries', 'stream=codec_name,width,height', '-of', 'json', $output) $false $true
    $probeTask = $probeChild.Process.StandardOutput.ReadToEndAsync()
    Finish-Hidden $probeChild 'probe-output'
    $probeText = $probeTask.GetAwaiter().GetResult()
    [IO.File]::WriteAllText($probe, $probeText)
    $streams = ($probeText | ConvertFrom-Json).streams
    if ($streams.Count -ne 1 -or $streams[0].codec_name -ne 'h264' -or $streams[0].width -ne 800 -or $streams[0].height -ne 480) { throw 'Output codec, dimensions or stream count is incorrect' }
    $bytes = [IO.File]::ReadAllBytes($output)
    $nalTypes = New-Object 'Collections.Generic.HashSet[int]'
    for ($offset = 0; $offset -lt $bytes.Length - 4; $offset++) {
        if ($bytes[$offset] -eq 0 -and $bytes[$offset + 1] -eq 0 -and $bytes[$offset + 2] -eq 1) {
            [void]$nalTypes.Add($bytes[$offset + 3] -band 31)
        }
    }
    foreach ($nalType in @(7, 8, 5)) { if (-not $nalTypes.Contains($nalType)) { throw "Missing Annex-B NAL type $nalType" } }
    $results.Add([ordered]@{ Test = 'annex-b-sps-pps-idr'; Passed = $true })
    $rawCopy = Join-Path $OutputDirectory 'raw_copy.h264'
    Run-Hidden $Executable @('--source', $output, '--ffmpeg', $ffmpeg, '--output', $rawCopy, '--read-size', '7') 'api-raw-small-blocks'
    if ((Get-FileHash -LiteralPath $output).Hash -ne (Get-FileHash -LiteralPath $rawCopy).Hash) { throw 'Raw Annex-B bytes changed across small reads' }
    foreach ($iteration in 1..5) {
        $repeat = Join-Path $OutputDirectory "repeat_$iteration.h264"
        Run-Hidden $Executable @('--source', $source, '--ffmpeg', $ffmpeg, '--output', $repeat, '--read-size', '4096') "api-repeat-$iteration"
        if ((Get-FileHash -LiteralPath $output).Hash -ne (Get-FileHash -LiteralPath $repeat).Hash) { throw "Repeat $iteration lost bytes at EOF" }
    }
    Run-Hidden $Executable @('--source', $source, '--ffmpeg', $ffmpeg, '--output', (Join-Path $OutputDirectory 'stopped.h264'), '--stop-after', '32768') 'api-early-stop'
    Run-Hidden $Executable @('--source', (Join-Path $OutputDirectory 'missing.mp4'), '--ffmpeg', $ffmpeg, '--output', (Join-Path $OutputDirectory 'missing.h264')) 'api-missing-source' $true
    Run-Hidden $Executable @('--source', $source, '--ffmpeg', (Join-Path $OutputDirectory 'missing-ffmpeg.exe'), '--output', (Join-Path $OutputDirectory 'missing_tool.h264')) 'api-missing-tool' $true
    $unsupported = Join-Path $OutputDirectory 'not_h264.mp4'
    Run-Hidden $ffmpeg @('-hide_banner', '-loglevel', 'error', '-nostdin', '-y', '-f', 'lavfi', '-i', 'testsrc2=size=64x64:rate=1', '-frames:v', '1', '-c:v', 'mpeg4', $unsupported) 'generate-non-h264'
    $rejected = Join-Path $OutputDirectory 'rejected.h264'
    Run-Hidden $Executable @('--source', $unsupported, '--ffmpeg', $ffmpeg, '--output', $rejected) 'api-reject-non-h264' $true
    if ((Get-Item -LiteralPath $rejected).Length -ne 0) { throw 'Non-H264 source leaked bytes' }
    foreach ($codec in @('libx265', 'libaom-av1')) {
        $codecSource = Join-Path $OutputDirectory "$codec.mp4"
        $codecOutput = Join-Path $OutputDirectory "$codec-rejected.h264"
        $codecArguments = @('-hide_banner', '-loglevel', 'error', '-nostdin', '-y', '-f', 'lavfi', '-i', 'testsrc2=size=64x64:rate=1', '-frames:v', '1', '-c:v', $codec, '-threads', '1')
        if ($codec -eq 'libx265') { $codecArguments += @('-x265-params', 'log-level=error:pools=none:frame-threads=1') }
        else { $codecArguments += @('-cpu-used', '8') }
        Run-Hidden $ffmpeg ($codecArguments + @($codecSource)) "generate-$codec"
        Run-Hidden $Executable @('--source', $codecSource, '--ffmpeg', $ffmpeg, '--output', $codecOutput) "api-reject-$codec" $true
        if ((Get-Item -LiteralPath $codecOutput).Length -ne 0) { throw "$codec leaked non-H264 bytes" }
    }
    $corruptSource = Join-Path $OutputDirectory 'corrupt.mp4'
    [IO.File]::WriteAllText($corruptSource, 'not a video')
    Run-Hidden $Executable @('--source', $corruptSource, '--ffmpeg', $ffmpeg, '--output', (Join-Path $OutputDirectory 'corrupt.h264')) 'api-corrupt-source' $true
    $audioSource = Join-Path $OutputDirectory 'audio-only.m4a'
    Run-Hidden $ffmpeg @('-hide_banner', '-loglevel', 'error', '-nostdin', '-y', '-f', 'lavfi', '-i', 'sine=frequency=440:sample_rate=48000', '-t', '0.1', '-c:a', 'aac', $audioSource) 'generate-audio-only'
    Run-Hidden $Executable @('--source', $audioSource, '--ffmpeg', $ffmpeg, '--output', (Join-Path $OutputDirectory 'audio-only.h264')) 'api-reject-audio-only' $true
    $player = $null
    $producer = $null
    try {
        $player = Start-Hidden $ffplay @('-hide_banner', '-loglevel', 'info', '-autoexit', '-an', '-f', 'h264', '-threads', '1', '-probesize', '65536', '-analyzeduration', '1000000', '-i', 'pipe:0') $true $false $true
        $producer = Start-Hidden $Executable @('--source', $source, '--ffmpeg', $ffmpeg, '--stdout') $false $true
        $copyTask = $producer.Process.StandardOutput.BaseStream.CopyToAsync($player.Process.StandardInput.BaseStream)
        if (-not $copyTask.Wait(20000)) { throw 'API to ffplay pipe timed out' }
        [void]$copyTask.GetAwaiter().GetResult()
        $player.Process.StandardInput.Close()
        Finish-Hidden $producer 'api-live-pipe-producer'
        $producer = $null
        Finish-Hidden $player 'ffplay-background-consumer'
        $player = $null
        $playerLog = Get-Content -Raw -LiteralPath (Join-Path $OutputDirectory 'ffplay-background-consumer.log')
        if ($playerLog -notmatch 'Video: h264' -or $playerLog -match 'Error while decoding|Invalid data found') { throw 'ffplay did not accept the H264 stream' }
    } finally {
        foreach ($child in @($producer, $player)) {
            if ($null -ne $child) {
                try { if (-not $child.Process.HasExited) { $child.Process.Kill(); $child.Process.WaitForExit() } } finally { $child.Process.Dispose() }
            }
        }
    }
} finally {
    $results | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'results.json') -Encoding UTF8
}
