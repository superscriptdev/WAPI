/**
 * WAPI Desktop Runtime - Input
 *
 * Two responsibilities:
 *   1. Translate wapi_plat_event_t → WAPI event blob (wapi.h layouts)
 *      and push to the host event queue. Called by
 *      wapi_host_pump_platform_events from the IO bridge.
 *   2. Register the "wapi_input" sync imports defined in
 *      wapi_input.h (device lifecycle, mouse / keyboard / touch /
 *      pen / gamepad / HID / pointer / IME / hotkey).
 *
 * Event codes and structure offsets come from wapi.h; see comments
 * above each case for the pinned layout.
 *
 * IME side-store
 * --------------
 * Text input on Win32 arrives as WM_CHAR sequences in
 * WAPI_PLAT_EV_TEXT_INPUT.  Per wapi_input.h the host assigns a
 * monotonic `sequence` id, emits WAPI_EVENT_IME_COMMIT, and holds
 * the UTF-8 payload in a small ring until the module reads it back
 * via wapi_input.ime_read_text / ime_read_segment.  The header
 * promises the sequence is valid "until the next io.poll"; the ring
 * naturally recycles after IME_STORE_SLOTS commits.
 */

#include "wapi_host.h"
#include "wapi/wapi_input.h"

/* ============================================================
 * Input device handles
 * ============================================================
 * All input devices (aggregate mouse / keyboard / pointer / touch /
 * pen plus per-slot gamepads and future HID devices) are allocated
 * out of the main wapi_handle table with type WAPI_HTYPE_INPUT_DEVICE.
 * Aggregate devices are pre-minted at init and never freed. Gamepads
 * allocate / free on WAPI_PLAT_EV_GPAD_ADDED / _REMOVED so hotplug
 * yields stable tracked handles with real UIDs.
 */

#define WAPI_DEVICE_MOUSE       0
#define WAPI_DEVICE_KEYBOARD    1
#define WAPI_DEVICE_TOUCH       2
#define WAPI_DEVICE_PEN         3
#define WAPI_DEVICE_GAMEPAD     4
#define WAPI_DEVICE_HID         5
#define WAPI_DEVICE_POINTER     6

/* Handles for pre-minted aggregate devices, resolved at init. */
static int32_t s_dev_mouse    = 0;
static int32_t s_dev_keyboard = 0;
static int32_t s_dev_pointer  = 0;
static int32_t s_dev_touch    = 0;
static int32_t s_dev_pen      = 0;

/* Per-XInput-slot gamepad handles. 0 = slot currently unbound. */
#define WAPI_GAMEPAD_SLOTS 4
static int32_t s_dev_gamepad[WAPI_GAMEPAD_SLOTS];

static inline wapi_input_device_t* dev_get(int32_t h) {
    if (!wapi_handle_valid(h, WAPI_HTYPE_INPUT_DEVICE)) return NULL;
    return &g_rt.handles[h].data.input_device;
}

static inline int dev_is_kind(int32_t h, uint32_t kind) {
    wapi_input_device_t* d = dev_get(h);
    return d && d->kind == kind;
}

static inline int dev_is_mouse(int32_t h)    { return dev_is_kind(h, WAPI_DEVICE_MOUSE); }
static inline int dev_is_keyboard(int32_t h) { return dev_is_kind(h, WAPI_DEVICE_KEYBOARD); }
static inline int dev_is_pointer(int32_t h)  { return dev_is_kind(h, WAPI_DEVICE_POINTER); }
static inline int dev_is_touch(int32_t h)    { return dev_is_kind(h, WAPI_DEVICE_TOUCH); }
static inline int dev_is_pen(int32_t h)      { return dev_is_kind(h, WAPI_DEVICE_PEN); }
static inline int dev_is_gamepad(int32_t h)  { return dev_is_kind(h, WAPI_DEVICE_GAMEPAD); }

static int32_t dev_alloc_fixed(uint32_t kind, const char* name, uint32_t backend_slot) {
    int32_t h = wapi_handle_alloc(WAPI_HTYPE_INPUT_DEVICE);
    if (h <= 0) return 0;
    wapi_input_device_t* d = &g_rt.handles[h].data.input_device;
    d->kind = kind;
    d->backend_slot = backend_slot;
    memset(d->uid, 0, sizeof(d->uid));
    d->uid[0] = (uint8_t)kind;
    d->uid[1] = (uint8_t)backend_slot;
    size_t nl = strlen(name);
    if (nl >= sizeof(d->name)) nl = sizeof(d->name) - 1;
    memcpy(d->name, name, nl);
    d->name[nl] = 0;
    return h;
}

/* Resolve the gamepad handle for a given XInput slot. Allocates on
 * first use (so events from a slot referenced before any ADDED event
 * still get a valid handle) and returns the same handle across
 * ADDED/REMOVED cycles for a live slot. */
static int32_t dev_gamepad_for_slot(uint32_t slot) {
    if (slot >= WAPI_GAMEPAD_SLOTS) return 0;
    if (s_dev_gamepad[slot]) return s_dev_gamepad[slot];
    int32_t h = dev_alloc_fixed(WAPI_DEVICE_GAMEPAD, "Gamepad", slot);
    if (h <= 0) return 0;
    wapi_input_device_t* d = &g_rt.handles[h].data.input_device;
    /* Bump generation on each alloc so reconnecting a slot yields a
     * distinguishable uid from the prior session. */
    static uint8_t s_gen[WAPI_GAMEPAD_SLOTS];
    d->uid[2] = ++s_gen[slot];
    snprintf(d->name, sizeof(d->name), "XInput Gamepad %u", (unsigned)slot);
    s_dev_gamepad[slot] = h;
    return h;
}

static void dev_gamepad_free_slot(uint32_t slot) {
    if (slot >= WAPI_GAMEPAD_SLOTS) return;
    int32_t h = s_dev_gamepad[slot];
    if (h > 0) {
        wapi_handle_free(h);
        s_dev_gamepad[slot] = 0;
    }
}

void wapi_host_input_init(void) {
    if (!s_dev_mouse)    s_dev_mouse    = dev_alloc_fixed(WAPI_DEVICE_MOUSE,    "Mouse",    0);
    if (!s_dev_keyboard) s_dev_keyboard = dev_alloc_fixed(WAPI_DEVICE_KEYBOARD, "Keyboard", 0);
    if (!s_dev_pointer)  s_dev_pointer  = dev_alloc_fixed(WAPI_DEVICE_POINTER,  "Pointer",  0);
    if (!s_dev_touch && wapi_plat_touch_available())
        s_dev_touch = dev_alloc_fixed(WAPI_DEVICE_TOUCH, "Touch", 0);
    if (!s_dev_pen && wapi_plat_pen_available())
        s_dev_pen = dev_alloc_fixed(WAPI_DEVICE_PEN, "Pen", 0);
}

/* ============================================================
 * IME side-store
 * ============================================================
 * One slot per recent IME event. The wapi_input.h contract is that a
 * sequence stays valid "until the next io.poll" — we keep IME_STORE_SLOTS
 * slots so a guest can interleave a few events before draining without
 * losing payload. */

#define IME_STORE_SLOTS     8
#define IME_STORE_TEXT_MAX  1024
#define IME_STORE_SEGS_MAX  32

typedef struct {
    uint32_t start;
    uint32_t length;
    uint32_t flags;
} ime_segment_store_t;

typedef struct {
    uint64_t seq;
    char     text[IME_STORE_TEXT_MAX];
    uint32_t text_len;
    uint32_t cursor;
    uint32_t segment_count;
    uint8_t  flags;
    ime_segment_store_t segments[IME_STORE_SEGS_MAX];
} ime_slot_t;

static ime_slot_t s_ime_slots[IME_STORE_SLOTS];
static uint64_t   s_ime_next_seq = 1;

static uint64_t ime_alloc_event(const char* text, uint32_t len,
                                uint32_t cursor, uint8_t flags,
                                const ime_segment_store_t* segs, uint32_t seg_count)
{
    uint64_t seq = s_ime_next_seq++;
    int slot = (int)((seq - 1) % IME_STORE_SLOTS);
    ime_slot_t* s = &s_ime_slots[slot];
    uint32_t copy = len < IME_STORE_TEXT_MAX ? len : IME_STORE_TEXT_MAX - 1;
    if (copy > 0 && text) memcpy(s->text, text, copy);
    s->text[copy] = '\0';
    s->text_len = len; /* report original length */
    s->cursor = cursor;
    s->flags = flags;
    s->segment_count = seg_count > IME_STORE_SEGS_MAX ? IME_STORE_SEGS_MAX : seg_count;
    if (s->segment_count > 0 && segs) {
        memcpy(s->segments, segs, sizeof(segs[0]) * s->segment_count);
    }
    s->seq = seq;
    return seq;
}

static const ime_slot_t* ime_find(uint64_t seq) {
    if (seq == 0) return NULL;
    for (int i = 0; i < IME_STORE_SLOTS; i++) {
        if (s_ime_slots[i].seq == seq) return &s_ime_slots[i];
    }
    return NULL;
}

/* ============================================================
 * Aggregate pointer state (maintained as pointer events land)
 * ============================================================ */

static struct {
    float    x, y;
    uint32_t buttons;
} s_ptr_state;

/* ============================================================
 * Event helpers
 * ============================================================ */

static inline void ev_header(wapi_host_event_t* ev, uint32_t type,
                             uint32_t surface_id, uint64_t ts) {
    memcpy(ev->data +  0, &type,       4);
    memcpy(ev->data +  4, &surface_id, 4);
    memcpy(ev->data +  8, &ts,         8);
}

/* wapi_pointer_event_t (72B header->end):
 *  +16 i32 pointer_id,  +20 u8 pointer_type, +21 u8 button,
 *  +22 u8 buttons,      +23 u8 _pad0,        +24 f32 x/y,
 *  +32 f32 dx/dy,       +40 f32 pressure,    +44 f32 tilt_x/y,
 *  +52 f32 twist,       +56 f32 width/height,+64 u32 _reserved/_pad1 */
static void push_pointer(uint32_t type, uint32_t sid,
                         int32_t pid, uint8_t ptype,
                         uint8_t button, uint8_t buttons,
                         float x, float y, float dx, float dy,
                         float pressure, float tilt_x, float tilt_y,
                         float twist, float width, float height,
                         uint64_t ts)
{
    wapi_host_event_t ev; memset(&ev, 0, sizeof(ev));
    ev_header(&ev, type, sid, ts);
    memcpy(ev.data + 16, &pid, 4);
    ev.data[20] = ptype;
    ev.data[21] = button;
    ev.data[22] = buttons;
    memcpy(ev.data + 24, &x,        4);
    memcpy(ev.data + 28, &y,        4);
    memcpy(ev.data + 32, &dx,       4);
    memcpy(ev.data + 36, &dy,       4);
    memcpy(ev.data + 40, &pressure, 4);
    memcpy(ev.data + 44, &tilt_x,   4);
    memcpy(ev.data + 48, &tilt_y,   4);
    memcpy(ev.data + 52, &twist,    4);
    memcpy(ev.data + 56, &width,    4);
    memcpy(ev.data + 60, &height,   4);
    wapi_event_queue_push(&ev);

    s_ptr_state.x = x;
    s_ptr_state.y = y;
    s_ptr_state.buttons = buttons;
}

/* ============================================================
 * Translator: wapi_plat_event_t → WAPI event
 * ============================================================
 * WAPI event codes from wapi.h §Event Types.  Struct offsets from
 * the typedef layouts in wapi.h.  Surface / window lifecycle events
 * live in 0x0200+ range; note WINDOW_CLOSE etc. are 0x0210+, NOT
 * 0x0201+ as an earlier draft had them. */

/* Shortcut aliases to keep the switch readable */
#define EVT_QUIT            0x100u
#define EVT_SURF_RESIZED    0x0200u
#define EVT_SURF_DPI        0x020Au
#define EVT_WIN_CLOSE       0x0210u
#define EVT_WIN_FOCUS_GAIN  0x0211u
#define EVT_WIN_FOCUS_LOST  0x0212u
#define EVT_WIN_SHOWN       0x0213u
#define EVT_WIN_HIDDEN      0x0214u
#define EVT_WIN_MIN         0x0215u
#define EVT_WIN_MAX         0x0216u
#define EVT_WIN_RESTORED    0x0217u
#define EVT_WIN_MOVED       0x0218u
#define EVT_KEY_DOWN        0x300u
#define EVT_KEY_UP          0x301u
#define EVT_IME_START       0x320u
#define EVT_IME_UPDATE      0x321u
#define EVT_IME_COMMIT      0x322u
#define EVT_IME_CANCEL      0x323u
#define EVT_MOUSE_MOTION    0x400u
#define EVT_MOUSE_BTN_DOWN  0x401u
#define EVT_MOUSE_BTN_UP    0x402u
#define EVT_MOUSE_WHEEL     0x403u
#define EVT_DEVICE_ADDED    0x500u
#define EVT_DEVICE_REMOVED  0x501u
#define EVT_GAMEPAD_AXIS    0x652u
#define EVT_GAMEPAD_BTN_DN  0x653u
#define EVT_GAMEPAD_BTN_UP  0x654u
#define EVT_TOUCH_DOWN      0x700u
#define EVT_TOUCH_UP        0x701u
#define EVT_TOUCH_MOTION    0x702u
#define EVT_PEN_DOWN        0x800u
#define EVT_PEN_UP          0x801u
#define EVT_PEN_MOTION      0x802u
#define EVT_POINTER_DOWN    0x900u
#define EVT_POINTER_UP      0x901u
#define EVT_POINTER_MOTION  0x902u
#define EVT_HOTKEY          0x1830u
#define EVT_TRAY_CLICK      0x1820u
#define EVT_TRAY_MENU       0x1821u
#define EVT_MENU_SELECT     0x1850u
#define EVT_TRANSFER_DELIVER 0x1603u

/* Resolve a menu/tray token back to the guest-facing handle by
 * scanning the handle table. Tokens are scarce (<= 0x7FFF) and the
 * number of live menus/trays is small, so linear scan is fine. */
static int32_t handle_from_menu_token(uint32_t token) {
    for (int i = 1; i < WAPI_MAX_HANDLES; i++) {
        if (g_rt.handles[i].type == WAPI_HTYPE_MENU &&
            g_rt.handles[i].data.menu.token == token)
            return (int32_t)i;
    }
    return 0;
}
static int32_t handle_from_tray_token(uint32_t token) {
    for (int i = 1; i < WAPI_MAX_HANDLES; i++) {
        if (g_rt.handles[i].type == WAPI_HTYPE_TRAY &&
            g_rt.handles[i].data.tray.token == token)
            return (int32_t)i;
    }
    return 0;
}

#define PTYPE_MOUSE  0
#define PTYPE_TOUCH  1
#define PTYPE_PEN    2

void wapi_host_translate_event(const wapi_plat_event_t* pe) {
    wapi_host_event_t ev; memset(&ev, 0, sizeof(ev));
    uint32_t sid = wapi_surface_handle_from_window_id(pe->window_id);
    uint64_t ts  = pe->timestamp_ns;

    switch (pe->type) {
    case WAPI_PLAT_EV_QUIT:
        ev_header(&ev, EVT_QUIT, 0, ts);
        wapi_event_queue_push(&ev);
        g_rt.running = false;
        return;

    case WAPI_PLAT_EV_WINDOW_CLOSE:
        ev_header(&ev, EVT_WIN_CLOSE, sid, ts);
        wapi_event_queue_push(&ev);
        return;

    case WAPI_PLAT_EV_WINDOW_RESIZE: {
        /* wapi_surface_event_t: +16 i32 data1 (width), +20 i32 data2 (height) */
        ev_header(&ev, EVT_SURF_RESIZED, sid, ts);
        int32_t w = pe->u.resize.w, h = pe->u.resize.h;
        memcpy(ev.data + 16, &w, 4);
        memcpy(ev.data + 20, &h, 4);
        wapi_event_queue_push(&ev);
        return;
    }

    case WAPI_PLAT_EV_WINDOW_FOCUS:   ev_header(&ev, EVT_WIN_FOCUS_GAIN, sid, ts); wapi_event_queue_push(&ev); return;
    case WAPI_PLAT_EV_WINDOW_BLUR:    ev_header(&ev, EVT_WIN_FOCUS_LOST, sid, ts); wapi_event_queue_push(&ev); return;
    case WAPI_PLAT_EV_WINDOW_SHOW:    ev_header(&ev, EVT_WIN_SHOWN,      sid, ts); wapi_event_queue_push(&ev); return;
    case WAPI_PLAT_EV_WINDOW_HIDE:    ev_header(&ev, EVT_WIN_HIDDEN,     sid, ts); wapi_event_queue_push(&ev); return;
    case WAPI_PLAT_EV_WINDOW_MIN:     ev_header(&ev, EVT_WIN_MIN,        sid, ts); wapi_event_queue_push(&ev); return;
    case WAPI_PLAT_EV_WINDOW_MAX:     ev_header(&ev, EVT_WIN_MAX,        sid, ts); wapi_event_queue_push(&ev); return;
    case WAPI_PLAT_EV_WINDOW_RESTORE: ev_header(&ev, EVT_WIN_RESTORED,   sid, ts); wapi_event_queue_push(&ev); return;

    case WAPI_PLAT_EV_WINDOW_MOVE: {
        ev_header(&ev, EVT_WIN_MOVED, sid, ts);
        int32_t x = pe->u.move.x, y = pe->u.move.y;
        memcpy(ev.data + 16, &x, 4);
        memcpy(ev.data + 20, &y, 4);
        wapi_event_queue_push(&ev);
        return;
    }

    case WAPI_PLAT_EV_KEY_DOWN:
    case WAPI_PLAT_EV_KEY_UP: {
        /* wapi_keyboard_event_t:
         *  +16 u32 keyboard_handle
         *  +20 u32 scancode
         *  +24 u32 keycode
         *  +28 u16 mod
         *  +30 u8  down
         *  +31 u8  repeat */
        ev_header(&ev, pe->type == WAPI_PLAT_EV_KEY_DOWN ? EVT_KEY_DOWN : EVT_KEY_UP, sid, ts);
        uint32_t kbd = (uint32_t)s_dev_keyboard;
        memcpy(ev.data + 16, &kbd,                4);
        memcpy(ev.data + 20, &pe->u.key.scancode, 4);
        memcpy(ev.data + 24, &pe->u.key.keycode,  4);
        memcpy(ev.data + 28, &pe->u.key.mod,      2);
        ev.data[30] = pe->u.key.down;
        ev.data[31] = pe->u.key.repeat;
        wapi_event_queue_push(&ev);
        return;
    }

    case WAPI_PLAT_EV_TEXT_INPUT: {
        /* wapi_ime_event_t (40B):
         *  +16 u64 sequence
         *  +24 u32 text_len
         *  +28 u32 cursor  (byte offset, end-of-text for commit)
         *  +32 u32 segment_count  (0 for WM_CHAR-derived commits)
         *  +36 u8  flags
         * Payload is stashed in s_ime_slots keyed by sequence. */
        uint32_t len = (uint32_t)strnlen(pe->u.text.text, sizeof(pe->u.text.text));
        uint64_t seq = ime_alloc_event(pe->u.text.text, len, len, 0, NULL, 0);

        uint32_t type = EVT_IME_COMMIT;
        uint32_t zero = 0;
        memcpy(ev.data +  0, &type, 4);
        memcpy(ev.data +  4, &sid,  4);
        memcpy(ev.data +  8, &ts,   8);
        memcpy(ev.data + 16, &seq,  8);
        memcpy(ev.data + 24, &len,  4);
        memcpy(ev.data + 28, &len,  4);   /* cursor = end */
        memcpy(ev.data + 32, &zero, 4);
        ev.data[36] = 0;                  /* flags */
        wapi_event_queue_push(&ev);
        return;
    }

    case WAPI_PLAT_EV_IME_START:
    case WAPI_PLAT_EV_IME_UPDATE: {
        wapi_plat_window_t* window = NULL;
        if (sid > 0 && sid < WAPI_MAX_HANDLES &&
            g_rt.handles[sid].type == WAPI_HTYPE_SURFACE) {
            window = g_rt.handles[sid].data.window;
        }
        char     buf[IME_STORE_TEXT_MAX];
        uint32_t text_len = 0, cursor = 0, seg_count = 0;
        ime_segment_store_t segs[IME_STORE_SEGS_MAX];
        uint32_t actual_segs = 0;
        if (window) {
            wapi_plat_window_ime_get_preedit(window, buf, sizeof(buf),
                                             &text_len, &cursor, &seg_count);
            uint32_t cap = seg_count < IME_STORE_SEGS_MAX ? seg_count : IME_STORE_SEGS_MAX;
            for (uint32_t i = 0; i < cap; i++) {
                if (wapi_plat_window_ime_get_segment(window, i,
                        &segs[actual_segs].start,
                        &segs[actual_segs].length,
                        &segs[actual_segs].flags)) {
                    actual_segs++;
                }
            }
        }
        uint8_t flags = WAPI_IME_F_CURSORVISIBLE;
        uint64_t seq = ime_alloc_event(buf, text_len, cursor, flags, segs, actual_segs);

        uint32_t type = (pe->type == WAPI_PLAT_EV_IME_START) ? EVT_IME_START : EVT_IME_UPDATE;
        memcpy(ev.data +  0, &type,        4);
        memcpy(ev.data +  4, &sid,         4);
        memcpy(ev.data +  8, &ts,          8);
        memcpy(ev.data + 16, &seq,         8);
        memcpy(ev.data + 24, &text_len,    4);
        memcpy(ev.data + 28, &cursor,      4);
        memcpy(ev.data + 32, &actual_segs, 4);
        ev.data[36] = flags;
        wapi_event_queue_push(&ev);
        return;
    }

    case WAPI_PLAT_EV_IME_CANCEL: {
        uint64_t seq = ime_alloc_event(NULL, 0, 0, 0, NULL, 0);
        uint32_t type = EVT_IME_CANCEL;
        uint32_t zero = 0;
        memcpy(ev.data +  0, &type, 4);
        memcpy(ev.data +  4, &sid,  4);
        memcpy(ev.data +  8, &ts,   8);
        memcpy(ev.data + 16, &seq,  8);
        memcpy(ev.data + 24, &zero, 4);
        memcpy(ev.data + 28, &zero, 4);
        memcpy(ev.data + 32, &zero, 4);
        ev.data[36] = 0;
        wapi_event_queue_push(&ev);
        return;
    }

    case WAPI_PLAT_EV_MOUSE_MOTION: {
        /* wapi_mouse_motion_event_t:
         *  +16 u32 mouse_handle, +20 u32 button_state,
         *  +24 f32 x,+28 f32 y,+32 f32 xrel,+36 f32 yrel */
        ev_header(&ev, EVT_MOUSE_MOTION, sid, ts);
        uint32_t mh = (uint32_t)s_dev_mouse;
        memcpy(ev.data + 16, &mh,                        4);
        memcpy(ev.data + 20, &pe->u.motion.button_state, 4);
        memcpy(ev.data + 24, &pe->u.motion.x,            4);
        memcpy(ev.data + 28, &pe->u.motion.y,            4);
        memcpy(ev.data + 32, &pe->u.motion.xrel,         4);
        memcpy(ev.data + 36, &pe->u.motion.yrel,         4);
        wapi_event_queue_push(&ev);

        push_pointer(EVT_POINTER_MOTION, sid, 0, PTYPE_MOUSE,
                     0, (uint8_t)(pe->u.motion.button_state & 0xFFu),
                     pe->u.motion.x, pe->u.motion.y,
                     pe->u.motion.xrel, pe->u.motion.yrel,
                     pe->u.motion.button_state ? 0.5f : 0.0f,
                     0, 0, 0, 1.0f, 1.0f, ts);
        return;
    }

    case WAPI_PLAT_EV_MOUSE_DOWN:
    case WAPI_PLAT_EV_MOUSE_UP: {
        /* wapi_mouse_button_event_t:
         *  +16 u32 mouse_id, +20 u8 button, +21 u8 down,
         *  +22 u8 clicks, +23 u8 _pad, +24 f32 x, +28 f32 y */
        bool down = (pe->type == WAPI_PLAT_EV_MOUSE_DOWN);
        ev_header(&ev, down ? EVT_MOUSE_BTN_DOWN : EVT_MOUSE_BTN_UP, sid, ts);
        uint32_t mh = (uint32_t)s_dev_mouse;
        memcpy(ev.data + 16, &mh, 4);
        ev.data[20] = pe->u.button.button;
        ev.data[21] = pe->u.button.down;
        ev.data[22] = pe->u.button.clicks;
        memcpy(ev.data + 24, &pe->u.button.x, 4);
        memcpy(ev.data + 28, &pe->u.button.y, 4);
        wapi_event_queue_push(&ev);

        uint32_t buttons = 0;
        wapi_plat_mouse_state(NULL, NULL, &buttons);
        push_pointer(down ? EVT_POINTER_DOWN : EVT_POINTER_UP,
                     sid, 0, PTYPE_MOUSE,
                     pe->u.button.button, (uint8_t)(buttons & 0xFFu),
                     pe->u.button.x, pe->u.button.y, 0, 0,
                     down ? 0.5f : 0.0f,
                     0, 0, 0, 1.0f, 1.0f, ts);
        return;
    }

    case WAPI_PLAT_EV_MOUSE_WHEEL: {
        /* wapi_mouse_wheel_event_t:
         *  +16 u32 mouse_handle, +20 u32 _pad, +24 f32 x, +28 f32 y */
        ev_header(&ev, EVT_MOUSE_WHEEL, sid, ts);
        uint32_t mh = (uint32_t)s_dev_mouse;
        memcpy(ev.data + 16, &mh,             4);
        memcpy(ev.data + 24, &pe->u.wheel.x,  4);
        memcpy(ev.data + 28, &pe->u.wheel.y,  4);
        wapi_event_queue_push(&ev);
        return;
    }

    case WAPI_PLAT_EV_FINGER_DOWN:
    case WAPI_PLAT_EV_FINGER_UP:
    case WAPI_PLAT_EV_FINGER_MOTION: {
        /* wapi_touch_event_t:
         *  +16 i32 touch_handle
         *  +20 i32 finger_index
         *  +24 f32 x, +28 f32 y, +32 f32 dx, +36 f32 dy,
         *  +40 f32 pressure, +44 u32 _pad */
        uint32_t t;
        switch (pe->type) {
        case WAPI_PLAT_EV_FINGER_DOWN:   t = EVT_TOUCH_DOWN;   break;
        case WAPI_PLAT_EV_FINGER_UP:     t = EVT_TOUCH_UP;     break;
        default:                         t = EVT_TOUCH_MOTION; break;
        }
        ev_header(&ev, t, sid, ts);
        int32_t touch_h = (int32_t)pe->u.touch.touch_id;
        int32_t finger  = (int32_t)pe->u.touch.finger_id;
        memcpy(ev.data + 16, &touch_h,             4);
        memcpy(ev.data + 20, &finger,              4);
        memcpy(ev.data + 24, &pe->u.touch.x,       4);
        memcpy(ev.data + 28, &pe->u.touch.y,       4);
        memcpy(ev.data + 32, &pe->u.touch.dx,      4);
        memcpy(ev.data + 36, &pe->u.touch.dy,      4);
        memcpy(ev.data + 40, &pe->u.touch.pressure,4);
        wapi_event_queue_push(&ev);

        uint32_t pt = (pe->type == WAPI_PLAT_EV_FINGER_DOWN) ? EVT_POINTER_DOWN :
                      (pe->type == WAPI_PLAT_EV_FINGER_UP)   ? EVT_POINTER_UP   :
                                                               EVT_POINTER_MOTION;
        int32_t pid = finger + 1;
        uint8_t btn_state = (pe->type != WAPI_PLAT_EV_FINGER_UP) ? 1 : 0;
        push_pointer(pt, sid, pid, PTYPE_TOUCH, 1, btn_state,
                     pe->u.touch.x, pe->u.touch.y,
                     pe->u.touch.dx, pe->u.touch.dy,
                     pe->u.touch.pressure, 0, 0, 0, 1.0f, 1.0f, ts);
        return;
    }

    case WAPI_PLAT_EV_PEN_DOWN:
    case WAPI_PLAT_EV_PEN_UP:
    case WAPI_PLAT_EV_PEN_MOTION: {
        uint32_t t;
        switch (pe->type) {
        case WAPI_PLAT_EV_PEN_DOWN:    t = EVT_PEN_DOWN;   break;
        case WAPI_PLAT_EV_PEN_UP:      t = EVT_PEN_UP;     break;
        default:                       t = EVT_PEN_MOTION; break;
        }
        /* wapi_pen_event_t:
         *  +16 u32 pen_handle, +20 u8 tool_type, +21 u8 button, +22 _pad[2],
         *  +24 f32 x/y, +32 f32 pressure, +36 f32 tilt_x/y, +44 f32 twist,
         *  +48 f32 distance, +52 u32 _pad2 */
        ev_header(&ev, t, sid, ts);
        memcpy(ev.data + 16, &pe->u.pen.pen_id,   4);
        ev.data[20] = pe->u.pen.tool_type;
        ev.data[21] = pe->u.pen.button;
        memcpy(ev.data + 24, &pe->u.pen.x,        4);
        memcpy(ev.data + 28, &pe->u.pen.y,        4);
        memcpy(ev.data + 32, &pe->u.pen.pressure, 4);
        memcpy(ev.data + 36, &pe->u.pen.tilt_x,   4);
        memcpy(ev.data + 40, &pe->u.pen.tilt_y,   4);
        memcpy(ev.data + 44, &pe->u.pen.twist,    4);
        memcpy(ev.data + 48, &pe->u.pen.distance, 4);
        wapi_event_queue_push(&ev);

        uint32_t pt = (pe->type == WAPI_PLAT_EV_PEN_DOWN) ? EVT_POINTER_DOWN :
                      (pe->type == WAPI_PLAT_EV_PEN_UP)   ? EVT_POINTER_UP   :
                                                            EVT_POINTER_MOTION;
        uint8_t btn_state = (pe->type != WAPI_PLAT_EV_PEN_UP) ? 1 : 0;
        push_pointer(pt, sid, -1, PTYPE_PEN, 1, btn_state,
                     pe->u.pen.x, pe->u.pen.y, 0, 0,
                     pe->u.pen.pressure, pe->u.pen.tilt_x, pe->u.pen.tilt_y,
                     pe->u.pen.twist, 1.0f, 1.0f, ts);
        return;
    }

    case WAPI_PLAT_EV_GPAD_ADDED: {
        /* wapi_device_event_t:
         *  +16 u32 device_type, +20 i32 device_handle, +24 u8[16] uid */
        uint32_t slot = pe->u.gpad.gamepad_id;
        int32_t  dh   = dev_gamepad_for_slot(slot);
        ev_header(&ev, EVT_DEVICE_ADDED, 0, ts);
        uint32_t dtype = WAPI_DEVICE_GAMEPAD;
        memcpy(ev.data + 16, &dtype, 4);
        memcpy(ev.data + 20, &dh,    4);
        wapi_input_device_t* d = dev_get(dh);
        if (d) memcpy(ev.data + 24, d->uid, 16);
        wapi_event_queue_push(&ev);
        return;
    }
    case WAPI_PLAT_EV_GPAD_REMOVED: {
        uint32_t slot = pe->u.gpad.gamepad_id;
        int32_t  dh   = s_dev_gamepad[slot < WAPI_GAMEPAD_SLOTS ? slot : 0];
        ev_header(&ev, EVT_DEVICE_REMOVED, 0, ts);
        uint32_t dtype = WAPI_DEVICE_GAMEPAD;
        memcpy(ev.data + 16, &dtype, 4);
        memcpy(ev.data + 20, &dh,    4);
        wapi_input_device_t* d = dev_get(dh);
        if (d) memcpy(ev.data + 24, d->uid, 16);
        wapi_event_queue_push(&ev);
        /* Free the tracked handle AFTER the event is queued so modules
         * that look up by handle in their own REMOVED handler still
         * see a valid record. Handle id is safe to reuse on the next
         * ADDED because generations are encoded in the uid. */
        dev_gamepad_free_slot(slot);
        return;
    }

    case WAPI_PLAT_EV_GPAD_AXIS: {
        /* wapi_gamepad_axis_event_t:
         *  +16 u32 gamepad_handle, +20 u8 axis, +21 u8[3] _pad,
         *  +24 i16 value, +26 u16 _pad2, +28 u32 _pad3 */
        int32_t dh = dev_gamepad_for_slot(pe->u.gpad.gamepad_id);
        ev_header(&ev, EVT_GAMEPAD_AXIS, 0, ts);
        memcpy(ev.data + 16, &dh, 4);
        ev.data[20] = pe->u.gpad.axis;
        memcpy(ev.data + 24, &pe->u.gpad.axis_value, 2);
        wapi_event_queue_push(&ev);
        return;
    }

    case WAPI_PLAT_EV_GPAD_BUTTON_DOWN:
    case WAPI_PLAT_EV_GPAD_BUTTON_UP: {
        /* wapi_gamepad_button_event_t:
         *  +16 u32 gamepad_handle, +20 u8 button, +21 u8 down, +22 u8[2] _pad */
        bool down = (pe->type == WAPI_PLAT_EV_GPAD_BUTTON_DOWN);
        int32_t dh = dev_gamepad_for_slot(pe->u.gpad.gamepad_id);
        ev_header(&ev, down ? EVT_GAMEPAD_BTN_DN : EVT_GAMEPAD_BTN_UP, 0, ts);
        memcpy(ev.data + 16, &dh, 4);
        ev.data[20] = pe->u.gpad.button;
        ev.data[21] = down ? 1 : 0;
        wapi_event_queue_push(&ev);
        return;
    }

    case WAPI_PLAT_EV_HOTKEY: {
        /* wapi_hotkey_event_t (24B): +16 u32 hotkey_id, +20 u32 _pad */
        ev_header(&ev, EVT_HOTKEY, 0, ts);
        memcpy(ev.data + 16, &pe->u.hotkey.hotkey_id, 4);
        wapi_event_queue_push(&ev);
        return;
    }

    case WAPI_PLAT_EV_MENU_SELECT: {
        /* wapi_menu_event_t: +16 u32 menu_handle, +20 u32 item_id */
        int32_t mh = handle_from_menu_token(pe->u.menu.menu_token);
        ev_header(&ev, EVT_MENU_SELECT, 0, ts);
        memcpy(ev.data + 16, &mh,                4);
        memcpy(ev.data + 20, &pe->u.menu.item_id, 4);
        wapi_event_queue_push(&ev);
        return;
    }
    case WAPI_PLAT_EV_TRAY_CLICK:
    case WAPI_PLAT_EV_TRAY_MENU: {
        /* wapi_tray_event_t: +16 u32 tray_handle, +20 u32 item_id */
        int32_t th = handle_from_tray_token(pe->u.tray.tray_token);
        uint32_t type = (pe->type == WAPI_PLAT_EV_TRAY_CLICK) ? EVT_TRAY_CLICK : EVT_TRAY_MENU;
        ev_header(&ev, type, 0, ts);
        memcpy(ev.data + 16, &th,                 4);
        memcpy(ev.data + 20, &pe->u.tray.item_id, 4);
        wapi_event_queue_push(&ev);
        return;
    }

    case WAPI_PLAT_EV_DROP_FILES: {
        /* wapi_transfer_event_t (40B):
         *   +16 i32 pointer_id, +20 i32 x, +24 i32 y,
         *   +28 u32 item_count, +32 u32 available_actions, +36 u32 _pad
         *
         * We emit WAPI_EVENT_TRANSFER_DELIVER for a completed drop;
         * the guest reads the URI-list payload through
         * wapi_transfer_read(POINTED, "text/uri-list"). item_count
         * is unknown at event time (we only have byte length); leave
         * it 0 and let the guest discover via the read. */
        ev_header(&ev, EVT_TRANSFER_DELIVER, sid, ts);
        int32_t pid = 0;
        int32_t xx = (int32_t)pe->u.drop.x;
        int32_t yy = (int32_t)pe->u.drop.y;
        uint32_t items = 0;
        /* Copy action is the only Win32 drop action we honour — OLE
         * IDropTarget would distinguish move/link/copy; DragAcceptFiles
         * always delivers copy. */
        uint32_t actions = 1u /* WAPI_TRANSFER_ACTION_COPY */;
        memcpy(ev.data + 16, &pid,     4);
        memcpy(ev.data + 20, &xx,      4);
        memcpy(ev.data + 24, &yy,      4);
        memcpy(ev.data + 28, &items,   4);
        memcpy(ev.data + 32, &actions, 4);
        wapi_event_queue_push(&ev);
        return;
    }
    }
}

void wapi_host_pump_platform_events(void) {
    wapi_plat_event_t batch[32];
    int n;
    while ((n = wapi_plat_poll_events(batch, 32)) > 0) {
        for (int i = 0; i < n; i++) wapi_host_translate_event(&batch[i]);
    }
}

/* ============================================================
 * "wapi_input" sync imports (wapi_input.h)
 * ============================================================
 * Device lifecycle, keyboard / mouse / touch / pen / pointer state
 * queries, IME accessors, hotkey registration.  Anything not yet
 * backed by platform code returns WAPI_ERR_NOSYS/NOTSUP so guests
 * can probe feature availability at runtime. */

static wasm_trap_t* h_nosys(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(WAPI_ERR_NOSYS);
    return NULL;
}

/* ---- Device lifecycle ---- */

static wasm_trap_t* h_device_count(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t type = WAPI_ARG_I32(0);
    int32_t n;
    switch (type) {
    case WAPI_DEVICE_MOUSE:
    case WAPI_DEVICE_KEYBOARD:
    case WAPI_DEVICE_POINTER: n = 1; break;
    case WAPI_DEVICE_TOUCH:   n = wapi_plat_touch_available() ? 1 : 0; break;
    case WAPI_DEVICE_PEN:     n = wapi_plat_pen_available()   ? 1 : 0; break;
    case WAPI_DEVICE_GAMEPAD: {
        n = 0;
        for (uint32_t slot = 0; slot < WAPI_GAMEPAD_SLOTS; slot++) {
            if (wapi_plat_gamepad_connected(slot)) n++;
        }
        break;
    }
    default: n = 0; break;
    }
    WAPI_RET_I32(n);
    return NULL;
}

static wasm_trap_t* h_device_open(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  type  = WAPI_ARG_I32(0);
    int32_t  index = WAPI_ARG_I32(1);
    uint32_t out   = WAPI_ARG_U32(2);
    int32_t h = 0;
    switch (type) {
    case WAPI_DEVICE_MOUSE:    if (index != 0) { WAPI_RET_I32(WAPI_ERR_RANGE); return NULL; } h = s_dev_mouse;    break;
    case WAPI_DEVICE_KEYBOARD: if (index != 0) { WAPI_RET_I32(WAPI_ERR_RANGE); return NULL; } h = s_dev_keyboard; break;
    case WAPI_DEVICE_POINTER:  if (index != 0) { WAPI_RET_I32(WAPI_ERR_RANGE); return NULL; } h = s_dev_pointer;  break;
    case WAPI_DEVICE_TOUCH:
        if (index != 0) { WAPI_RET_I32(WAPI_ERR_RANGE); return NULL; }
        if (!wapi_plat_touch_available() || !s_dev_touch) { WAPI_RET_I32(WAPI_ERR_NOENT); return NULL; }
        h = s_dev_touch; break;
    case WAPI_DEVICE_PEN:
        if (index != 0) { WAPI_RET_I32(WAPI_ERR_RANGE); return NULL; }
        if (!wapi_plat_pen_available() || !s_dev_pen) { WAPI_RET_I32(WAPI_ERR_NOENT); return NULL; }
        h = s_dev_pen; break;
    case WAPI_DEVICE_GAMEPAD: {
        /* Map index -> Nth currently-connected slot. */
        int seen = 0;
        for (uint32_t slot = 0; slot < WAPI_GAMEPAD_SLOTS; slot++) {
            if (!wapi_plat_gamepad_connected(slot)) continue;
            if (seen == index) { h = dev_gamepad_for_slot(slot); break; }
            seen++;
        }
        if (!h) { WAPI_RET_I32(WAPI_ERR_RANGE); return NULL; }
        break;
    }
    default: WAPI_RET_I32(WAPI_ERR_RANGE); return NULL;
    }
    if (h <= 0) { WAPI_RET_I32(WAPI_ERR_NOENT); return NULL; }
    wapi_wasm_write_i32(out, h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_device_close(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h = WAPI_ARG_I32(0);
    wapi_input_device_t* d = dev_get(h);
    if (!d) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    /* Aggregate devices (mouse/keyboard/pointer/touch/pen) live until
     * shutdown; closing one is a no-op. Gamepads are freed on REMOVED.
     * Only HID devices own platform resources the guest can release. */
    if (d->kind == WAPI_DEVICE_HID) {
        if (d->hid_dev) { wapi_plat_hid_close(d->hid_dev); d->hid_dev = NULL; }
        wapi_handle_free(h);
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_device_get_type(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h = WAPI_ARG_I32(0);
    wapi_input_device_t* d = dev_get(h);
    if (!d) { WAPI_RET_I32(-1); return NULL; }
    WAPI_RET_I32((int32_t)d->kind);
    return NULL;
}

static wasm_trap_t* h_device_get_uid(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h   = WAPI_ARG_I32(0);
    uint32_t out = WAPI_ARG_U32(1);
    wapi_input_device_t* d = dev_get(h);
    if (!d) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    void* dst = wapi_wasm_ptr(out, 16);
    if (!dst) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    memcpy(dst, d->uid, 16);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_device_get_name(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h         = WAPI_ARG_I32(0);
    uint32_t buf_ptr   = WAPI_ARG_U32(1);
    uint64_t buf_len   = WAPI_ARG_U64(2);
    uint32_t out_ptr   = WAPI_ARG_U32(3);

    wapi_input_device_t* d = dev_get(h);
    if (!d) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    const char* name = d->name;
    uint64_t name_len = (uint64_t)strlen(name);
    uint64_t copy = (name_len < buf_len) ? name_len : buf_len;
    if (copy > 0) {
        void* dst = wapi_wasm_ptr(buf_ptr, (uint32_t)copy);
        if (!dst) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
        memcpy(dst, name, (size_t)copy);
    }
    wapi_wasm_write_u64(out_ptr, name_len);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_device_seat(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    /* Single-seat systems: always seat 0 (WAPI_SEAT_DEFAULT). */
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32(0);
    return NULL;
}

/* ---- Mouse ---- */

static wasm_trap_t* h_mouse_get_info(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h       = WAPI_ARG_I32(0);
    uint32_t info_pt = WAPI_ARG_U32(1);
    if (!dev_is_mouse(h)) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    /* wapi_mouse_info_t (16B): {u32 button_count, u8 has_wheel, u8[11] _reserved} */
    uint8_t info[16] = {0};
    uint32_t bc = 5; memcpy(info, &bc, 4);
    info[4] = 1; /* has_wheel */
    void* dst = wapi_wasm_ptr(info_pt, 16);
    if (!dst) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    memcpy(dst, info, 16);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_mouse_get_position(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h   = WAPI_ARG_I32(0);
    int32_t sfc = WAPI_ARG_I32(1);
    uint32_t xp = WAPI_ARG_U32(2), yp = WAPI_ARG_U32(3);
    if (!dev_is_mouse(h)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    (void)sfc;
    float x, y;
    wapi_plat_mouse_state(&x, &y, NULL);
    wapi_wasm_write_f32(xp, x);
    wapi_wasm_write_f32(yp, y);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_mouse_get_button_state(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h = WAPI_ARG_I32(0);
    if (!dev_is_mouse(h)) { WAPI_RET_I32(0); return NULL; }
    uint32_t b = 0;
    wapi_plat_mouse_state(NULL, NULL, &b);
    WAPI_RET_I32((int32_t)b);
    return NULL;
}

static wasm_trap_t* h_mouse_set_relative(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h  = WAPI_ARG_I32(0);
    int32_t sh = WAPI_ARG_I32(1);
    int32_t on = WAPI_ARG_I32(2);
    if (!dev_is_mouse(h)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    if (!wapi_handle_valid(sh, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    wapi_plat_window_set_relative_mouse(g_rt.handles[sh].data.window, on != 0);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_mouse_warp(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h  = WAPI_ARG_I32(0);
    int32_t sh = WAPI_ARG_I32(1);
    float   x  = WAPI_ARG_F32(2);
    float   y  = WAPI_ARG_F32(3);
    if (!dev_is_mouse(h)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    if (!wapi_handle_valid(sh, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    bool ok = wapi_plat_window_warp_mouse(g_rt.handles[sh].data.window, x, y);
    WAPI_RET_I32(ok ? WAPI_OK : WAPI_ERR_NOTSUP);
    return NULL;
}

static wasm_trap_t* h_mouse_set_cursor_image(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h        = WAPI_ARG_I32(0);
    uint32_t data_ptr = WAPI_ARG_U32(1);
    int32_t  w_px     = WAPI_ARG_I32(2);
    int32_t  h_px     = WAPI_ARG_I32(3);
    int32_t  hot_x    = WAPI_ARG_I32(4);
    int32_t  hot_y    = WAPI_ARG_I32(5);
    if (!dev_is_mouse(h)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    if (w_px <= 0 || h_px <= 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    uint64_t need = (uint64_t)w_px * (uint64_t)h_px * 4ull;
    if (need > (uint64_t)0xFFFFFFFFu) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    void* src = wapi_wasm_ptr(data_ptr, (uint32_t)need);
    if (!src) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    /* Aggregate-mouse semantics: apply to every live surface, same as set_cursor. */
    bool any = false;
    for (int i = 1; i < WAPI_MAX_HANDLES; i++) {
        if (g_rt.handles[i].type == WAPI_HTYPE_SURFACE &&
            g_rt.handles[i].data.window) {
            if (wapi_plat_window_set_cursor_image(g_rt.handles[i].data.window,
                                                  src, w_px, h_px, hot_x, hot_y))
                any = true;
        }
    }
    WAPI_RET_I32(any ? WAPI_OK : WAPI_ERR_NOTSUP);
    return NULL;
}

static wasm_trap_t* h_mouse_set_cursor(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h      = WAPI_ARG_I32(0);
    int32_t cursor = WAPI_ARG_I32(1);
    if (!dev_is_mouse(h)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    /* Apply to every live surface; wapi_plat stores per-window. */
    for (int i = 1; i < WAPI_MAX_HANDLES; i++) {
        if (g_rt.handles[i].type == WAPI_HTYPE_SURFACE &&
            g_rt.handles[i].data.window) {
            wapi_plat_window_set_cursor(g_rt.handles[i].data.window,
                                        (uint32_t)cursor);
        }
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ---- Keyboard ---- */

static wasm_trap_t* h_keyboard_key_pressed(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h  = WAPI_ARG_I32(0);
    int32_t sc = WAPI_ARG_I32(1);
    if (!dev_is_keyboard(h)) { WAPI_RET_I32(0); return NULL; }
    if (sc < 0 || sc >= 256) { WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32(wapi_plat_key_pressed((uint32_t)sc) ? 1 : 0);
    return NULL;
}

static wasm_trap_t* h_keyboard_get_modstate(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h = WAPI_ARG_I32(0);
    if (!dev_is_keyboard(h)) { WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32((int32_t)wapi_plat_mod_state());
    return NULL;
}

/* ---- Text input ---- */

static wasm_trap_t* h_start_textinput(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)results; (void)nresults;
    int32_t sh = WAPI_ARG_I32(0);
    if (wapi_handle_valid(sh, WAPI_HTYPE_SURFACE))
        wapi_plat_window_start_text_input(g_rt.handles[sh].data.window);
    return NULL;
}

static wasm_trap_t* h_stop_textinput(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)results; (void)nresults;
    int32_t sh = WAPI_ARG_I32(0);
    if (wapi_handle_valid(sh, WAPI_HTYPE_SURFACE))
        wapi_plat_window_stop_text_input(g_rt.handles[sh].data.window);
    return NULL;
}

/* ---- Pointer (unified) ---- */

/* ---- Touch ---- */

static wasm_trap_t* h_touch_get_info(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h        = WAPI_ARG_I32(0);
    uint32_t info_ptr = WAPI_ARG_U32(1);
    if (!dev_is_touch(h)) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    /* wapi_touch_info_t (16B): {u32 type, u32 max_fingers, u8[8] _reserved} */
    uint8_t buf[16]; memset(buf, 0, sizeof(buf));
    uint32_t type = 0; /* WAPI_TOUCH_DEVICE_DIRECT (touchscreen). */
    uint32_t mf   = wapi_plat_touch_max_fingers();
    memcpy(buf + 0, &type, 4);
    memcpy(buf + 4, &mf,   4);
    if (!wapi_wasm_write_bytes(info_ptr, buf, sizeof(buf))) {
        WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_touch_finger_count(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h = WAPI_ARG_I32(0);
    if (!dev_is_touch(h)) { WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32(wapi_plat_touch_finger_count());
    return NULL;
}

static wasm_trap_t* h_touch_get_finger(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h         = WAPI_ARG_I32(0);
    int32_t  fi        = WAPI_ARG_I32(1);
    uint32_t state_ptr = WAPI_ARG_U32(2);
    if (!dev_is_touch(h)) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }

    int32_t finger_idx = 0;
    float   x = 0, y = 0, pressure = 0;
    if (!wapi_plat_touch_get_finger(fi, &finger_idx, &x, &y, &pressure)) {
        WAPI_RET_I32(WAPI_ERR_RANGE); return NULL;
    }
    /* wapi_finger_state_t (16B): i32 finger_index, f32 x, f32 y, f32 pressure */
    uint8_t buf[16];
    memcpy(buf + 0,  &finger_idx, 4);
    memcpy(buf + 4,  &x,          4);
    memcpy(buf + 8,  &y,          4);
    memcpy(buf + 12, &pressure,   4);
    if (!wapi_wasm_write_bytes(state_ptr, buf, sizeof(buf))) {
        WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ---- Pen ---- */

static wasm_trap_t* h_pen_get_info(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h        = WAPI_ARG_I32(0);
    uint32_t info_ptr = WAPI_ARG_U32(1);
    if (!dev_is_pen(h)) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    uint32_t tool = 0, caps = 0;
    wapi_plat_pen_get_info(&tool, &caps);
    /* wapi_pen_info_t (16B): {u32 tool_type, u32 capabilities_mask, u8[8] _reserved} */
    uint8_t buf[16]; memset(buf, 0, sizeof(buf));
    memcpy(buf + 0, &tool, 4);
    memcpy(buf + 4, &caps, 4);
    if (!wapi_wasm_write_bytes(info_ptr, buf, sizeof(buf))) {
        WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_pen_get_axis(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h       = WAPI_ARG_I32(0);
    int32_t  axis    = WAPI_ARG_I32(1);
    uint32_t out_ptr = WAPI_ARG_U32(2);
    if (!dev_is_pen(h)) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    float v = 0.0f;
    if (!wapi_plat_pen_get_axis(axis, &v)) {
        WAPI_RET_I32(WAPI_ERR_NOTSUP); return NULL;
    }
    wapi_wasm_write_f32(out_ptr, v);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_pen_get_position(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h    = WAPI_ARG_I32(0);
    uint32_t x_pt = WAPI_ARG_U32(1);
    uint32_t y_pt = WAPI_ARG_U32(2);
    if (!dev_is_pen(h)) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    float x = 0, y = 0;
    wapi_plat_pen_get_position(&x, &y);
    wapi_wasm_write_f32(x_pt, x);
    wapi_wasm_write_f32(y_pt, y);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ---- Raw HID ---- */

static wasm_trap_t* h_hid_request_device(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t vendor      = WAPI_ARG_U32(0);
    uint32_t product     = WAPI_ARG_U32(1);
    uint32_t usage_page  = WAPI_ARG_U32(2);
    uint32_t out_handle  = WAPI_ARG_U32(3);

    /* Enumerate matching devices; open the first one the backend
     * accepts. A proper permission broker (Phase C) will swap the
     * auto-open for a modal prompt. */
    int count = wapi_plat_hid_enumerate((uint16_t)vendor, (uint16_t)product,
                                        (uint16_t)usage_page, NULL, 0);
    if (count <= 0) { WAPI_RET_I32(WAPI_ERR_NOENT); return NULL; }

    wapi_plat_hid_info_t* infos = (wapi_plat_hid_info_t*)
        calloc((size_t)count, sizeof(*infos));
    if (!infos) { WAPI_RET_I32(WAPI_ERR_NOMEM); return NULL; }
    int actual = wapi_plat_hid_enumerate((uint16_t)vendor, (uint16_t)product,
                                         (uint16_t)usage_page, infos, count);
    if (actual <= 0) { free(infos); WAPI_RET_I32(WAPI_ERR_NOENT); return NULL; }

    wapi_plat_hid_device_t* pdev = NULL;
    wapi_plat_hid_info_t info = {0};
    for (int i = 0; i < actual; i++) {
        pdev = wapi_plat_hid_open(infos[i].uid);
        if (pdev) { info = infos[i]; break; }
    }
    free(infos);
    if (!pdev) { WAPI_RET_I32(WAPI_ERR_ACCES); return NULL; }

    int32_t h = wapi_handle_alloc(WAPI_HTYPE_INPUT_DEVICE);
    if (h <= 0) { wapi_plat_hid_close(pdev); WAPI_RET_I32(WAPI_ERR_NOMEM); return NULL; }
    wapi_input_device_t* d = &g_rt.handles[h].data.input_device;
    d->kind = WAPI_DEVICE_HID;
    d->backend_slot = 0;
    memcpy(d->uid, info.uid, 16);
    size_t nl = strlen(info.name);
    if (nl >= sizeof(d->name)) nl = sizeof(d->name) - 1;
    memcpy(d->name, info.name, nl);
    d->name[nl] = 0;
    d->hid_dev        = pdev;
    d->hid_vendor     = info.vendor_id;
    d->hid_product    = info.product_id;
    d->hid_usage_page = info.usage_page;
    d->hid_usage      = info.usage;
    wapi_wasm_write_i32(out_handle, h);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_hid_get_info(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h        = WAPI_ARG_I32(0);
    uint32_t info_ptr = WAPI_ARG_U32(1);
    wapi_input_device_t* d = dev_get(h);
    if (!d || d->kind != WAPI_DEVICE_HID) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    /* wapi_hid_info_t (16B, align 2):
     *   u16 vendor_id, u16 product_id, u16 usage_page, u16 usage, u8[8] _reserved */
    uint8_t buf[16]; memset(buf, 0, sizeof(buf));
    memcpy(buf + 0, &d->hid_vendor,     2);
    memcpy(buf + 2, &d->hid_product,    2);
    memcpy(buf + 4, &d->hid_usage_page, 2);
    memcpy(buf + 6, &d->hid_usage,      2);
    if (!wapi_wasm_write_bytes(info_ptr, buf, sizeof(buf))) {
        WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* Helpers: prepend report_id if nonzero, else write as-is (HID spec:
 * first byte of the transfer is the report id, 0 for non-numbered). */
static int hid_prep_buffer(uint8_t report_id, const void* src, uint32_t src_len,
                           uint8_t* out, uint32_t out_cap)
{
    if (src_len + 1 > out_cap) return -1;
    out[0] = report_id;
    if (src_len > 0 && src) memcpy(out + 1, src, src_len);
    return (int)(src_len + 1);
}

static wasm_trap_t* h_hid_send_report(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h         = WAPI_ARG_I32(0);
    uint32_t report_id = WAPI_ARG_U32(1);
    uint32_t data_ptr  = WAPI_ARG_U32(2);
    uint64_t data_len  = WAPI_ARG_U64(3);
    wapi_input_device_t* d = dev_get(h);
    if (!d || d->kind != WAPI_DEVICE_HID || !d->hid_dev) {
        WAPI_RET_I32(WAPI_ERR_BADF); return NULL;
    }
    if (data_len > 65535) { WAPI_RET_I32(WAPI_ERR_RANGE); return NULL; }
    const void* src = data_len ? wapi_wasm_ptr(data_ptr, (uint32_t)data_len) : NULL;
    if (data_len && !src) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    uint8_t stk[1024];
    uint8_t* buf = stk;
    uint32_t need = (uint32_t)data_len + 1;
    bool heap = false;
    if (need > sizeof(stk)) { buf = (uint8_t*)malloc(need); if (!buf) { WAPI_RET_I32(WAPI_ERR_NOMEM); return NULL; } heap = true; }
    hid_prep_buffer((uint8_t)report_id, src, (uint32_t)data_len, buf, need);
    int w = wapi_plat_hid_write_report(d->hid_dev, buf, (int)need);
    if (heap) free(buf);
    WAPI_RET_I32(w >= 0 ? WAPI_OK : WAPI_ERR_IO);
    return NULL;
}

static wasm_trap_t* h_hid_send_feature(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h         = WAPI_ARG_I32(0);
    uint32_t report_id = WAPI_ARG_U32(1);
    uint32_t data_ptr  = WAPI_ARG_U32(2);
    uint64_t data_len  = WAPI_ARG_U64(3);
    wapi_input_device_t* d = dev_get(h);
    if (!d || d->kind != WAPI_DEVICE_HID || !d->hid_dev) {
        WAPI_RET_I32(WAPI_ERR_BADF); return NULL;
    }
    if (data_len > 65535) { WAPI_RET_I32(WAPI_ERR_RANGE); return NULL; }
    const void* src = data_len ? wapi_wasm_ptr(data_ptr, (uint32_t)data_len) : NULL;
    if (data_len && !src) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    uint8_t stk[1024];
    uint8_t* buf = stk;
    uint32_t need = (uint32_t)data_len + 1;
    bool heap = false;
    if (need > sizeof(stk)) { buf = (uint8_t*)malloc(need); if (!buf) { WAPI_RET_I32(WAPI_ERR_NOMEM); return NULL; } heap = true; }
    hid_prep_buffer((uint8_t)report_id, src, (uint32_t)data_len, buf, need);
    int w = wapi_plat_hid_send_feature(d->hid_dev, buf, (int)need);
    if (heap) free(buf);
    WAPI_RET_I32(w >= 0 ? WAPI_OK : WAPI_ERR_IO);
    return NULL;
}

static wasm_trap_t* h_hid_receive_report(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h       = WAPI_ARG_I32(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint64_t buf_len = WAPI_ARG_U64(2);
    uint32_t out_ptr = WAPI_ARG_U32(3);
    wapi_input_device_t* d = dev_get(h);
    if (!d || d->kind != WAPI_DEVICE_HID || !d->hid_dev) {
        WAPI_RET_I32(WAPI_ERR_BADF); return NULL;
    }
    if (buf_len > 65535) { WAPI_RET_I32(WAPI_ERR_RANGE); return NULL; }
    void* dst = wapi_wasm_ptr(buf_ptr, (uint32_t)buf_len);
    if (!dst && buf_len) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    /* Non-blocking: HID devices are polled, not owned; host does the
     * blocking (timeout 0) so the guest I/O stays cooperative. */
    int got = wapi_plat_hid_read_report(d->hid_dev, dst, (int)buf_len, 0);
    if (got < 0) { WAPI_RET_I32(WAPI_ERR_IO); return NULL; }
    uint64_t got_u64 = (uint64_t)got;
    wapi_wasm_write_u64(out_ptr, got_u64);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ---- Hotkeys ---- */

static wasm_trap_t* h_hotkey_register(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t binding_ptr = WAPI_ARG_U32(0);
    uint32_t out_id_ptr  = WAPI_ARG_U32(1);

    /* wapi_hotkey_binding_t (16B): u32 device_type, u32 device_id, u32 modifiers, u32 code */
    void* src = wapi_wasm_ptr(binding_ptr, 16);
    if (!src) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    uint32_t fields[4];
    memcpy(fields, src, 16);
    uint32_t device_type = fields[0];
    uint32_t modifiers   = fields[2];
    uint32_t code        = fields[3];

    /* Only keyboard bindings are supported on Win32. Gamepad/HID
     * hotkeys need the HID backend (not yet online on this host). */
    if (device_type != WAPI_DEVICE_KEYBOARD) {
        WAPI_RET_I32(WAPI_ERR_NOTSUP); return NULL;
    }

    /* Guest-assigned ids must be unique; the platform layer handles
     * collisions on the RegisterHotKey side. Use the scancode +
     * modifier bits as the id seed so unregister can find it. */
    static uint32_t s_next_hotkey = 1;
    uint32_t id = s_next_hotkey++;
    if (s_next_hotkey == 0) s_next_hotkey = 1;

    if (!wapi_plat_hotkey_register(id, modifiers, code)) {
        WAPI_RET_I32(WAPI_ERR_BUSY); return NULL;
    }
    wapi_wasm_write_u32(out_id_ptr, id);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_hotkey_unregister(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint32_t id = WAPI_ARG_U32(0);
    wapi_plat_hotkey_unregister(id);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_pointer_get_info(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  h  = WAPI_ARG_I32(0);
    uint32_t pi = WAPI_ARG_U32(1);
    (void)h;
    /* wapi_pointer_info_t (16B): {u8 has_pressure, u8 has_tilt, u8 has_twist,
     *                             u8 has_width_height, u8[12] _reserved} */
    uint8_t info[16] = {0};
    info[0] = 1; info[1] = 1; info[2] = 1; info[3] = 1;
    void* dst = wapi_wasm_ptr(pi, 16);
    if (!dst) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    memcpy(dst, info, 16);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_pointer_get_position(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t h  = WAPI_ARG_I32(0);
    int32_t sh = WAPI_ARG_I32(1);
    uint32_t xp = WAPI_ARG_U32(2), yp = WAPI_ARG_U32(3);
    (void)h; (void)sh;
    wapi_wasm_write_f32(xp, s_ptr_state.x);
    wapi_wasm_write_f32(yp, s_ptr_state.y);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_pointer_get_buttons(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)args; (void)nargs; (void)nresults;
    WAPI_RET_I32((int32_t)s_ptr_state.buttons);
    return NULL;
}

/* ---- IME ---- */

static wasm_trap_t* h_ime_enable(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t sh   = WAPI_ARG_I32(0);
    int32_t hint = WAPI_ARG_I32(1);
    (void)hint; /* Platform IME doesn't yet consume the hint */
    if (!wapi_handle_valid(sh, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    wapi_plat_window_start_text_input(g_rt.handles[sh].data.window);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_ime_disable(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t sh = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(sh, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    wapi_plat_window_stop_text_input(g_rt.handles[sh].data.window);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_ime_set_candidate_rect(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t sh = WAPI_ARG_I32(0);
    float x = WAPI_ARG_F32(1), y = WAPI_ARG_F32(2);
    float w = WAPI_ARG_F32(3), hh = WAPI_ARG_F32(4);
    if (!wapi_handle_valid(sh, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    wapi_plat_window_ime_set_candidate_rect(g_rt.handles[sh].data.window,
                                            (int32_t)(x + 0.5f), (int32_t)(y + 0.5f),
                                            (int32_t)(w + 0.5f), (int32_t)(hh + 0.5f));
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_ime_commit_force(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t sh = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(sh, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    wapi_plat_window_ime_force_commit(g_rt.handles[sh].data.window);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_ime_cancel(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t sh = WAPI_ARG_I32(0);
    if (!wapi_handle_valid(sh, WAPI_HTYPE_SURFACE)) { WAPI_RET_I32(WAPI_ERR_BADF); return NULL; }
    wapi_plat_window_ime_force_cancel(g_rt.handles[sh].data.window);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_ime_read_text(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint64_t seq     = WAPI_ARG_U64(0);
    uint32_t buf_ptr = WAPI_ARG_U32(1);
    uint32_t buf_len = WAPI_ARG_U32(2);
    uint32_t out_ptr = WAPI_ARG_U32(3);

    const ime_slot_t* slot = ime_find(seq);
    if (!slot) { WAPI_RET_I32(WAPI_ERR_NOENT); return NULL; }

    if (out_ptr) wapi_wasm_write_u32(out_ptr, slot->text_len);

    uint32_t avail = (slot->text_len < IME_STORE_TEXT_MAX - 1) ? slot->text_len : IME_STORE_TEXT_MAX - 1;
    uint32_t copy  = avail < buf_len ? avail : buf_len;
    if (copy > 0 && buf_ptr) {
        void* dst = wapi_wasm_ptr(buf_ptr, copy);
        if (!dst) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
        memcpy(dst, slot->text, copy);
    }
    WAPI_RET_I32(buf_len < slot->text_len ? WAPI_ERR_RANGE : WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_ime_read_segment(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    uint64_t seq     = WAPI_ARG_U64(0);
    uint32_t index   = WAPI_ARG_U32(1);
    uint32_t out_ptr = WAPI_ARG_U32(2);
    const ime_slot_t* slot = ime_find(seq);
    if (!slot) { WAPI_RET_I32(WAPI_ERR_NOENT); return NULL; }
    if (index >= slot->segment_count) { WAPI_RET_I32(WAPI_ERR_RANGE); return NULL; }
    /* wapi_ime_segment_t (12B): {u32 start, u32 length, u32 flags} */
    uint32_t out[3] = {
        slot->segments[index].start,
        slot->segments[index].length,
        slot->segments[index].flags,
    };
    if (!wapi_wasm_write_bytes(out_ptr, out, sizeof(out))) {
        WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Gamepad — handle is a tracked WAPI_HTYPE_INPUT_DEVICE routing
 * to the XInput slot via `backend_slot`.
 * ============================================================ */

/* Resolve a gamepad handle to its XInput slot; returns -1 on bad handle. */
static int gamepad_slot_from_handle(int32_t gh) {
    wapi_input_device_t* d = dev_get(gh);
    if (!d || d->kind != WAPI_DEVICE_GAMEPAD) return -1;
    if (d->backend_slot >= WAPI_GAMEPAD_SLOTS) return -1;
    return (int)d->backend_slot;
}

static wasm_trap_t* h_gamepad_get_info(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  gh       = WAPI_ARG_I32(0);
    uint32_t info_ptr = WAPI_ARG_U32(1);
    int slot = gamepad_slot_from_handle(gh);
    if (slot < 0 || !wapi_plat_gamepad_connected((uint32_t)slot) || !info_ptr) {
        WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }
    /* wapi_gamepad_info_t (48B, align 4):
     *   +0  u32 type              (WAPI_GAMEPAD_TYPE_XBOX360/ONE for XInput)
     *   +4  u16 vendor_id
     *   +6  u16 product_id
     *   +8  u8  has_rumble
     *   +9  u8  has_trigger_rumble
     *  +10  u8  has_led
     *  +11  u8  has_sensors
     *  +12  u8  has_touchpad
     *  +13  u8  battery_percent
     *  +14  u8  battery (wapi_gamepad_battery_t)
     *  +15  u8  _pad
     *  +16  u8[32] _reserved
     */
    uint8_t bp = wapi_plat_gamepad_battery_percent((uint32_t)slot);
    uint8_t batt = 0; /* UNKNOWN */
    if (bp == 255) { batt = 0; bp = 0; }
    else if (bp == 100) batt = 4; /* WIRED */
    else batt = 1; /* DISCHARGING */

    uint8_t buf[48];
    memset(buf, 0, sizeof(buf));
    uint32_t type = WAPI_GAMEPAD_TYPE_XBOXONE;
    memcpy(buf + 0, &type, 4);
    /* vendor/product unknown through XInput */
    buf[8]  = 1;  /* has_rumble */
    buf[9]  = 1;  /* has_trigger_rumble (XInputOneAPI supports it on triggers for Series pads; we map nominal) */
    buf[10] = 0;  /* has_led */
    buf[11] = 0;  /* has_sensors */
    buf[12] = 0;  /* has_touchpad */
    buf[13] = bp;
    buf[14] = batt;
    if (!wapi_wasm_write_bytes(info_ptr, buf, sizeof(buf))) {
        WAPI_RET_I32(WAPI_ERR_INVAL); return NULL;
    }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_gamepad_get_button(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t gh  = WAPI_ARG_I32(0);
    int32_t btn = WAPI_ARG_I32(1);
    int slot = gamepad_slot_from_handle(gh);
    if (slot < 0 || btn < 0) { WAPI_RET_I32(0); return NULL; }
    WAPI_RET_I32(wapi_plat_gamepad_button_pressed((uint32_t)slot, (uint8_t)btn) ? 1 : 0);
    return NULL;
}

static wasm_trap_t* h_gamepad_get_axis(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  gh       = WAPI_ARG_I32(0);
    int32_t  axis     = WAPI_ARG_I32(1);
    uint32_t out_ptr  = WAPI_ARG_U32(2);
    int slot = gamepad_slot_from_handle(gh);
    if (slot < 0 || axis < 0 || !out_ptr) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    int16_t v = wapi_plat_gamepad_axis_value((uint32_t)slot, (uint8_t)axis);
    if (!wapi_wasm_write_bytes(out_ptr, &v, 2)) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

static wasm_trap_t* h_gamepad_rumble(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  gh   = WAPI_ARG_I32(0);
    uint32_t lo   = WAPI_ARG_U32(1);
    uint32_t hi   = WAPI_ARG_U32(2);
    uint32_t dur  = WAPI_ARG_U32(3);
    int slot = gamepad_slot_from_handle(gh);
    if (slot < 0) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    WAPI_RET_I32(wapi_plat_gamepad_rumble((uint32_t)slot,
                                          (uint16_t)lo, (uint16_t)hi, dur)
                 ? WAPI_OK : WAPI_ERR_NOTSUP);
    return NULL;
}

static wasm_trap_t* h_gamepad_get_battery(void* env, wasmtime_caller_t* caller,
    const wasmtime_val_t* args, size_t nargs,
    wasmtime_val_t* results, size_t nresults)
{
    (void)env; (void)caller; (void)nargs; (void)nresults;
    int32_t  gh      = WAPI_ARG_I32(0);
    uint32_t out_ptr = WAPI_ARG_U32(1);
    int slot = gamepad_slot_from_handle(gh);
    if (slot < 0 || !out_ptr) { WAPI_RET_I32(WAPI_ERR_INVAL); return NULL; }
    uint8_t bp = wapi_plat_gamepad_battery_percent((uint32_t)slot);
    wapi_wasm_write_bytes(out_ptr, &bp, 1);
    WAPI_RET_I32(WAPI_OK);
    return NULL;
}

/* ============================================================
 * Registration
 * ============================================================ */

#define MOD "wapi_input"

void wapi_host_register_input(wasmtime_linker_t* linker) {
    /* Device lifecycle (wapi_seat.h defines seat_t, used only as
     * return value of device_seat — treated as i32 here). */
    WAPI_DEFINE_1_1(linker, MOD, "device_count",    h_device_count);
    WAPI_DEFINE_3_1(linker, MOD, "device_open",     h_device_open);
    WAPI_DEFINE_1_1(linker, MOD, "device_close",    h_device_close);
    WAPI_DEFINE_1_1(linker, MOD, "device_get_type", h_device_get_type);
    WAPI_DEFINE_2_1(linker, MOD, "device_get_uid",  h_device_get_uid);
    /* device_get_name takes (i32 handle, i32 buf, i64 buf_len, i32 out_len) */
    wapi_linker_define(linker, MOD, "device_get_name", h_device_get_name,
        4, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I64, WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
    WAPI_DEFINE_1_1(linker, MOD, "device_seat",     h_device_seat);

    /* Mouse */
    WAPI_DEFINE_2_1(linker, MOD, "mouse_get_info",           h_mouse_get_info);
    WAPI_DEFINE_4_1(linker, MOD, "mouse_get_position",       h_mouse_get_position);
    WAPI_DEFINE_1_1(linker, MOD, "mouse_get_button_state",   h_mouse_get_button_state);
    WAPI_DEFINE_3_1(linker, MOD, "mouse_set_relative",       h_mouse_set_relative);
    /* mouse_warp takes (i32 handle, i32 surface, f32 x, f32 y) -> i32 */
    wapi_linker_define(linker, MOD, "mouse_warp", h_mouse_warp,
        4, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_F32, WASM_F32},
        1, (wasm_valkind_t[]){WASM_I32});
    WAPI_DEFINE_2_1(linker, MOD, "mouse_set_cursor",         h_mouse_set_cursor);
    WAPI_DEFINE_6_1(linker, MOD, "mouse_set_cursor_image",   h_mouse_set_cursor_image);

    /* Keyboard + text input */
    WAPI_DEFINE_2_1(linker, MOD, "keyboard_key_pressed",  h_keyboard_key_pressed);
    WAPI_DEFINE_1_1(linker, MOD, "keyboard_get_modstate", h_keyboard_get_modstate);
    WAPI_DEFINE_1_0(linker, MOD, "start_textinput",       h_start_textinput);
    WAPI_DEFINE_1_0(linker, MOD, "stop_textinput",        h_stop_textinput);

    /* Touch — WM_TOUCH wired in Win32 platform */
    WAPI_DEFINE_2_1(linker, MOD, "touch_get_info",     h_touch_get_info);
    WAPI_DEFINE_1_1(linker, MOD, "touch_finger_count", h_touch_finger_count);
    WAPI_DEFINE_3_1(linker, MOD, "touch_get_finger",   h_touch_get_finger);
    /* Pen — WM_POINTER wired in Win32 platform */
    WAPI_DEFINE_2_1(linker, MOD, "pen_get_info",       h_pen_get_info);
    WAPI_DEFINE_3_1(linker, MOD, "pen_get_axis",       h_pen_get_axis);
    WAPI_DEFINE_3_1(linker, MOD, "pen_get_position",   h_pen_get_position);

    /* Gamepad queries — XInput state, slot index is the handle */
    WAPI_DEFINE_2_1(linker, MOD, "gamepad_get_info",            h_gamepad_get_info);
    WAPI_DEFINE_2_1(linker, MOD, "gamepad_get_button",          h_gamepad_get_button);
    WAPI_DEFINE_3_1(linker, MOD, "gamepad_get_axis",            h_gamepad_get_axis);
    WAPI_DEFINE_4_1(linker, MOD, "gamepad_rumble",              h_gamepad_rumble);
    /* trigger rumble / led / sensors / touchpad: no XInput surface on
     * stock Windows API. Real sources land with the HID backend later. */
    WAPI_DEFINE_4_1(linker, MOD, "gamepad_rumble_triggers",     h_nosys);
    WAPI_DEFINE_4_1(linker, MOD, "gamepad_set_led",             h_nosys);
    WAPI_DEFINE_3_1(linker, MOD, "gamepad_enable_sensor",       h_nosys);
    WAPI_DEFINE_3_1(linker, MOD, "gamepad_get_sensor_data",     h_nosys);
    WAPI_DEFINE_4_1(linker, MOD, "gamepad_get_touchpad_finger", h_nosys);
    WAPI_DEFINE_2_1(linker, MOD, "gamepad_get_battery",         h_gamepad_get_battery);

    /* Pointer (unified) */
    WAPI_DEFINE_2_1(linker, MOD, "pointer_get_info",     h_pointer_get_info);
    WAPI_DEFINE_4_1(linker, MOD, "pointer_get_position", h_pointer_get_position);
    WAPI_DEFINE_1_1(linker, MOD, "pointer_get_buttons",  h_pointer_get_buttons);

    /* HID — permission-gated (auto-grant on Win32 pending broker UI).
     * hid_send_report / hid_send_feature_report / hid_receive_report
     * take (i32 handle, i32 report_id, i32 data_ptr, i64 data_len). */
    WAPI_DEFINE_4_1(linker, MOD, "hid_request_device", h_hid_request_device);
    WAPI_DEFINE_2_1(linker, MOD, "hid_get_info",       h_hid_get_info);
    wapi_linker_define(linker, MOD, "hid_send_report", h_hid_send_report,
        4, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I32, WASM_I64},
        1, (wasm_valkind_t[]){WASM_I32});
    wapi_linker_define(linker, MOD, "hid_send_feature_report", h_hid_send_feature,
        4, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I32, WASM_I64},
        1, (wasm_valkind_t[]){WASM_I32});
    wapi_linker_define(linker, MOD, "hid_receive_report", h_hid_receive_report,
        4, (wasm_valkind_t[]){WASM_I32, WASM_I32, WASM_I64, WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});

    /* IME */
    WAPI_DEFINE_2_1(linker, MOD, "ime_enable",  h_ime_enable);
    WAPI_DEFINE_1_1(linker, MOD, "ime_disable", h_ime_disable);
    wapi_linker_define(linker, MOD, "ime_set_candidate_rect", h_ime_set_candidate_rect,
        5, (wasm_valkind_t[]){WASM_I32, WASM_F32, WASM_F32, WASM_F32, WASM_F32},
        1, (wasm_valkind_t[]){WASM_I32});
    WAPI_DEFINE_1_1(linker, MOD, "ime_commit", h_ime_commit_force);
    WAPI_DEFINE_1_1(linker, MOD, "ime_cancel", h_ime_cancel);
    wapi_linker_define(linker, MOD, "ime_read_text", h_ime_read_text,
        4, (wasm_valkind_t[]){WASM_I64, WASM_I32, WASM_I32, WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});
    wapi_linker_define(linker, MOD, "ime_read_segment", h_ime_read_segment,
        3, (wasm_valkind_t[]){WASM_I64, WASM_I32, WASM_I32},
        1, (wasm_valkind_t[]){WASM_I32});

    /* Hotkeys — Win32 RegisterHotKey; emits WAPI_EVENT_HOTKEY (0x1830) */
    WAPI_DEFINE_2_1(linker, MOD, "hotkey_register",   h_hotkey_register);
    WAPI_DEFINE_1_1(linker, MOD, "hotkey_unregister", h_hotkey_unregister);
}
