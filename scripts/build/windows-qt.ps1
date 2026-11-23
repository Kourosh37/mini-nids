param(
    [Parameter(Mandatory=$true)][string]$QtRoot,
    [Parameter(Mandatory=$true)][string]$CompilerRoot,
    [string]$BuildDirectory = 'build/gui-qt',
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release'
)
$ErrorActionPreference = 'Stop'
$qtBin = Join-Path (Resolve-Path -LiteralPath $QtRoot).Path 'bin'
$compilerBin = Join-Path (Resolve-Path -LiteralPath $CompilerRoot).Path 'bin'
$env:PATH = "$compilerBin;$qtBin;" + $env:PATH
# Windows PowerShell otherwise treats native stderr warnings as terminating errors.
# Each native command below is checked using its actual exit code.
$ErrorActionPreference = 'Continue'
cmake --preset gui -B $BuildDirectory "-DCMAKE_PREFIX_PATH=$QtRoot" "-DCMAKE_CXX_COMPILER=$compilerBin/g++.exe" "-DCMAKE_C_COMPILER=$compilerBin/gcc.exe" "-DCMAKE_BUILD_TYPE=$Configuration"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build $BuildDirectory --parallel 4
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
ctest --test-dir $BuildDirectory --output-on-failure
exit $LASTEXITCODE
