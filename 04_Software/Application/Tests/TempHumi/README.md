# Temperature/humidity host test

This stress test links the production Service, Wrapper, Port and Handler code
against Win32 OSAL/storage/I2C test doubles. It checks persisted calibration
loading, filtered production, concurrent cached consumers, calibration changes
at group boundaries, concurrent persistent writers, sleep/wakeup serialization,
persistence, validation and two-point calculation.

The test does not replace target tests for GPIO timing, electrical I2C faults,
FreeRTOS tick conversion or real sensor conversion timing.

Run five stress iterations plus missing/corrupt FlashDB startup cases:

```powershell
powershell -ExecutionPolicy Bypass -File Tests\TempHumi\run_host_test.ps1
```
