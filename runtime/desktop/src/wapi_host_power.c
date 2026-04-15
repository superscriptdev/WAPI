/* Stub -- power subsystem not yet implemented for desktop.
 *
 * Unified handler for wapi_power: battery/source info, wake lock,
 * user idle detection, OS saver mode, and thermal state. All
 * callbacks return WAPI_ERR_NOTSUP until a real implementation
 * lands. See include/wapi/wapi_power.h for the ABI. */

#include "wapi_host.h"

#define WAPI_POWER_STUB(name)                                               \
    static wasm_trap_t* cb_power_##name(void* env, wasmtime_caller_t* caller,\
        const wasmtime_val_t* args, size_t nargs,                           \
        wasmtime_val_t* results, size_t nresults) {                         \
        (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;   \
        WAPI_RET_I32(WAPI_ERR_NOTSUP);                                      \
        return NULL;                                                        \
    }

WAPI_POWER_STUB(get_info)      /* (i32 out_ptr) -> i32 */
WAPI_POWER_STUB(wake_acquire)  /* (i32 type, i32 out_lock) -> i32 */
WAPI_POWER_STUB(wake_release)  /* (i32 lock) -> i32 */
WAPI_POWER_STUB(idle_start)    /* (i32 threshold_ms) -> i32 */
WAPI_POWER_STUB(idle_stop)     /* () -> i32 */
WAPI_POWER_STUB(idle_get)      /* (i32 out_state) -> i32 */
WAPI_POWER_STUB(saver_get)     /* (i32 out_state) -> i32 */
WAPI_POWER_STUB(thermal_get)   /* (i32 out_state) -> i32 */

void wapi_host_register_power(wasmtime_linker_t* linker) {
    WAPI_DEFINE_1_1(linker, "wapi_power", "get_info",     cb_power_get_info);
    WAPI_DEFINE_2_1(linker, "wapi_power", "wake_acquire", cb_power_wake_acquire);
    WAPI_DEFINE_1_1(linker, "wapi_power", "wake_release", cb_power_wake_release);
    WAPI_DEFINE_1_1(linker, "wapi_power", "idle_start",   cb_power_idle_start);
    WAPI_DEFINE_0_1(linker, "wapi_power", "idle_stop",    cb_power_idle_stop);
    WAPI_DEFINE_1_1(linker, "wapi_power", "idle_get",     cb_power_idle_get);
    WAPI_DEFINE_1_1(linker, "wapi_power", "saver_get",    cb_power_saver_get);
    WAPI_DEFINE_1_1(linker, "wapi_power", "thermal_get",  cb_power_thermal_get);
}
