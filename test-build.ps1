# Test script to verify Visual Studio Build Tools installation
Write-Host "Testing Visual Studio Build Tools installation..."

# Test 1: Check if vswhere exists
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWhere) {
    Write-Host "✅ Visual Studio Installer found"
    
    # Get VS installation info
    $vsInfo = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | ConvertFrom-Json
    if ($vsInfo) {
        Write-Host "✅ Visual Studio Build Tools found:"
        Write-Host "   Version: $($vsInfo.catalog.productDisplayVersion)"
        Write-Host "   Path: $($vsInfo.installationPath)"
    } else {
        Write-Host "❌ Visual Studio Build Tools with C++ not found"
    }
} else {
    Write-Host "❌ Visual Studio Installer not found"
}

# Test 2: Try to find cl.exe
$vcvarsPath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if (Test-Path $vcvarsPath) {
    Write-Host "✅ vcvars64.bat found - C++ compiler should be available"
} else {
    Write-Host "❌ vcvars64.bat not found"
}

Write-Host ""
Write-Host "After installation completes, run:"
Write-Host "cd 'bindings/node'"
Write-Host "npm run build"