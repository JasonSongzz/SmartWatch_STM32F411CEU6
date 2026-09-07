param(
    [ValidateRange(1, 20)]
    [int]$Runs = 3
)

$ErrorActionPreference = 'Stop'
$projectRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$testExe = Join-Path $env:TEMP 'flash_pipeline_test.exe'
$gcc = (Get-Command gcc -ErrorAction Stop).Source
$arguments = @(
    '-std=gnu11', '-O2', '-Wall', '-Wextra',
    '-Wno-int-to-pointer-cast', '-Wno-pointer-to-int-cast',
    '-Wno-format', '-Wno-unused-parameter',
    '-DSTM32F411xE', '-DUSE_HAL_DRIVER',
    '-I', 'Bsp\SPIFlash', '-I', 'Platform\Wrapper', '-I', 'Platform\Port',
    '-I', 'Service\Storage',
    '-I', 'Osal\inc', '-I', 'Core\Inc',
    '-I', 'Middlewares\Third_Party\FlashDB\inc',
    '-I', 'Middlewares\Third_Party\FlashDB\port',
    '-I', 'Middlewares\Third_Party\FlashDB\port\fal\inc',
    '-I', 'Drivers\STM32F4xx_HAL_Driver\Inc',
    '-I', 'Drivers\STM32F4xx_HAL_Driver\Inc\Legacy',
    '-I', 'Drivers\CMSIS\Device\ST\STM32F4xx\Include',
    '-I', 'Drivers\CMSIS\Include',
    'Platform\Wrapper\drv_adapter_flash.c',
    'Platform\Port\drv_adapter_port_flash.c',
    'Service\Storage\storage_service.c',
    'Middlewares\Third_Party\FlashDB\src\fdb.c',
    'Middlewares\Third_Party\FlashDB\src\fdb_kvdb.c',
    'Middlewares\Third_Party\FlashDB\src\fdb_tsdb.c',
    'Middlewares\Third_Party\FlashDB\src\fdb_utils.c',
    'Middlewares\Third_Party\FlashDB\port\fal\src\fal.c',
    'Middlewares\Third_Party\FlashDB\port\fal\src\fal_flash.c',
    'Middlewares\Third_Party\FlashDB\port\fal\src\fal_partition.c',
    'Tests\Flash\test_flash_pipeline.c',
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
