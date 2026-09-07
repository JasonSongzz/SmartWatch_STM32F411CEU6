param(
    [ValidateRange(1, 100)]
    [int]$StressRuns = 10,
    [ValidateRange(1, 20)]
    [int]$FlashRuns = 3
)

$ErrorActionPreference = 'Stop'
$projectRoot = Resolve-Path (Join-Path $PSScriptRoot '..')

function Invoke-TestScript {
    param([string]$Path, [string[]]$Arguments = @())
    Write-Host "`n== $Path =="
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $Path @Arguments
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Push-Location $projectRoot
try {
    Invoke-TestScript 'Tests\Drivers\run_host_test.ps1' @('-Runs', "$StressRuns")
    Invoke-TestScript 'Tests\TempHumi\run_host_test.ps1' @('-Runs', "$StressRuns")
    Invoke-TestScript 'Tests\Accel\run_host_test.ps1' @('-Runs', "$StressRuns")
    Invoke-TestScript 'Tests\Touch\run_host_test.ps1'
    Invoke-TestScript 'Tests\Display\run_host_test.ps1' @('-Runs', "$StressRuns")
    Invoke-TestScript 'Tests\Flash\run_host_test.ps1' @('-Runs', "$FlashRuns")
}
finally {
    Pop-Location
}

Write-Host "`nAll host unit, logic and stress tests completed."
