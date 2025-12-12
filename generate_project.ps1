# STM32CubeMX ProjectCode Generation Script
# Generates code and project files from DCC_tester.ioc file

$CubeMXPath = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeMX"

if (-not (Test-Path $CubeMXPath)) {
    Write-Error "STM32CubeMX not found at $CubeMXPath"
    Write-Error "Please install STM32CubeMX or update the path in this script"
    exit 1
}

Write-Host "Running STM32CubeMX code generation..." -ForegroundColor Cyan

& $CubeMXPath\jre\bin\java -jar $CubeMXPath\STM32CubeMX.exe -q .\generate_project.txt

# Check if exit code is null or non-zero
if ($null -eq $LASTEXITCODE) {
    Write-Host "Code generation completed (no error code returned)" -ForegroundColor Yellow
    exit 0
} elseif ($LASTEXITCODE -ne 0) {
    Write-Error "Code generation failed with error code $LASTEXITCODE"
    exit $LASTEXITCODE
}

Write-Host "Code generation completed successfully" -ForegroundColor Green
exit 0
