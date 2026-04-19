/**
 * WAPI SDL Runtime - Input State & Event Production
 *
 * Implements wapi_input.* imports using SDL3 and converts SDL events
 * into WAPI events on the unified event queue. Called from
 * wapi_host_io.c's wait loop and main.c's frame loop.
 */

#include "wapi_host.h"

static int32_t find_surface_for_window(SDL_WindowID window_id) {
    SDL_Window* win = SDL_GetWindowFromID(window_id);
    if (!win) return 0;
    for (int i = 1; i < WAPI_MAX_HANDLES; i++) {
        if (g_rt.handles[i].type == WAPI_HTYPE_SURFACE &&
            g_rt.handles[i].data.window == win) {
            return (int32_t)i;
        }
    }
    return 0;
}

static void push_pointer_event(uint32_t type, uint32_t surface_id,
                               int32_t pointer_id, uint8_t pointer_type,
                               uint8_t button, uint8_t buttons,
                               float x, float y, float dx, float dy,
                               float pressure,
                               float tilt_x, float tilt_y, float twist,
                               float width, float height) {
    wapi_host_event_t ev;
    memset(&ev, 0, sizeof(ev));
    uint64_t ts = SDL_GetTicksNS();
    memcpy(ev.data + 0,  &type, 4);
    memcpy(ev.data + 4,  &surface_id, 4);
    memcpy(ev.data + 8,  &ts, 8);
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

    g_rt.pointer_x = x;
    g_rt.pointer_y = y;
    g_rt.pointer_buttons = buttons;
}

void wapi_input_process_sdl_event(const SDL_Event* e) {
    wapi_host_event_t ev;
    memset(&ev, 0, sizeof(ev));
    uint32_t* type_p = (uint32_t*)(ev.data + 0);
    uint32_t* sid_p  = (uint32_t*)(ev.data + 4);
    uint64_t* ts_p   = (uint64_t*)(ev.data + 8);

    switch (e->type) {
    case SDL_EVENT_QUIT:
        *type_p = 0x100;  *ts_p = SDL_GetTicksNS();
        wapi_event_queue_push(&ev);
        g_rt.running = false;
        return;

    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        *type_p = 0x0201; *sid_p = (uint32_t)find_surface_for_window(e->window.windowID);
        *ts_p = SDL_GetTicksNS();
        wapi_event_queue_push(&ev);
        return;

    case SDL_EVENT_WINDOW_RESIZED: {
        *type_p = 0x0200; *sid_p = (uint32_t)find_surface_for_window(e->window.windowID);
        *ts_p = SDL_GetTicksNS();
        int32_t w = e->window.data1, h = e->window.data2;
        memcpy(ev.data + 16, &w, 4);
        memcpy(ev.data + 20, &h, 4);
        wapi_event_queue_push(&ev);
        return;
    }

    case SDL_EVENT_WINDOW_FOCUS_GAINED:
    case SDL_EVENT_WINDOW_FOCUS_LOST:
    case SDL_EVENT_WINDOW_SHOWN:
    case SDL_EVENT_WINDOW_HIDDEN:
    case SDL_EVENT_WINDOW_MINIMIZED:
    case SDL_EVENT_WINDOW_MAXIMIZED:
    case SDL_EVENT_WINDOW_RESTORED: {
        static const uint32_t map[] = {
            0x0202, /* focus gained */
            0x0203, /* focus lost */
            0x0204, /* shown */
            0x0205, /* hidden */
            0x0206, /* minimized */
            0x0207, /* maximized */
            0x0208, /* restored */
        };
        int idx = (int)e->type - SDL_EVENT_WINDOW_FOCUS_GAINED;
        if (idx < 0 || idx >= (int)(sizeof(map)/sizeof(map[0]))) return;
        *type_p = map[idx];
        *sid_p = (uint32_t)find_surface_for_window(e->window.windowID);
        *ts_p = SDL_GetTicksNS();
        wapi_event_queue_push(&ev);
        return;
    }

    case SDL_EVENT_WINDOW_MOVED: {
        *type_p = 0x0209; *sid_p = (uint32_t)find_surface_for_window(e->window.windowID);
        *ts_p = SDL_GetTicksNS();
        int32_t x = e->window.data1, y = e->window.data2;
        memcpy(ev.data + 16, &x, 4);
        memcpy(ev.data + 20, &y, 4);
        wapi_event_queue_push(&ev);
        return;
    }

    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        *type_p = (e->type == SDL_EVENT_KEY_DOWN) ? 0x300 : 0x301;
        *sid_p = (uint32_t)find_surface_for_window(e->key.windowID);
        *ts_p = SDL_GetTicksNS();
        uint32_t scancode = (uint32_t)e->key.scancode;
        uint32_t keycode  = (uint32_t)e->key.key;
        memcpy(ev.data + 16, &scancode, 4);
        memcpy(ev.data + 20, &keycode, 4);

        uint16_t mod = 0;
        SDL_Keymod sm = e->key.mod;
        if (sm & SDL_KMOD_LSHIFT) mod |= 0x0001;
        if (sm & SDL_KMOD_RSHIFT) mod |= 0x0002;
        if (sm & SDL_KMOD_LCTRL)  mod |= 0x0040;
        if (sm & SDL_KMOD_RCTRL)  mod |= 0x0080;
        if (sm & SDL_KMOD_LALT)   mod |= 0x0100;
        if (sm & SDL_KMOD_RALT)   mod |= 0x0200;
        if (sm & SDL_KMOD_LGUI)   mod |= 0x0400;
        if (sm & SDL_KMOD_RGUI)   mod |= 0x0800;
        if (sm & SDL_KMOD_CAPS)   mod |= 0x2000;
        if (sm & SDL_KMOD_NUM)    mod |= 0x1000;
        memcpy(ev.data + 24, &mod, 2);
        ev.data[26] = e->key.down ? 1 : 0;
        ev.data[27] = e->key.repeat ? 1 : 0;

        if (scancode < 256) g_rt.key_state[scancode] = e->key.down != 0;
        g_rt.mod_state = mod;
        wapi_event_queue_push(&ev);
        return;
    }

    case SDL_EVENT_TEXT_INPUT: {
        *type_p = 0x302;
        *sid_p = (uint32_t)find_surface_for_window(e->text.windowID);
        *ts_p = SDL_GetTicksNS();
        const char* text = e->text.text;
        size_t len = strlen(text);
        if (len > 31) len = 31;
        memcpy(ev.data + 16, text, len);
        ev.data[16 + len] = '\0';
        wapi_event_queue_push(&ev);
        return;
    }

    case SDL_EVENT_MOUSE_MOTION: {
        *type_p = 0x400;
        *sid_p = (uint32_t)find_surface_for_window(e->motion.windowID);
        *ts_p = SDL_GetTicksNS();
        uint32_t mid = (uint32_t)e->motion.which;
        uint32_t bstate = (uint32_t)e->motion.state;
        float x = e->motion.x, y = e->motion.y;
        float dx = e->motion.xrel, dy = e->motion.yrel;
        memcpy(ev.data + 16, &mid, 4);
        memcpy(ev.data + 20, &bstate, 4);
        memcpy(ev.data + 24, &x, 4);
        memcpy(ev.data + 28, &y, 4);
        memcpy(ev.data + 32, &dx, 4);
        memcpy(ev.data + 36, &dy, 4);
        g_rt.mouse_x = x; g_rt.mouse_y = y;
        wapi_event_queue_push(&ev);
        push_pointer_event(0x902, *sid_p, 0, 0, 0, (uint8_t)(bstate & 0xFF),
                           x, y, dx, dy, bstate ? 0.5f : 0.0f,
                           0, 0, 0, 1.0f, 1.0f);
        return;
    }

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        *type_p = (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN) ? 0x401 : 0x402;
        *sid_p = (uint32_t)find_surface_for_window(e->button.windowID);
        *ts_p = SDL_GetTicksNS();
        uint32_t mid = (uint32_t)e->button.which;
        memcpy(ev.data + 16, &mid, 4);
        ev.data[20] = e->button.button;
        ev.data[21] = e->button.down ? 1 : 0;
        ev.data[22] = e->button.clicks;
        float x = e->button.x, y = e->button.y;
        memcpy(ev.data + 24, &x, 4);
        memcpy(ev.data + 28, &y, 4);
        if (e->button.down) g_rt.mouse_buttons |= (1u << e->button.button);
        else                g_rt.mouse_buttons &= ~(1u << e->button.button);
        wapi_event_queue_push(&ev);
        push_pointer_event(e->button.down ? 0x900 : 0x901, *sid_p,
                           0, 0, e->button.button,
                           (uint8_t)(g_rt.mouse_buttons & 0xFF),
                           x, y, 0, 0,
                           e->button.down ? 0.5f : 0.0f,
                           0, 0, 0, 1.0f, 1.0f);
        return;
    }

    case SDL_EVENT_MOUSE_WHEEL: {
        *type_p = 0x403;
        *sid_p = (uint32_t)find_surface_for_window(e->wheel.windowID);
        *ts_p = SDL_GetTicksNS();
        uint32_t mid = (uint32_t)e->wheel.which;
        uint32_t pad = 0;
        memcpy(ev.data + 16, &mid, 4);
        memcpy(ev.data + 20, &pad, 4);
        float x = e->wheel.x, y = e->wheel.y;
        memcpy(ev.data + 24, &x, 4);
        memcpy(ev.data + 28, &y, 4);
        wapi_event_queue_push(&ev);
        return;
    }

    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_MOTION: {
        uint32_t wt = e->type == SDL_EVENT_FINGER_DOWN ? 0x700 :
                      e->type == SDL_EVENT_FINGER_UP   ? 0x701 : 0x702;
        *type_p = wt;
        *sid_p = (uint32_t)find_surface_for_window(e->tfinger.windowID);
        *ts_p = SDL_GetTicksNS();
        uint64_t tid = (uint64_t)e->tfinger.touchID;
        uint64_t fid = (uint64_t)e->tfinger.fingerID;
        memcpy(ev.data + 16, &tid, 8);
        memcpy(ev.data + 24, &fid, 8);
        float nx = e->tfinger.x, ny = e->tfinger.y;
        float ndx = e->tfinger.dx, ndy = e->tfinger.dy;
        float pressure = e->tfinger.pressure;
        float x = nx, y = ny, dx = ndx, dy = ndy;
        SDL_Window* win = SDL_GetWindowFromID(e->tfinger.windowID);
        if (win) {
            int w, h;
            SDL_GetWindowSize(win, &w, &h);
            x  = nx  * (float)w; y  = ny  * (float)h;
            dx = ndx * (float)w; dy = ndy * (float)h;
        }
        memcpy(ev.data + 32, &x, 4);
        memcpy(ev.data + 36, &y, 4);
        memcpy(ev.data + 40, &dx, 4);
        memcpy(ev.data + 44, &dy, 4);
        memcpy(ev.data + 48, &pressure, 4);
        wapi_event_queue_push(&ev);

        uint32_t pt = e->type == SDL_EVENT_FINGER_DOWN ? 0x900 :
                      e->type == SDL_EVENT_FINGER_UP   ? 0x901 : 0x902;
        uint8_t btn = (e->type != SDL_EVENT_FINGER_UP) ? 1 : 0;
        push_pointer_event(pt, *sid_p,
                           (int32_t)((fid & 0x7FFFFFFF) + 1), 1, 1, btn,
                           x, y, dx, dy, pressure,
                           0, 0, 0, 1.0f, 1.0f);
        return;
    }

    case SDL_EVENT_PEN_DOWN:
    case SDL_EVENT_PEN_UP:
    case SDL_EVENT_PEN_MOTION: {
        uint32_t wt = e->type == SDL_EVENT_PEN_DOWN ? 0x800 :
                      e->type == SDL_EVENT_PEN_UP   ? 0x801 : 0x802;
        *type_p = wt;
        *sid_p = (uint32_t)find_surface_for_window(e->ptouch.windowID);
        *ts_p = SDL_GetTicksNS();
        uint32_t ph = (uint32_t)e->ptouch.which;
        memcpy(ev.data + 16, &ph, 4);
        float px = e->ptouch.x, py = e->ptouch.y;
        float pressure = e->ptouch.pressure;
        float tx = 0, ty = 0, tw = 0, dist = 0;
        for (int i = 0; i < e->ptouch.num_axes; i++) {
            switch (e->ptouch.axes[i].type) {
            case SDL_PEN_AXIS_XTILT:    tx   = e->ptouch.axes[i].value; break;
            case SDL_PEN_AXIS_YTILT:    ty   = e->ptouch.axes[i].value; break;
            case SDL_PEN_AXIS_ROTATION: tw   = e->ptouch.axes[i].value; break;
            case SDL_PEN_AXIS_DISTANCE: dist = e->ptouch.axes[i].value; break;
            default: break;
            }
        }
        memcpy(ev.data + 24, &px, 4);
        memcpy(ev.data + 28, &py, 4);
        memcpy(ev.data + 32, &pressure, 4);
        memcpy(ev.data + 36, &tx, 4);
        memcpy(ev.data + 40, &ty, 4);
        memcpy(ev.data + 44, &tw, 4);
        memcpy(ev.data + 48, &dist, 4);
        wapi_event_queue_push(&ev);

        uint32_t pt = e->type == SDL_EVENT_PEN_DOWN ? 0x900 :
                      e->type == SDL_EVENT_PEN_UP   ? 0x901 : 0x902;
        uint8_t btn = (e->type != SDL_EVENT_PEN_UP) ? 1 : 0;
        push_pointer_event(pt, *sid_p, -1, 2, 1, btn,
                           px, py, 0, 0, pressure, tx, ty, tw, 1.0f, 1.0f);
        return;
    }

    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
        *type_p = 0x652; *ts_p = SDL_GetTicksNS();
        uint32_t gid = (uint32_t)e->gaxis.which;
        memcpy(ev.data + 16, &gid, 4);
        ev.data[20] = (uint8_t)e->gaxis.axis;
        int16_t val = e->gaxis.value;
        memcpy(ev.data + 24, &val, 2);
        wapi_event_queue_push(&ev);
        return;
    }

    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
        *type_p = (e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) ? 0x653 : 0x654;
        *ts_p = SDL_GetTicksNS();
        uint32_t gid = (uint32_t)e->gbutton.which;
        memcpy(ev.data + 16, &gid, 4);
        ev.data[20] = (uint8_t)e->gbutton.button;
        ev.data[21] = e->gbutton.down ? 1 : 0;
        wapi_event_queue_push(&ev);
        return;
    }

    case SDL_EVENT_GAMEPAD_ADDED:
    case SDL_EVENT_GAMEPAD_REMOVED: {
        *type_p = (e->type == SDL_EVENT_GAMEPAD_ADDED) ? 0x650 : 0x651;
        *ts_p = SDL_GetTicksNS();
        uint32_t gid = (uint32_t)e->gdevice.which;
        memcpy(ev.data + 16, &gid, 4);
        wapi_event_queue_push(&ev);
        return;
    }

    default: break;
    }
}

/* ---- Imports ---- */

static int32_t host_key_pressed(wasm_exec_env_t env, int32_t scancode) {
    (void)env;
    if (scancode < 0 || scancode >= 256) return 0;
    return g_rt.key_state[scancode] ? 1 : 0;
}

static int32_t host_get_mod_state(wasm_exec_env_t env) {
    (void)env;
    return (int32_t)g_rt.mod_state;
}

static int32_t host_mouse_position(wasm_exec_env_t env,
                                   int32_t surface, uint32_t x_ptr, uint32_t y_ptr) {
    (void)env;
    if (!wapi_handle_valid(surface, WAPI_HTYPE_SURFACE)) return WAPI_ERR_BADF;
    float x, y;
    SDL_GetMouseState(&x, &y);
    wapi_wasm_write_f32(x_ptr, x);
    wapi_wasm_write_f32(y_ptr, y);
    return WAPI_OK;
}

static int32_t host_mouse_button_state(wasm_exec_env_t env) {
    (void)env;
    return (int32_t)g_rt.mouse_buttons;
}

static int32_t host_set_relative_mouse(wasm_exec_env_t env,
                                       int32_t surface, int32_t enabled) {
    (void)env;
    if (!wapi_handle_valid(surface, WAPI_HTYPE_SURFACE)) return WAPI_ERR_BADF;
    SDL_SetWindowRelativeMouseMode(g_rt.handles[surface].data.window, enabled != 0);
    return WAPI_OK;
}

static void host_start_text_input(wasm_exec_env_t env, int32_t surface) {
    (void)env;
    if (wapi_handle_valid(surface, WAPI_HTYPE_SURFACE)) {
        SDL_StartTextInput(g_rt.handles[surface].data.window);
    }
}
static void host_stop_text_input(wasm_exec_env_t env, int32_t surface) {
    (void)env;
    if (wapi_handle_valid(surface, WAPI_HTYPE_SURFACE)) {
        SDL_StopTextInput(g_rt.handles[surface].data.window);
    }
}

static int32_t host_pointer_get_info(wasm_exec_env_t env,
                                     int32_t handle, uint32_t info_ptr) {
    (void)env; (void)handle;
    uint8_t info[16] = {0};
    info[0] = 1; info[1] = 1; info[2] = 1; info[3] = 1;
    return wapi_wasm_write_bytes(info_ptr, info, 16) ? WAPI_OK : WAPI_ERR_INVAL;
}

static int32_t host_pointer_get_position(wasm_exec_env_t env,
                                         int32_t handle, int32_t surface,
                                         uint32_t x_ptr, uint32_t y_ptr) {
    (void)env; (void)handle; (void)surface;
    wapi_wasm_write_f32(x_ptr, g_rt.pointer_x);
    wapi_wasm_write_f32(y_ptr, g_rt.pointer_y);
    return WAPI_OK;
}

static int32_t host_pointer_get_buttons(wasm_exec_env_t env, int32_t handle) {
    (void)env; (void)handle;
    return (int32_t)g_rt.pointer_buttons;
}

static NativeSymbol g_symbols[] = {
    { "key_pressed",          (void*)host_key_pressed,          "(i)i",  NULL },
    { "get_mod_state",        (void*)host_get_mod_state,        "()i",   NULL },
    { "mouse_position",       (void*)host_mouse_position,       "(iii)i", NULL },
    { "mouse_button_state",   (void*)host_mouse_button_state,   "()i",   NULL },
    { "set_relative_mouse",   (void*)host_set_relative_mouse,   "(ii)i", NULL },
    { "start_text_input",     (void*)host_start_text_input,     "(i)",   NULL },
    { "stop_text_input",      (void*)host_stop_text_input,      "(i)",   NULL },
    { "pointer_get_info",     (void*)host_pointer_get_info,     "(ii)i", NULL },
    { "pointer_get_position", (void*)host_pointer_get_position, "(iiii)i", NULL },
    { "pointer_get_buttons",  (void*)host_pointer_get_buttons,  "(i)i",  NULL },
};

wapi_cap_registration_t wapi_host_input_registration(void) {
    wapi_cap_registration_t r = {
        .module_name = "wapi_input",
        .symbols = g_symbols,
        .count = (uint32_t)(sizeof(g_symbols) / sizeof(g_symbols[0])),
    };
    return r;
}
