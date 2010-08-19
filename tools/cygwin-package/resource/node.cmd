@echo off

rem Keep changes to the environment within this script
setlocal

rem Path to this script
set cmd_path=%~dp0

rem Set path to cygwin binutils
set path=%cmd_path%bin\;%cmd_path%node\bin;%path%

rem Start building the node command
set run="%cmd_path%node\bin\node.exe"

rem For each command line argument, try to convert to a cygwin path
rem If cygpath can't convert it we assume that it is not meant to be a file/folder name and pass it as-is
:loop

rem Test if there are more arguments to convert
if %1.==. goto :done

rem Fetch the next argument
set next_arg=%1

rem Let cygpath try to convert this argument, if it fails the do clause won't be executed;
rem if cygpath did not change the argument at all but only stripped quotes, we use the original argument.
for /f "delims=:" %%i in ('%cmd_path%bin\cygpath -- %next_arg%') do if not "%~1"=="%%i" set next_arg="%%~i"

rem Push the converted arg to the arg list
set run=%run% %next_arg%

rem Process next arg
shift
goto :loop

rem End of argument list building loop
:done

rem Run node
echo invoking %run%
echo.
%run%

echo.
echo node has exited.
pause

rem Restore original environment
endlocal