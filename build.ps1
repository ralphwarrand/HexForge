$ErrorActionPreference = "Stop"

# 1. Try to find vcvars64.bat to set up MSVC environment (required for Ninja + CUDA)
if (-not $env:VCINSTALLDIR) {
    Write-Host "Setting up MSVC environment..."
    $vcvars = Get-ChildItem -Path "C:\Program Files\Microsoft Visual Studio" -Filter "vcvars64.bat" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
    if ($vcvars) {
        $env:temp_file = New-TemporaryFile
        cmd /c "call `"$vcvars`" && set > `"$env:temp_file`""
        Get-Content $env:temp_file | Foreach-Object {
            if ($_ -match "^(.*?)=(.*)$") {
                Set-Item -Path "Env:\$($Matches[1])" -Value $Matches[2]
            }
        }
        Remove-Item $env:temp_file
        Write-Host "MSVC environment loaded."
    } else {
        Write-Host "Warning: vcvars64.bat not found. Build may fail if not in a Developer Command Prompt."
    }
}

# 2. Try to find Ninja in Visual Studio installation
$ninjaPath = Get-ChildItem -Path "C:\Program Files\Microsoft Visual Studio" -Filter "ninja.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
if ($ninjaPath) {
    $ninjaDir = [System.IO.Path]::GetDirectoryName($ninjaPath)
    if ($env:PATH -notlike "*$ninjaDir*") {
        $env:PATH += ";$ninjaDir"
    }
}

# 3. Clean up if generator mismatch (switching from VS to Ninja)
if (Test-Path "out/build/CMakeCache.txt") {
    $cache = Get-Content "out/build/CMakeCache.txt"
    if ($cache -match "CMAKE_GENERATOR:INTERNAL=Visual Studio") {
        Write-Host "Detected generator mismatch. Cleaning build directory..."
        Remove-Item -Recurse -Force out/build
    }
}

# 4. Build
$cmakeArgs = "-G", "Ninja", "-B", "out/build", "-S", ".", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"

Write-Host "Building Debug..."
cmake @cmakeArgs "-DCMAKE_BUILD_TYPE=Debug"
cmake --build out/build --config Debug

Write-Host "Building Release..."
cmake @cmakeArgs "-DCMAKE_BUILD_TYPE=Release"
cmake --build out/build --config Release

# 5. Fix IntelliSense (Copy compile_commands.json to root for clangd)
if (Test-Path "out/build/compile_commands.json") {
    Copy-Item -Path "out/build/compile_commands.json" -Destination "." -Force
    Write-Host "compile_commands.json copied to project root for clangd IntelliSense!"
}

Write-Host "Builds completed. Binaries are in out/bin/"
