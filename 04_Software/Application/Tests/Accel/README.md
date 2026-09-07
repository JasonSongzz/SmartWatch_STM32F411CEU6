# Accelerometer/IMU host test

This test links the production Handler, Wrapper, Port and Service modules to
Win32 OSAL, storage, I2C and sensor doubles. It covers median spike rejection,
low-pass state, calibration switching, atomic six-axis snapshots, failed reads,
sleep/wakeup serialization, concurrent persistent writers, FlashDB startup
fallback and six-position calibration math.

It does not replace target testing for I2C electrical faults, MPU6050 data-ready
timing, FreeRTOS execution jitter, stack watermarks or calibration fixture error.

```powershell
powershell -ExecutionPolicy Bypass -File Tests\Accel\run_host_test.ps1
```
