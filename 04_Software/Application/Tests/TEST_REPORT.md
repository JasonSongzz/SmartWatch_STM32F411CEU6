# Driver pipeline validation report

Date: 2026-09-04

Scope: AHT21 (the request called it DHT21), MPU6050, CST816T, ST7789T3 and
external SPI Flash/SFUD/FAL/FlashDB/Storage Service.

## Executed validation

- Concrete BSP protocol tests repeated 10 times: all passed.
- Temperature/humidity pipeline: missing and corrupt calibration cases plus
  10 valid-calibration stress runs passed. The runs completed 1,692,365 cached
  consumer reads, concurrent calibration switching and two calibration writers
  without torn samples or deadlock.
- Accelerometer pipeline: missing and corrupt calibration cases plus 10 valid
  stress runs passed. The runs completed 1,568,844 cached snapshot reads,
  concurrent filter/calibration switching and two calibration writers without
  torn snapshots or deadlock.
- Touch pipeline: Handler behavior passed; 8000 concurrent Wrapper/Port reads
  and 500 processing-configuration switches passed. Maximum simultaneous full
  I2C transactions was one.
- Display pipeline: five runs, 10000 concurrent area writes per run, with zero
  window/pixel mismatches after the atomic API change.
- Flash pipeline: three Storage Service -> Wrapper -> Port -> FlashDB/FAL runs
  passed. Each run performed 600 dual-copy config writes, 1000 concurrent
  config reads and 400 concurrent log appends using an 8 MiB NOR model. A
  120 ms visitor caused 0 ms measured concurrent append delay in all 3 runs.
- Firmware build: `cmake --build build\Debug` passed. RAM is 61,712 bytes
  (47.08%) and internal Flash is 416,848 bytes (79.51%).

## Original findings and implemented regression expectations

1. **Display transaction race fixed.** The public chain now exposes one atomic
   `write_area()` operation. CASET, RASET, RAMWR and DMA completion execute
   under one device lock. Concurrent writers must complete with zero
   wrong-window transfers.

2. **Flash log visitor blocking fixed.** The Port now copies a record while the
   database is locked and invokes user code only after releasing TSDB/API
   locks. A visitor delayed by 120 ms must leave concurrent append latency
   below 80 ms, and callback re-entry through log append must succeed.

3. **Temperature/humidity control latency is bounded by a full sample group.**
   The Port holds `sample_mutex` for the complete group, including sensor wait
   and inter-sample OS delays. With AHT21 defaults this is approximately
   `5 * (80 ms + 200 ms) = 1.4 s`; cached consumers are unaffected, but
   calibration, sleep and wake calls can wait that long. This is not a
   deadlock, but it matters to high-priority control paths.

4. **First initialization and registration are single-owner APIs.** Static
   `registered`/`ready` checks and mutex creation are not themselves protected.
   Concurrent first calls could race. In the current startup, every Port is
   registered before the scheduler and each device is initialized by one
   designated task, so the condition is not currently reachable. Preserve
   this rule or add an initialization state mutex if runtime re-initialization
   is introduced.

5. **Long hardware transactions legitimately hold mutexes.** Display DMA wait,
   SFUD program/erase busy polling and CST816T wake reset serialize peers for
   their transaction duration. OS delays yield the CPU, and all configured OS
   mutex backends provide priority-aware mutex behavior, but yielding does not
   release the device mutex. Do not invoke these blocking APIs from ISR context.

## Hardware-only residual validation

Host tests do not prove GPIO electrical timing, I2C clock stretching, SPI/DMA
interrupt completion, real FreeRTOS preemption/tick jitter, stack high-water
marks, watchdog margins, cache/coherency on another MCU, sensor noise/physics,
or power loss during NOR program/erase. Those require target tests with fault
injection and logic-analyzer traces.
