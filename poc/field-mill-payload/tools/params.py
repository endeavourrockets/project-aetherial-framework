"""Galvan field mill payload: single source of design parameters for Python tools.

Every value here is a baseline assumption unless marked as sourced. The firmware
copy lives in firmware/Payload/app/config.h; tests/test_params_consistency.py
checks that the two agree.

Units are SI throughout (m, s, V, A, F, Pa, K) unless the name says otherwise.
"""
import math

EPS0 = 8.8541878128e-12          # F/m, vacuum permittivity

# --- Site and mission (sourced: IREC 2026 DTEG 6.3.2, 10.1.1; GVN-GEN-000) ---
SITE_ELEVATION_M = 890.0         # Midland launch site elevation used by ESRA
TARGET_APOGEE_AGL_M = 9144.0     # 30,000 ft AGL
LAUNCH_ELEVATION_DEG = 84.0      # typical single-stage rail elevation

# --- Ground test site (Adriana, 1 Oct 2026: "most likely Edinburgh, on a small hill") ---
# The hill is not named; ~150 m is assumed (Blackford Hill 164 m, next to King's
# Buildings, and Calton Hill 103 m are typical). Assumption A-24.
TEST_SITE_NAME = "Edinburgh, small hill (unspecified)"
TEST_SITE_ELEVATION_M = 150.0
TEST_SITE_LAT_DEG = 55.95
TEST_FAIR_WEATHER_VPM = (80.0, 200.0)   # typical UK flat-site range, urban air raises it
TEST_HILL_ENHANCEMENT = (1.2, 2.0)      # orographic factor at a rounded summit (estimate)
TEST_TEMP_C = (-2.0, 18.0)              # Oct to Apr outdoor range (approximate)
TEST_RH_PCT = (60.0, 100.0)
WIND_FAIR_MPS = (1.0, 8.0)              # Harrison and Nicoll (2018) fair-weather window

# --- Mill mechanics (assumed baseline) ---
N_APERTURES = 2                  # rotor openings (and electrode sectors), confirmed by Aaryan
ROTOR_RPS = 50.0                 # 3000 rpm
F_MOD_HZ = N_APERTURES * ROTOR_RPS   # 100 Hz modulation frequency
ELECTRODE_R_IN_M = 8.0e-3
ELECTRODE_R_OUT_M = 25.0e-3
# Fully exposed electrode area: two 90 degree sectors = half the annulus
A_SENSE_M2 = 0.5 * math.pi * (ELECTRODE_R_OUT_M**2 - ELECTRODE_R_IN_M**2)
K_GEOM_NOMINAL = 0.8             # fringing/geometry factor, replaced by calibration

# --- Stepper drive (assumed baseline) ---
STEPS_PER_REV = 20               # can-stack stepper, full steps
STEPS_PER_MOD_CYCLE = STEPS_PER_REV // N_APERTURES   # 10
STEP_HZ = STEPS_PER_REV * ROTOR_RPS                  # 1000 Hz

# --- Sampling and lock-in (assumed baseline) ---
SYSCLK_HZ = 16_000_000                 # HSI, like the team firmware.ioc (TBD: 170 MHz with the PLL)
M_SAMPLES = 80                   # ADC samples per modulation cycle
FS_HZ = M_SAMPLES * F_MOD_HZ     # 8000 Hz
TIM2_ARR = SYSCLK_HZ // int(FS_HZ) - 1               # 1999 (21249 at 170 MHz)
ADC_PER_STEP = M_SAMPLES // STEPS_PER_MOD_CYCLE      # 8 ADC triggers per step
K_BLOCKS = 10                    # modulation cycles per lock-in output
OUTPUT_HZ = F_MOD_HZ / K_BLOCKS  # 10 Hz
ADC_BITS = 12
ADC_OVERSAMPLE = 16              # hardware oversampling, no shift
ADC_FULL_SCALE = (2**ADC_BITS - 1) * ADC_OVERSAMPLE  # 65520
VREF_V = 3.3
V_PER_COUNT = VREF_V / ADC_FULL_SCALE

# --- Analogue front end: charge amplifier with two ranges (assumed baseline) ---
V_MID_V = 1.65                   # mid-rail bias
V_SWING_V = 1.5                  # usable +/- swing around mid-rail
CF_F = {"HI": 12e-12, "LO": 1.2e-9}   # E12 values; 10 pF clipped below 4 kV/m (triangle peak)
RF_OHM = {"HI": 1e9, "LO": 10e6} # DC bleed resistors (corner ~16 Hz both)


def sensitivity_v_per_vpm(gain="HI", k_geom=K_GEOM_NOMINAL, area=A_SENSE_M2):
    """Peak AC output voltage per V/m of field at the electrode (charge mode)."""
    return 0.5 * EPS0 * area * k_geom / CF_F[gain]


def full_scale_vpm(gain="HI"):
    """Largest field before the raw waveform reaches the 5 % rail margin.

    Clipping is set by the peak of the triangle waveform, which is
    0.5 * eps0 * A * E / Cf, not by its f_mod fundamental (smaller by 4/pi^2 / 0.5).
    """
    usable = V_MID_V - 0.05 * VREF_V
    return usable / (0.5 * EPS0 * A_SENSE_M2 / CF_F[gain])


def corner_hz(gain="HI"):
    return 1.0 / (2 * math.pi * RF_OHM[gain] * CF_F[gain])


# --- Vehicle geometry (sourced: Galvan Payload Software Reference whiteboard, read from photo) ---
BODY_OD_M = 0.1524               # assumed equal to 6 in tank tube; not confirmed
NOSE_LEN_M = 0.500
PA_BAY_LEN_M = 0.113
VEHICLE_LEN_M = 3.283            # 2883 mm stacked sections + 400 mm fin can (approx.)
PA_WINDOW_FROM_TIP_M = NOSE_LEN_M + PA_BAY_LEN_M / 2

# --- Payload envelope (sourced: R&R 2026 2.3; decision 2026-10-01) ---
PAYLOAD_CUBE_M = 0.100           # 1U
PAYLOAD_MIN_MASS_KG = 4.4 * 0.45359237
BALLAST_MAX_KG = 1.15 * 0.45359237

# --- State machine thresholds (assumed baseline) ---
LAUNCH_ACCEL_G = 3.0             # sustained for LAUNCH_DWELL_MS
LAUNCH_DWELL_MS = 300
BURNOUT_ACCEL_G = 0.5            # axial accel below this ends BOOST
APOGEE_DESCENT_M = 15.0          # altitude below max to declare apogee
APOGEE_DWELL_MS = 1000
LANDED_VSPEED_MPS = 2.0          # |v| below this ...
LANDED_DWELL_MS = 10_000         # ... for this long
LAUNCH_BARO_RISE_M = 50.0        # backup launch detection
BOOST_MAX_MS = 8000
APOGEE_LOCKOUT_MS = 32000       # TBD: fitted sim transonic until ~29.5 s after launch
APOGEE_TIMEOUT_MS = 70000       # TBD: OpenRocket apogee 51-53.5 s +-2 s
LANDED_TIMEOUT_MS = 900_000
# Mill deployment (team decision 2026-10 D1): motor off in SAFE, ARMED, BOOST
# and LANDED; mills extended once in COAST, never retracted in flight.
# PLACEHOLDERS until the solenoid actuator is chosen and tested.
DEPLOY_DELAY_MS = 1000           # COAST entry -> extend command
DEPLOY_PULSE_MS = 500            # solenoid energised
DEPLOY_CONFIRM_TIMEOUT_MS = 2000 # not extended by then -> motor stays off
DEPLOY_HAS_SENSOR = 1            # end-stop switch fitted (unknown yet)
MILL_SETTLE_MS = 500             # extended -> motor start
SAFE_LOG_PERIOD_MS = 1000        # 1 Hz sensor-only records with the motor off on the ground
FAST_LOG_PERIOD_MS = 100         # 10 Hz sensor-only records with the motor off in flight
SELFTEST_SPIN_RETRACTED = 1      # power-on self-test spins with the mills retracted

# Architecture still open (2026-10), TBD; mirrors config.h, not used yet
N_MILLS = 1                      # TBD: two motors mentioned by Dom
HW_LOCKIN_AMP = 0                # TBD: hardware lock-in amplifier?
CAN_ENABLED = 0                  # TBD: CAN between PCBs; FDCAN1 on PA11/PA12 clashes with the pin map
PARTICULATE_FITTED = 0           # TBD: particulate sensor

# Orientation filter (app/attitude.h)
ATT_KP = 1.0                     # TBD: accelerometer correction gain, 1/s
ATT_ACC_GATE_MG = 150            # accelerometer used only within 1 g +- this

# Full power loss in flight (team decision 2026-10 D4)
POWERLOSS_MIN_ALT_CM = 30000     # TBD: 300 m above saved pad pressure = in flight
POWERLOSS_DECIDE_MS = 1000       # look at baro + IMU before deciding
POWERLOSS_BARO_WAIT_MS = 3000    # no barometer by then -> unknown, mills stay in
NV_PAD_REFRESH_MS = 600000       # TBD: pad pressure rewritten every 10 min while armed
BURNOUT_DWELL_MS = 100
VBAT_LOW_V = 3.4
LOCKIN_QUEUE_LEN = 16            # outputs (1.6 s) between ISR and main loop
RECORD_SIZE = 256

if __name__ == "__main__":
    print(f"A_sense = {A_SENSE_M2*1e4:.3f} cm^2, f_mod = {F_MOD_HZ:.0f} Hz, fs = {FS_HZ:.0f} Hz")
    for g in ("HI", "LO"):
        print(f"{g}: S = {sensitivity_v_per_vpm(g)*1e3:.4f} mV/(V/m), "
              f"FS = +/-{full_scale_vpm(g):.0f} V/m, corner = {corner_hz(g):.1f} Hz")
    print(f"TIM2 ARR = {TIM2_ARR}, payload min mass = {PAYLOAD_MIN_MASS_KG:.3f} kg, "
          f"ballast max = {BALLAST_MAX_KG:.3f} kg")
