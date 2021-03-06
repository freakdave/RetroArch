/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __DRIVER__H
#define __DRIVER__H

#include <sys/types.h>
#include "boolean.h"
#include "libretro.h"
#include <stdlib.h>
#include <stdint.h>
#include "msvc/msvc_compat.h"
#include "gfx/scaler/scaler.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_COMMAND
#include "command.h"
#endif

#define AUDIO_CHUNK_SIZE_BLOCKING 64
#define AUDIO_CHUNK_SIZE_NONBLOCKING 2048 // So we don't get complete line-noise when fast-forwarding audio.
#define AUDIO_MAX_RATIO 16

// libretro has 16 buttons from 0-15 (libretro.h)
// Analog binds use RETRO_DEVICE_ANALOG, but we follow the same scheme internally
// in RetroArch for simplicity,
// so they are mapped into [16, 23].
#define RARCH_FIRST_CUSTOM_BIND 16
#define RARCH_FIRST_META_KEY RARCH_CUSTOM_BIND_LIST_END
enum // RetroArch specific bind IDs.
{
   // Custom binds that extend the scope of RETRO_DEVICE_JOYPAD for RetroArch specifically.
   // Turbo
   RARCH_TURBO_ENABLE = RARCH_FIRST_CUSTOM_BIND, 

   // Analogs (RETRO_DEVICE_ANALOG)
   RARCH_ANALOG_LEFT_X_PLUS,
   RARCH_ANALOG_LEFT_X_MINUS,
   RARCH_ANALOG_LEFT_Y_PLUS,
   RARCH_ANALOG_LEFT_Y_MINUS,
   RARCH_ANALOG_RIGHT_X_PLUS,
   RARCH_ANALOG_RIGHT_X_MINUS,
   RARCH_ANALOG_RIGHT_Y_PLUS,
   RARCH_ANALOG_RIGHT_Y_MINUS,

   RARCH_CUSTOM_BIND_LIST_END,

   // Command binds. Not related to game input, only usable for port 0.
   RARCH_FAST_FORWARD_KEY = RARCH_FIRST_META_KEY,
   RARCH_FAST_FORWARD_HOLD_KEY,
   RARCH_LOAD_STATE_KEY,
   RARCH_SAVE_STATE_KEY,
   RARCH_FULLSCREEN_TOGGLE_KEY,
   RARCH_QUIT_KEY,
   RARCH_STATE_SLOT_PLUS,
   RARCH_STATE_SLOT_MINUS,
   RARCH_REWIND,
   RARCH_MOVIE_RECORD_TOGGLE,
   RARCH_PAUSE_TOGGLE,
   RARCH_FRAMEADVANCE,
   RARCH_RESET,
   RARCH_SHADER_NEXT,
   RARCH_SHADER_PREV,
   RARCH_CHEAT_INDEX_PLUS,
   RARCH_CHEAT_INDEX_MINUS,
   RARCH_CHEAT_TOGGLE,
   RARCH_SCREENSHOT,
   RARCH_DSP_CONFIG,
   RARCH_MUTE,
   RARCH_NETPLAY_FLIP,
   RARCH_SLOWMOTION,

#ifdef RARCH_CONSOLE
   RARCH_CHEAT_INPUT,
   RARCH_SRAM_WRITE_PROTECT,
#endif

   RARCH_BIND_LIST_END,
   RARCH_BIND_LIST_END_NULL
};

struct retro_keybind
{
   bool valid;
   int id;
   enum retro_key key;

   // PC only uses lower 16-bits.
   // Full 64-bit can be used for port-specific purposes, like simplifying multiple binds, etc.
   uint64_t joykey;

   uint32_t joyaxis;
};

enum rarch_shader_type
{
   RARCH_SHADER_CG,
   RARCH_SHADER_BSNES,
   RARCH_SHADER_AUTO,
   RARCH_SHADER_NONE
};

typedef struct video_info
{
   unsigned width;
   unsigned height;
   bool fullscreen;
   bool vsync;
   bool force_aspect;
   bool smooth;
   unsigned input_scale; // Maximum input size: RARCH_SCALE_BASE * input_scale
   bool rgb32; // Use 32-bit RGBA rather than native XBGR1555.
} video_info_t;

typedef struct audio_driver
{
   void *(*init)(const char *device, unsigned rate, unsigned latency);
   ssize_t (*write)(void *data, const void *buf, size_t size);
   bool (*stop)(void *data);
   bool (*start)(void *data);
   void (*set_nonblock_state)(void *data, bool toggle); // Should we care about blocking in audio thread? Fast forwarding.
   void (*free)(void *data);
   bool (*use_float)(void *data); // Defines if driver will take standard floating point samples, or int16_t samples.
   const char *ident;

   size_t (*write_avail)(void *data); // Optional
   size_t (*buffer_size)(void *data); // Optional
} audio_driver_t;

#define AXIS_NEG(x) (((uint32_t)(x) << 16) | UINT16_C(0xFFFF))
#define AXIS_POS(x) ((uint32_t)(x) | UINT32_C(0xFFFF0000))
#define AXIS_NONE UINT32_C(0xFFFFFFFF)
#define AXIS_DIR_NONE UINT16_C(0xFFFF)

#define AXIS_NEG_GET(x) (((uint32_t)(x) >> 16) & UINT16_C(0xFFFF))
#define AXIS_POS_GET(x) ((uint32_t)(x) & UINT16_C(0xFFFF))

#define NO_BTN UINT16_C(0xFFFF) // I hope no joypad will ever have this many buttons ... ;)

#define HAT_UP_SHIFT 15
#define HAT_DOWN_SHIFT 14
#define HAT_LEFT_SHIFT 13
#define HAT_RIGHT_SHIFT 12
#define HAT_UP_MASK (1 << HAT_UP_SHIFT)
#define HAT_DOWN_MASK (1 << HAT_DOWN_SHIFT)
#define HAT_LEFT_MASK (1 << HAT_LEFT_SHIFT)
#define HAT_RIGHT_MASK (1 << HAT_RIGHT_SHIFT)
#define HAT_MAP(x, hat) ((x & ((1 << 12) - 1)) | hat)

#define HAT_MASK (HAT_UP_MASK | HAT_DOWN_MASK | HAT_LEFT_MASK | HAT_RIGHT_MASK)
#define GET_HAT_DIR(x) (x & HAT_MASK)
#define GET_HAT(x) (x & (~HAT_MASK))

typedef struct input_driver
{
   void *(*init)(void);
   void (*poll)(void *data);
   int16_t (*input_state)(void *data, const struct retro_keybind **retro_keybinds, unsigned port, unsigned device, unsigned index, unsigned id);
   bool (*key_pressed)(void *data, int key);
   void (*free)(void *data);
#ifdef RARCH_CONSOLE
   void (*set_default_keybind_lut)(unsigned device, unsigned port);
   void (*set_analog_dpad_mapping)(unsigned device, unsigned map_dpad_enum, unsigned controller_id);
   void (*post_init)(void);
   unsigned max_pads;
#endif
   const char *ident;
} input_driver_t;

typedef struct video_driver
{
   void *(*init)(const video_info_t *video, const input_driver_t **input, void **input_data); 
   // Should the video driver act as an input driver as well? :) The video init might preinitialize an input driver to override the settings in case the video driver relies on input driver for event handling, e.g.
   bool (*frame)(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch, const char *msg); // msg is for showing a message on the screen along with the video frame.
   void (*set_nonblock_state)(void *data, bool toggle); // Should we care about syncing to vblank? Fast forwarding.
   // Is the window still active?
   bool (*alive)(void *data);
   bool (*focus)(void *data); // Does the window have focus?
   bool (*set_shader)(void *data, enum rarch_shader_type type, const char *path); // Sets shader. Might not be implemented.
   void (*free)(void *data);
   const char *ident;

   // Callbacks essentially useless on PC, but useful on consoles where the drivers are used for more stuff.
#ifdef RARCH_CONSOLE
   void (*start)(void);
   void (*stop)(void);
   void (*restart)(void);
   void (*apply_state_changes)(void);
   void (*set_aspect_ratio)(void *data, unsigned aspectratio_index);
#endif

   void (*set_rotation)(void *data, unsigned rotation);
   void (*viewport_size)(void *data, unsigned *width, unsigned *height);

   // Reads out in BGR byte order (24bpp).
   bool (*read_viewport)(void *data, uint8_t *buffer);
} video_driver_t;

enum rarch_display_type
{
   RARCH_DISPLAY_NONE = 0, // Non-bindable types like consoles, KMS, VideoCore, etc.
   RARCH_DISPLAY_X11, // video_display => Display*, video_window => Window
   RARCH_DISPLAY_WIN32, // video_display => N/A, video_window => HWND
   RARCH_DISPLAY_OSX // ?!
};

typedef struct driver
{
   const audio_driver_t *audio;
   const video_driver_t *video;
   const input_driver_t *input;
   void *audio_data;
   void *video_data;
   void *input_data;

#ifdef HAVE_COMMAND
   rarch_cmd_t *command;
#endif
   bool stdin_claimed;

   // Opaque handles to currently running window.
   // Used by e.g. input drivers which bind to a window.
   // Drivers are responsible for setting these if an input driver
   // could potentially make use of this.
   uintptr_t video_display;
   uintptr_t video_window;
   enum rarch_display_type display_type;

   struct scaler_ctx scaler;
   void *scaler_out;
} driver_t;

void init_drivers(void);
void init_drivers_pre(void);
void uninit_drivers(void);

void init_video_input(void);
void uninit_video_input(void);
void init_audio(void);
void uninit_audio(void);

extern driver_t driver;

//////////////////////////////////////////////// Backends
extern const audio_driver_t audio_rsound;
extern const audio_driver_t audio_oss;
extern const audio_driver_t audio_alsa;
extern const audio_driver_t audio_roar;
extern const audio_driver_t audio_openal;
extern const audio_driver_t audio_opensl;
extern const audio_driver_t audio_jack;
extern const audio_driver_t audio_sdl;
extern const audio_driver_t audio_xa;
extern const audio_driver_t audio_pulse;
extern const audio_driver_t audio_ext;
extern const audio_driver_t audio_dsound;
extern const audio_driver_t audio_coreaudio;
extern const audio_driver_t audio_xenon360;
extern const audio_driver_t audio_xdk360;
extern const audio_driver_t audio_ps3;
extern const audio_driver_t audio_gx;
extern const audio_driver_t audio_null;
extern const video_driver_t video_gl;
extern const video_driver_t video_d3d9;
extern const video_driver_t video_gx;
extern const video_driver_t video_xenon360;
extern const video_driver_t video_xvideo;
extern const video_driver_t video_xdk_d3d;
extern const video_driver_t video_sdl;
extern const video_driver_t video_vg;
extern const video_driver_t video_ext;
extern const video_driver_t video_null;
extern const input_driver_t input_android;
extern const input_driver_t input_sdl;
extern const input_driver_t input_dinput;
extern const input_driver_t input_x;
extern const input_driver_t input_ps3;
extern const input_driver_t input_xenon360;
extern const input_driver_t input_gx;
extern const input_driver_t input_xinput;
extern const input_driver_t input_linuxraw;
extern const input_driver_t input_null;
////////////////////////////////////////////////

// Convenience macros.

#ifdef HAVE_GRIFFIN
#include "console/griffin/hook.h"
#else
#define audio_init_func(device, rate, latency)  driver.audio->init(device, rate, latency)
#define audio_write_func(buf, size)             driver.audio->write(driver.audio_data, buf, size)
#define audio_stop_func()                       driver.audio->stop(driver.audio_data)
#define audio_start_func()                      driver.audio->start(driver.audio_data)
#define audio_set_nonblock_state_func(state)    driver.audio->set_nonblock_state(driver.audio_data, state)
#define audio_free_func()                       driver.audio->free(driver.audio_data)
#define audio_use_float_func()                  driver.audio->use_float(driver.audio_data)
#define audio_write_avail_func()                driver.audio->write_avail(driver.audio_data)
#define audio_buffer_size_func()                driver.audio->buffer_size(driver.audio_data)

#define video_init_func(video_info, input, input_data) \
                                                driver.video->init(video_info, input, input_data)
#define video_frame_func(data, width, height, pitch, msg) \
                                                driver.video->frame(driver.video_data, data, width, height, pitch, msg)
#define video_set_nonblock_state_func(state) driver.video->set_nonblock_state(driver.video_data, state)
#define video_alive_func()                      driver.video->alive(driver.video_data)
#define video_focus_func()                      driver.video->focus(driver.video_data)
#define video_set_shader_func(type, path)       driver.video->set_shader(driver.video_data, type, path)
#define video_set_rotation_func(rotate)         driver.video->set_rotation(driver.video_data, rotate)
#define video_set_aspect_ratio_func(aspect_idx) driver.video->set_aspect_ratio(driver.video_data, aspect_idx)
#define video_viewport_size_func(width, height) driver.video->viewport_size(driver.video_data, width, height)
#define video_read_viewport_func(buffer)        driver.video->read_viewport(driver.video_data, buffer)
#define video_free_func()                       driver.video->free(driver.video_data)

#define input_init_func()                       driver.input->init()
#define input_poll_func()                       driver.input->poll(driver.input_data)
#define input_input_state_func(retro_keybinds, port, device, index, id) \
                                                driver.input->input_state(driver.input_data, retro_keybinds, port, device, index, id)
#define input_free_func()                       driver.input->free(driver.input_data)

static inline bool input_key_pressed_func(int key)
{
   bool ret = driver.input->key_pressed(driver.input_data, key);
#ifdef HAVE_COMMAND
   if (!ret && driver.command)
      ret = rarch_cmd_get(driver.command, key);
#endif
   return ret;
}
#endif

#endif

