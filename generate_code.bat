@echo off
REM STM32CubeMX Code Generation Script
REM Generates code from DCC_tester.ioc project file

set CUBEMX_PATH="C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeMX\STM32CubeMX.exe"

if not exist %CUBEMX_PATH% (
    echo Error: STM32CubeMX not found at %CUBEMX_PATH%
    echo Please install STM32CubeMX or update the path in this script
    exit /b 1
)

echo Running STM32CubeMX code generation...
%CUBEMX_PATH% -q generatecode.txt

if %ERRORLEVEL% NEQ 0 (
    echo Error: Code generation failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo Code generation completed successfully
exit /b 0
