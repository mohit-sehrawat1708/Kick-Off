<#
Screenshot capture for the art-direction critic. Plays a live AI-vs-AI match in Pitch Kings and photographs it from the
camera positions in a shot list (see example_shots.txt for the format), or captures the front-end menus.

  powershell -File Tools\critic\capture.ps1 -Shots my_shots.txt -Out C:\some\folder
  powershell -File Tools\critic\capture.ps1 -Menus -Out C:\some\folder

Shots are 1920x1080 PNGs named after each line. The script prints the files it produced.
#>
param(
    [string]$Shots = "",
    [switch]$Menus,
    [string]$Out = "",
    [int]$Width = 1920,
    [int]$Height = 1080,
    [string]$Extra = ""
)
$ErrorActionPreference = "Stop"
$Project = Join-Path $PSScriptRoot "..\..\PitchKings.uproject" | Resolve-Path
$Saved = Join-Path (Split-Path $Project) "Saved\Screenshots"
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
Get-Process UnrealEditor -ErrorAction SilentlyContinue | Stop-Process -Force

if ($Menus) {
    Get-ChildItem $Saved -Recurse -Filter "Menu_*.png" -ErrorAction SilentlyContinue | Remove-Item
    $args = "`"$Project`" -game -PKMenuShots -windowed -ResX=$Width -ResY=$Height -nosound `"-ExecCmds=DisableAllScreenMessages`" $Extra"
    $pattern = "Menu_*.png"
    $timeout = 240
} else {
    if (-not (Test-Path $Shots)) { throw "Shot list not found: $Shots" }
    $ShotsFull = (Resolve-Path $Shots).Path
    $critic = Join-Path $Saved "Critic"
    if (Test-Path $critic) { Remove-Item "$critic\*.png" -ErrorAction SilentlyContinue }
    $last = 0
    foreach ($line in Get-Content $ShotsFull) {
        if ($line -match '^\s*#' -or $line.Trim() -eq "") { continue }
        $f = $line.Split('|')
        if ($f.Count -ge 2) { $t = [double]$f[1].Trim(); if ($t -gt $last) { $last = $t } }
    }
    $args = "`"$Project`" -game -PKSpectate -PKNoMenu `"-PKCritic=$ShotsFull`" -windowed -ResX=$Width -ResY=$Height -nosound `"-ExecCmds=DisableAllScreenMessages`" $Extra"
    $pattern = "*.png"
    $timeout = [int]($last + 420)
}

$p = Start-Process -FilePath $Editor -ArgumentList $args -PassThru
if (-not $p.WaitForExit($timeout * 1000)) { Stop-Process -Id $p.Id -Force; Write-Output "capture timed out after $timeout s" }

$dir = if ($Menus) { $Saved } else { Join-Path $Saved "Critic" }
$files = Get-ChildItem $dir -Recurse -Filter $pattern -ErrorAction SilentlyContinue | Where-Object { $Menus -or $_.DirectoryName -like "*Critic*" }
if ($Out -ne "") {
    New-Item -ItemType Directory -Force -Path $Out | Out-Null
    foreach ($f in $files) { Copy-Item $f.FullName (Join-Path $Out $f.Name) -Force }
    $files = Get-ChildItem $Out -Filter $pattern
}
foreach ($f in $files) { Write-Output $f.FullName }
Write-Output ("{0} screenshot(s)" -f @($files).Count)
