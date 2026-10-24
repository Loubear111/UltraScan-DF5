# Setup script for Windows users to install GoogleTest dependencies.
# Run this script from the project root.

$ErrorActionPreference = "Stop"

Write-Host "--- Starting Windows Setup for GoogleTest ---" -ForegroundColor Cyan

# 1. Check if we are actually on Windows
if ($([System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform("windows")) -eq $false) {
    Write-Error "This script is intended for Windows. For Linux systems, simply run 'make -f build.mk test' as usual."
}

# 2. Check for required tools
$tools = @("cmake", "g++")
foreach ($tool in $tools) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        Write-Error "Required tool '$tool' was not found. Please ensure it is installed and in your PATH."
    }
}

# 3. Define paths
$workDir = Get-Location
$gtestPath = Join-Path $workDir "third_party\googletest"
$zipFile = Join-Path $workDir "gtest_source.zip"

# 4. Check if GoogleTest is already built
if (Test-Path (Join-Path $gtestPath "build\lib\libgtest.a")) {
    Write-Host "GoogleTest appears to be already built in third_party/googletest." -ForegroundColor Green
    Write-Host "You can proceed to run 'make -f build.mk test' in the src directory." -ForegroundColor Green
    exit
}

# 5. Download and Extract
if (-not (Test-Path $gtestPath)) {
    Write-Host "Downloading GoogleTest source..." -ForegroundColor Yellow
    Invoke-WebRequest -Uri "https://github.com/google/googletest/archive/refs/tags/v1.18.0.zip" -OutFile $zipFile
    
    Write-Host "Extracting source..." -ForegroundColor Yellow
    if (Test-Path $zipFile) {
        Expand-Archive -Path $zipFile -DestinationPath $gtestPath -Force
        Remove-Item $zipFile
    }
}

# 6. Build GoogleTest
$buildDir = Join-Path $gtestPath "build"
if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir -Force }

Write-Host "Configuring GoogleTest..." -ForegroundColor Yellow
cmake -S $gtestPath -B $buildDir -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles"

Write-Host "Building GoogleTest..." -ForegroundColor Yellow
cmake --build $buildDir --config Release

if ($? -eq $true) {
    Write-Host "--- Setup Complete! ---" -ForegroundColor Green
    Write-Host "You can now run 'make -f build.mk test' in the src directory." -ForegroundColor Green
} else {
    Write-Error "Build failed. Please check the output above for errors."
}
