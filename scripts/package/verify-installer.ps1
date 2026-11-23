param(
    [Parameter(Mandatory=$true)][string]$Installer,
    [Parameter(Mandatory=$true)][string]$CompilerRoot,
    [string]$WorkDirectory = 'build/installer-verification'
)
$ErrorActionPreference = 'Stop'
$registryKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{B8374B6D-457E-4A32-B2BC-B0E10597887F}_is1'
foreach ($hive in @('HKCU:','HKLM:')) {
    if (Test-Path -LiteralPath "$hive\$registryKey") { throw 'An installed Mini-NIDS already exists; use a clean test user for installer verification' }
}
if (Test-Path -LiteralPath $WorkDirectory) { throw 'Choose a new verification directory' }
New-Item -ItemType Directory -Path $WorkDirectory | Out-Null
$work = (Resolve-Path -LiteralPath $WorkDirectory).Path
$application = Join-Path $work 'Installed application'
$userData = Join-Path $work 'User data'
New-Item -ItemType Directory -Path $userData | Out-Null
$installerPath = (Resolve-Path -LiteralPath $Installer).Path
function Invoke-InstallerProcess([string]$Executable, [string[]]$Arguments) {
    $process = Start-Process -FilePath $Executable -ArgumentList $Arguments -PassThru -WindowStyle Hidden
    $null = $process.Handle
    if (!$process.WaitForExit(60000)) { $process.Kill(); throw 'Installer/uninstaller timed out' }
    if ($process.ExitCode -ne 0) { throw "Installer/uninstaller failed: $($process.ExitCode)" }
}
try {
    Invoke-InstallerProcess $installerPath @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-','/NOICONS',("/DIR=`"$application`""),("/LOG=`"$work/install.log`""))
    & "$PSScriptRoot/verify-windows.ps1" -Directory $application -CompilerRoot $CompilerRoot
    $header = [byte[]](0xd4,0xc3,0xb2,0xa1,2,0,4,0,0,0,0,0,0,0,0,0,0,0,4,0,1,0,0,0)
    [System.IO.File]::WriteAllBytes("$userData/input.pcap", $header)
    & "$application/mininids.exe" analyze --input "$userData/input.pcap" --output "$userData/reports" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Installed CLI could not create user reports' }
} finally {
    $uninstaller = Join-Path $application 'unins000.exe'
    if (Test-Path -LiteralPath $uninstaller) {
        Invoke-InstallerProcess $uninstaller @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',("/LOG=`"$work/uninstall.log`""))
    }
}
if (Test-Path -LiteralPath "$application/Mini-NIDS.exe") { throw 'Uninstall left the application executable behind' }
if (!(Test-Path -LiteralPath "$userData/reports/capture_summary.json")) { throw 'Uninstall removed user data' }
if (Test-Path -LiteralPath "HKCU:\$registryKey") { throw 'Uninstall registration was not removed' }
Write-Output 'Offline install, installed runtime, report storage and uninstall/data preservation passed.'
