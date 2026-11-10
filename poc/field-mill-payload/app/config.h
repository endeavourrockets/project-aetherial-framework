/*
 * config.h-Galvan field mill payload: compile-time configuration. 💩
 *
 * The numbers here my baseline assumption (assumptions
 * A-06 to A-09 and A-23) and has a twin in tools/params.py (for me).
 * Bonjour ^•ﻌ•^ฅ
 * Units are in the macro names: _HZ, _MS, _S, _MG (milli-g), _CM, _PA. :)
 */
#ifndef PAYLOAD_CONFIG_H
#define PAYLOAD_CONFIG_H

/*Firmware identity*/
#define FW_VERSION_MAJOR 1 /*relase version*/
#define FW_VERSION_MINOR 0
#ifndef FW_GIT_HASH
#define FW_GIT_HASH "nogit000" /*commit hash, the Makefile puts the real one in */
#endif

/*Mill mechanics and sampling (A-06, A-08, A-09), aaryan told me 2 aptures...*/
#define N_APERTURES 2 /*opening in rotot, rach exposed electode once per revolution*/
#define ROTOR_RPS 50 /*rotor speed 3000rpm*/
#define F_MOD_HZ 100 /* N_APERTURES * ROTOR_RPS, signal modulation */
#define M_SAMPLES 80 /* ADC samples per modulation cycle */
#define FS_HZ 8000 /* M_SAMPLES * F_MOD_HZ , ADC smaples rate 40*200 */
#define K_BLOCKS 10 /* cycles per lock-in output , modulation sycles averaged by lock-in demodulator per output value*/
#define OUTPUT_HZ 10 /* F_MOD_HZ / K_BLOCKS , electric field redaings per second 200:20*/
#define STEPS_PER_REV 20 /*rotor drive by stepper motor with 20 steps per turn*/
#define STEPS_PER_MOD_CYCLE 10 /* STEPS_PER_REV / N_APERTURES , motor steps per apeture passage 20:4*/
#define ADC_PER_STEP 8 /* M_SAMPLES / STEPS_PER_MOD_CYCLE , ADC samples per moto step 40:5, fo rsompling to rotor position*/
#define SYSCLK_HZ 16000000u /*MCU core clock, 16 MHz HSI like the team firmware.ioc; TBD: 170 MHz possible later with the PLL in CubeMX (Dav: just a config change), then TIM2_ARR changes too*/
#define TIM2_ARR 1999u /* SYSCLK_HZ / FS_HZ - 1 , timer2 relaod value, it triggers ADC at 8khz*/
/*16000000:8000-1, was 21249 at 170 MHz*/
#define MOTOR_SPINUP_MS 600 /* mototr ramp before data is trusted, data before this is not trusted*/
#define N_MILLS 1 /*TBD: number of field mills; Dom's drive file mentions controlling two motors. The code supports 1 only (one motor, one ADC channel, one lock-in)*/
#define HW_LOCKIN_AMP 0 /*TBD: 1 if the team uses a hardware phase-lock (lock-in) amplifier; then the ADC reads its output and the software lock-in (lockin.c) is not needed*/

/*ADC and front end (A-07, A-09), how to digitilise, convert to electode signal*/
/*anf thresholds used ot detect singal whne near clipping*/
#define ADC_BITS 12 /*native ADC resolution*/
#define ADC_OVERSAMPLE 16 /*hardware oversampling, 16 conversion summed per sample, to reduce noise*/
#define ADC_FULL_SCALE 65520 /* (2^12 - 1) * 16 */
#define ADC_MID_COUNTS 32760 /* nominal mid-rail, signal will swing around this */
#define ADC_RAIL_MARGIN 3276 /* 5 % of full scale, if sample near 0 or 65520, it is near rail*/

/*Range control */
/*this si for gain ranges, 2 types: HI sentitive, for chill weather, LO for strong electric filds fro clouds or ares with great electric fields*/
#define RANGE_HI 0 /*range identifier*/
#define RANGE_LO 1 /*same as above, rnage identifier*/
#define RANGE_UP_CYCLES 2/* consecutive near rail cycles to switch HI->LO */
#define RANGE_SETTLE_CYCLES 2/* cycles discarded after a switch (20 ms at 100 Hz)*/
#define RANGE_DOWN_OUTPUTS 10/* consecutive quiet outputs to switch LO->HI */
#define RANGE_DOWN_AMPL_COUNTS 149.0f /* LO amplitude below which HI would sit near 60 % of FS */
/*quiet LO amplitude below 149 counts, like that switch back does not inmediately clip*/

/*Timing of main loop */
/*what i think the scheduler should look like, runs from 1KHz tick, each periodic task fires every N ticks*/
#define TICK_HZ 1000 /*1ms sytem tick, every task will run on*/
#define IMU_PERIOD_MS 10 /* 100 Hz */
#define BARO_PERIOD_MS 20 /* 50 Hz conversions, 25 Hz pairs */
#define SM_PERIOD_MS 10 /* state machine at 100 Hz */
#define SAFE_LOG_PERIOD_MS 1000 /* 1 Hz records in SAFE, ARMED and LANDED (motor off) */
#define SYNC_PERIOD_MS 1000 /* flush the file at least every second */
#define WATCHDOG_TIMEOUT_MS 500 /*if loop freezes for 0.5s, then chip reset*/

/*IMU mounting (set when the payload is integrated) */
#define IMU_AXIAL_AXIS 2 /* 0 = x, 1 = y, 2 = z points to the nose, choose whihc accelerometre axis point rocket, or z axis if that is converntion, i lowkey dunno*/
#define IMU_AXIAL_SIGN 1 /* +1 or -1, this depends on if board is mounted with axis pointing toward tail, it depend */
/*TBD: axis and sign above must be set from the final board mounting*/

/* Orientation from raw IMU data (team direction 2026-10: raw data + our own
 * filter, not on-chip fusion), see app/attitude.h. Mahony filter. */
#define ATT_KP 1.0f /*TBD: how hard the accelerometer pulls the tilt back (1/s), only while it reads 1 g*/
#define ATT_ACC_GATE_MG 150 /*accelerometer used only within 1 g +- this (0.85 to 1.15 g): on the pad, not under thrust, coast or chutes*/

/* State machine thresholds (A-23), attained by finlay, and AI and internet */
#define LAUNCH_ACCEL_MG 3000/* axial specific force */
#define LAUNCH_DWELL_MS 300 /*holding time, so rapid chnages dont count :) */
#define LAUNCH_BARO_RISE_CM 5000 /*backup: 50 m above pad, other oncdition in case aboove is not detected*/
#define BURNOUT_ACCEL_MG 500 /*if axial force this, then motor stopped, i think...*/
#define BURNOUT_DWELL_MS 100 /*holding time fo rcondition above*/
#define BOOST_MAX_MS 8000 /*5 s burn + margin, assumed burnout time, if condition above fails; TBD: burn time from the team engine file (Davide/Aarnav)*/
#define APOGEE_LOCKOUT_MS 32000 /*TBD (was 15000): sim fitted to OpenRocket 2026-10 is supersonic 2.7-23.8 s, transonic until ~29.5 s; old sim: supersonic from 2.2 s to 13.4 s, ignore baro (transonic) until then, at mach 1 there is waves, affect baronitc measurement, generally not reliable data in this time period, i think...*/
#define APOGEE_DESCENT_CM 1500 /*when it is 15m below heighets altitude*/
#define APOGEE_DWELL_MS 1000 /*holding time fo rcondition above*/
#define APOGEE_TIMEOUT_MS 70000/* force apogee if never detected; TBD (was 60000): OpenRocket apogee 51-53.5 s +-2 s, so 60 s left only 4.5 s margin */
#define LANDED_VSPEED_CMPS 200 /*lading, vertical speed 2m/s */
#define LANDED_DWELL_MS 10000 /*holding time for condition above*/
#define LANDED_TIMEOUT_MS 900000 /*15 min after launch, forced :)*/

/* Mill deployment (team decision 2026-10 D1): mills retracted and motor OFF
 * on the pad, in BOOST and after landing; extended once in COAST and never
 * retracted in flight. All values below are PLACEHOLDERS until the
 * actuator is chosen and tested (solenoid, pins TBD). */
#define DEPLOY_DELAY_MS 1000 /*PLACEHOLDER: COAST entry -> extend command, aero/structures to set; TBD: the fitted sim is still at Mach ~1.8 at burnout and supersonic until ~24 s after launch, so deploying 1 s after burnout may be too early*/
#define DEPLOY_PULSE_MS 500 /*PLACEHOLDER: solenoid energised this long, then released*/
#define DEPLOY_CONFIRM_TIMEOUT_MS 2000 /*PLACEHOLDER: not extended by then -> motor stays off*/
#define DEPLOY_HAS_SENSOR 1 /*PLACEHOLDER: 1 = end-stop switch read, 0 = assume extended after the pulse*/
#define MILL_SETTLE_MS 500 /*PLACEHOLDER: extended -> motor start*/
#define FAST_LOG_PERIOD_MS 100 /* 10 Hz sensor-only records while fast logging with the motor off */
#define SELFTEST_SPIN_RETRACTED 1 /* 1 = power-on self-test spins the motor with the mills retracted */

/* Full power loss in flight (team decision 2026-10 D4), see app/powerloss.h.
 * A small record in flash (pad pressure + armed/launched/deployed flags)
 * survives a power cut; at power-on with the arm pin already out the
 * payload logs at once and works out where it is before anything moves. */
#define POWERLOSS_MIN_ALT_CM 30000 /*TBD: 300 m above the saved pad pressure = "in flight"; must beat pad weather drift (about 8 m per 100 Pa)*/
#define POWERLOSS_DECIDE_MS 1000 /*look at baro + IMU this long after power-on before deciding*/
#define POWERLOSS_BARO_WAIT_MS 3000 /*no barometer by then -> verdict unknown, mills stay in*/
#define NV_PAD_REFRESH_MS 600000 /*TBD: rewrite the pad pressure every 10 min while ARMED (flash wear: about 12 writes per 2 h on the pad)*/

/* Power*/
#define VBAT_LOW_MV 3400 /*low battery warning at 3.4 V*/
#define VBAT_DIVIDER_NUM 2/* TBD: 1:1 divider, Vbat = 2 * Vpin, this i need ot check, since it chnages with design of amplifying method of input current sing*/
#define VBAT_DIVIDER_DEN 1
/*I SEE GAY!😧*/

/* Logging */
#define RECORD_SIZE 256 /*bytes per log record*/
#define RECORD_RING_LEN 16 /* TBD: 1.6 s at 10 Hz, byted held in RAM at a time, this si good for SD card write*/
/* needs cheking depending on SD card*/

/* Team architecture still open (2026-10). Recorded here so nobody forgets;
 * none of this is implemented yet. */
#define CAN_ENABLED 0 /*TBD: CAN between PCBs (Angelo/Dom), each board with its own small MCU; libcsp suggested on top. On the STM32G431, FDCAN1 is on PA11/PA12 (to confirm in the datasheet), which this pin map uses for SD chip select and the arm pin: pin map must change if CAN goes ahead*/
#define CAN_BITRATE 500000 /*TBD: placeholder*/
#define CAN_NODE_ID 0 /*TBD: this board's node id*/
#define PARTICULATE_FITTED 0 /*TBD: particulate sensor (not on CAN, Dom); needs a driver and log fields*/
/* Accelerations: the fitted sim peaks at 14.6 g along the axis (specific
 * force) and OpenRocket at about 133 m/s^2 (13.6 g, to confirm). The IMU is
 * set to +-32 g; electronics rated 16 g (Jack) leaves little margin. TBD. */

#endif /* PAYLOAD_CONFIG_H */
