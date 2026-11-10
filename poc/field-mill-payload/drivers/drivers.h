/*
 * drivers.h - hardware interface used by the application layer.
 * firnware calls functions below, but never the hardware directly, 
 * app code run on the real board, and on PC simulation, or renode...
 * Two implementations exist:
 *   drivers/hal/  STM32 HAL (CubeMX handles), for the NUCLEO-G431KB and flight
 *                 board.
 *   drivers/sim/  synthetic sensors and a simulated flight, on my pc, used by the 
 *                 host tests and by the Renode build.
 *
 * Pin map (XLR-PAY-004) logged by firmware on boot, and used by HAL and renode builds/drivers:
 *   PA0  ADC1_IN1   field mill signal        PA1  ADC1_IN2  battery divider
 *   PA2  USART2_TX  console (ST-LINK VCP)    PA3  USART2_RX console
 *   PA4  GPIO out   range select (1 = LO)    PA8  TIM1_CH1  stepper STEP
 *   PA11 GPIO out   SD chip select           PA12 GPIO in   arm (pull-up, open = armed)
 *   PB0  GPIO out   motor driver nENABLE     PB3  SPI1_SCK  SD
 *   PB4  SPI1_MISO  SD                       PB5  SPI1_MOSI SD
 *   PB6  TIM4_CH1   rotor index (capture)    PB7  I2C1_SDA  baro + IMU
 *   PA15 I2C1_SCL   baro + IMU               PB8  GPIO out  LED LD2
 *   PA5, PA6: leave as floating inputs (bridged to PB7/PA15 on the Nucleo).
 *   TBD (2026-10): if the team adopts CAN between PCBs, FDCAN1 sits on PA11/PA12
 *   (to confirm), which clashes with SD CS (PA11) and the arm pin (PA12) above.
 * this is what i deciced, and I believe it works, it can chnage, but it wouold hurt my feelings
 * 
 * Pin map notes:
 *   ADC1_INx  analogue input channel x of ADC1
 *   GPIO      plain digital pin (on/off)
 *   TIMx_CHy  timer x, channel y: makes pulses (STEP) or measures them (capture)
 *   USART/SPI/I2C  serial buses: console / SD card / baro + IMU (shared bus)
 *   nENABLE   the "n" means active low: 0 = motor driver enabled
 *   arm pin   pull-up resistor holds it high; a jumper to ground pulls it
 *             low. Removing the jumper (remove before flight) = armed.
 *   PA5, PA6  on the Nucleo these are wired to PB7/PA15, so they must not
 *             drive anything or they would fight the I2C bus.
 * notes above for me since i forget what is what, :)
 */
#ifndef DRIVERS_H
#define DRIVERS_H

#include <stdbool.h>
#include <stdint.h>

/*time, watchdog, reset*/
void drv_time_init(void); /*start 1ms system tick*/
uint32_t drv_millis(void); /*ms since power-up*/
void drv_idle(void); /* sleep until the next interrupt */
void drv_watchdog_init(void); /*restart the reset timer, if stuck*/
void drv_watchdog_kick(void); /*if still alive, restart that timer*/
uint32_t drv_reset_cause(void); /* RCC_CSR snapshot (or sim value)*/

/*GPIO*/
void drv_gpio_init(void); /*set every pins mode, from map above*/
void drv_led_set(bool on); /*LED LD2 on PB8*/
bool drv_arm_switch_armed(void); /*true when arm jumper out, arming mechanism*/

/*console UART*/
void drv_uart_init(void); /* USART2, 115200 8N1 */
void drv_uart_write(const char *s, uint32_t n); /*sends n bytes*/
int drv_uart_getc(void); /* -1 if nothing received, else the byte 0...255 */

/*field mill ADC (timer-triggered, DMA, one callback per cycle)
*the driver calls payload_adc_cycle_isr() (payload.h) with M_SAMPLES
* oversampled counts for every modulation cycle while running. 
* timer triggered: TIM2 starts each conversion at exactly FS_HZ.
* DMA: hardware copies the results to memory without using CPU.*/
void drv_adc_init(void);
void drv_adc_start(void); /*begin field mill smapling*/
void drv_adc_stop(void); /*stop field mill sampling*/
uint16_t drv_adc_vbat_mv(void); /*battery voltage in mV, divider undone*/
int16_t drv_adc_mcu_temp_cc(void); /*MCU temp in celcius, from chip internal temp sensor*/

/*analogue front end*/
void drv_frontend_set_range(uint8_t range); /* this seen in other code fro RANGE_HI and LO, in config.h*
*drives PA4, also seen in map above
*/

/*shutter motor and index sensor*/
void drv_motor_init(void); /*initialize motor control*/
void drv_motor_start(void); /* begins the spin up ramp */
void drv_motor_stop(void); /* stops the motors, and siables the drivers*/
void drv_motor_tick(uint32_t now_ms); /* ramp and housekeeping */
bool drv_motor_at_speed(void); /*TRUE once at ROTOR_RPS*/
uint32_t drv_motor_slip_count(void); /*times stepper lost a step/s */
uint16_t drv_motor_rev_period_us(void); /* 0 if no recent index pulse */
/*time for one turn 20000 at 50 rev/s*/

/*ADC sample number (0..M_SAMPLES-1) within the modulation cycle at which
*the last index pulse arrived. Resolves the stepper
*sort of tells the lock.in wher ethe holes are relative to samples, so sign and 
*size of field come out good
*/
uint8_t  drv_motor_index_phase(void); /*false is sensor does not respond*/

/*mill deployment actuator (solenoid, team decision 2026-10 D1; pins TBD)
*the mills are extended once in COAST and never retracted in flight.
*/
void drv_deploy_init(void); /*configure pins; does not move the actuator*/
void drv_deploy_extend(void); /*energise for DEPLOY_PULSE_MS (released by drv_deploy_tick)*/
void drv_deploy_retract(void); /*bench/ground use only, never called in flight*/
void drv_deploy_tick(uint32_t now_ms); /*ends the solenoid pulse*/
/*true when the mills are out: end-stop switch if DEPLOY_HAS_SENSOR,
*otherwise "extend commanded and pulse finished"*/
bool drv_deploy_is_extended(void);

/*non-volatile store that survives a power cut (team decision 2026-10 D4):
*one small record (app/powerloss.h nv_state_t) in the last 2 KB flash page on
*the board; a static buffer in the sim. Written only at a few events, so flash
*wear is negligible. Returns false if nothing valid could be read / written.
*/
bool drv_nv_read(void *buf, uint32_t len);
bool drv_nv_write(const void *buf, uint32_t len);

/*barometer (MS5611), dont know it, read it somewhere, assumed, if needs changing i can flag it in my code */
bool drv_baro_init(void);
/*Advance the D1/D2 conversion cycle; returns true when a new compensated
*compensated -->corrected with the chip's factory calibration. 
*results written through two pointers below.
*/
bool drv_baro_poll(uint32_t now_ms, int32_t *p_pa, int16_t *temp_cc);

/*IMU (ICM-45686)*/
bool drv_imu_init(void); /*arrays passed as pointers, 
*so fucntion writes to callers array
*/
bool drv_imu_read(int16_t accel_mg[3], int16_t gyro_dps10[3]);

/*storage (microSD, FatFs)*/
bool drv_storage_init(void); /*mount card FAT file system*/
/* create the next unused log file and write its name into name[]:*/
bool drv_storage_open_new(char name[13]);
bool drv_storage_write(const uint8_t *buf, uint32_t len); /*append bytes*/
bool drv_storage_sync(void); /*forced buffered data onto SD*/
#endif /* DRIVERS_H */

/*this is mainly for node, so that i can test code, and functions and if successful, the same is true for the board*/
/* ( -_•)ᡕᠵデᡁ᠊╾━💥  */