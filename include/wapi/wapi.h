/**
 * WAPI - Master Header
 * Version 1.0.0
 *
 * One binary. One ABI. Every platform.
 *
 * Include this header to get access to the complete WAPI.
 * Each capability module can also be included individually.
 *
 * The WAPI is a C-style calling convention document.
 * Wasm modules import these functions and the host implements them.
 * GPU access uses webgpu.h directly (from webgpu-native/webgpu-headers).
 * Include it alongside this header for full GPU functionality.
 */

#ifndef WAPI_H
#define WAPI_H

/* Core types: handles, errors, strings, buffers, chained structs */
#include "wapi_types.h"

/* Context, allocator, and I/O vtable */
#include "wapi_context.h"

/* Capability detection and enumeration */
#include "wapi_capability.h"

/* Environment: args, env vars, random, exit */
#include "wapi_env.h"

/* Host-provided memory allocation */
#include "wapi_memory.h"

/* Async I/O: submit/poll/wait (io_uring pattern) */
#include "wapi_io.h"

/* Clocks, timers, sleep */
#include "wapi_clock.h"

/* Capability-based filesystem (WASI P1 shaped) */
#include "wapi_fs.h"

/* QUIC/WebTransport networking */
#include "wapi_net.h"

/* GPU compute and rendering (webgpu.h bridge) */
#include "wapi_gpu.h"

/* Surfaces: render targets (on-screen and offscreen) */
#include "wapi_surface.h"

/* Window management for on-screen surfaces */
#include "wapi_window.h"

/* Central event queue: poll/wait for all platform events */
#include "wapi_event.h"

/* Input devices: mouse, keyboard, touch, pen, gamepad, HID */
#include "wapi_input.h"

/* Audio playback and recording (SDL3 stream shaped) */
#include "wapi_audio.h"

/* Content tree declaration: semantic nodes for a11y, keyboard nav, indexing */
#include "wapi_content.h"

/* Text: declaration types, shaping, and layout */
#include "wapi_text.h"

/* System clipboard */
#include "wapi_clipboard.h"

/* Thread management */
#include "wapi_thread.h"

/* Thread synchronization: mutex, rwlock, semaphore, condvar */
#include "wapi_sync.h"

/* Subprocess spawning and control */
#include "wapi_process.h"

/* Native file dialogs and message boxes */
#include "wapi_dialog.h"

/* Shared module linking and inter-component communication */
#include "wapi_module.h"

/* Font system queries */
#include "wapi_font.h"

/* Video/media playback */
#include "wapi_video.h"

/* Geolocation / GPS */
#include "wapi_geolocation.h"

/* System notifications */
#include "wapi_notifications.h"

/* Sensors: accelerometer, gyroscope, etc. */
#include "wapi_sensors.h"

/* Text-to-speech, speech recognition */
#include "wapi_speech.h"

/* Cryptographic hashing, encryption, signing */
#include "wapi_crypto.h"

/* Biometric authentication */
#include "wapi_biometric.h"

/* System share sheet */
#include "wapi_share.h"

/* Persistent key-value storage */
#include "wapi_kv_storage.h"

/* Payments (Apple Pay, Google Pay) */
#include "wapi_payments.h"

/* USB device access */
#include "wapi_usb.h"

/* MIDI input/output */
#include "wapi_midi.h"

/* Bluetooth LE / GATT */
#include "wapi_bluetooth.h"

/* Camera capture */
#include "wapi_camera.h"

/* XR: VR/AR (WebXR, OpenXR) */
#include "wapi_xr.h"

/* Audio plugin host interface (VST3/AU/LV2/CLAP equivalent) */
#include "wapi_audio_plugin.h"

/* App registration: URL schemes, file types, preview providers */
#include "wapi_register.h"

/* Taskbar/dock: progress, badges, attention */
#include "wapi_taskbar.h"

/* Runtime permission queries */
#include "wapi_permissions.h"

/* Wake lock: prevent screen/system sleep */
#include "wapi_wake_lock.h"

/* Screen orientation queries and lock */
#include "wapi_orientation.h"

/* Low-level video/audio encode/decode */
#include "wapi_codec.h"

/* Streaming compression/decompression */
#include "wapi_compression.h"

/* OS media session (now-playing metadata) */
#include "wapi_media_session.h"

/* Codec capability queries */
#include "wapi_media_caps.h"

/* Text encoding conversion */
#include "wapi_encoding.h"

/* Web Authentication (passkeys/FIDO2) */
#include "wapi_authn.h"

/* Network connection info */
#include "wapi_network_info.h"

/* Battery status */
#include "wapi_battery.h"

/* Idle detection */
#include "wapi_idle.h"

/* Haptics and vibration */
#include "wapi_haptics.h"

/* Peer-to-peer data channels */
#include "wapi_p2p.h"

/* Serial port communication */
#include "wapi_serial.h"

/* Screen capture */
#include "wapi_screen_capture.h"

/* Contact picker */
#include "wapi_contacts.h"

/* Barcode detection */
#include "wapi_barcode.h"

/* NFC tag reading/writing */
#include "wapi_nfc.h"

/* Drag and drop */
#include "wapi_dnd.h"

/* Platform detection and system info */
#include "wapi_sysinfo.h"

/* Multi-monitor display info */
#include "wapi_display.h"

/* System theme / appearance */
#include "wapi_theme.h"

/* System tray / menu bar extras */
#include "wapi_tray.h"

/* Global hotkeys */
#include "wapi_hotkey.h"

/* File/directory change monitoring */
#include "wapi_file_watcher.h"

/* Native context menus and menu bars */
#include "wapi_menu.h"

/* Screen color picker */
#include "wapi_eyedropper.h"

#endif /* WAPI_H */
