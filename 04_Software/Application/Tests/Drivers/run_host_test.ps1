param(
    [ValidateRange(1, 100)]
    [int]$Runs = 5
)

$ErrorActionPreference = 'Stop'
$projectRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$i2cTestExe = Join-Path $env:TEMP 'i2c_sensor_driver_tests.exe'
$spiTestExe = Join-Path $env:TEMP 'spi_device_driver_tests.exe'
$gcc = (Get-Command gcc -ErrorAction Stop).Source

$common = @('-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror')
$i2cArguments = $common + @(
    '-I', 'Bsp\TempHumi', '-I', 'Bsp\Accel', '-I', 'Bsp\Touch',
    'Bsp\TempHumi\bsp_aht21_driver.c',
    'Bsp\Accel\bsp_mpu6050_driver.c',
    'Bsp\Touch\bsp_cst816t_driver.c',
    'Tests\Drivers\test_i2c_sensor_drivers.c',
    '-lm', '-o', $i2cTestExe
)
$spiArguments = $common + @(
    '-I', 'Bsp\Display', '-I', 'Bsp\SPIFlash',
    '-I', 'Middlewares\Third_Party\sfud\inc',
    'Bsp\Display\bsp_st7789t3_driver.c',
    'Bsp\SPIFlash\bsp_spiflash_driver.c',
    'Tests\Drivers\test_spi_device_drivers.c',
    '-o', $spiTestExe
)

Push-Location $projectRoot
try {
    & $gcc @i2cArguments
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $gcc @spiArguments
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    for ($run = 1; $run -le $Runs; $run++) {
        & $i2cTestExe
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & $spiTestExe
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
}
finally {
    Pop-Location
    foreach ($testExe in @($i2cTestExe, $spiTestExe)) {
        if (Test-Path -LiteralPath $testExe) {
            Remove-Item -LiteralPath $testExe -Force
        }
    }
}
