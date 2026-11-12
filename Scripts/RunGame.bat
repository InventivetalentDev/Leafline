@echo off
rem Standalone game from the editor binaries (no packaging): RunGame.bat <logname> "<execcmds>". The log goes to Saved\Logs.
rem Example (from WSL, in /mnt/c): cmd.exe /c "D:\UnrealProjects\AntTreeGame\Scripts\RunGame.bat" Standalone.log "Leaf.AutoMove 1"
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0..\AntTreeGame.uproject" -game -windowed -ResX=1280 -ResY=720 -log=%1 -ExecCmds="%~2"
