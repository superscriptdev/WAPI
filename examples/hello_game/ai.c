/**
 * WAPI hello_game — AI child module.
 *
 * Exports a pure `ai_tick(shared_off, count)` function the parent calls
 * through wapi_module_call. The parent writes an enemy array to shared
 * memory, passes the offset + count, this module reads/advances/writes
 * the same region via wapi_module_shared_read / shared_write.
 *
 * No host imports beyond the reactor shim (submit / poll / shared_*).
 * Deterministic — no random source, no time.
 */

#include <wapi/wapi.h>
#include <wapi/wapi_module.h>

/* Layout must match the parent's enemy_t in game.c. */
typedef struct enemy_t {
    float x, y;     /* position (pixels) */
    float vx, vy;   /* velocity (pixels/frame) */
    uint32_t color; /* 0xRRGGBBAA, untouched */
    uint32_t alive; /* 1 = active, 0 = recycled */
} enemy_t;

/* Simple playfield bounds shared with the parent. */
#define PLAY_W 800.0f
#define PLAY_H 600.0f

/* Entry point: advances every alive enemy by its velocity, bounces off
 * the vertical walls, retires enemies that fall off the bottom. */
WAPI_EXPORT(ai_tick)
int32_t ai_tick(int64_t shared_off, int64_t count) {
    if (count <= 0 || count > 1024) return 0;

    /* Pull state out of shared memory. Using stack storage — the guest
     * stack is well above the AI's working-set. */
    enemy_t enemies[256];
    if (count > 256) count = 256;
    int64_t bytes = count * (int64_t)sizeof(enemy_t);
    if (wapi_module_shared_read((wapi_size_t)shared_off,
                                enemies, (wapi_size_t)bytes) != WAPI_OK) {
        return -1;
    }

    int active = 0;
    for (int64_t i = 0; i < count; i++) {
        enemy_t* e = &enemies[i];
        if (!e->alive) continue;

        e->x += e->vx;
        e->y += e->vy;

        /* Bounce off the left / right walls. */
        if (e->x < 0.0f)        { e->x = 0.0f;        e->vx = -e->vx; }
        if (e->x > PLAY_W - 16) { e->x = PLAY_W - 16; e->vx = -e->vx; }

        /* Retire past the bottom. */
        if (e->y > PLAY_H) {
            e->alive = 0;
            continue;
        }
        active++;
    }

    if (wapi_module_shared_write((wapi_size_t)shared_off,
                                 enemies, (wapi_size_t)bytes) != WAPI_OK) {
        return -1;
    }
    return active;
}

/* No wapi_main / wapi_frame — this is a pure library module, only
 * ai_tick is exported. The parent is responsible for its lifecycle. */
