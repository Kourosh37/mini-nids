param(
    [Parameter(Mandatory=$true)][string]$CaptureDirectory,
    [Parameter(Mandatory=$true)][string]$GroundTruthDirectory,
    [string]$Executable = 'build/release/apps/cli/mininids.exe',
    [string]$OutputDirectory = 'results/academic'
)
$ErrorActionPreference = 'Stop'
$captures = @('DoS-SYN-stu', 'DNSspoofing-stu', 'ARPspoofing-stu', 'portScan-stu')
foreach ($capture in $captures) {
    $inputPath = Join-Path $CaptureDirectory "$capture.pcap"
    $truthPath = Join-Path $GroundTruthDirectory "$capture.json"
    if (!(Test-Path -LiteralPath $inputPath) -or !(Test-Path -LiteralPath $truthPath)) {
        throw "Missing capture or independently annotated ground truth for $capture"
    }
}
foreach ($capture in $captures) {
    foreach ($threshold in @(10, 20, 40)) {
        $session = Join-Path $OutputDirectory "$capture-scan-$threshold"
        if (Test-Path -LiteralPath $session) { throw "Output already exists: $session" }
        $configPath = Join-Path ([System.IO.Path]::GetTempPath()) ([System.Guid]::NewGuid().ToString() + '.json')
        try {
            @{ scan_ports = $threshold } | ConvertTo-Json | Set-Content -LiteralPath $configPath -Encoding UTF8
            & $Executable analyze --input (Join-Path $CaptureDirectory "$capture.pcap") --output $session --ground-truth (Join-Path $GroundTruthDirectory "$capture.json") --config $configPath
            if ($LASTEXITCODE -ne 0) { throw "Analysis failed: $capture at threshold $threshold" }
        } finally {
            Remove-Item -LiteralPath $configPath -ErrorAction SilentlyContinue
        }
    }
}
