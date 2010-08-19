@echo off

setlocal

set p=%~dp0
set path=%p%bin;%p%node\bin;%path%

"%p%bin\bash.exe" --login -i

pause