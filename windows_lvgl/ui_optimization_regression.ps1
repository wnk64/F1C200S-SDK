param(
    [string]$Executable = 'D:\AITools\build\wiliwili_optimized_20260930\wiliwili_lvgl.exe',
    [string]$OutputDirectory = 'D:\AITools\build\wiliwili_optimized_20260930\optimization_regression',
    [int]$Iterations = 3,
    [switch]$SoftwareRendering
)

$ErrorActionPreference = 'Stop'
if ($Iterations -lt 1 -or $Iterations -gt 20) { throw 'Iterations must be between 1 and 20' }
Add-Type -AssemblyName System.Drawing
$tool = Join-Path $PSScriptRoot 'ui_audit.ps1'
$baseDirectory = Join-Path $OutputDirectory 'base_ui'
$fixtures = Join-Path $baseDirectory 'fixtures'
$results = New-Object 'Collections.Generic.List[object]'
$testProcessId = 0
$previousSoftware = $env:WILIWILI_UI_AUDIT_SOFTWARE

function Invoke-Ui([string]$action, [hashtable]$parameters = @{}) {
    $parameters['Action'] = $action
    $parameters['OutputDirectory'] = $OutputDirectory
    if ($action -eq 'Capture') { $parameters['VerifyTexture'] = $true }
    if ($action -eq 'Launch') {
        $parameters['Executable'] = $Executable
        $parameters['FixtureDirectory'] = $fixtures
    } else { $parameters['ProcessId'] = $script:testProcessId }
    $result = & $tool @parameters | ConvertFrom-Json
    $results.Add($result)
    return $result
}

function Capture([string]$name) { return Invoke-Ui 'Capture' @{ Name = $name; WaitMilliseconds = 250 } }

function Assert-Pixel([string]$path, [int]$x, [int]$y, [int[]]$expected) {
    $bitmap = [Drawing.Bitmap]::FromFile($path)
    try { $actual = $bitmap.GetPixel($x, $y) } finally { $bitmap.Dispose() }
    $channels = @($actual.R, $actual.G, $actual.B)
    foreach ($index in 0..2) {
        if ([Math]::Abs([int]$channels[$index] - $expected[$index]) -gt 7) {
            throw "Unexpected pixel at ($x,$y): $actual in $path"
        }
    }
}

function Interface-Top {
    Invoke-Ui 'Click' @{ X = 250; Y = 118; WaitMilliseconds = 300 } | Out-Null
}

function Cycle-Theme {
    Invoke-Ui 'Scroll' @{ X = 550; Y = 400; Delta = -20; WaitMilliseconds = 250 } | Out-Null
    Invoke-Ui 'Click' @{ X = 155; Y = 421; WaitMilliseconds = 500 } | Out-Null
}

try {
    if ($SoftwareRendering) { $env:WILIWILI_UI_AUDIT_SOFTWARE = '1' }
    & (Join-Path $PSScriptRoot 'ui_regression.ps1') -Executable $Executable -OutputDirectory $baseDirectory -Iterations $Iterations -CoverFormat Png
    $launch = Invoke-Ui 'Launch' @{ Arguments = '--board-player'; WaitMilliseconds = 2000 }
    $testProcessId = $launch.ProcessId
    $before = Capture 'paused_player_before'
    $after = Invoke-Ui 'Status' @{ WaitMilliseconds = 3000 }
    if ($before.DisplayPresentCount -le 0 -or $after.DisplayPresentCount - $before.DisplayPresentCount -gt 2) {
        throw 'Paused player continues generating unnecessary display presents'
    }
    $paused = Capture 'paused_player_after'
    if ($paused.Sha256 -ne $before.Sha256) { throw 'Paused player changed without input' }
    Write-Output 'PASS paused-player-idle: no repeated text redraws'
    Invoke-Ui 'Close' | Out-Null
    $testProcessId = 0

    $launch = Invoke-Ui 'Launch' @{ Arguments = '--board-settings'; WaitMilliseconds = 800 }
    $testProcessId = $launch.ProcessId
    Interface-Top
    $baseline = Capture 'theme_auto_initial'
    foreach ($round in 1..$Iterations) {
        Cycle-Theme
        $light = Capture "theme_light_$round"
        Assert-Pixel $light.Screenshot 600 38 @(248, 249, 251)
        Assert-Pixel $light.Screenshot 600 175 @(255, 255, 255)
        Assert-Pixel $light.Screenshot 728 183 @(220, 227, 235)
        Invoke-Ui 'Click' @{ X = 32; Y = 114; WaitMilliseconds = 1500 } | Out-Null
        $homeCapture = Capture "light_home_$round"
        Assert-Pixel $homeCapture.Screenshot 452 150 @(64, 138, 202)
        Invoke-Ui 'Click' @{ X = 32; Y = 310; WaitMilliseconds = 400 } | Out-Null
        Interface-Top
        Cycle-Theme
        $dark = Capture "theme_dark_$round"
        Assert-Pixel $dark.Screenshot 600 38 @(17, 24, 39)
        Cycle-Theme
        $restored = Capture "theme_auto_restored_$round"
        Assert-Pixel $restored.Screenshot 600 38 @(17, 24, 39)
        if ($restored.LvglHeapUsedBytes -gt $baseline.LvglHeapUsedBytes + 4096) {
            throw 'Theme recreation accumulates LVGL objects'
        }
    }
    Write-Output 'PASS native-theme: light/dark/auto, unchanged covers, navigation and bounded LVGL heap'
} finally {
    if ($testProcessId -and (Get-Process -Id $testProcessId -ErrorAction SilentlyContinue)) {
        Invoke-Ui 'Close' | Out-Null
    }
    $env:WILIWILI_UI_AUDIT_SOFTWARE = $previousSoftware
    New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
    $results | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'results.json') -Encoding UTF8
}
