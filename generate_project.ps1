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
} elseif ($LASTEXITCODE -ne 0) {
    Write-Error "Code generation failed with error code $LASTEXITCODE"
    exit $LASTEXITCODE
} else {
    Write-Host "Code generation completed successfully" -ForegroundColor Green
}

# Clean up any existing build artifacts
if (Test-Path "build") {
    Write-Host "`nRemoving existing build directory..." -ForegroundColor Cyan
    Remove-Item -Path "build" -Recurse -Force
    Write-Host "Build directory cleaned" -ForegroundColor Green
}

# Select CMake command (prefer cube-cmake if available)
$CMakeCommand = Get-Command cube-cmake -ErrorAction SilentlyContinue
if (-not $CMakeCommand) {
    $CMakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
}
if (-not $CMakeCommand) {
    $cubeCmakeCandidates = @(
        Join-Path $env:USERPROFILE ".vscode/extensions/stmicroelectronics.stm32cube-ide-build-cmake-*/resources/cube-cmake/win32/cube-cmake.exe"
    )
    $cubeCmakePath = Get-ChildItem -Path $cubeCmakeCandidates -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
    if ($cubeCmakePath) {
        $CMakeCommand = [PSCustomObject]@{
            Name = "cube-cmake"
            Source = $cubeCmakePath
        }
    }
}
if (-not $CMakeCommand) {
    Write-Error "Neither cube-cmake nor cmake is available in PATH"
    exit 1
}

# Ensure cube CLI is on PATH when using cube-cmake
if ($CMakeCommand.Name -eq "cube-cmake") {
    $cubePath = $null
    $cubeCommand = Get-Command cube -ErrorAction SilentlyContinue
    if ($cubeCommand) {
        $cubePath = $cubeCommand.Source
    }
    if (-not $cubePath) {
        $cubeCandidates = @(
            "$env:USERPROFILE\.vscode\extensions\stmicroelectronics.stm32cube-ide-core-*\resources\binaries\win32\x86_64\cube.exe",
            "$env:USERPROFILE\.vscode\extensions\stmicroelectronics.stm32cube-ide-build-cmake-*\resources\cube\win32\cube.exe"
        )
        $cubePath = Get-ChildItem -Path $cubeCandidates -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
    }
    if ($cubePath) {
        $cubeDir = Split-Path -Parent $cubePath
        $env:PATH = "$cubeDir;$env:PATH"
    }
}

$BuildDir = Join-Path (Get-Location) "build"

# Run CMake configuration
Write-Host "`nConfiguring CMake project with $($CMakeCommand.Name)..." -ForegroundColor Cyan
& $CMakeCommand.Source -S . -B $BuildDir -G Ninja
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed with error code $LASTEXITCODE"
    exit $LASTEXITCODE
}
Write-Host "CMake configuration completed successfully" -ForegroundColor Green

# Build all targets with Ninja
Write-Host "`nBuilding all targets with Ninja using $($CMakeCommand.Name)..." -ForegroundColor Cyan
& $CMakeCommand.Source --build $BuildDir -j
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed with error code $LASTEXITCODE"
    exit $LASTEXITCODE
}

Write-Host "`nFull project generation and build completed successfully!" -ForegroundColor Green
exit 0
