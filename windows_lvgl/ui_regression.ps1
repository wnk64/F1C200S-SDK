param(
    [int]$Iterations = 5,
    [string]$OutputDirectory = 'D:\AITools\build\wiliwili_ascii\offline_regression_20260926',
    [string]$Executable = 'D:\AITools\build\wiliwili_ascii\wiliwili_lvgl.exe',
    [switch]$OversizedCover,
    [switch]$LargeCoverDimensions,
    [switch]$CdnCover,
    [ValidateSet('Bmp', 'Png', 'Jpeg', 'Webp')]
    [string]$CoverFormat = 'Bmp'
)

$ErrorActionPreference = 'Stop'
if ($Iterations -lt 1 -or $Iterations -gt 100) { throw 'Iterations must be between 1 and 100' }
Add-Type -AssemblyName System.Drawing
$tool = Join-Path $PSScriptRoot 'ui_audit.ps1'
$fixtures = Join-Path $OutputDirectory 'fixtures'
New-Item -ItemType Directory -Path $fixtures -Force | Out-Null
$encoding = New-Object Text.UTF8Encoding $false
function Save-Fixture($name, $data) {
    [IO.File]::WriteAllText((Join-Path $fixtures $name), ($data | ConvertTo-Json -Depth 10 -Compress), $encoding)
}
$feed = @()
$search = @()
foreach ($index in 1..12) {
    $videoId = 'BVTEST' + $index.ToString('000')
    $title = "离线测试 $index 中文长标题与 English text 混排，检查标题省略和作者信息是否重叠"
    $owner = "测试作者 $index"
    $pic = if ($CdnCover) { "https://i0.hdslb.com/bfs/archive/cover_$index" } else { "https://audit.invalid/cover_$index.bmp" }
    $feed += [ordered]@{ bvid = $videoId; title = $title; pic = $pic; owner = @{ name = $owner }; stat = @{ view = 1000 + $index }; track_id = 'audit'; dimension = @{} }
    $search += [ordered]@{ bvid = $videoId; title = $title; pic = $pic; author = $owner; play = 1000 + $index; pubdate = 0 }
    Save-Fixture "view_$videoId.json" @{ code = 0; data = [ordered]@{ cid = $index; aid = $index; duration = 120; title = $title; desc = 'Offline fixture, not a real Bilibili video'; owner = @{ name = $owner }; stat = @{ view = 1000 + $index }; pages = @([ordered]@{ cid = $index; duration = 120; part = 'Fixture part' }) } }
    $image = New-Object Drawing.Bitmap 186, 104
    $graphics = [Drawing.Graphics]::FromImage($image)
    $color = [Drawing.Color]::FromArgb(255, (30 + $index * 17) % 255, (80 + $index * 29) % 255, (120 + $index * 41) % 255)
    $graphics.Clear($color)
    $font = New-Object Drawing.Font 'Arial', 18
    $graphics.DrawString("TEST $index", $font, [Drawing.Brushes]::White, 12, 32)
    $coverPath = Join-Path $fixtures "cover_$index.bmp"
    $imageFormat = if ($CoverFormat -eq 'Jpeg') { [Drawing.Imaging.ImageFormat]::Jpeg } elseif ($CoverFormat -in @('Png', 'Webp')) { [Drawing.Imaging.ImageFormat]::Png } else { [Drawing.Imaging.ImageFormat]::Bmp }
    $image.Save($coverPath, $imageFormat)
    $font.Dispose()
    $graphics.Dispose()
    $image.Dispose()
    if ($CoverFormat -eq 'Webp') {
        $webpPath = Join-Path $fixtures "cover_$index.webp"
        $info = New-Object Diagnostics.ProcessStartInfo
        $info.FileName = 'D:\AITools\ffmpeg\ffmpeg-9.0.2-essentials_build\bin\ffmpeg.exe'
        $info.Arguments = '-hide_banner -loglevel error -nostdin -y -i "' + $coverPath + '" -c:v libwebp -lossless 1 -f webp "' + $webpPath + '"'
        $info.UseShellExecute = $false
        $info.CreateNoWindow = $true
        $info.RedirectStandardError = $true
        $encoder = New-Object Diagnostics.Process
        $encoder.StartInfo = $info
        try {
            [void]$encoder.Start()
            $encodeError = $encoder.StandardError.ReadToEndAsync()
            if (-not $encoder.WaitForExit(15000)) { throw 'Webp fixture encode timed out' }
            if ($encoder.ExitCode) { throw $encodeError.GetAwaiter().GetResult() }
        } finally { if (-not $encoder.HasExited) { $encoder.Kill(); $encoder.WaitForExit() }; $encoder.Dispose() }
        Move-Item -LiteralPath $webpPath -Destination $coverPath -Force
    }
    if ($LargeCoverDimensions -and $index -eq 1) {
        $large = New-Object Drawing.Bitmap 2048, 2048
        $largeGraphics = [Drawing.Graphics]::FromImage($large)
        try { $largeGraphics.Clear($color); $large.Save($coverPath, [Drawing.Imaging.ImageFormat]::Png) } finally { $largeGraphics.Dispose(); $large.Dispose() }
        if ((Get-Item -LiteralPath $coverPath).Length -ge 128KB) { throw 'Large-dimension fixture must remain below encoded byte limit' }
    }
    if ($OversizedCover -and $index -eq 1) {
        $coverPath = Join-Path $fixtures 'cover_1.bmp'
        $original = [IO.File]::ReadAllBytes($coverPath)
        $oversized = New-Object byte[] (256 * 1024)
        [Buffer]::BlockCopy($original, 0, $oversized, 0, $original.Length)
        [IO.File]::WriteAllBytes($coverPath, $oversized)
    }
}
Save-Fixture 'feed.json' @{ code = 0; data = @{ item = @($feed[0..7]) } }
Save-Fixture 'search_1.json' @{ code = 0; data = @{ result = @($search[0..7]) } }
Save-Fixture 'search_2.json' @{ code = 0; data = @{ result = @($search[4..11]) } }
Save-Fixture 'empty.json' @{ code = 0; data = @{ result = @(); replies = @() } }
Save-Fixture 'hots.json' @{ code = 0; data = @{ list = @(@{ keyword = 'offline music'; heat_score = 100 }, @{ keyword = '离线热词'; heat_score = 50 }) } }
$results = New-Object 'Collections.Generic.List[object]'
$testProcessId = 0
function Invoke-Ui([string]$action, [hashtable]$parameters = @{}) {
    $parameters['Action'] = $action
    $parameters['OutputDirectory'] = $OutputDirectory
    if ($action -ne 'Launch') { $parameters['ProcessId'] = $testProcessId }
    $result = (& $tool @parameters | ConvertFrom-Json)
    $results.Add($result)
    return $result
}
function Snapshot([string]$name) { return Invoke-Ui 'Capture' @{ Name = $name; WaitMilliseconds = 150 } }
function Region-Hash([string]$path, [Drawing.Rectangle]$region) {
    $image = [Drawing.Bitmap]::FromFile($path)
    $crop = $image.Clone($region, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $stream = New-Object IO.MemoryStream
    $hash = [Security.Cryptography.SHA256]::Create()
    try {
        $crop.Save($stream, [Drawing.Imaging.ImageFormat]::Png)
        return [BitConverter]::ToString($hash.ComputeHash($stream.ToArray()))
    } finally { $hash.Dispose(); $stream.Dispose(); $crop.Dispose(); $image.Dispose() }
}
$header = New-Object Drawing.Rectangle 72, 0, 728, 74
$contentRegion = New-Object Drawing.Rectangle 72, 146, 720, 324
try {
    $launch = Invoke-Ui 'Launch' @{ Executable = $Executable; FixtureDirectory = $fixtures; WaitMilliseconds = 2000 }
    $testProcessId = $launch.ProcessId
    if ($launch.ClientWidth -ne 800 -or $launch.ClientHeight -ne 480) { throw 'Unexpected client size' }
    $baseline = Snapshot 'home_baseline'
    if ($CdnCover) {
        $image = [Drawing.Bitmap]::FromFile($baseline.Screenshot)
        try {
            foreach ($sample in @(@(2, 452, 150), @(3, 95, 360), @(4, 452, 360))) {
                $index = $sample[0]
                $actual = $image.GetPixel($sample[1], $sample[2])
                $expected = [Drawing.Color]::FromArgb(255, (30 + $index * 17) % 255, (80 + $index * 29) % 255, (120 + $index * 41) % 255)
                foreach ($channel in @('R', 'G', 'B')) {
                    if ([Math]::Abs([int]$actual.$channel - [int]$expected.$channel) -gt 15) { throw "Resized CDN cover $index missing: $actual" }
                }
            }
        } finally { $image.Dispose() }
        Write-Output 'PASS resized-cdn-covers'
    }
    if ($OversizedCover -or $LargeCoverDimensions) {
        $image = [Drawing.Bitmap]::FromFile($baseline.Screenshot)
        try { $coverPixel = $image.GetPixel(95, 150) } finally { $image.Dispose() }
        if ([Math]::Abs([int]$coverPixel.R - 47) -le 7 -and [Math]::Abs([int]$coverPixel.G - 109) -le 7 -and [Math]::Abs([int]$coverPixel.B - 161) -le 7) { throw 'Unsafe cover was still decoded' }
        if ($LargeCoverDimensions) { Write-Output 'PASS cover-dimension-limit' } else { Write-Output 'PASS oversized-cover-limit' }
    }
    $baselineHeader = Region-Hash $baseline.Screenshot $header
    foreach ($iteration in 1..$Iterations) {
        Invoke-Ui 'Click' @{ X = 150; Y = 180; WaitMilliseconds = 500 } | Out-Null
        $detail = Snapshot "detail_$iteration"
        if ((Region-Hash $detail.Screenshot $header) -eq $baselineHeader) { throw 'Card did not leave Home' }
        Invoke-Ui 'Click' @{ X = 250; Y = 390; WaitMilliseconds = 350 } | Out-Null
        Snapshot "player_$iteration" | Out-Null
        Invoke-Ui 'Key' @{ VirtualKey = 27; WaitMilliseconds = 400 } | Out-Null
        $returned = Snapshot "returned_$iteration"
        if ((Region-Hash $returned.Screenshot $header) -ne $baselineHeader) { throw 'Home header was not restored' }
        Write-Output "PASS player-return iteration $iteration"
    }
    Invoke-Ui 'Click' @{ X = 420; Y = 56; WaitMilliseconds = 100 } | Out-Null
    Invoke-Ui 'Type' @{ Text = 'music'; WaitMilliseconds = 100 } | Out-Null
    Invoke-Ui 'Click' @{ X = 690; Y = 56; WaitMilliseconds = 800 } | Out-Null
    $searchBefore = Snapshot 'search_before_scroll'
    Invoke-Ui 'Scroll' @{ X = 550; Y = 300; Delta = -4; WaitMilliseconds = 200 } | Out-Null
    $searchAfter = Snapshot 'search_after_scroll'
    if ((Region-Hash $searchBefore.Screenshot $contentRegion) -eq (Region-Hash $searchAfter.Screenshot $contentRegion)) { throw 'Search wheel did not move results' }
    Write-Output 'PASS search-wheel'
    Invoke-Ui 'Click' @{ X = 550; Y = 119; WaitMilliseconds = 800 } | Out-Null
    Snapshot 'search_append' | Out-Null
    foreach ($scrollPass in 1..2) {
        Invoke-Ui 'Scroll' @{ X = 550; Y = 300; Delta = -20; WaitMilliseconds = 200 } | Out-Null
    }
    $appended = Snapshot 'search_append_bottom'
    $image = [Drawing.Bitmap]::FromFile($appended.Screenshot)
    try {
        foreach ($coverIndex in 9..12) {
            $sampleX = if ($coverIndex % 2 -eq 1) { 86 } else { 452 }
            $sampleY = if ($coverIndex -le 10) { 162 } else { 332 }
            $actual = $image.GetPixel($sampleX, $sampleY)
            $expected = [Drawing.Color]::FromArgb(255, (30 + $coverIndex * 17) % 255, (80 + $coverIndex * 29) % 255, (120 + $coverIndex * 41) % 255)
            $wrongIndex = $coverIndex - 4
            $wrong = [Drawing.Color]::FromArgb(255, (30 + $wrongIndex * 17) % 255, (80 + $wrongIndex * 29) % 255, (120 + $wrongIndex * 41) % 255)
            $matchesExpected = $true
            $matchesWrong = $true
            foreach ($channel in @('R', 'G', 'B')) {
                if ([Math]::Abs([int]$actual.$channel - [int]$expected.$channel) -gt 7) { $matchesExpected = $false }
                if ([Math]::Abs([int]$actual.$channel - [int]$wrong.$channel) -gt 7) { $matchesWrong = $false }
            }
            if (-not $matchesExpected -or $matchesWrong) { throw "Appended result $coverIndex does not use its own cover: $actual" }
        }
    } finally { $image.Dispose() }
    Write-Output 'PASS append-cover-alignment'
    Invoke-Ui 'Click' @{ X = 32; Y = 114; WaitMilliseconds = 250 } | Out-Null
    Invoke-Ui 'Key' @{ VirtualKey = 82; WaitMilliseconds = 0 } | Out-Null
    Invoke-Ui 'Click' @{ X = 32; Y = 310; WaitMilliseconds = 350 } | Out-Null
    $settingsBefore = Snapshot 'settings_request_pending'
    Start-Sleep -Milliseconds 1300
    $settingsAfter = Snapshot 'settings_request_finished'
    if ($settingsBefore.Sha256 -ne $settingsAfter.Sha256) { throw 'Late feed response changed Settings' }
    Write-Output 'PASS late-feed-navigation'
    Invoke-Ui 'Click' @{ X = 250; Y = 118; WaitMilliseconds = 300 } | Out-Null
    $interfaceTop = Snapshot 'settings_interface'
    Invoke-Ui 'Scroll' @{ X = 550; Y = 400; Delta = -20; WaitMilliseconds = 200 } | Out-Null
    $interfaceBottom = Snapshot 'settings_interface_bottom'
    if ((Region-Hash $interfaceTop.Screenshot $contentRegion) -eq (Region-Hash $interfaceBottom.Screenshot $contentRegion)) { throw 'Interface settings cannot scroll to lower controls' }
    if ((Region-Hash $interfaceTop.Screenshot $header) -ne (Region-Hash $interfaceBottom.Screenshot $header)) { throw 'Settings scroll moved the fixed header' }
    Invoke-Ui 'Scroll' @{ X = 550; Y = 400; Delta = 20; WaitMilliseconds = 200 } | Out-Null
    $interfaceRestored = Snapshot 'settings_interface_restored'
    if ($interfaceTop.Sha256 -ne $interfaceRestored.Sha256) { throw 'Interface settings did not return to the top' }
    Invoke-Ui 'Click' @{ X = 125; Y = 118; WaitMilliseconds = 300 } | Out-Null
    $playbackTop = Snapshot 'settings_playback'
    Invoke-Ui 'Scroll' @{ X = 550; Y = 400; Delta = -20; WaitMilliseconds = 200 } | Out-Null
    $playbackBottom = Snapshot 'settings_playback_bottom'
    if ((Region-Hash $playbackTop.Screenshot $contentRegion) -eq (Region-Hash $playbackBottom.Screenshot $contentRegion)) { throw 'Playback settings cannot scroll to codec control' }
    Invoke-Ui 'Click' @{ X = 250; Y = 118; WaitMilliseconds = 300 } | Out-Null
    $interfaceReopened = Snapshot 'settings_interface_reopened'
    if ($interfaceTop.Sha256 -ne $interfaceReopened.Sha256) { throw 'Settings tab did not reset scroll position' }
    Write-Output 'PASS settings-scroll-and-tabs'
    Invoke-Ui 'Click' @{ X = 380; Y = 118; WaitMilliseconds = 300 } | Out-Null
    $otherTop = Snapshot 'settings_other'
    $otherImage = [Drawing.Bitmap]::FromFile($otherTop.Screenshot)
    try {
        $proxyBackground = $otherImage.GetPixel(100, 450)
        if ($proxyBackground.R -gt 100 -or $proxyBackground.G -gt 100 -or $proxyBackground.B -gt 100) {
            throw "Proxy input does not match the dark settings theme: $proxyBackground"
        }
    } finally { $otherImage.Dispose() }
    Invoke-Ui 'Scroll' @{ X = 550; Y = 400; Delta = -20; WaitMilliseconds = 200 } | Out-Null
    $otherAfter = Snapshot 'settings_other_after_wheel'
    if ($otherTop.Sha256 -ne $otherAfter.Sha256) { throw 'Other settings overflow the compact viewport' }
    Write-Output 'PASS settings-other-fits'
    foreach ($navigationY in @(160, 205, 264, 310, 114)) {
        Invoke-Ui 'Click' @{ X = 32; Y = $navigationY; WaitMilliseconds = 300 } | Out-Null
        Snapshot "navigation_$navigationY" | Out-Null
    }
    Write-Output 'PASS section-navigation'
} finally {
    if ($testProcessId -and (Get-Process -Id $testProcessId -ErrorAction SilentlyContinue)) {
        Invoke-Ui 'Close' | Out-Null
    }
    $results | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'results.json') -Encoding UTF8
}
