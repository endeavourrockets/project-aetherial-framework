/* ASSUMPTIONS FOR REVIEW, NOT FINAL SETTINGS
 * this is from my offline Galvan payload prototype --> only tested in
 * simulation and the Renode emulator, never on real flight hardware
 *
 * still TBD (search the file for TBD and PLACEHOLDER):
 * SYSCLK_HZ --> 16 MHz like the team firmware.ioc (HSI, PLL off), 170 MHz
 *   possible later with the PLL, and TIM2_ARR comes from it
 * N_MILLS --> are there two motors? / HW_LOCKIN_AMP --> hardware lock-in amp?
 * IMU_AXIAL_AXIS and IMU_AXIAL_SIGN --> set once we know how the board is mounted
 * ATT_KP --> orientation filter gain
 * BOOST_MAX_MS --> needs the burn time from the engine file
 * APOGEE_LOCKOUT_MS 32 s and APOGEE_TIMEOUT_MS 70 s --> from my sim fitted to
 *   the OpenRocket apogee of 51 to 53.5 s, but that fit gives an apogee of
 *   about 13.6 km, not 30,000 ft, so i think the OpenRocket inputs need checking
 * DEPLOY_* and MILL_SETTLE_MS --> actuator not chosen yet, and deploying 1 s
 *   after burnout would be at about Mach 1.8
 * POWERLOSS_MIN_ALT_CM and NV_PAD_REFRESH_MS --> power loss recovery
 * VBAT divider and RECORD_RING_LEN
 * CAN_* --> CAN between PCBs, FDCAN1 is on PA11 and PA12 which clashes with
 *   my pin map / PARTICULATE_FITTED
 * acceleration margin --> my sim peaks at 14.6 g along the axis vs
 *   electronics rated 16 g (i think...)
 */

/*
 * config.h --> compile time settings for the Galvan field mill payload
 * all the numbers here are my baseline assumptions (A-06 to A-09 and A-23,
 * that's my own numbering, nothing to do with the team docs)
 * it has a twin in tools/params.py that i keep in sync with a test
 * the units are in the names --> _HZ, _MS, _S, _MG (milli g), _CM, _PA
 */
#ifndef PAYLOAD_CONFIG_H
#define PAYLOAD_CONFIG_H

/* firmware identity */
#define FW_VERSION_MAJOR 1 /* release version */
#define FW_VERSION_MINOR 0
#ifndef FW_GIT_HASH
#define FW_GIT_HASH "nogit000" /* commit hash, the Makefile puts the real one in */
#endif

/* mill mechanics and sampling (A-06, A-08, A-09), Aaryan told me 2 apertures */
#define N_APERTURES 2 /* openings in the rotor --> each one uncovers the electrode once per turn */
#define ROTOR_RPS 50 /* rotor speed --> 3000 rpm */
#define F_MOD_HZ 100 /* N_APERTURES times ROTOR_RPS --> how fast the signal is chopped */
#define M_SAMPLES 80 /* ADC samples in one modulation cycle */
#define FS_HZ 8000 /* M_SAMPLES times F_MOD_HZ --> ADC sample rate */
#define K_BLOCKS 10 /* cycles the lock-in averages for one output */
#define OUTPUT_HZ 10 /* F_MOD_HZ divided by K_BLOCKS --> field readings per second */
#define STEPS_PER_REV 20 /* the stepper does 20 steps per turn */
#define STEPS_PER_MOD_CYCLE 10 /* STEPS_PER_REV divided by N_APERTURES --> motor steps per aperture */
#define ADC_PER_STEP 8 /* M_SAMPLES divided by STEPS_PER_MOD_CYCLE --> ADC samples per motor step, so sampling stays locked to the rotor */
#define SYSCLK_HZ 16000000u /* MCU core clock, 16 MHz HSI like the team firmware.ioc --> TBD: 170 MHz possible later with the PLL on in CubeMX (Dav said it's just a config change), then TIM2_ARR changes too */
#define TIM2_ARR 1999u /* SYSCLK_HZ divided by FS_HZ, minus 1 --> timer 2 reload, it triggers the ADC at 8 kHz (was 21249 at 170 MHz) */
#define MOTOR_SPINUP_MS 600 /* motor ramp up --> data before this isn't trusted */
#define N_MILLS 1 /* TBD: how many field mills --> Dom's drive file talks about controlling two motors, my code only does 1 (one motor, one ADC channel, one lock-in) */
#define HW_LOCKIN_AMP 0 /* TBD: 1 if we use a hardware phase lock (lock-in) amplifier --> then the ADC just reads its output and my software lock-in isn't needed */

/* ADC and front end (A-07, A-09) --> how the electrode signal gets digitised,
 * and the limits used to spot the signal getting close to clipping */
#define ADC_BITS 12 /* native ADC resolution */
#define ADC_OVERSAMPLE 16 /* the hardware adds 16 conversions per sample --> less noise */
#define ADC_FULL_SCALE 65520 /* 4095 times 16 */
#define ADC_MID_COUNTS 32760 /* middle of the range --> the signal swings around this */
#define ADC_RAIL_MARGIN 3276 /* 5 % of full scale --> a sample this close to 0 or 65520 counts as near the rail */

/* range control --> two gain ranges: HI is sensitive, for calm weather,
 * LO is for strong fields like under storm clouds */
#define RANGE_HI 0 /* range id */
#define RANGE_LO 1 /* range id */
#define RANGE_UP_CYCLES 2 /* this many near rail cycles in a row --> switch HI to LO */
#define RANGE_SETTLE_CYCLES 2 /* cycles thrown away after a switch (20 ms at 100 Hz) */
#define RANGE_DOWN_OUTPUTS 10 /* this many quiet outputs in a row --> switch LO back to HI */
#define RANGE_DOWN_AMPL_COUNTS 149.0f /* LO amplitude below this --> HI would sit near 60 % of full scale, so switching back won't clip straight away */

/* main loop timing --> what i think the scheduler should look like, everything
 * runs off a 1 kHz tick and each task fires every N ticks */
#define TICK_HZ 1000 /* 1 ms system tick */
#define IMU_PERIOD_MS 10 /* 100 Hz */
#define BARO_PERIOD_MS 20 /* 50 Hz conversions --> 25 Hz pressure and temperature pairs */
#define SM_PERIOD_MS 10 /* state machine at 100 Hz */
#define SAFE_LOG_PERIOD_MS 1000 /* 1 record a second in SAFE, ARMED and LANDED (motor off) */
#define SYNC_PERIOD_MS 1000 /* save the file to the card at least every second */
#define WATCHDOG_TIMEOUT_MS 500 /* if the loop freezes for 0.5 s --> chip resets */

/* IMU mounting (set when the payload is integrated) */
#define IMU_AXIAL_AXIS 2 /* which accelerometer axis points to the nose --> 0 = x, 1 = y, 2 = z, i went with z but i'm not sure */
#define IMU_AXIAL_SIGN 1 /* 1 or -1 --> depends if the board is mounted with that axis pointing to the tail */
/* TBD: axis and sign above need the final board mounting */

/* orientation from raw IMU data (what the team went for in 2026-10 --> raw data
 * plus our own filter, not the chip's own fusion), see app/attitude.h --> Mahony filter */
#define ATT_KP 1.0f /* TBD: how hard the accelerometer pulls the tilt back (per second), only while it reads 1 g */
#define ATT_ACC_GATE_MG 150 /* only use the accelerometer between 0.85 and 1.15 g --> on the pad, not under thrust, coasting or on chutes */

/* state machine thresholds (A-23), from Finlay and published references --> still to be checked */
#define LAUNCH_ACCEL_MG 3000 /* axial acceleration that counts as launch */
#define LAUNCH_DWELL_MS 300 /* has to hold this long, so a quick bump doesn't count */
#define LAUNCH_BARO_RISE_CM 5000 /* backup --> 50 m above the pad counts as launch if the one above is missed */
#define BURNOUT_ACCEL_MG 500 /* axial acceleration below this --> engine stopped, i think */
#define BURNOUT_DWELL_MS 100 /* has to hold this long */
#define BOOST_MAX_MS 8000 /* 5 s burn plus margin, in case burnout isn't seen --> TBD: real burn time from the team engine file (Davide or Aarnav) */
#define APOGEE_LOCKOUT_MS 32000 /* TBD (was 15000): ignore the baro until then --> around Mach 1 there are shock waves that mess up the pressure reading. my sim fitted to OpenRocket is supersonic from 2.7 to 23.8 s and transonic until about 29.5 s (my old sim was supersonic 2.2 to 13.4 s) */
#define APOGEE_DESCENT_CM 1500 /* 15 m below the highest altitude --> apogee */
#define APOGEE_DWELL_MS 1000 /* has to hold this long */
#define APOGEE_TIMEOUT_MS 70000 /* force apogee if it's never seen --> TBD (was 60000): OpenRocket apogee is 51 to 53.5 s plus or minus 2 s, so 60 s only left 4.5 s of margin */
#define LANDED_VSPEED_CMPS 200 /* landed --> vertical speed under 2 m/s */
#define LANDED_DWELL_MS 10000 /* has to hold this long */
#define LANDED_TIMEOUT_MS 900000 /* 15 min after launch --> landed no matter what */

/* mill deployment --> mills stay in and the motor stays off on the pad, during
 * the burn and after landing, they go out once in COAST and never come back in
 * during flight. everything below is a PLACEHOLDER until the actuator is chosen
 * and tested (solenoid, pins TBD) */
#define DEPLOY_DELAY_MS 1000 /* PLACEHOLDER: time from entering COAST --> extend command, aero or structures to set it. TBD: my fitted sim is still at about Mach 1.8 at burnout and supersonic until about 24 s after launch, so 1 s after burnout might be too early */
#define DEPLOY_PULSE_MS 500 /* PLACEHOLDER: how long the solenoid is powered */
#define DEPLOY_CONFIRM_TIMEOUT_MS 2000 /* PLACEHOLDER: not out by then --> motor stays off */
#define DEPLOY_HAS_SENSOR 1 /* PLACEHOLDER: 1 = there's an end stop switch to read, 0 = assume it's out after the pulse */
#define MILL_SETTLE_MS 500 /* PLACEHOLDER: mills out --> wait this long --> motor on */
#define FAST_LOG_PERIOD_MS 100 /* 10 records a second (sensors only) while in flight with the motor off */
#define SELFTEST_SPIN_RETRACTED 1 /* 1 = the power on self test spins the motor with the mills still in */

/* full power loss in flight, see app/powerloss.h --> DRAFT, sending it for review, not final yet
 * a small record in flash survives a power cut --> if it powers on with the arm
 * pin already out, it starts logging straight away and works out where it is
 * before anything moves */
#define POWERLOSS_MIN_ALT_CM 30000 /* TBD: 300 m above the saved pad pressure --> in flight. has to be bigger than weather drift on the pad (about 8 m per 100 Pa) */
#define POWERLOSS_DECIDE_MS 1000 /* look at the baro and IMU this long after power on before deciding */
#define POWERLOSS_BARO_WAIT_MS 3000 /* no baro by then --> don't know, mills stay in */
#define NV_PAD_REFRESH_MS 600000 /* TBD: rewrite the pad pressure every 10 min while ARMED --> about 12 flash writes in 2 h on the pad */

/* power */
#define VBAT_LOW_MV 3400 /* low battery warning at 3.4 V */
#define VBAT_DIVIDER_NUM 2 /* TBD: 1 to 1 divider --> Vbat is 2 times the pin voltage. i need to check this, it changes with how electrical designs the battery sensing */
#define VBAT_DIVIDER_DEN 1

/* logging */
#define RECORD_SIZE 256 /* bytes per log record */
#define RECORD_RING_LEN 16 /* TBD: records held in RAM at once --> 1.6 s at 10 Hz, should be fine for SD card writes but needs checking with the real card */

/* team architecture still open (2026-10) --> written here so we don't forget,
 * none of this is in the code yet */
#define CAN_ENABLED 0 /* TBD: CAN between PCBs (Angelo and Dom), each board with its own small MCU, libcsp on top maybe. on the STM32G431 FDCAN1 is on PA11 and PA12 (need to confirm in the datasheet) --> my pin map uses those for SD chip select and the arm pin, so it would need changing */
#define CAN_BITRATE 500000 /* TBD: placeholder */
#define CAN_NODE_ID 0 /* TBD: this board's node id */
#define PARTICULATE_FITTED 0 /* TBD: particulate sensor (not on CAN, Dom said) --> needs a driver and space in the log */
/* accelerations --> my fitted sim peaks at 14.6 g along the axis and OpenRocket
 * at about 133 m/s2 (13.6 g, still to confirm). the IMU is set to 32 g either
 * way, but electronics rated at 16 g (Jack) doesn't leave much margin. TBD */

#endif /* PAYLOAD_CONFIG_H */
