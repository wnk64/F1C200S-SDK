param(
    [string]$Executable = 'D:\AITools\build\wiliwili_ascii\wiliwili_lvgl.exe',
    [string]$OutputDirectory = 'D:\AITools\build\wiliwili_ascii\qr_login_regression_20260927'
)

$ErrorActionPreference = 'Stop'
$tool = Join-Path $PSScriptRoot 'ui_audit.ps1'
$fixtures = Join-Path $OutputDirectory 'fixtures'
New-Item -ItemType Directory -Path $fixtures -Force | Out-Null
$utf8 = New-Object Text.UTF8Encoding $false
[IO.File]::WriteAllText((Join-Path $fixtures 'empty.json'), '{}', $utf8)

function Invoke-Audit($action, $processId, $name) {
    $parameters = @{ Action = $action; OutputDirectory = $OutputDirectory }
    if ($action -eq 'Launch') {
        $parameters['Executable'] = $Executable
        $parameters['Arguments'] = '--mine'
        $parameters['FixtureDirectory'] = $fixtures
        $parameters['WaitMilliseconds'] = 300
    } else {
        $parameters['ProcessId'] = $processId
        if ($action -eq 'Click') {
            $parameters['X'] = 1120
            $parameters['Y'] = 160
            $parameters['WaitMilliseconds'] = 400
        }
        if ($action -eq 'Capture') {
            $parameters['Name'] = $name
            $parameters['WaitMilliseconds'] = 300
        }
    }
    return (& $tool @parameters | ConvertFrom-Json)
}

function Wait-Log($path, $pattern) {
    for ($iteration = 0; $iteration -lt 30; ++$iteration) {
        if ((Test-Path -LiteralPath $path) -and
            ((Get-Content -LiteralPath $path -Raw) -match $pattern)) { return }
        Start-Sleep -Milliseconds 350
    }
    throw "QR state was not observed: $pattern"
}

function Assert-DistinctDynamicRows($screenshot, $count) {
    $image = [Drawing.Bitmap]::FromFile($screenshot)
    try {
        $hashes = @()
        for ($index = 0; $index -lt $count; ++$index) {
            $crop = New-Object Drawing.Bitmap(360, 22)
            $graphics = [Drawing.Graphics]::FromImage($crop)
            $stream = New-Object IO.MemoryStream
            try {
                $graphics.DrawImage($image, (New-Object Drawing.Rectangle(0, 0, 360, 22)),
                    (New-Object Drawing.Rectangle(450, (190 + 60 * $index), 360, 22)),
                    [Drawing.GraphicsUnit]::Pixel)
                $crop.Save($stream, [Drawing.Imaging.ImageFormat]::Png)
                $hashes += [Convert]::ToBase64String([Security.Cryptography.SHA256]::Create().ComputeHash($stream.ToArray()))
            } finally { $stream.Dispose(); $graphics.Dispose(); $crop.Dispose() }
        }
        if (@($hashes | Select-Object -Unique).Count -ne $count) {
            throw "Dynamic rows repeat or are not visible: $screenshot"
        }
    } finally { $image.Dispose() }
}

[IO.File]::WriteAllText((Join-Path $fixtures 'qr_generate.json'), '{"code":403}', $utf8)
$launch = Invoke-Audit 'Launch' 0 ''
$processId = $launch.ProcessId
try {
    $session = Get-Content -LiteralPath (Join-Path $OutputDirectory "session_$processId.json") -Raw | ConvertFrom-Json
    $log = Join-Path $session.Directory 'wiliwili_config.json.qr-poll.log'
    Invoke-Audit 'Click' $processId '' | Out-Null
    Wait-Log $log 'generate url=0 key=0.*api_code=403'
    Write-Output 'PASS QR generation failure reported without polling'
} finally {
    Invoke-Audit 'Close' $processId '' | Out-Null
}

[IO.File]::WriteAllText((Join-Path $fixtures 'qr_generate.json'),
    '{"code":0,"data":{"url":"https://example.invalid/qrcode/offline-test","qrcode_key":"offline-test-key"}}', $utf8)
[IO.File]::WriteAllText((Join-Path $fixtures 'nav.json'), '{"code":0,"data":{"uname":"OfflineUser"}}', $utf8)
[IO.File]::WriteAllText((Join-Path $fixtures 'dynamic_ups.json'), '{"code":0,"data":{"items":[]}}', $utf8)
[IO.File]::WriteAllText((Join-Path $fixtures 'dynamic_videos.json'), '{"code":0,"data":{"items":[]}}', $utf8)
[IO.File]::WriteAllText((Join-Path $fixtures 'dynamic_articles.json'), '{"code":0,"data":{"items":[]}}', $utf8)
[IO.File]::WriteAllText((Join-Path $fixtures 'dynamic_creator.json'), '{"code":0,"data":{"items":[]}}', $utf8)
[IO.File]::WriteAllText((Join-Path $fixtures 'qr_poll.json'), '{"code":0,"data":{"code":0}}', $utf8)
[IO.File]::WriteAllText((Join-Path $fixtures 'qr_poll_headers.txt'),
    "HTTP/1.1 200 OK`r`nSet-Cookie: SESSDATA=offline-fake; Path=/`r`nSet-Cookie: DedeUserID=1; Path=/`r`nSet-Cookie: bili_jct=offline-fake; Path=/`r`n", $utf8)
$launch = Invoke-Audit 'Launch' 0 ''
$processId = $launch.ProcessId
try {
    $session = Get-Content -LiteralPath (Join-Path $OutputDirectory "session_$processId.json") -Raw | ConvertFrom-Json
    $log = Join-Path $session.Directory 'wiliwili_config.json.qr-poll.log'
    Invoke-Audit 'Click' $processId '' | Out-Null
    Wait-Log $log 'success session_headers=1'
    Start-Sleep -Milliseconds 1500
    & $tool -Action Click -ProcessId $processId -X 50 -Y 145 -OutputDirectory $OutputDirectory -WaitMilliseconds 300 | Out-Null
    Invoke-Audit 'Capture' $processId 'logged_in_home' | Out-Null
    & $tool -Action Click -ProcessId $processId -X 50 -Y 200 -OutputDirectory $OutputDirectory -WaitMilliseconds 300 | Out-Null
    Invoke-Audit 'Capture' $processId 'logged_in_dynamic' | Out-Null
    $cpuBefore = (Get-Process -Id $processId).CPU
    Start-Sleep -Seconds 4
    $cpuAfter = (Get-Process -Id $processId).CPU
    if ($cpuAfter - $cpuBefore -gt 2) { throw "Authenticated empty feed is busy-looping: $($cpuAfter - $cpuBefore)s CPU in 4s" }
    $emptyDynamic = Invoke-Audit 'Capture' $processId 'logged_in_dynamic_idle'
    [IO.File]::WriteAllText((Join-Path $fixtures 'dynamic_ups.json'),
        '{"code":0,"data":{"items":[{"mid":42,"uname":"Offline\ud83d\ude00\uc2dc Creator"}]}}', $utf8)
    [IO.File]::WriteAllText((Join-Path $fixtures 'dynamic_videos.json'),
        '{"code":0,"data":{"items":[{"id_str":"123","modules":{"module_dynamic":{"major":{"archive":{"bvid":"BV1Da411Y7U4","title":"Offline \"video\" \ud83d\ude00\uc2dc title with a very long headline that should never overlap its like button or the next item in the list","desc":"Offline summary that intentionally exceeds the visible card width to test the single line clipping instead of drawing into the next card"}}}}},{"id_str":"126","modules":{"module_dynamic":{"major":{"archive":{"bvid":"BV1xx411c7mD","title":"Second offline video","desc":"Second summary"}}}}}],"offset":"next"}}', $utf8)
    [IO.File]::WriteAllText((Join-Path $fixtures 'view_BV1Da411Y7U4.json'),
        '{"code":0,"data":{"owner":{"name":"Offline Creator","vip":{"label":{"title":"\u5e74\u5ea6\u5927\u4f1a\u5458"}}},"bvid":"BV1Da411Y7U4","aid":100,"cid":200,"title":"Offline video detail","duration":15,"pages":[{"cid":200,"duration":15,"part":"Part one"}]}}', $utf8)
    [IO.File]::WriteAllText((Join-Path $fixtures 'view_BV1xx411c7mD.json'),
        '{"code":0,"data":{"owner":{"name":"Second Creator","vip":{"label":{"title":"\u5e74\u5ea6\u5927\u4f1a\u5458"}}},"bvid":"BV1xx411c7mD","aid":101,"cid":201,"title":"Second video detail","duration":16,"pages":[{"cid":201,"duration":16,"part":"Part one"}]}}', $utf8)
    [IO.File]::WriteAllText((Join-Path $fixtures 'dynamic_articles.json'),
        ('{"code":0,"data":{"items":[' + (($articleItems = for ($index = 0; $index -lt 8; ++$index) {
            $padding = ',"extra_metadata":"' + ('X' * 70000) + '"'
            '{"id_str":"' + (124 + $index) + '"' + $padding + ',"modules":{"module_author":{"vip":{"label":{"title":"\u5e74\u5ea6\u5927\u4f1a\u5458"}}},"module_dynamic":{"desc":{"text":"Article update number ' + $index + ' has distinct content"}}}}'
        }) -join ',') + ']}}'), $utf8)
    [IO.File]::WriteAllText((Join-Path $fixtures 'dynamic_creator.json'),
        ('{"code":0,"data":{"items":[' + (($creatorItems = for ($index = 0; $index -lt 8; ++$index) {
            $data = if ($index -eq 6) { '{"module_type":"MODULE_TYPE_DYNAMIC","module_dynamic":{"type":"MDL_DYN_TYPE_FORWARD","dyn_forward":{"item":{"modules":[{"module_type":"MODULE_TYPE_DESC","module_desc":{"text":"Forwarded original number 6 is unique"}}]}}}}' }
                    elseif ($index -eq 7) { '{"module_type":"MODULE_TYPE_DYNAMIC","module_dynamic":{"major":{"draw":{"items":[{"src":"offline"}]}}}}' }
                    elseif ($index % 2 -eq 0) { '{"module_type":"MODULE_TYPE_DYNAMIC","module_dynamic":{"type":"MDL_DYN_TYPE_ARCHIVE","dyn_archive":{"title":"Creator video number ' + $index + ' is unique","bvid":"BV1Da411Y7U4","desc":"Archive summary"}}}' }
                    else { '{"module_type":"MODULE_TYPE_DESC","module_desc":{"text":"Creator text number ' + $index + ' is unique"}}' }
            $original = if ($index -eq 7) { ',"orig":{"modules":{"module_dynamic":{"major":{"opus":{"summary":{"text":"Forwarded original number 7 is unique"}}}}}}' } else { '' }
            '{"id_str":"' + (125 + $index) + '","modules":[{"module_type":"MODULE_TYPE_AUTHOR","module_author":{"user":{"vip":{"label":{"title":"\u5e74\u5ea6\u5927\u4f1a\u5458"}}}}},' + $data + ']' + $original + '}'
        }) -join ',') + ']}}'), $utf8)
    foreach ($fixtureName in @('dynamic_articles.json', 'dynamic_creator.json')) {
        $null = Get-Content -LiteralPath (Join-Path $fixtures $fixtureName) -Raw | ConvertFrom-Json
    }
    & $tool -Action Click -ProcessId $processId -X 50 -Y 145 -OutputDirectory $OutputDirectory -WaitMilliseconds 300 | Out-Null
    & $tool -Action Click -ProcessId $processId -X 50 -Y 200 -OutputDirectory $OutputDirectory -WaitMilliseconds 600 | Out-Null
    & $tool -Action Click -ProcessId $processId -X 445 -Y 135 -OutputDirectory $OutputDirectory -WaitMilliseconds 900 | Out-Null
    $loadedDynamic = Invoke-Audit 'Capture' $processId 'logged_in_dynamic_with_items'
    if ($emptyDynamic.Sha256 -eq $loadedDynamic.Sha256) { throw 'Dynamic content did not render after offline feed update' }
    & $tool -Action Click -ProcessId $processId -X 530 -Y 224 -OutputDirectory $OutputDirectory -WaitMilliseconds 1200 | Out-Null
    $videoDetail = Invoke-Audit 'Capture' $processId 'logged_in_dynamic_video_detail'
    $detailImage = [Drawing.Bitmap]::FromFile($videoDetail.Screenshot)
    try {
        $commentBackground = $detailImage.GetPixel(1150, 445)
        if ($commentBackground.R -gt 100 -or $commentBackground.G -gt 100 -or $commentBackground.B -gt 100) {
            throw "Comment input does not match the dark detail theme: $commentBackground"
        }
    } finally { $detailImage.Dispose() }
    if ($videoDetail.Sha256 -eq $loadedDynamic.Sha256) { throw 'Dynamic video row did not open detail' }
    & $tool -Action Click -ProcessId $processId -X 950 -Y 635 -OutputDirectory $OutputDirectory -WaitMilliseconds 600 | Out-Null
    $backToDynamic = Invoke-Audit 'Capture' $processId 'logged_in_dynamic_return'
    if ($backToDynamic.Sha256 -ne $loadedDynamic.Sha256) { throw 'Video detail did not return to Dynamic' }
    [IO.File]::WriteAllText((Join-Path $fixtures 'view_BV1Da411Y7U4.json'),
        '{"code":0,"data":{"owner":{"name":"Offline Creator"},"bvid":"BV1Da411Y7U4","aid":100,"cid":200,"title":"Offline video detail","duration":15,"pages":[{"cid":200,"duration":15,"part":"Part one"}]}}', $utf8)
    & $tool -Action Click -ProcessId $processId -X 530 -Y 224 -OutputDirectory $OutputDirectory -WaitMilliseconds 1200 | Out-Null
    $withoutBadge = Invoke-Audit 'Capture' $processId 'logged_in_dynamic_detail_without_badge'
    if ($videoDetail.Sha256 -ne $withoutBadge.Sha256) { throw 'VIP label replaced the actual video title' }
    & $tool -Action Click -ProcessId $processId -X 950 -Y 635 -OutputDirectory $OutputDirectory -WaitMilliseconds 600 | Out-Null
    & $tool -Action Click -ProcessId $processId -X 530 -Y 284 -OutputDirectory $OutputDirectory -WaitMilliseconds 1200 | Out-Null
    $secondDetail = Invoke-Audit 'Capture' $processId 'logged_in_dynamic_second_video_detail'
    if ($secondDetail.Sha256 -eq $videoDetail.Sha256) { throw 'Different videos display the same detail' }
    & $tool -Action Click -ProcessId $processId -X 950 -Y 635 -OutputDirectory $OutputDirectory -WaitMilliseconds 600 | Out-Null
    & $tool -Action Click -ProcessId $processId -X 530 -Y 224 -OutputDirectory $OutputDirectory -WaitMilliseconds 1200 | Out-Null
    $reopenedDetail = Invoke-Audit 'Capture' $processId 'logged_in_dynamic_reopened_video_detail'
    if ($withoutBadge.Sha256 -ne $reopenedDetail.Sha256) { throw 'Video detail changed after opening another Dynamic card' }
    & $tool -Action Click -ProcessId $processId -X 950 -Y 635 -OutputDirectory $OutputDirectory -WaitMilliseconds 600 | Out-Null
    & $tool -Action Click -ProcessId $processId -X 560 -Y 135 -OutputDirectory $OutputDirectory -WaitMilliseconds 900 | Out-Null
    $articlesDynamic = Invoke-Audit 'Capture' $processId 'logged_in_dynamic_articles'
    Assert-DistinctDynamicRows $articlesDynamic.Screenshot 8
    & $tool -Action Click -ProcessId $processId -X 530 -Y 224 -OutputDirectory $OutputDirectory -WaitMilliseconds 250 | Out-Null
    $externalLog = Join-Path $session.Directory 'external-url.log'
    if (!(Test-Path -LiteralPath $externalLog) -or
        (Get-Content -LiteralPath $externalLog -Raw) -notmatch 'https://www\.bilibili\.com/opus/124') {
        throw 'Dynamic article row did not open its original page'
    }
    & $tool -Action Click -ProcessId $processId -X 222 -Y 245 -OutputDirectory $OutputDirectory -WaitMilliseconds 850 | Out-Null
    $creatorDynamic = Invoke-Audit 'Capture' $processId 'logged_in_dynamic_creator'
    Assert-DistinctDynamicRows $creatorDynamic.Screenshot 8
    if ($articlesDynamic.Sha256 -eq $creatorDynamic.Sha256) { throw 'Selecting an UP did not update the feed' }
    & $tool -Action Click -ProcessId $processId -X 212 -Y 184 -OutputDirectory $OutputDirectory -WaitMilliseconds 850 | Out-Null
    Invoke-Audit 'Capture' $processId 'logged_in_dynamic_all_updates' | Out-Null
    foreach ($round in 1..5) {
        & $tool -Action Click -ProcessId $processId -X 450 -Y 135 -OutputDirectory $OutputDirectory -WaitMilliseconds 30 | Out-Null
        & $tool -Action Click -ProcessId $processId -X 560 -Y 135 -OutputDirectory $OutputDirectory -WaitMilliseconds 1300 | Out-Null
        $afterFastSwitch = Invoke-Audit 'Capture' $processId "logged_in_dynamic_fast_switch_$round"
        if ($afterFastSwitch.Sha256 -ne $articlesDynamic.Sha256) { throw "Rapid Dynamic switch lost the Articles click, round $round" }
    }
    $cpuBefore = (Get-Process -Id $processId).CPU
    Start-Sleep -Seconds 3
    $cpuAfter = (Get-Process -Id $processId).CPU
    if ($cpuAfter - $cpuBefore -gt 2) { throw 'Populated dynamic feed is busy-looping' }
    foreach ($page in @(@('mine', 250), @('inbox', 303), @('settings', 355), @('home', 145))) {
        & $tool -Action Click -ProcessId $processId -X 50 -Y $page[1] -OutputDirectory $OutputDirectory -WaitMilliseconds 1100 | Out-Null
        Invoke-Audit 'Capture' $processId ("logged_in_" + $page[0]) | Out-Null
    }
    Write-Output 'PASS fake login, empty and populated feeds, creator selection, and authenticated navigation remain responsive'
} finally {
    try { Invoke-Audit 'Close' $processId '' | Out-Null } catch {
        $testProcess = Get-Process -Id $processId -ErrorAction SilentlyContinue
        if ($testProcess -and $testProcess.Path -eq $Executable -and
            $testProcess.StartTime.ToUniversalTime().ToString('o') -eq $session.StartTime) {
            Stop-Process -Id $processId -Force
        }
    }
}

[IO.File]::WriteAllText((Join-Path $fixtures 'qr_generate.json'),
    '{"code":0,"data":{"url":"https://example.invalid/qrcode/offline-test","qrcode_key":"offline-test-key"}}', $utf8)
[IO.File]::WriteAllText((Join-Path $fixtures 'qr_poll.json'),
    '{"code":0,"data":{"code":86101}}', $utf8)
$launch = Invoke-Audit 'Launch' 0 ''
$processId = $launch.ProcessId
try {
    $session = Get-Content -LiteralPath (Join-Path $OutputDirectory "session_$processId.json") -Raw | ConvertFrom-Json
    $log = Join-Path $session.Directory 'wiliwili_config.json.qr-poll.log'
    $before = Invoke-Audit 'Capture' $processId 'mine_before_login'
    Invoke-Audit 'Click' $processId '' | Out-Null
    Wait-Log $log 'poll 1 api=0 state=86101'
    $after = Invoke-Audit 'Capture' $processId 'qr_waiting_for_scan'
    if ($before.Sha256 -eq $after.Sha256) { throw 'QR screen did not appear' }
    [IO.File]::WriteAllText((Join-Path $fixtures 'qr_poll.json'),
        '{"code":0,"data":{"code":86090}}', $utf8)
    Wait-Log $log 'state=86090'
    & $tool -Action Click -ProcessId $processId -X 50 -Y 145 -OutputDirectory $OutputDirectory -WaitMilliseconds 300 | Out-Null
    Start-Sleep -Seconds 4
    Invoke-Audit 'Capture' $processId 'left_qr_during_poll' | Out-Null
    Write-Output 'PASS QR shows and polls waiting/phone-confirmation states'
    Write-Output 'PASS leaving QR page during polling keeps UI alive'
} finally {
    Invoke-Audit 'Close' $processId '' | Out-Null
}
