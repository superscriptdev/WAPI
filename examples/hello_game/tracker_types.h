/**
 * Shared layout between hello_game and the tracker child module.
 * Both sides include this header so struct offsets agree.
 */

#ifndef HELLO_GAME_TRACKER_TYPES_H
#define HELLO_GAME_TRACKER_TYPES_H

#include <stdint.h>

#define TRACKER_WAVE_SINE      0
#define TRACKER_WAVE_SAW       1
#define TRACKER_WAVE_SQUARE    2
#define TRACKER_WAVE_TRIANGLE  3
#define TRACKER_WAVE_NOISE     4

typedef struct tracker_instrument_t {
    uint32_t waveform;   /* TRACKER_WAVE_* */
    float    attack;     /* seconds, 0..N */
    float    decay;      /* seconds */
    float    sustain;    /* 0..1 level after decay */
    float    release;    /* seconds after note_off */
    float    volume;     /* 0..1 */
} tracker_instrument_t;

typedef struct tracker_note_t {
    uint32_t start_sample;     /* absolute sample index within loop */
    uint32_t length_samples;   /* note_on duration; release tail extends it */
    uint32_t instrument;       /* index into instruments[] */
    uint16_t midi_note;        /* 0..127 */
    uint16_t velocity;         /* 0..127 */
} tracker_note_t;

/* Request struct the game writes into shared memory. The tracker reads
 * it, pulls notes/instruments from the linked offsets, and writes S16
 * mono samples into out_off. */
typedef struct tracker_request_t {
    uint32_t sample_rate;
    uint32_t num_instruments;
    uint32_t num_notes;
    uint32_t out_samples;         /* number of mono S16 samples to render */
    uint64_t instruments_off;     /* shared-mem offset -> tracker_instrument_t[] */
    uint64_t notes_off;           /* shared-mem offset -> tracker_note_t[] */
    uint64_t out_off;             /* shared-mem offset -> int16_t[] */
} tracker_request_t;

#endif /* HELLO_GAME_TRACKER_TYPES_H */
