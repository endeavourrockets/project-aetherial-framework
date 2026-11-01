#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { CHECK_PENDING, CHECK_READY, CHECK_FAILED } check_result_t;
typedef enum { BOOT_CHECKING, BOOT_COMPLETE, BOOT_FAULT } boot_state_t;

enum {
    CAP_CONFIG    = 1u << 0,
    CAP_CAN       = 1u << 1,
    CAP_SENSORS   = 1u << 2,
    CAP_LOGGING   = 1u << 3,
    CAP_TELEMETRY = 1u << 4
};

/* Contract: set local outputs inactive without homing/retraction. Also applies
 * before HAL/peripheral setup, using board-specific earliest-safe code.
 * Hardware defaults must cover the time before software can execute.
 */
extern void board_inhibit_experiment(void);

/* poll functions must be nonblocking, bounded and safe to call repeatedly.
 * First call starts an operation; later calls observe its completion.
 * They must never enable experimental loads. Cancel must stop outstanding
 * activity so a timed-out operation cannot later declare itself ready.
 */
extern check_result_t configuration_poll(void);
extern check_result_t can_readiness_poll(void);
extern check_result_t sensors_poll(void);
extern check_result_t logging_poll(void);
extern check_result_t telemetry_poll(void);
extern void configuration_cancel(void);
extern void can_readiness_cancel(void);
extern void sensors_cancel(void);
extern void logging_cancel(void);
extern void telemetry_cancel(void);

typedef struct {
    check_result_t (*poll)(void);
    void (*cancel)(void);
    uint32_t capability;
    uint32_t required_capabilities;
    uint32_t timeout_ms;
    bool failure_blocks_boot;
} boot_step_t;

/* Example policies/deadlines only. Here invalid configuration blocks boot;
 * unavailable device services are reported for a separate policy to consume.
 * Split grouped services into individual devices in a real implementation.
 */
static const boot_step_t steps[] = {
    {configuration_poll, configuration_cancel, CAP_CONFIG, 0u, 100u, true},
    {can_readiness_poll, can_readiness_cancel, CAP_CAN, CAP_CONFIG, 2000u, false},
    {sensors_poll, sensors_cancel, CAP_SENSORS, CAP_CONFIG, 1000u, false},
    {logging_poll, logging_cancel, CAP_LOGGING, CAP_CONFIG, 1500u, false},
    {telemetry_poll, telemetry_cancel, CAP_TELEMETRY, CAP_CONFIG, 1000u, false}
};

#define STEP_COUNT (sizeof steps / sizeof steps[0])

typedef struct {
    boot_state_t state;
    size_t step;
    uint32_t step_started_ms;
    uint32_t capabilities;
    uint32_t failed;
    uint32_t timed_out;
    uint32_t skipped;
} boot_context_t;

void boot_begin(boot_context_t *ctx, uint32_t now_ms)
{
    board_inhibit_experiment();
    *ctx = (boot_context_t){
        .state = BOOT_CHECKING,
        .step_started_ms = now_ms
    };
}

void boot_tick(boot_context_t *ctx, uint32_t now_ms)
{
    if (ctx->state != BOOT_CHECKING) {
        return;
    }

    const boot_step_t *step = &steps[ctx->step];
    check_result_t result;
    if ((ctx->capabilities & step->required_capabilities) !=
        step->required_capabilities) {
        ctx->skipped |= step->capability;
        result = CHECK_FAILED;
    } else if ((uint32_t)(now_ms - ctx->step_started_ms) >= step->timeout_ms) {
        /* Unsigned elapsed-time arithmetic tolerates a timer wrap. Caller must
         * run regularly; intervals must be much shorter than a full wrap.
         */
        step->cancel();
        ctx->timed_out |= step->capability;
        result = CHECK_FAILED;
    } else {
        result = step->poll();
    }

    if (result == CHECK_PENDING) {
        return;
    }
    if (result == CHECK_READY) {
        ctx->capabilities |= step->capability;
    } else {
        step->cancel(); /* Contract: safe and idempotent, even before start. */
        ctx->failed |= step->capability;
        if (step->failure_blocks_boot) {
            board_inhibit_experiment();
            ctx->state = BOOT_FAULT;
            return;
        }
    }

    ++ctx->step;
    ctx->step_started_ms = now_ms;
    if (ctx->step == STEP_COUNT) {
        ctx->state = BOOT_COMPLETE;
    }
}
