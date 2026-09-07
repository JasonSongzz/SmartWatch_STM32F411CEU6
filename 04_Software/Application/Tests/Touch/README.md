# Touch handler host test

These tests cover both the Handler logic and the production CST816T -> Handler
-> Port -> Wrapper chain. The stress case concurrently performs 8000 reads and
500 filter/configuration changes, verifies event sequence uniqueness and
checks that no more than one complete I2C transaction is active at a time.

It does not replace target testing for CST816T electrical timing, panel-edge
linearity, water rejection, glove behavior, interrupt wiring or production
fixture accuracy.

```powershell
powershell -ExecutionPolicy Bypass -File Tests\Touch\run_host_test.ps1
```
