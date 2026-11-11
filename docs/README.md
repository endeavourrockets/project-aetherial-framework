# Project notes

These are the short, practical notes for working on the prototype. Start with [usage](usage.md), [contributing](CONTRIBUTING.md) or the [test guide](../tests/README.md). `structure.md` is waiting for the team's main-loop, boot and CAN diagrams.

## The four boards

The electronics block diagram splits the payload into four MCU-based modules:

- **OBC:** coordinates the payload, handles storage and telemetry, and interfaces with the field-mill deployment motor.
- **EPS:** monitors power and handles the agreed local protection/control.
- **Aerosol:** reads the gas/aerosol instruments and controls their heater.
- **Fieldmill:** runs the field-mill experiment, including its motors and acquisition.

Telemetry is a group of OBC peripherals, not a fifth MCU. The diagram includes IMU, barometer, GPS and LoRa connections, an SD card, and two field mills. Exact parts, pins and some interfaces still need agreeing with electronics and science.

The current code only builds the OBC. Its readings are synthetic, and its demo counters do not mean that data was written to an SD card or sent over a radio.

## Things still to agree

The diagram proposes two CAN buses. The [G431 has one FDCAN controller](https://www.st.com/en/microcontrollers-microprocessors/stm32g431kb.html), so two transceivers alone do not give us two independent buses. Electronics and software need to agree the second-controller or switching arrangement before we claim redundancy. The diagram shows an external controller on the OBC; the other boards also need a clear plan. F405 experiments do not settle the G431 design.

We also need the message format, units, timestamps, command responses and timeout behaviour agreed between boards. Decide what each motor/heater does during boot, reset and lost communication with the relevant subteam. Keep those decisions out of convenient-looking demo defaults.

The proposed 50 Hz logging and 10 Hz downlink rates are planning values. They do not specify raw sensor or field-mill acquisition rates. Science needs to confirm those rates and the rotor synchronisation requirements.

## References

The team's **Software Setup Guide**, **Useful Links** and **Electronics_Block_Diagram** are the background references for these notes. The setup guide predates the four-board layout; use the paths and commands here for this branch. Its push command uploads a branch; opening a PR is a separate step.

The [Sounding Rocket useful links document](https://docs.google.com/document/d/1j0_V_2mv3sfK8wf8FN7-OvJxQ4NYg1ImsrFonHdGRdk/edit?usp=drive_link) has the wider rocket references.

Useful starting points from the links document:

- [2027 IREC rules, revision P1](https://static1.squarespace.com/static/687d3841f7d71450d1ba824d/t/6ad8124c0c5dd20e2a0bddc0/1792545356387/2027+IREC+R%26R+REV+P1.pdf)
- [2027 IREC design guide, revision P1](https://static1.squarespace.com/static/687d3841f7d71450d1ba824d/t/6ad64a9f7bca727e90f32d2a/1792428703896/2027+IREC+DTEG+REV+P1.pdf)
- [NASA CubeSat 101](https://www.nasa.gov/wp-content/uploads/2017/03/nasa_csli_cubesat_101_508.pdf)
- [CubeSat flight-software resources](https://cubesat-resources.space/development/flight-software/)

Check the team's current competition documents before using a requirement from these links. General CubeSat reading is useful context, not proof that our rocket payload meets its requirements.
