# Tests

Add small changes. Test the bit of code you just changed, including what happens with funny inputs. This is not as important now, but after finishing the prototype tests will become vital.

## Run the PC tests

From the repository root, with CMake, Ninja and a native C compiler available:

```sh
cmake --preset host-tests
cmake --build --preset host-tests
ctest --preset host-tests
```

Use `ctest --preset host-tests --verbose` to see each Unity check. CTest currently reports one executable (`pressure`); that executable runs three Unity tests. These run on your computer, so use a PC compiler such as GCC, not Arm GCC. See [usage](../docs/usage.md) for local tool paths.

## What we have so far

`host/test_pressure.c` tests `firmware/obc/App/pressure.c`, which is also compiled into the OBC firmware. It checks a normal reading, the excluded limits and out-of-range values. The limits are inherited demo values, not agreed flight requirements. Passing these tests does not tell us whether a barometer works.

Unity is vendored in `third_party/Unity`. There are no generated runners or mock frameworks to learn: each test file has a `main()` with `RUN_TEST(...)` calls.

## Add a test for your component

1. Keep calculations or decisions in a small `.c`/`.h` pair that can compile without STM32 HAL or FreeRTOS. Leave peripheral reads and task scheduling in the board code; pass readings and elapsed time into the function.
2. Add `host/test_<component>.c`. Use the pressure test as a starting point, with a normal case, boundary cases and relevant failures. Register each test with `RUN_TEST(...)`.
3. In `host/CMakeLists.txt`, add an executable containing the test and the actual application source, its include directory, and a link to `unity`. Register it with `add_test(...)` and a short timeout, like the pressure test.
4. Run the PC tests and build the affected firmware. A host build will not catch every Arm-specific problem.

We can use the same setup across all four boards. These are ideas for future tests, not coverage we already have:

| Component | Useful PC checks | Follow up in emulation or on the bench |
| --- | --- | --- |
| OBC | Reading validation, packet encoding, flight-phase transitions once agreed | Task queues, storage and radio behaviour |
| EPS | Unit conversions and decisions at agreed thresholds | ADC readings, power switching and reset behaviour |
| Aerosol | Sensor packet parsing, stale readings and timeout decisions | Sensor communication and heater behaviour |
| Fieldmill | Field-mill processing with known signals, state transitions | ADC timing, rotor feedback and motor control |
| Shared CAN protocol | Encode/decode, malformed frames, duplicate commands | Communication between nodes, disconnects and recovery |

For code that needs I/O, a small fake function can supply readings or record commands. Add one when needed; we do not need a pretend spacecraft just to test a conversion. Agree expected behaviour with the relevant subteam before turning an uncertain threshold or actuator action into a test expectation.

## Renode and hardware

The root [Renode script](../renode/aetherial.resc) is a minimal OBC firmware/LED smoke check. It is not an automated assertion suite or proof of working CAN, sensors, SD logging or watchdog hardware. `renode/tests/` and `tests/integration/` are still placeholders.

Use Renode for supported peripherals and firmware interactions, then check the real boards for electrical behaviour and timing. POCs keep their own checks and run instructions under `poc/`. In a PR, say what ran, what passed, and what still needs a bench check.
