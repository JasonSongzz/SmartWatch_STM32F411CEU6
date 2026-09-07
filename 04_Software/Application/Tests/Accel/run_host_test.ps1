param(
    [ValidateRange(1, 100)]
    [int]$Runs = 5
)

$ErrorActionPreference = 'Stop'
$projectRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$testExe = Join-Path $env:TEMP 'accel_pipeline_test.exe'
$gcc = (Get-Command gcc -ErrorAction Stop).Source

$arguments = @(
    '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Wno-error=int-to-pointer-cast',
    '-DSTM32F411xE', '-DUSE_HAL_DRIVER',
    '-I', 'Bsp\Accel',
    '-I', 'Platform\Wrapper',
    '-I', 'Platform\Port',
    '-I', 'Service\Sensor',
    '-I', 'Service\Storage',
    '-I', 'Osal\inc',
    '-I', 'Core\Inc',
    '-I', 'Drivers\STM32F4xx_HAL_Driver\Inc',
    '-I', 'Drivers\STM32F4xx_HAL_Driver\Inc\Legacy',
    '-I', 'Drivers\CMSIS\Device\ST\STM32F4xx\Include',
    '-I', 'Drivers\CMSIS\Include',
    'Bsp\Accel\bsp_accel_handler.c',
    'Platform\Wrapper\drv_adapter_accel.c',
    'Platform\Port\drv_adapter_port_accel.c',
    'Service\Sensor\accel_service.c',
    'Tests\Accel\test_accel_pipeline.c',
    '-o', $testExe
)

Push-Location $projectRoot
try {
    & $gcc @arguments
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $testExe missing
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $testExe corrupt
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    for ($run = 1; $run -le $Runs; $run++) {
        & $testExe valid
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
}
finally {
    Pop-Location
    if (Test-Path -LiteralPath $testExe) {
        Remove-Item -LiteralPath $testExe -Force
    }
}
