param(
    [string]$CMake = 'C:/Qt/Tools/CMake_64/bin/cmake.exe',
    [string]$Ninja = 'C:/Qt/Tools/Ninja/ninja.exe'
)

$ErrorActionPreference = 'Stop'
$build = Join-Path $PSScriptRoot 'build-firmware'
$toolchain = Join-Path $PSScriptRoot 'cmake/arm-none-eabi.cmake'

$configureArgs = @('-S', $PSScriptRoot, '-B', $build, '-G', 'Ninja',
                   "-DCMAKE_MAKE_PROGRAM=$Ninja")
if (!(Test-Path -LiteralPath (Join-Path $build 'CMakeCache.txt'))) {
    $configureArgs += "-DCMAKE_TOOLCHAIN_FILE=$toolchain"
}

& $CMake @configureArgs
if ($LASTEXITCODE -ne 0) {
    throw "Firmware configuration failed with exit code $LASTEXITCODE"
}

& $CMake --build $build
if ($LASTEXITCODE -ne 0) {
    throw "Firmware build failed with exit code $LASTEXITCODE"
}
