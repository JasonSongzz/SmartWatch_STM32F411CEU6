# OSAL backend selection

The public API in `inc/` and the validation/record management in `shared/`
are RTOS independent. Exactly one native backend is compiled.

```text
Application/BSP/Platform
          |
       OSAL API
          |
   shared validation
          |
  +-------+--------+
  |       |        |
FreeRTOS RT-Thread Zephyr
```

Select the backend at CMake configure time:

```sh
cmake -S . -B build -DOSAL_BACKEND=FreeRTOS
cmake -S . -B build -DOSAL_BACKEND=RTThread
cmake -S . -B build -DOSAL_BACKEND=Zephyr
```

`FreeRTOS` is wired to the current STM32CubeMX `stm32cubemx` target. For an
RT-Thread or Zephyr project, either expose the native kernel headers through
the parent target or pass them with `OSAL_NATIVE_INCLUDE_DIRS`. If the native
project exports a CMake target, pass its name with `OSAL_NATIVE_TARGET`.

Examples:

```sh
cmake -DOSAL_BACKEND=RTThread \
      -DOSAL_NATIVE_TARGET=rtthread ...

cmake -DOSAL_BACKEND=Zephyr \
      -DOSAL_NATIVE_TARGET=zephyr_interface ...
```

Backend requirements:

- FreeRTOS: task notifications and software timers enabled as required by the
  public OSAL calls used by the application.
- RT-Thread: heap, threads, mutexes, semaphores, message queues and timers.
  Soft timers are used when `RT_TIMER_FLAG_SOFT_TIMER` is available.
- Zephyr: kernel heap and dynamic thread stacks. Enable a non-zero
  `CONFIG_HEAP_MEM_POOL_SIZE`; stack watermark reporting additionally needs
  `CONFIG_THREAD_STACK_INFO` and `CONFIG_INIT_STACKS`.

Portable limitations are explicit:

- RT-Thread cannot receive a message queue item from ISR context, so that
  operation returns `OSAL_ERR_OPERATION_NOT_SUPPORTED`.
- Zephyr has no generic application idle hook, so registration returns
  `OSAL_ERR_NOT_IMPLEMENTED`.
- RT-Thread stack watermark and Zephyr global heap statistics return zero when
  the native kernel has no portable query for that metric.
- The critical-section bookkeeping targets the single-core MCUs used by this
  project. An SMP port must replace it with per-CPU/per-thread state.

Changing an RTOS also requires the native startup, linker and kernel build of
that RTOS. This directory isolates the OSAL-facing change; it does not turn a
CubeMX FreeRTOS image into a Zephyr or RT-Thread image by itself.
