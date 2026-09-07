# Display pipeline stress test

This host test runs the production ST7789T3 driver, display Handler and
Wrapper with a mutex-protected SPI double. Two concurrent writers repeatedly
use the atomic `write_area()` API while the double verifies that every pixel
payload is delivered to its own window.

Run from PowerShell:

```powershell
.\Tests\Display\run_host_test.ps1 -Runs 20
```

The test fails on any window/pixel mismatch.
