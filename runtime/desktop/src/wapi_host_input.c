/**
 * WAPI Desktop Runtime - Input State & Event Production
 *
 * Implements wapi_input.* state-query imports using SDL3.
 * Also converts SDL events to WAPI event format and pushes them
 * onto the shared event queue (consumed by wapi_host_event.c).
 *
 * For event queue polling (poll/wait/flush), see wapi_host_event.c.
 */

#include "wapi_host.h"

/* ============================================================
 * SDL Window ID -> WAPI Surface Handle mapping
 * ============================================================ */

static int32_t find_surface_handle_for_sdl_window(SDL_WindowID window_id) {
    SDL_Window* window = SDL_GetWindowFromID(window_id);
    if (!window) return 0;

    for (int i = 1; i < WAPI_MAX_HANDLES; i++) {
        if (g_rt.handles[i].type == WAPI_HTYPE_SURFACE &&
            g_rt.handles[i].data.window == window) {
            return (int32_t)i;
        }
    }
    return 0;
}

/* ============================================================
 * SDL -> WAPI Event Conversion
 * ============================================================
 * Called from the main loop to convert SDL events to WAPI format
 * and push them onto the event queue.
 */

void wapi_input_process_sdl_event(const SDL_Event* sdl_ev) {
    wapi_host_event_t ev;
    memset(&ev, 0, sizeof(ev));

    /* Event header pointers into our blob */
    uint32_t* type_ptr       = (uint32_t*)(ev.data + 0);
    uint32_t* surface_id_ptr = (uint32_t*)(ev.data + 4);
    uint64_t* timestamp_ptr  = (uint64_t*)(ev.data + 8);

    switch (sdl_ev->type) {
    case SDL_EVENT_QUIT:
        *type_ptr = 0x100; /* WAPI_EVENT_QUIT */
        *timestamp_ptr = SDL_GetTicksNS();
        wapi_event_queue_push(&ev);
        g_rt.running = false;
        return;

    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        *type_ptr = 0x0201; /* WAPI_EVENT_SURFACE_CLOSE */
        *surface_id_ptr = (uint32_t)find_surface_handle_for_sdl_window(sdl_ev->window.windowID);
        *timestamp_ptr = SDL_GetTicksNS();
        wapi_event_queue_push(&ev);
        return;

    case SDL_EVENT_WINDOW_RESIZED: {
        *type_ptr = 0x0200; /* WAPI_EVENT_SURFACE_RESIZED */
        *surface_id_ptr = (uint32_t)find_surface_handle_for_sdl_window(sdl_ev->window.windowID);
        *timestamp_ptr = SDL_GetTicksNS();
        /* data1 = width, data2 = height at offset 16, 20 */
        int32_t w = sdl_ev->window.data1;
        int32_t h = sdl_ev->window.data2;
        memcpy(ev.data + 16, &w, 4);
        memcpy(ev.data + 20, &h, 4);
        wapi_event_queue_push(&ev);
        return;
    }

    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        *type_ptr = 0x0202;
        *surface_id_ptr = (uint32_t)find_surface_handle_for_sdl_window(sdl_ev->window.windowID);
        *timestamp_ptr = SDL_GetTicksNS();
        wapi_event_queue_push(&ev);
        return;

    case SDL_EVENT_WINDOW_FOCUS_LOST:
        *type_ptr = 0x0203;
        *surface_id_ptr = (uint32_t)find_surface_handle_for_sdl_window(sdl_ev->window.windowID);
        *timestamp_ptr = SDL_GetTicksNS();
        wapi_event_queue_push(&ev);
        return;

    case SDL_EVENT_WINDOW_SHOWN:
        *type_ptr = 0x0204;
        *surface_id_ptr = (uint32_t)find_surface_handle_for_sdl_window(sdl_ev->window.windowID);
        *timestamp_ptr = SDL_GetTicksNS();
        wapi_event_queue_push(&ev);
        return;

    case SDL_EVENT_WINDOW_HIDDEN:
        *type_ptr = 0x0205;
        *surface_id_ptr = (uint32_t)find_surface_handle_for_sdl_window(sdl_ev->window.windowID);
        *timestamp_ptr = SDL_GetTicksNS();
        wapi_event_queue_push(&ev);
        return;

    case SDL_EVENT_WINDOW_MINIMIZED:
        *type_ptr = 0x0206;
        *surface_id_ptr = (uint32_t)find_surface_handle_for_sdl_window(sdl_ev->window.windowID);
        *timestamp_ptr = SDL_GetTicksNS();
        wapi_event_queue_push(&ev);
        return;

    case SDL_EVENT_WINDOW_MAXIMIZED:
        *type_ptr = 0x0207;
        *surface_id_ptr = (uint32_t)find_surface_handle_for_sdl_window(sdl_ev->window.windowID);
        *timestamp_ptr = SDL_GetTicksNS();
        wapi_event_queue_push(&ev);
        return;

    case SDL_EVENT_WINDOW_RESTORED:
        *type_ptr = 0x0208;
        *surface_id_ptr = (uint32_t)find_surface_handle_for_sdl_window(sdl_ev->window.windowID);
        *timestamp_ptr = SDL_GetTicksNS();
        wapi_event_queue_push(&ev);
        return;

    case SDL_EVENT_WINDOW_MOVED: {
        *type_ptr = 0x0209;
        *surface_id_ptr = (uint32_t)find_surface_handle_for_sdl_window(sdl_ev->window.windowID);
        *timestamp_ptr = SDL_GetTicksNS();
        int32_t x = sdl_ev->window.data1;
        int32_t y = sdl_ev->window.data2;
        memcpy(ev.data + 16, &x, 4);
        memcpy(ev.data + 20, &y, 4);
        wapi_event_queue_push(&ev);
        return;
    }

    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        /*
         * wapi_keyboard_event_t layout:
         *   +0:  u32 type
         *   +4:  u32 surface_id
         *   +8:  u64 timestamp
         *  +16:  u32 scancode
         *  +20:  u32 keycode
         *  +24:  u16 mod
         *  +26:  u8  down
         *  +27:  u8  repeat
         */
        *type_ptr = (sdl_ev->type == SDL_EVENT_KEY_DOWN) ? 0x300 : 0x301;
        *surface_id_ptr = (uint32_t)find_surface_handle_for_sdl_window(sdl_ev->key.windowID);
        *timestamp_ptr = SDL_GetTicksNS();

        /* SDL3 scancodes are USB HID based, same as WAPI scancodes */
        uint32_t scancode = (uint32_t)sdl_ev->key.scancode;
        uint32_t keycode = (uint32_t)sdl_ev->key.key;
        memcpy(ev.data + 16, &scancode, 4);
        memcpy(ev.data + 20, &keycode, 4);

        /* Map SDL mod flags to WAPI mod flags */
        uint16_t mod = 0;
        SDL_Keymod sdl_mod = sdl_ev->key.mod;
        if (sdl_mod & SDL_KMOD_LSHIFT) mod |= 0x0001;
        if (sdl_mod & SDL_KMOD_RSHIFT) mod |= 0x0002;
        if (sdl_mod & SDL_KMOD_LCTRL)  mod |= 0x0040;
        if (sdl_mod & SDL_KMOD_RCTRL)  mod |= 0x0080;
        if (sdl_mod & SDL_KMOD_LALT)   mod |= 0x0100;
        if (sdl_mod & SDL_KMOD_RALT)   mod |= 0x0200;
        if (sdl_mod & SDL_KMOD_LGUI)   mod |= 0x0400;
        if (sdl_mod & SDL_KMOD_RGUI)   mod |= 0x0800;
        if (sdl_mod & SDL_KMOD_CAPS)   mod |= 0x2000;
        if (sdl_mod & SDL_KMOD_NUM)    mod |= 0x1000;
        memcpy(ev.data + 24, &mod, 2);

        uint8_t down = sdl_ev->key.down ? 1 : 0;
        uint8_t repeat = sdl_ev->key.repeat ? 1 : 0;
        ev.data[26] = down;
        ev.data[27] = repeat;

        /* Update keyboard state */
        if (scancode < 256) {
            g_rt.key_state[scancode] = (down != 0);
        }
        g_rt.mod_state = mod;

        wapi_event_queue_push(&ev);
        return;
    }

    case SDL_EVENT_TEXT_INPUT: {
        /*
         * wapi_text_input_event_t layout:
         *  +0:  u32 type
         *  +4:  u32 surface_id
         *  +8:  u64 timestamp
         * +16:  char text[32]
         */
        *type_ptr = 0x302; /* WAPI_EVENT_TEXT_INPUT */
        *surface_id_ptr = (uint32_t)find_surface_handle_for_sdl_window(sdl_ev->text.windowID);
        *timestamp_ptr = SDL_GetTicksNS();

        const char* text = sdl_ev->text.text;
        size_t len = strlen(text);
        if (len > 31) len = 31;
        memcpy(ev.data + 16, text, len);
        ev.data[16 + len] = '\0';

        wapi_event_queue_push(&ev);
        return;
    }

    case SDL_EVENT_MOUSE_MOTION: {
        /*
         * wapi_mouse_motion_event_t layout:
         *  +0:  u32 type
         *  +4:  u32 surface_id
         *  +8:  u64 timestamp
         * +16:  u32 mouse_id
         * +20:  u32 button_state
         * +24:  f32 x
         * +28:  f32 y
         * +32:  f32 xrel
         * +36:  f32 yrel
         */
        *type_ptr = 0x400; /* WAPI_EVENT_MOUSE_MOTION */
        *surface_id_ptr = (uint32_t)find_surface_handle_for_sdl_window(sdl_ev->motion.windowID);
        *timestamp_ptr = SDL_GetTicksNS();

        uint32_t mouse_id = (uint32_t)sdl_ev->motion.which;
        uint32_t button_state = (uint32_t)sdl_ev->motion.state;
        float x = sdl_ev->motion.x;
        float y = sdl_ev->motion.y;
        float xrel = sdl_ev->motion.xrel;
        float yrel = sdl_ev->motion.yrel;

        memcpy(ev.data + 16, &mouse_id, 4);
        memcpy(ev.data + 20, &button_state, 4);
        memcpy(ev.data + 24, &x, 4);
        memcpy(ev.data + 28, &y, 4);
        memcpy(ev.data + 32, &xrel, 4);
        memcpy(ev.data + 36, &yrel, 4);

        g_rt.mouse_x = x;
        g_rt.mouse_y = y;

        wapi_event_queue_push(&ev);
        return;
    }

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        /*
         * wapi_mouse_button_event_t layout:
         *  +0:  u32 type
         *  +4:  u32 surface_id
         *  +8:  u64 timestamp
         * +16:  u32 mouse_id
         * +20:  u8  button
         * +21:  u8  down
         * +22:  u8  clicks
         * +23:  u8  _pad
         * +24:  f32 x
         * +28:  f32 y
         */
        *type_ptr = (sdl_ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN) ? 0x401 : 0x402;
        *surface_id_ptr = (uint32_t)find_surface_handle_for_sdl_window(sdl_ev->button.windowID);
        *timestamp_ptr = SDL_GetTicksNS();

        uint32_t mouse_id = (uint32_t)sdl_ev->button.which;
        memcpy(ev.data + 16, &mouse_id, 4);
        ev.data[20] = sdl_ev->button.button;
        ev.data[21] = sdl_ev->button.down ? 1 : 0;
        ev.data[22] = sdl_ev->button.clicks;
        ev.data[23] = 0;

        float x = sdl_ev->button.x;
        float y = sdl_ev->button.y;
        memcpy(ev.data + 24, &x, 4);
        memcpy(ev.data + 28, &y, 4);

        /* Update mouse button state */
        if (sdl_ev->button.down) {
            g_rt.mouse_buttons |= (1u << sdl_ev->button.button);
        } else {
            g_rt.mouse_buttons &= ~(1u << sdl_ev->button.button);
        }

        wapi_event_queue_push(&ev);
        return;
    }

    case SDL_EVENT_MOUSE_WHEEL: {
        /*
         * wapi_mouse_wheel_event_t layout:
         *  +0:  u32 type
         *  +4:  u32 surface_id
         *  +8:  u64 timestamp
         * +16:  u32 mouse_id
         * +20:  u32 _pad
         * +24:  f32 x
         * +28:  f32 y
         */
        *type_ptr = 0x403; /* WAPI_EVENT_MOUSE_WHEEL */
        *surface_id_ptr = (uint32_t)find_surface_handle_for_sdl_window(sdl_ev->wheel.windowID);
        *timestamp_ptr = SDL_GetTicksNS();

        uint32_t mouse_id = (uint32_t)sdl_ev->wheel.which;
        memcpy(ev.data + 16, &mouse_id, 4);
        uint32_t pad = 0;
        memcpy(ev.data + 20, &pad, 4);

        float x = sdl_ev->wheel.x;
        float y = sdl_ev->wheel.y;
        memcpy(ev.data + 24, &x, 4);
        memcpy(ev.data + 28, &y, 4);

        wapi_event_queue_push(&ev);
        return;
    }

    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_MOTION: {
        /*
         * wapi_touch_event_t layout:
         *  +0:  u32 type
         *  +4:  u32 surface_id
         *  +8:  u64 timestamp
         * +16:  u64 touch_id
         * +24:  u64 finger_id
         * +32:  f32 x
         * +36:  f32 y
         * +40:  f32 dx
         * +44:  f32 dy
         * +48:  f32 pressure
         */
        uint32_t wapi_type;
        switch (sdl_ev->type) {
        case SDL_EVENT_FINGER_DOWN:   wapi_type = 0x700; break;
        case SDL_EVENT_FINGER_UP:     wapi_type = 0x701; break;
        default:                      wapi_type = 0x702; break;
        }
        *type_ptr = wapi_type;
        *surface_id_ptr = (uint32_t)find_surface_handle_for_sdl_window(sdl_ev->tfinger.windowID);
        *timestamp_ptr = SDL_GetTicksNS();

        uint64_t touch_id = (uint64_t)sdl_ev->tfinger.touchID;
        uint64_t finger_id = (uint64_t)sdl_ev->tfinger.fingerID;
        memcpy(ev.data + 16, &touch_id, 8);
        memcpy(ev.data + 24, &finger_id, 8);

        float x = sdl_ev->tfinger.x;
        float y = sdl_ev->tfinger.y;
        float dx = sdl_ev->tfinger.dx;
        float dy = sdl_ev->tfinger.dy;
        float pressure = sdl_ev->tfinger.pressure;
        memcpy(ev.data + 32, &x, 4);
        memcpy(ev.data + 36, &y, 4);
        memcpy(ev.data + 40, &dx, 4);
        memcpy(ev.data + 44, &dy, 4);
        memcpy(ev.data + 48, &pressure, 4);

        wapi_event_queue_push(&ev);
        return;
    }

    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
        *type_ptr = 0x652; /* WAPI_EVENT_GAMEPAD_AXIS */
        *timestamp_ptr = SDL_GetTicksNS();

        uint32_t gpad_id = (uint32_t)sdl_ev->gaxis.which;
        memcpy(ev.data + 16, &gpad_id, 4);
        ev.data[20] = (uint8_t)sdl_ev->gaxis.axis;
        ev.data[21] = 0;
        ev.data[22] = 0;
        ev.data[23] = 0;
        int16_t val = sdl_ev->gaxis.value;
        memcpy(ev.data + 24, &val, 2);

        wapi_event_queue_push(&ev);
        return;
    }

    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
        *type_ptr = (sdl_ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) ? 0x653 : 0x654;
        *timestamp_ptr = SDL_GetTicksNS();

        uint32_t gpad_id = (uint32_t)sdl_ev->gbutton.which;
        memcpy(ev.data + 16, &gpad_id, 4);
        ev.data[20] = (uint8_t)sdl_ev->gbutton.button;
        ev.data[21] = sdl_ev->gbutton.down ? 1 : 0;

        wapi_event_queue_push(&ev);
        return;
    }

    case SDL_EVENT_GAMEPAD_ADDED: {
        *type_ptr = 0x650;
        *timestamp_ptr = SDL_GetTicksNS();
        uint32_t gpad_id = (uint32_t)sdl_ev->gdevice.which;
        memcpy(ev.data + 16, &gpad_id, 4);
        wapi_event_queue_push(&ev);
        return;
    }

    case SDL_EVENT_GAMEPAD_REMOVED: {
        *type_ptr = 0x651;
        *timestamp_ptr = SDL_GetTicksNS();
        uint32_t gpad_id = (uint32_t)sdl_ev->gdevice.which;
        memcpy(ev.data + 16, &gpad_id, 4);
        wapi_event_queue_push(&ev);
        return;
    }

    default:
        /* Unhandled SDL event type, skip */
        break;
    }
}

/* ============================================================
 * Host Callbacks for Wasm imports (module: "wapi_input")
 * ============================================================ */

/* key_pressed: (i32 scancode) -> i32 */
static wasm_trap_t* host_input_key_pressed(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t scancode = WAPI_ARG_I32(0);
    if (scancode < 0 || scancode >= 256) {
        WAPI_RET_I32(0);
        return NULL;
    }
    WAPI_RET_I32(g_rt.key_state[scancode] ? 1 : 0);
    return NULL;
}

/* get_mod_state: () -> i32 */
static wasm_trap_t* host_input_get_mod_state(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32((int32_t)g_rt.mod_state);
    return NULL;
}

/* mouse_position: (i32 surface, i32 x_ptr, i32 y_ptr) -> i32 */
static wasm_trap_t* host_input_mouse_position(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    uint32_t x_ptr = WAPI_ARG_U32(1);
    uint32_t y_ptr = WAPI_ARG_U32(2);

    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    float x, y;
    SDL_GetMouseState(&x, &y);
    wapi_wasm_write_f32(x_ptr, x);
    wapi_wasm_write_f32(y_ptr, y);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* mouse_button_state: () -> i32 */
static wasm_trap_t* host_input_mouse_button_state(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args;
    WAPI_RET_I32((int32_t)g_rt.mouse_buttons);
    return NULL;
}

/* set_relative_mouse: (i32 surface, i32 enabled) -> i32 */
static wasm_trap_t* host_input_set_relative_mouse(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    int32_t enabled = WAPI_ARG_I32(1);

    if (!wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) {
        WAPI_RET_I32(WAPI_ERR_BADF);
        return NULL;
    }

    SDL_SetWindowRelativeMouseMode(g_rt.handles[h].data.window, enabled ? true : false);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* start_text_input: (i32 surface) -> void */
static wasm_trap_t* host_input_start_text_input(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    if (wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) {
        SDL_StartTextInput(g_rt.handles[h].data.window);
    }
    return NULL;
}

/* stop_text_input: (i32 surface) -> void */
static wasm_trap_t* host_input_stop_text_input(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    int32_t h = WAPI_ARG_I32(0);
    if (wapi_handle_valid(h, WAPI_HTYPE_SURFACE)) {
        SDL_StopTextInput(g_rt.handles[h].data.window);
    }
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

void wapi_host_register_input(wasmtime_linker_t* linker) {
    WAPI_DEFINE_1_1(linker, "wapi_input", "key_pressed",        host_input_key_pressed);
    WAPI_DEFINE_0_1(linker, "wapi_input", "get_mod_state",      host_input_get_mod_state);
    WAPI_DEFINE_3_1(linker, "wapi_input", "mouse_position",     host_input_mouse_position);
    WAPI_DEFINE_0_1(linker, "wapi_input", "mouse_button_state", host_input_mouse_button_state);
    WAPI_DEFINE_2_1(linker, "wapi_input", "set_relative_mouse", host_input_set_relative_mouse);
    WAPI_DEFINE_1_0(linker, "wapi_input", "start_text_input",   host_input_start_text_input);
    WAPI_DEFINE_1_0(linker, "wapi_input", "stop_text_input",    host_input_stop_text_input);
}
