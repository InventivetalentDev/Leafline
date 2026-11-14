@echo off
setlocal
rem Standalone game from the editor binaries (no packaging): RunGame.bat <logname> "<execcmds>" ["<extra args>"]. The log goes to Saved\Logs.
rem Example (from WSL, in /mnt/c): cmd.exe /c "D:\UnrealProjects\AntTreeGame\Scripts\RunGame.bat" Standalone.log "Leaf.AutoMove 1"
rem An explicit -ResX in the extra arguments owns the resolution (UE uses the first occurrence).
set "leafline_extra_args=%~3"
set "leafline_resolution=-ResX=1280 -ResY=720"
if /I not "%leafline_extra_args:-ResX=%"=="%leafline_extra_args%" set "leafline_resolution="
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0..\AntTreeGame.uproject" -game -windowed %leafline_resolution% -log=%1 -ExecCmds="%~2" %~3
