# Flash Wrapper/Port/FlashDB stress test

This host test runs the production Storage Service, Flash Wrapper and Port
together with the official FlashDB/FAL sources and an 8 MiB in-memory NOR model. It validates
partition bounds/alignment, dual-copy configuration records, concurrent
configuration readers/writers, concurrent TSDB writers and record integrity.

It also verifies that the Port copies each record under the TSDB lock and
releases all database/API locks before invoking user code. A deliberately slow
visitor must not block a concurrent append, and a visitor may safely append a
new log record.

Run from PowerShell:

```powershell
.\Tests\Flash\run_host_test.ps1 -Runs 5
```

The memory model preserves NOR semantics (`write` can only change 1 to 0).
Power-loss behavior and actual SPI timing still require target hardware fault
injection.
