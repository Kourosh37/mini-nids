param(
    [Parameter(Mandatory=$true)][string]$QtRoot,
    [Parameter(Mandatory=$true)][string]$CompilerRoot,
    [string]$BuildDirectory = 'build/gui-qt',
    [string]$StageDirectory = 'build/package-windows'
)
$ErrorActionPreference = 'Stop'
if (Test-Path -LiteralPath $StageDirectory) { throw 'Choose a new staging directory' }
$qtPath = (Resolve-Path -LiteralPath $QtRoot).Path
$compilerPath = (Resolve-Path -LiteralPath $CompilerRoot).Path
$env:PATH = "$compilerPath/bin;$qtPath/bin;" + $env:PATH
cmake --install $BuildDirectory --prefix $StageDirectory
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$nativeErrorPreference = $ErrorActionPreference
try {
    # Windows PowerShell can turn successful native-tool stderr warnings into exceptions.
    $ErrorActionPreference = 'Continue'
    & "$qtPath/bin/windeployqt.exe" --release --verbose 0 --qmldir apps/gui/qml --compiler-runtime --no-translations --no-opengl-sw "$StageDirectory/bin/mininids_gui.exe" 2>&1 | ForEach-Object { $_.ToString() }
    $deploymentExitCode = $LASTEXITCODE
} finally {
    $ErrorActionPreference = $nativeErrorPreference
}
if ($deploymentExitCode -ne 0) { exit $deploymentExitCode }
$licenseDirectory = Join-Path $StageDirectory 'licenses'
New-Item -ItemType Directory -Path $licenseDirectory -Force | Out-Null
Copy-Item -LiteralPath packaging/licenses -Destination (Join-Path $licenseDirectory 'qt') -Recurse
foreach ($component in @('gcc','mingw-w64','winpthreads')) {
    Copy-Item -LiteralPath (Join-Path $compilerPath "licenses/$component") -Destination $licenseDirectory -Recurse
}
if (Test-Path -LiteralPath "$qtPath/sbom") { Copy-Item -LiteralPath "$qtPath/sbom" -Destination (Join-Path $licenseDirectory 'qt-sbom') -Recurse }
Copy-Item -LiteralPath packaging/THIRD_PARTY_NOTICES.md -Destination $StageDirectory
Write-Output "Staged development bundle: $StageDirectory"
