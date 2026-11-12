@echo off
rem Package a Windows Development build (as the jam asks) into Build\Windows. Close this project's editor first: the cook runs its own editor process.
rem -prereqs adds the Unreal prerequisites installer, so players need no extra software.
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="D:\UnrealProjects\AntTreeGame\AntTreeGame.uproject" -platform=Win64 -clientconfig=Development -build -cook -stage -pak -prereqs -archive -archivedirectory="D:\UnrealProjects\AntTreeGame\Build" -noP4 -unattended -utf8output -WaitForUATMutex
