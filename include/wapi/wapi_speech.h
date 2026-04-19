/**
 * WAPI - Speech
 * Version 1.0.0
 *
 * Maps to: Web Speech API (SpeechSynthesis, SpeechRecognition),
 *          AVSpeechSynthesizer (iOS), Android TTS/SpeechRecognizer
 *
 * Import module: "wapi_speech"
 */

#ifndef WAPI_SPEECH_H
#define WAPI_SPEECH_H

#include "wapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Speech Synthesis (Text-to-Speech)
 * ============================================================ */

/**
 * Utterance descriptor.
 *
 * Layout (48 bytes, align 8):
 *   Offset  0: wapi_stringview_t text
 *   Offset 16: wapi_stringview_t lang  BCP 47 tag (e.g., "en-US"), NULL for default
 *   Offset 32: float    rate       0.1-10.0, 1.0 = normal
 *   Offset 36: float    pitch      0.0-2.0, 1.0 = normal
 *   Offset 40: float    volume     0.0-1.0
 *   Offset 44: uint32_t _pad
 */
typedef struct wapi_speech_utterance_t {
    wapi_stringview_t text;
    wapi_stringview_t lang;
    float       rate;
    float       pitch;
    float       volume;
    uint32_t    _pad;
} wapi_speech_utterance_t;

/** Submit an utterance. Completion = synthesis finished. */
static inline wapi_result_t wapi_speech_speak(
    const wapi_io_t* io, const wapi_speech_utterance_t* utt,
    wapi_handle_t* out_id, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_SPEECH_SPEAK;
    op.addr       = (uint64_t)(uintptr_t)utt;
    op.len        = sizeof(*utt);
    op.result_ptr = (uint64_t)(uintptr_t)out_id;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Cancel an outstanding speak by its user_data. */
static inline wapi_result_t wapi_speech_cancel(
    const wapi_io_t* io, uint64_t speak_user_data)
{
    return io->cancel(io->impl, speak_user_data);
}

/** Bounded-local: is any utterance currently speaking? */
WAPI_IMPORT(wapi_speech, is_speaking)
wapi_bool_t wapi_speech_is_speaking(void);

/** Bounded-local: cancel everything (fire-and-forget). */
WAPI_IMPORT(wapi_speech, cancel_all)
wapi_result_t wapi_speech_cancel_all(void);

/* ============================================================
 * Speech Recognition (async, submitted via wapi_io_t)
 * ============================================================ */

static inline wapi_result_t wapi_speech_recognize_start(
    const wapi_io_t* io, wapi_stringview_t lang, wapi_bool_t continuous,
    wapi_handle_t* out_session, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode     = WAPI_IO_OP_SPEECH_RECOGNIZE_START;
    op.flags      = continuous ? 1 : 0;
    op.addr       = lang.data;
    op.len        = lang.length;
    op.result_ptr = (uint64_t)(uintptr_t)out_session;
    op.user_data  = user_data;
    return io->submit(io->impl, &op, 1);
}

/** Stop recognition — cancels the active recognize_start subscription. */
static inline wapi_result_t wapi_speech_recognize_stop(
    const wapi_io_t* io, uint64_t start_user_data)
{
    return io->cancel(io->impl, start_user_data);
}

/** Pull the latest recognition result. Confidence arrives in the
 *  completion's payload[0..3] as f32 on success. */
static inline wapi_result_t wapi_speech_recognize_result(
    const wapi_io_t* io, wapi_handle_t session,
    char* buf, wapi_size_t buf_len, uint64_t user_data)
{
    wapi_io_op_t op = {0};
    op.opcode    = WAPI_IO_OP_SPEECH_RECOGNIZE_RESULT;
    op.fd        = session;
    op.addr      = (uint64_t)(uintptr_t)buf;
    op.len       = buf_len;
    op.user_data = user_data;
    return io->submit(io->impl, &op, 1);
}

#ifdef __cplusplus
}
#endif

#endif /* WAPI_SPEECH_H */
