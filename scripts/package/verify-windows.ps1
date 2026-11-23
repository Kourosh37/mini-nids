param(
    [Parameter(Mandatory=$true)][string]$Directory,
    [Parameter(Mandatory=$true)][string]$CompilerRoot
)
$ErrorActionPreference = 'Stop'
$directoryPath = (Resolve-Path -LiteralPath $Directory).Path
$objdump = Join-Path (Resolve-Path -LiteralPath $CompilerRoot).Path 'bin/objdump.exe'
$binaries = @(Get-ChildItem -LiteralPath $directoryPath -Recurse -File | Where-Object { $_.Extension -in @('.exe','.dll') })
$names = @{}
foreach ($binary in $binaries) { $names[$binary.Name] = $true }
foreach ($binary in $binaries) {
    $imports = & $objdump -p $binary.FullName | Select-String 'DLL Name:\s*(\S+)'
    if ($LASTEXITCODE -ne 0) { throw "Cannot inspect $($binary.Name)" }
    foreach ($import in $imports) {
        $name = $import.Matches.Groups[1].Value
        if ($name -match '^(api-ms-|ext-ms-)') { continue }
        if (!$names.ContainsKey($name) -and !(Test-Path -LiteralPath "$env:SystemRoot/System32/$name")) {
            throw "Missing runtime dependency: $($binary.Name) -> $name"
        }
    }
}
$savedPath = $env:PATH
$qtEnvironment = @{}
$logRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('mininids-smoke-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $logRoot | Out-Null
try {
    foreach ($name in @('QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','QML_IMPORT_PATH','QML2_IMPORT_PATH','QTDIR','QT_QPA_PLATFORM','QT_QUICK_BACKEND')) {
        $qtEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
        [Environment]::SetEnvironmentVariable($name, $null, 'Process')
    }
    $env:PATH = "$env:SystemRoot/System32;$env:SystemRoot;$env:SystemRoot/System32/Wbem"
    $env:QT_QPA_PLATFORM = 'offscreen'
    $env:QT_QUICK_BACKEND = 'software'
    & "$directoryPath/mininids.exe" --version
    if ($LASTEXITCODE -ne 0) { throw 'Deployed CLI failed' }
    $process = Start-Process -FilePath "$directoryPath/Mini-NIDS.exe" -ArgumentList '--smoke-test' -WorkingDirectory $logRoot -PassThru -WindowStyle Hidden -RedirectStandardOutput "$logRoot/stdout.txt" -RedirectStandardError "$logRoot/stderr.txt"
    $null = $process.Handle # Retain the handle so Windows PowerShell can read the exit code after exit.
    if (!$process.WaitForExit(30000)) { $process.Kill(); throw 'Deployed QML smoke test timed out' }
    if ($process.ExitCode -ne 0) { throw "Deployed GUI failed ($($process.ExitCode)): $(Get-Content -LiteralPath "$logRoot/stderr.txt" -Raw)" }
    Write-Output "All $($binaries.Count) binary dependency graphs and deployed QML smoke test passed."
} finally {
    $env:PATH = $savedPath
    foreach ($name in $qtEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name, $qtEnvironment[$name], 'Process') }
}
