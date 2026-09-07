param(
    [ValidateRange(1, 100)]
    [int]$Runs = 5
)

$ErrorActionPreference = 'Stop'
$projectRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$testExe = Join-Path $env:TEMP 'display_pipeline_test.exe'
$gcc = (Get-Command gcc -ErrorAction Stop).Source
$arguments = @(
    '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-I', 'Bsp\Display', '-I', 'Platform\Wrapper',
    'Bsp\Display\bsp_st7789t3_driver.c',
    'Bsp\Display\bsp_display_handler.c',
    'Platform\Wrapper\drv_adapter_display.c',
    'Tests\Display\test_display_pipeline.c',
    '-o', $testExe
)

Push-Location $projectRoot
try {
    & $gcc @arguments
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    for ($run = 1; $run -le $Runs; $run++) {
        & $testExe
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
}
finally {
    Pop-Location
    if (Test-Path -LiteralPath $testExe) {
        Remove-Item -LiteralPath $testExe -Force
    }
}
