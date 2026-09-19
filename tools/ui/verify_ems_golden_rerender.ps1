param(
    [Parameter(Mandatory = $true)][string]$Python,
    [Parameter(Mandatory = $true)][string]$Executable,
    [Parameter(Mandatory = $true)][string]$ReferenceDir,
    [string]$Platform = "windows",
    [int]$DelayMs = 800
)

$ErrorActionPreference = "Stop"
$temporary = Join-Path ([System.IO.Path]::GetTempPath()) ("avionix-ems-rerender-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $temporary | Out-Null
$sizes = @(@(800, 480), @(1024, 600), @(1280, 800))
$states = @(@("LIVE", 0), @("STALE", 600), @("DEGRADED", 600), @("LOST", 1500), @("FAIL", 0), @("SIM", 0))
try {
    foreach ($size in $sizes) {
        foreach ($state in $states) {
            $width = $size[0]
            $height = $size[1]
            $name = "ems-$($width)x$($height)-$($state[0].ToLower()).png"
            $env:QT_QPA_PLATFORM = $Platform
            $env:QT_SCALE_FACTOR = "1"
            $env:EFIS_CAPTURE = Join-Path $temporary $name
            $env:EFIS_CAPTURE_SIZE = "$($width)x$($height)"
            $env:EFIS_CAPTURE_DELAY_MS = "$DelayMs"
            $env:EFIS_CAPTURE_EXIT = "1"
            $env:EFIS_HMI_GOLDEN_STATE = $state[0]
            $env:EFIS_HMI_GOLDEN_AGE_MS = "$($state[1])"
            Write-Host "capture $($width)x$($height)/$($state[0])"
            $process = Start-Process -FilePath $Executable -ArgumentList "--hmi-golden" -WindowStyle Hidden -Wait -PassThru
            if ($process.ExitCode -ne 0 -or -not (Test-Path $env:EFIS_CAPTURE)) {
                throw "renderer failed for $name with exit code $($process.ExitCode)"
            }
        }
    }
    & $Python tools/ui/capture_ems_responsive_state_matrix.py --output-dir $temporary --platform $Platform --manifest-only | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "manifest generation failed" }
    & $Python tools/ui/verify_ems_golden_rerender.py --rerender-dir $temporary --reference-dir $ReferenceDir
    exit $LASTEXITCODE
}
finally {
    Remove-Item -LiteralPath $temporary -Recurse -Force -ErrorAction SilentlyContinue
}
