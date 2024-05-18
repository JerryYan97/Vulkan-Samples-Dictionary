@echo off
setlocal

set searchString=example

set found=false

for /f "delims=" %%i in ('your_command_here') do (
    if "%%i"=="%searchString%" (
        set found=true
        goto :found
    )
)

:found
if %found%==true (
    echo String found!
) else (
    echo String not found!
)

endlocal