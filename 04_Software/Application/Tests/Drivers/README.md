# Concrete driver host tests

These tests compile the production AHT21, MPU6050, CST816T, ST7789T3 and
SPI-Flash BSP drivers against deterministic protocol doubles. They cover
normal transfers, conversion/parsing, boundary checks, injected I/O errors,
lock/unlock balance, DMA completion and sleep/wakeup behavior.

Run from PowerShell:

```powershell
.\Tests\Drivers\run_host_test.ps1 -Runs 20
```

The SPI-Flash test mocks SFUD's public API so it validates the BSP/SFUD
adapter. FlashDB and full Wrapper/Port concurrency are covered separately.
