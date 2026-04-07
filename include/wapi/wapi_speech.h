/**
 * WAPI - Speech Capability
 * Version 1.0.0
 *
 * Maps to: Web Speech API (SpeechSynthesis, SpeechRecognition),
 *          AVSpeechSynthesizer (iOS), Android TTS/SpeechRecognizer
 *
 * Import module: "wapi_speech"
 *
 * Query availability with wapi_capability_supported("wapi.speech", 9)
 */

#ifndef WAPI_SPEECH_H
#define WAPI_SPEECH_H

#include "wapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Speech Synthesis (Text-to-Speech)
 * ============================================================ */

/**
 * Utterance descriptor.
 *
 * Layout (28 bytes, align 4):
 *   Offset  0: ptr      text
 *   Offset  4: uint32_t text_len
 *   Offset  8: ptr      lang       BCP 47 tag (e.g., "en-US"), NULL for default
 *   Offset 12: uint32_t lang_len
 *   Offset 16: float    rate       0.1-10.0, 1.0 = normal
 *   Offset 20: float    pitch      0.0-2.0, 1.0 = normal
 *   Offset 24: float    volume     0.0-1.0
 */
typedef struct wapi_speech_utterance_t {
    const char* text;
    wapi_size_t   text_len;
    const char* lang;
    wapi_size_t   lang_len;
    float       rate;
    float       pitch;
    float       volume;
} wapi_speech_utterance_t;

/**
 * Speak text.
 * @param utterance  Text and voice parameters.
 * @param id         [out] Utterance handle (for cancel).
 */
WAPI_IMPORT(wapi_speech, speak)
wapi_result_t wapi_speech_speak(const wapi_speech_utterance_t* utterance,
                             wapi_handle_t* id);

/**
 * Cancel an in-progress utterance.
 */
WAPI_IMPORT(wapi_speech, cancel)
wapi_result_t wapi_speech_cancel(wapi_handle_t id);

/**
 * Cancel all speech.
 */
WAPI_IMPORT(wapi_speech, cancel_all)
wapi_result_t wapi_speech_cancel_all(void);

/**
 * Check if speech synthesis is currently speaking.
 */
WAPI_IMPORT(wapi_speech, is_speaking)
wapi_bool_t wapi_speech_is_speaking(void);

/* ============================================================
 * Speech Recognition (Speech-to-Text)
 * ============================================================ */

/**
 * Start speech recognition.
 *
 * @param lang       BCP 47 language tag (NULL for default).
 * @param lang_len   Language tag length.
 * @param continuous If true, keep recognizing until stopped.
 * @param session    [out] Recognition session handle.
 */
WAPI_IMPORT(wapi_speech, recognize_start)
wapi_result_t wapi_speech_recognize_start(const char* lang, wapi_size_t lang_len,
                                       wapi_bool_t continuous, wapi_handle_t* session);

/**
 * Stop speech recognition.
 */
WAPI_IMPORT(wapi_speech, recognize_stop)
wapi_result_t wapi_speech_recognize_stop(wapi_handle_t session);

/**
 * Get the latest recognition result.
 *
 * @param session     Session handle.
 * @param buf         Buffer for recognized text (UTF-8).
 * @param buf_len     Buffer capacity.
 * @param text_len    [out] Actual text length.
 * @param confidence  [out] Confidence 0.0-1.0.
 * @return WAPI_OK on success, WAPI_ERR_AGAIN if no result yet.
 */
WAPI_IMPORT(wapi_speech, recognize_result)
wapi_result_t wapi_speech_recognize_result(wapi_handle_t session, char* buf,
                                        wapi_size_t buf_len, wapi_size_t* text_len,
                                        float* confidence);

#ifdef __cplusplus
}
#endif

#endif /* WAPI_SPEECH_H */
