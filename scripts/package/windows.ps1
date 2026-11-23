param(
    [Parameter(Mandatory=$true)][string]$QtRoot,
    [Parameter(Mandatory=$true)][string]$CompilerRoot,
    [string]$BuildDirectory = 'build/gui-qt',
    [string]$StageDirectory = 'build/package-windows',
    [string]$OutputDirectory = 'output/releases',
    [string]$Iscc = 'iscc.exe'
)
$ErrorActionPreference = 'Stop'
if (Test-Path -LiteralPath $StageDirectory) { throw 'Choose a new staging directory' }
$qtPath = (Resolve-Path -LiteralPath $QtRoot).Path
$compilerPath = (Resolve-Path -LiteralPath $CompilerRoot).Path
$env:PATH = "$compilerPath/bin;$qtPath/bin;" + $env:PATH
$cache = Get-Content -LiteralPath (Join-Path $BuildDirectory 'CMakeCache.txt')
if ($cache -notcontains 'CMAKE_BUILD_TYPE:STRING=Release') { throw 'Distribution requires a Release build' }
if ($cache -notcontains 'MININIDS_DEPLOY_RUNTIME:BOOL=ON') { throw 'Reconfigure with MININIDS_DEPLOY_RUNTIME=ON' }
$version = ($cache | Select-String '^MININIDS_PACKAGE_LABEL:STRING=(.+)$').Matches.Groups[1].Value
if (!$version) { throw 'Missing package version in CMakeCache.txt' }
$output = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
$stage = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($StageDirectory)
New-Item -ItemType Directory -Path $output -Force | Out-Null
$installerCompiler = (Get-Command $Iscc -ErrorAction Stop).Source
$ErrorActionPreference = 'Continue' # Check native commands by their exit codes.
cmake --install $BuildDirectory --prefix $stage
if ($LASTEXITCODE -ne 0) { throw 'Runtime deployment failed' }
$ErrorActionPreference = 'Stop'
$stage = (Resolve-Path -LiteralPath $StageDirectory).Path
foreach ($file in @('Mini-NIDS.exe','mininids.exe','Qt6Core.dll','Qt6Qml.dll','Qt6Quick.dll','plugins/platforms/qwindows.dll')) {
    if (!(Test-Path -LiteralPath (Join-Path $stage $file))) { throw "Incomplete deployment: $file" }
}
Copy-Item -LiteralPath "$qtPath/plugins/platforms/qoffscreen.dll" -Destination "$stage/plugins/platforms/qoffscreen.dll"
$licenseDirectory = Join-Path $stage 'share/mininids/licenses'
foreach ($component in @('gcc','mingw-w64','winpthreads')) {
    Copy-Item -LiteralPath (Join-Path $compilerPath "licenses/$component") -Destination $licenseDirectory -Recurse
}
if (Test-Path -LiteralPath "$qtPath/sbom") { Copy-Item -LiteralPath "$qtPath/sbom" -Destination "$licenseDirectory/qt-sbom" -Recurse }
& "$PSScriptRoot/sign-windows.ps1" -Files @("$stage/Mini-NIDS.exe", "$stage/mininids.exe")
& "$PSScriptRoot/verify-windows.ps1" -Directory $stage -CompilerRoot $compilerPath
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = Join-Path $output "Mini-NIDS-$version-windows-x64-portable.zip"
if (Test-Path -LiteralPath $archive) { throw "Refusing to overwrite $archive" }
[System.IO.Compression.ZipFile]::CreateFromDirectory($stage, $archive)
$ErrorActionPreference = 'Continue'
$numericVersion = ($version -split '[-+]')[0]
& $installerCompiler "/DStageDirectory=$stage" "/DOutputDirectory=$output" "/DPackageVersion=$version" "/DNumericVersion=$numericVersion" "$PSScriptRoot/../../packaging/windows/installer.iss"
if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed' }
$ErrorActionPreference = 'Stop'
$installer = Join-Path $output "Mini-NIDS-$version-windows-x64-Setup.exe"
& "$PSScriptRoot/sign-windows.ps1" -Files @($installer)
Write-Output "Verified portable ZIP and offline installer: $output"
