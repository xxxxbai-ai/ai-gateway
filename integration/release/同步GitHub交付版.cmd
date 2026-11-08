@echo off
chcp 65001 >nul
set "GATEWAY_SYNC_PYTHON=%USERPROFILE%\anaconda3\python.exe"
if not exist "%GATEWAY_SYNC_PYTHON%" (
    echo 找不到本机 Anaconda Python，尚未上传任何文件。
    pause
    exit /b 1
)
"%GATEWAY_SYNC_PYTHON%" "%~dp0sync_existing_release.py" --artifacts "%~dp0..\..\..\交付包\GitHub-V1.0-rc.1"
pause
