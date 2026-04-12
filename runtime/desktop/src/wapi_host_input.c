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
 * Pointer Event Synthesis Helper
 * ============================================================
 * Builds a wapi_pointer_event_t (72 bytes) inside a 128-byte
 * event blob and pushes it to the queue.  Called after the
 * device-specific event has already been pushed.
 */

static void push_pointer_event(
    uint32_t pointer_event_type,
    uint32_t surface_id,
    int32_t  pointer_id,
    uint8_t  pointer_type,
    uint8_t  button,
    uint8_t  buttons,
    float x, float y,
    float dx, float dy,
    float pressure,
    float tilt_x, float tilt_y,
    float twist,
    float width, float height)
{
    wapi_host_event_t ev;
    memset(&ev, 0, sizeof(ev));

    uint64_t ts = SDL_GetTicksNS();
    memcpy(ev.data +  0, &pointer_event_type, 4);
    memcpy(ev.data +  4, &surface_id, 4);
    memcpy(ev.data +  8, &ts, 8);
    memcpy(ev.data + 16, &pointer_id, 4);
    ev.data[20] = pointer_type;
    ev.data[21] = button;
    ev.data[22] = buttons;
    ev.data[23] = 0;
    memcpy(ev.data + 24, &x, 4);
    memcpy(ev.data + 28, &y, 4);
    memcpy(ev.data + 32, &dx, 4);
    memcpy(ev.data + 36, &dy, 4);
    memcpy(ev.data + 40, &pressure, 4);
    memcpy(ev.data + 44, &tilt_x, 4);
    memcpy(ev.data + 48, &tilt_y, 4);
    memcpy(ev.data + 52, &twist, 4);
    memcpy(ev.data + 56, &width, 4);
    memcpy(ev.data + 60, &height, 4);

    wapi_event_queue_push(&ev);

    /* Update aggregate pointer state */
    g_rt.pointer_x = x;
    g_rt.pointer_y = y;
    g_rt.pointer_buttons = (uint32_t)buttons;
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

        /* Synthesize pointer event */
        push_pointer_event(0x902 /* POINTER_MOTION */, *surface_id_ptr,
            0, 0 /* MOUSE */, 0, (uint8_t)(button_state & 0xFF),
            x, y, xrel, yrel,
            button_state ? 0.5f : 0.0f,
            0, 0, 0, 1.0f, 1.0f);
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

        /* Synthesize pointer event */
        push_pointer_event(
            sdl_ev->button.down ? 0x900 /* POINTER_DOWN */ : 0x901 /* POINTER_UP */,
            *surface_id_ptr, 0, 0 /* MOUSE */,
            sdl_ev->button.button, (uint8_t)(g_rt.mouse_buttons & 0xFF),
            x, y, 0, 0,
            sdl_ev->button.down ? 0.5f : 0.0f,
            0, 0, 0, 1.0f, 1.0f);
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
         * +32:  f32 x          (surface pixels)
         * +36:  f32 y          (surface pixels)
         * +40:  f32 dx         (surface pixels)
         * +44:  f32 dy         (surface pixels)
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

        /* Convert SDL normalized 0..1 coords to surface pixels */
        float nx = sdl_ev->tfinger.x;
        float ny = sdl_ev->tfinger.y;
        float ndx = sdl_ev->tfinger.dx;
        float ndy = sdl_ev->tfinger.dy;
        float pressure = sdl_ev->tfinger.pressure;

        float x = nx, y = ny, dx = ndx, dy = ndy;
        SDL_Window* win = SDL_GetWindowFromID(sdl_ev->tfinger.windowID);
        if (win) {
            int w, h;
            SDL_GetWindowSize(win, &w, &h);
            x  = nx  * (float)w;  y  = ny  * (float)h;
            dx = ndx * (float)w;  dy = ndy * (float)h;
        }

        memcpy(ev.data + 32, &x, 4);
        memcpy(ev.data + 36, &y, 4);
        memcpy(ev.data + 40, &dx, 4);
        memcpy(ev.data + 44, &dy, 4);
        memcpy(ev.data + 48, &pressure, 4);

        wapi_event_queue_push(&ev);

        /* Synthesize pointer event (same surface-pixel coords) */
        {
            uint32_t ptr_type;
            switch (sdl_ev->type) {
            case SDL_EVENT_FINGER_DOWN: ptr_type = 0x900; break;
            case SDL_EVENT_FINGER_UP:   ptr_type = 0x901; break;
            default:                    ptr_type = 0x902; break;
            }
            int32_t ptr_id = (int32_t)((finger_id & 0x7FFFFFFF) + 1);
            uint8_t btn_state = (sdl_ev->type != SDL_EVENT_FINGER_UP) ? 1 : 0;
            push_pointer_event(ptr_type, *surface_id_ptr,
                ptr_id, 1 /* TOUCH */,
                1 /* LEFT */, btn_state,
                x, y, dx, dy,
                pressure, 0, 0, 0, 1.0f, 1.0f);
        }
        return;
    }

    case SDL_EVENT_PEN_DOWN:
    case SDL_EVENT_PEN_UP:
    case SDL_EVENT_PEN_MOTION: {
        /*
         * wapi_pen_event_t layout:
         *  +0:  u32 type
         *  +4:  u32 surface_id
         *  +8:  u64 timestamp
         * +16:  u32 pen_handle
         * +20:  u8  tool_type
         * +21:  u8  button
         * +22:  u8  _pad[2]
         * +24:  f32 x
         * +28:  f32 y
         * +32:  f32 pressure
         * +36:  f32 tilt_x
         * +40:  f32 tilt_y
         * +44:  f32 twist
         * +48:  f32 distance
         */
        uint32_t pen_type;
        switch (sdl_ev->type) {
        case SDL_EVENT_PEN_DOWN:   pen_type = 0x800; break;
        case SDL_EVENT_PEN_UP:     pen_type = 0x801; break;
        default:                   pen_type = 0x802; break;
        }
        *type_ptr = pen_type;
        *surface_id_ptr = (uint32_t)find_surface_handle_for_sdl_window(sdl_ev->ptouch.windowID);
        *timestamp_ptr = SDL_GetTicksNS();

        uint32_t pen_handle = (uint32_t)sdl_ev->ptouch.which;
        memcpy(ev.data + 16, &pen_handle, 4);
        ev.data[20] = 0; /* tool_type: PEN */
        ev.data[21] = 0; /* button */
        ev.data[22] = 0;
        ev.data[23] = 0;

        float pen_x = sdl_ev->ptouch.x;
        float pen_y = sdl_ev->ptouch.y;
        float pen_pressure = sdl_ev->ptouch.pressure;
        float pen_tilt_x = 0, pen_tilt_y = 0, pen_twist = 0, pen_distance = 0;
        for (int i = 0; i < sdl_ev->ptouch.num_axes; i++) {
            switch (sdl_ev->ptouch.axes[i].type) {
            case SDL_PEN_AXIS_XTILT:    pen_tilt_x   = sdl_ev->ptouch.axes[i].value; break;
            case SDL_PEN_AXIS_YTILT:    pen_tilt_y   = sdl_ev->ptouch.axes[i].value; break;
            case SDL_PEN_AXIS_ROTATION: pen_twist    = sdl_ev->ptouch.axes[i].value; break;
            case SDL_PEN_AXIS_DISTANCE: pen_distance = sdl_ev->ptouch.axes[i].value; break;
            default: break;
            }
        }
        memcpy(ev.data + 24, &pen_x, 4);
        memcpy(ev.data + 28, &pen_y, 4);
        memcpy(ev.data + 32, &pen_pressure, 4);
        memcpy(ev.data + 36, &pen_tilt_x, 4);
        memcpy(ev.data + 40, &pen_tilt_y, 4);
        memcpy(ev.data + 44, &pen_twist, 4);
        memcpy(ev.data + 48, &pen_distance, 4);

        wapi_event_queue_push(&ev);

        /* Synthesize pointer event */
        {
            uint32_t ptr_type;
            switch (sdl_ev->type) {
            case SDL_EVENT_PEN_DOWN: ptr_type = 0x900; break;
            case SDL_EVENT_PEN_UP:   ptr_type = 0x901; break;
            default:                 ptr_type = 0x902; break;
            }
            uint8_t btn_state = (sdl_ev->type != SDL_EVENT_PEN_UP) ? 1 : 0;
            push_pointer_event(ptr_type, *surface_id_ptr,
                -1, 2 /* PEN */,
                1 /* LEFT */, btn_state,
                pen_x, pen_y, 0, 0,
                pen_pressure,
                pen_tilt_x, pen_tilt_y, pen_twist,
                1.0f, 1.0f);
        }
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
 * Pointer (Unified) State Queries
 * ============================================================ */

/* pointer_get_info: (i32 handle, i32 info_ptr) -> i32 */
static wasm_trap_t* host_input_pointer_get_info(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t info_ptr = WAPI_ARG_U32(1);

    /* All capabilities available (mouse + any connected touch/pen) */
    uint8_t info[16];
    memset(info, 0, sizeof(info));
    info[0] = 1; /* has_pressure */
    info[1] = 1; /* has_tilt */
    info[2] = 1; /* has_twist */
    info[3] = 1; /* has_width_height */

    void* dest = wapi_wasm_ptr(info_ptr, 16);
    if (!dest) {
        WAPI_RET_I32(WAPI_ERR_INVAL);
        return NULL;
    }
    memcpy(dest, info, 16);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* pointer_get_position: (i32 handle, i32 surface, i32 x_ptr, i32 y_ptr) -> i32 */
static wasm_trap_t* host_input_pointer_get_position(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    uint32_t x_ptr = WAPI_ARG_U32(2);
    uint32_t y_ptr = WAPI_ARG_U32(3);

    wapi_wasm_write_f32(x_ptr, g_rt.pointer_x);
    wapi_wasm_write_f32(y_ptr, g_rt.pointer_y);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* pointer_get_buttons: (i32 handle) -> i32 */
static wasm_trap_t* host_input_pointer_get_buttons(
    void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller;
    WAPI_RET_I32((int32_t)g_rt.pointer_buttons);
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
    WAPI_DEFINE_2_1(linker, "wapi_input", "pointer_get_info",     host_input_pointer_get_info);
    WAPI_DEFINE_4_1(linker, "wapi_input", "pointer_get_position", host_input_pointer_get_position);
    WAPI_DEFINE_1_1(linker, "wapi_input", "pointer_get_buttons",  host_input_pointer_get_buttons);
}
