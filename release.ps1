# release.ps1 - Fast Release-only build script for HexForge

$ErrorActionPreference = "Stop"

# 1. Setup MSVC Environment
Write-Host "Setting up MSVC environment..." -ForegroundColor Cyan
if (-not (Get-Command "cl.exe" -ErrorAction SilentlyContinue)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsInstallPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsInstallPath) {
            $vcvars = "$vsInstallPath\VC\Auxiliary\Build\vcvarsall.bat"
            if (Test-Path $vcvars) {
                $envVars = cmd /c "`"$vcvars`" x64 && set"
                foreach ($line in $envVars) {
                    if ($line -match "^(.*?)=(.*)$") {
                        $name = $matches[1]
                        $value = $matches[2]
                        if ($name -notmatch "^(ALLUSERSPROFILE|APPDATA|COMPUTERNAME|ComSpec|CommonProgramFiles|CommonProgramFiles\(x86\)|CommonProgramW64|ConfigSetRoot|DriverData|HOMEDRIVE|HOMEPATH|LOCALAPPDATA|LOGONSERVER|NUMBER_OF_PROCESSORS|OS|PATHEXT|PROCESSOR_ARCHITECTURE|PROCESSOR_IDENTIFIER|PROCESSOR_LEVEL|PROCESSOR_REVISION|PSModulePath|PUBLIC|ProgramData|ProgramFiles|ProgramFiles\(x86\)|ProgramW64|SystemDrive|SystemRoot|TEMP|TMP|USERDOMAIN|USERDOMAIN_ROAMINGPROFILE|USERNAME|USERPROFILE|windir)$") {
                            Set-Item -Path "Env:\$name" -Value $value
                        }
                    }
                }
            }
        }
    }
}

if (-not (Get-Command "cl.exe" -ErrorAction SilentlyContinue)) {
    Write-Error "Could not find MSVC compiler. Please run from Developer PowerShell or ensure VS 2022 is installed."
}

# 2. Configure and Build Release
$buildDir = "out/build_release"
if (-not (Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir }

Write-Host "Configuring Release build..." -ForegroundColor Cyan
cmake -S . -B $buildDir -G "Ninja" -DCMAKE_BUILD_TYPE=Release -Wno-dev

Write-Host "Building Release..." -ForegroundColor Cyan
cmake --build $buildDir --config Release --parallel

# 3. Deploy
$binDir = "out/bin/Release"
if (-not (Test-Path $binDir)) { New-Item -ItemType Directory -Path $binDir }

Write-Host "Deploying binaries to $binDir..." -ForegroundColor Cyan
Get-ChildItem -Path "$buildDir" -Filter "*.exe" -Recurse | Copy-Item -Destination $binDir -Force
Get-ChildItem -Path "$buildDir" -Filter "*.dll" -Recurse | Copy-Item -Destination $binDir -Force
if (Test-Path "examples/sandbox/resources") { Copy-Item -Path "examples/sandbox/resources" -Destination $binDir -Recurse -Force }

Write-Host "`nRelease build complete! Binaries are in $binDir" -ForegroundColor Green
