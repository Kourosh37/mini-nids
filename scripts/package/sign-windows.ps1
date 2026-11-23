param([Parameter(Mandatory=$true)][string[]]$Files)
$ErrorActionPreference = 'Stop'
if (!$env:WINDOWS_SIGN_PFX_BASE64) {
    if ($env:MININIDS_REQUIRE_SIGNING -eq 'true') { throw 'Windows signing certificate is required' }
    Write-Output 'No signing certificate configured; Windows artifacts will be unsigned.'
    return
}
if (!$env:WINDOWS_SIGN_PASSWORD) { throw 'WINDOWS_SIGN_PASSWORD is required' }
$certificatePath = Join-Path ([System.IO.Path]::GetTempPath()) (([guid]::NewGuid().ToString()) + '.pfx')
$certificate = $null
$imported = @()
$existing = @(Get-ChildItem Cert:\CurrentUser\My | ForEach-Object { $_.Thumbprint })
try {
    [System.IO.File]::WriteAllBytes($certificatePath, [Convert]::FromBase64String($env:WINDOWS_SIGN_PFX_BASE64))
    $password = ConvertTo-SecureString $env:WINDOWS_SIGN_PASSWORD -AsPlainText -Force
    $imported = @(Import-PfxCertificate -FilePath $certificatePath -CertStoreLocation Cert:\CurrentUser\My -Password $password)
    $certificate = $imported | Where-Object { $_.HasPrivateKey } | Select-Object -First 1
    if (!$certificate) { throw 'The PFX has no signing private key' }
    foreach ($file in $Files) {
        $signature = Set-AuthenticodeSignature -FilePath $file -Certificate $certificate -HashAlgorithm SHA256 -TimestampServer 'http://timestamp.digicert.com'
        if ($signature.Status -ne 'Valid') { throw "Signing failed: $file ($($signature.Status))" }
    }
} finally {
    foreach ($entry in $imported) {
        if ($entry.Thumbprint -notin $existing) { Remove-Item -LiteralPath "Cert:\CurrentUser\My\$($entry.Thumbprint)" -ErrorAction SilentlyContinue }
    }
    Remove-Item -LiteralPath $certificatePath -ErrorAction SilentlyContinue
}
