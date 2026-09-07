$ErrorActionPreference = 'Stop'
$projectRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$handlerTestExe = Join-Path $env:TEMP 'touch_handler_test.exe'
$pipelineTestExe = Join-Path $env:TEMP 'touch_pipeline_test.exe'
$gcc = (Get-Command gcc -ErrorAction Stop).Source

$handlerArguments = @(
    '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-I', 'Bsp\Touch',
    'Bsp\Touch\bsp_touch_handler.c',
    'Tests\Touch\test_touch_handler.c',
    '-o', $handlerTestExe
)

$pipelineArguments = @(
    '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror',
    '-Wno-error=int-to-pointer-cast',
    '-DSTM32F411xE', '-DUSE_HAL_DRIVER',
    '-I', 'Bsp\Touch', '-I', 'Platform\Wrapper', '-I', 'Platform\Port',
    '-I', 'Osal\inc', '-I', 'Core\Inc',
    '-I', 'Drivers\STM32F4xx_HAL_Driver\Inc',
    '-I', 'Drivers\STM32F4xx_HAL_Driver\Inc\Legacy',
    '-I', 'Drivers\CMSIS\Device\ST\STM32F4xx\Include',
    '-I', 'Drivers\CMSIS\Include',
    'Bsp\Touch\bsp_cst816t_driver.c',
    'Bsp\Touch\bsp_touch_handler.c',
    'Platform\Wrapper\drv_adapter_touch.c',
    'Platform\Port\drv_adapter_port_touch.c',
    'Tests\Touch\test_touch_pipeline.c',
    '-o', $pipelineTestExe
)

Push-Location $projectRoot
try {
    & $gcc @handlerArguments
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $gcc @pipelineArguments
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $handlerTestExe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $pipelineTestExe
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
finally {
    Pop-Location
    foreach ($testExe in @($handlerTestExe, $pipelineTestExe)) {
        if (Test-Path -LiteralPath $testExe) {
            Remove-Item -LiteralPath $testExe -Force
        }
    }
}
