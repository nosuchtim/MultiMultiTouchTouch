@echo off
set a=%1
if "%a%" == "" goto err:
oscutil listen %a%@127.0.0.1
goto end:
:err
echo You must specify a port
:end
