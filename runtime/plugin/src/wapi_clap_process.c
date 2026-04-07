/**
 * WAPI CLAP Plugin Wrapper - Audio Processing
 *
 * This file contains THE hot path: the audio thread process callback.
 * It bridges between CLAP's clap_process_t and the WAPI wapi_process_data_t
 * that the Wasm plugin expects.
 *
 * Data flow:
 *   1. Copy input audio buffers from CLAP into Wasm linear memory
 *   2. Convert CLAP events (params, MIDI) to WAPI format in Wasm memory
 *   3. Build wapi_process_data_t in Wasm memory
 *   4. Call Wasm wapi_plugin_process(process_data_ptr)
 *   5. Copy output audio buffers back from Wasm memory to CLAP
 *   6. Push output events (param changes, MIDI) back to CLAP
 */

#include <clap/clap.h>
#include "wapi_plugin_host.h"

/* ============================================================
 * wapi_wasm_process: Core Wasm Process Bridge
 * ============================================================
 * Writes inputs into Wasm memory, calls the Wasm process function,
 * and reads outputs back. This is separated from the CLAP-specific
 * event handling so the bridge logic is clear.
 */

int wapi_wasm_process(wapi_wasm_plugin_t* plugin,
                    float** inputs, uint32_t num_inputs,
                    float** outputs, uint32_t num_outputs,
                    uint32_t num_samples,
                    const wapi_transport_t* transport,
                    const wapi_midi_event_t* midi_events, uint32_t midi_count)
{
    wasmtime_context_t* ctx = wasmtime_store_context(plugin->store);
    uint8_t* mem = wapi_wasm_mem(plugin);
    if (!mem) return WAPI_ERR_UNKNOWN;

    uint32_t max_block = plugin->max_block_size;
    if (num_samples > max_block) num_samples = max_block;

    /* Clamp channel counts to what we allocated scratch for */
    if (num_inputs > WAPI_MAX_CHANNELS)  num_inputs = WAPI_MAX_CHANNELS;
    if (num_outputs > WAPI_MAX_CHANNELS) num_outputs = WAPI_MAX_CHANNELS;
    if (midi_count > WAPI_MAX_MIDI_EVENTS) midi_count = WAPI_MAX_MIDI_EVENTS;

    /* --- Step 1: Write input audio buffers into Wasm memory --- */
    uint32_t bytes_per_channel = num_samples * sizeof(float);
    for (uint32_t ch = 0; ch < num_inputs; ch++) {
        uint32_t dst_offset = plugin->input_buf_offset + ch * max_block * sizeof(float);
        if (inputs[ch]) {
            memcpy(mem + dst_offset, inputs[ch], bytes_per_channel);
        } else {
            memset(mem + dst_offset, 0, bytes_per_channel);
        }

        /* Write the Wasm pointer for this channel into the pointer array */
        uint32_t ptr_val = dst_offset;
        memcpy(mem + plugin->input_ptrs_offset + ch * 4, &ptr_val, 4);
    }

    /* --- Step 1b: Set up output pointer array --- */
    for (uint32_t ch = 0; ch < num_outputs; ch++) {
        uint32_t dst_offset = plugin->output_buf_offset + ch * max_block * sizeof(float);
        /* Zero the output buffer */
        memset(mem + dst_offset, 0, bytes_per_channel);

        uint32_t ptr_val = dst_offset;
        memcpy(mem + plugin->output_ptrs_offset + ch * 4, &ptr_val, 4);
    }

    /* --- Step 2: Write transport into Wasm memory --- */
    /*
     * wapi_transport_t layout (32 bytes):
     *   0: double tempo (8 bytes)
     *   8: double beat_position (8 bytes)
     *  16: int32_t time_sig_num (4 bytes)
     *  20: int32_t time_sig_denom (4 bytes)
     *  24: int32_t sample_pos (4 bytes)
     *  28: uint32_t flags (4 bytes)
     */
    if (transport) {
        memcpy(mem + plugin->transport_offset, transport, sizeof(wapi_transport_t));
    } else {
        memset(mem + plugin->transport_offset, 0, sizeof(wapi_transport_t));
    }

    /* --- Step 3: Write MIDI events into Wasm memory --- */
    /*
     * wapi_midi_event_t layout (8 bytes each):
     *   0: uint32_t sample_offset
     *   4: uint8_t status
     *   5: uint8_t data1
     *   6: uint8_t data2
     *   7: uint8_t _pad
     */
    if (midi_count > 0 && midi_events) {
        memcpy(mem + plugin->midi_buf_offset,
               midi_events,
               midi_count * sizeof(wapi_midi_event_t));
    }

    /* --- Step 4: Build wapi_process_data_t in Wasm memory --- */
    /*
     * wapi_process_data_t layout in wasm32 (36 bytes):
     *   0: inputs         (i32 ptr to float*[])
     *   4: outputs        (i32 ptr to float*[])
     *   8: num_inputs     (u32)
     *  12: num_outputs    (u32)
     *  16: num_samples    (u32)
     *  20: sample_rate    (f32)
     *  24: transport      (i32 ptr to wapi_transport_t)
     *  28: midi_events    (i32 ptr to wapi_midi_event_t[])
     *  32: midi_event_count (u32)
     */
    uint8_t* pd = mem + plugin->process_data_offset;
    uint32_t tmp;

    tmp = plugin->input_ptrs_offset;   memcpy(pd + 0,  &tmp, 4);
    tmp = plugin->output_ptrs_offset;  memcpy(pd + 4,  &tmp, 4);
    memcpy(pd + 8,  &num_inputs, 4);
    memcpy(pd + 12, &num_outputs, 4);
    memcpy(pd + 16, &num_samples, 4);
    memcpy(pd + 20, &plugin->sample_rate, 4);
    tmp = plugin->transport_offset;    memcpy(pd + 24, &tmp, 4);
    tmp = plugin->midi_buf_offset;     memcpy(pd + 28, &tmp, 4);
    memcpy(pd + 32, &midi_count, 4);

    /* --- Step 5: Call Wasm wapi_plugin_process(process_data_ptr) --- */
    wasmtime_val_t call_args[1] = {
        { .kind = WASMTIME_I32, .of.i32 = (int32_t)plugin->process_data_offset }
    };
    wasmtime_val_t call_results[1];
    wasm_trap_t* trap = NULL;
    wasmtime_error_t* err = wasmtime_func_call(ctx, &plugin->fn_process,
                                                call_args, 1,
                                                call_results, 1, &trap);
    if (err) {
        wasmtime_error_delete(err);
        return WAPI_ERR_UNKNOWN;
    }
    if (trap) {
        wasm_trap_delete(trap);
        return WAPI_ERR_UNKNOWN;
    }

    /* Re-fetch memory base (it may have moved if Wasm grew memory) */
    mem = wapi_wasm_mem(plugin);
    if (!mem) return WAPI_ERR_UNKNOWN;

    /* --- Step 6: Copy output audio buffers back from Wasm memory --- */
    for (uint32_t ch = 0; ch < num_outputs; ch++) {
        uint32_t src_offset = plugin->output_buf_offset + ch * max_block * sizeof(float);
        if (outputs[ch]) {
            memcpy(outputs[ch], mem + src_offset, bytes_per_channel);
        }
    }

    return call_results[0].of.i32;
}

/* ============================================================
 * CLAP Process Callback
 * ============================================================
 * Converts CLAP events to WAPI format, calls wapi_wasm_process,
 * then pushes output events back to CLAP.
 */

clap_process_status plugin_process(const clap_plugin_t* plug,
                                   const clap_process_t* process)
{
    wapi_wasm_plugin_t* p = (wapi_wasm_plugin_t*)plug->plugin_data;
    if (!p->processing || !p->activated) return CLAP_PROCESS_ERROR;

    /* --- Extract audio buffers from CLAP --- */
    float* inputs[WAPI_MAX_CHANNELS];
    float* outputs[WAPI_MAX_CHANNELS];
    uint32_t num_inputs = 0;
    uint32_t num_outputs = 0;

    /* CLAP audio_inputs is an array of clap_audio_buffer_t.
     * Each buffer has channel_count channels. We flatten them
     * into a simple array of float* for the WAPI bridge. */
    for (uint32_t i = 0; i < process->audio_inputs_count && num_inputs < WAPI_MAX_CHANNELS; i++) {
        const clap_audio_buffer_t* buf = &process->audio_inputs[i];
        for (uint32_t ch = 0; ch < buf->channel_count && num_inputs < WAPI_MAX_CHANNELS; ch++) {
            inputs[num_inputs++] = buf->data32[ch];
        }
    }

    for (uint32_t i = 0; i < process->audio_outputs_count && num_outputs < WAPI_MAX_CHANNELS; i++) {
        clap_audio_buffer_t* buf = &process->audio_outputs[i];
        for (uint32_t ch = 0; ch < buf->channel_count && num_outputs < WAPI_MAX_CHANNELS; ch++) {
            outputs[num_outputs++] = buf->data32[ch];
        }
    }

    /* --- Process CLAP input events --- */
    wapi_transport_t transport;
    memset(&transport, 0, sizeof(transport));

    /* Convert CLAP transport to WAPI transport */
    if (process->transport) {
        if (process->transport->flags & CLAP_TRANSPORT_HAS_TEMPO)
            transport.tempo = process->transport->tempo;
        if (process->transport->flags & CLAP_TRANSPORT_HAS_BEATS_TIMELINE)
            transport.beat_position = (double)process->transport->song_pos_beats / CLAP_BEATTIME_FACTOR;
        if (process->transport->flags & CLAP_TRANSPORT_HAS_TIME_SIGNATURE) {
            transport.time_sig_num = (int32_t)process->transport->tsig_num;
            transport.time_sig_denom = (int32_t)process->transport->tsig_denom;
        }
        transport.sample_pos = (int32_t)(process->transport->song_pos_seconds / CLAP_SECTIME_FACTOR);
        if (process->transport->flags & CLAP_TRANSPORT_IS_PLAYING)
            transport.flags |= WAPI_TRANSPORT_PLAYING;
        if (process->transport->flags & CLAP_TRANSPORT_IS_RECORDING)
            transport.flags |= WAPI_TRANSPORT_RECORDING;
        if (process->transport->flags & CLAP_TRANSPORT_IS_LOOP_ACTIVE)
            transport.flags |= WAPI_TRANSPORT_LOOPING;
    }

    /* Accumulate MIDI events and handle parameter changes */
    wapi_midi_event_t midi_events[WAPI_MAX_MIDI_EVENTS];
    uint32_t midi_count = 0;

    /* Clear output state */
    p->midi_out_count = 0;
    memset(p->param_changed, 0, sizeof(bool) * p->param_count);

    const clap_input_events_t* in_events = process->in_events;
    uint32_t event_count = in_events->size(in_events);

    for (uint32_t i = 0; i < event_count; i++) {
        const clap_event_header_t* hdr = in_events->get(in_events, i);
        if (hdr->space_id != CLAP_CORE_EVENT_SPACE_ID) continue;

        switch (hdr->type) {
            case CLAP_EVENT_PARAM_VALUE: {
                const clap_event_param_value_t* ev = (const clap_event_param_value_t*)hdr;
                /* Update our cached param value */
                for (uint32_t j = 0; j < p->param_count; j++) {
                    if (p->params[j].id == ev->param_id) {
                        p->param_values[j] = (float)ev->value;

                        /* Notify the Wasm module */
                        wasmtime_context_t* ctx = wasmtime_store_context(p->store);
                        wasmtime_val_t args[2] = {
                            { .kind = WASMTIME_I32, .of.i32 = (int32_t)ev->param_id },
                            { .kind = WASMTIME_F32, .of.f32 = (float)ev->value },
                        };
                        wasm_trap_t* trap = NULL;
                        wasmtime_error_t* err = wasmtime_func_call(
                            ctx, &p->fn_param_changed, args, 2, NULL, 0, &trap);
                        if (err) wasmtime_error_delete(err);
                        if (trap) wasm_trap_delete(trap);
                        break;
                    }
                }
                break;
            }

            case CLAP_EVENT_MIDI: {
                if (midi_count < WAPI_MAX_MIDI_EVENTS) {
                    const clap_event_midi_t* ev = (const clap_event_midi_t*)hdr;
                    midi_events[midi_count].sample_offset = hdr->time;
                    midi_events[midi_count].status = ev->data[0];
                    midi_events[midi_count].data1 = ev->data[1];
                    midi_events[midi_count].data2 = ev->data[2];
                    midi_events[midi_count]._pad = 0;
                    midi_count++;
                }
                break;
            }

            default:
                break;
        }
    }

    /* --- Call the Wasm process function --- */
    int result = wapi_wasm_process(p,
                                 inputs, num_inputs,
                                 outputs, num_outputs,
                                 process->frames_count,
                                 &transport,
                                 midi_events, midi_count);

    /* --- Push output events back to CLAP --- */
    const clap_output_events_t* out_events = process->out_events;

    /* Parameter changes from the Wasm module (e.g., GUI interaction) */
    for (uint32_t i = 0; i < p->param_count; i++) {
        if (p->param_changed[i]) {
            clap_event_param_value_t ev;
            memset(&ev, 0, sizeof(ev));
            ev.header.size = sizeof(ev);
            ev.header.type = CLAP_EVENT_PARAM_VALUE;
            ev.header.time = 0;
            ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            ev.header.flags = 0;
            ev.param_id = p->params[i].id;
            ev.value = (double)p->param_values[i];
            out_events->try_push(out_events, &ev.header);
            p->param_changed[i] = false;
        }
    }

    /* MIDI output events (from Wasm calling wapi_plugin_send_midi) */
    for (uint32_t i = 0; i < p->midi_out_count; i++) {
        clap_event_midi_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.header.size = sizeof(ev);
        ev.header.type = CLAP_EVENT_MIDI;
        ev.header.time = p->midi_out[i].sample_offset;
        ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        ev.header.flags = 0;
        ev.port_index = 0;
        ev.data[0] = p->midi_out[i].status;
        ev.data[1] = p->midi_out[i].data1;
        ev.data[2] = p->midi_out[i].data2;
        out_events->try_push(out_events, &ev.header);
    }
    p->midi_out_count = 0;

    /* Map WAPI result to CLAP process status */
    if (WAPI_FAILED(result)) return CLAP_PROCESS_ERROR;
    return CLAP_PROCESS_CONTINUE;
}
