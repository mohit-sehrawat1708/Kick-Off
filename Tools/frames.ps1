param([string]$Frames, [string]$Tag)
$shots = "C:\Users\ROHIT KUSHWAHA\Downloads\PitchKings\Saved\Screenshots"
Remove-Item "$shots\PKSheet_*" -ErrorAction SilentlyContinue
$p = Start-Process -FilePath "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList '"C:\Users\ROHIT KUSHWAHA\Downloads\PitchKings\PitchKings.uproject"','-game','-PKClipSheet',"-PKClipFrames=$Frames",'-windowed','-ResX=1600','-ResY=900','-log=PKFrames.log','-unattended','-nosplash' -PassThru
$p.WaitForExit(300000) | Out-Null
$i = 0
Get-ChildItem "$shots\PKSheet_*" | Sort-Object Name | ForEach-Object { $dst = "$shots\Frames_${Tag}_$i.png"; Move-Item $_.FullName $dst -Force; $dst; $i++ }
