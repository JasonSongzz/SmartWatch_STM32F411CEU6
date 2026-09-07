# Host validation suite

The suite validates the current AHT21, MPU6050, CST816T, ST7789T3 and external
SPI-Flash chains at three levels: concrete BSP protocol behavior,
Handler/Wrapper/Port logic, and coordinated Win32 concurrency stress.

```powershell
powershell -ExecutionPolicy Bypass -File Tests\run_all_host_tests.ps1 -StressRuns 10 -FlashRuns 3
```

The display stress test drives concurrent writers through the atomic
`write_area` API and fails if any pixel payload is delivered to another
writer's window.

These tests cannot validate GPIO electrical characteristics, true interrupt
preemption, DMA cache/coherency, stack margins, tick jitter, power-loss timing
or sensor physics. Those require target hardware tests.
