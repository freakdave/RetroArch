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

#undef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include "../general.h"
#include "../boolean.h"
#include "input_common.h"
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

// Context has to be global as joypads also ride on this context.
static LPDIRECTINPUT8 g_ctx;

struct dinput_input
{
   LPDIRECTINPUTDEVICE8 keyboard;
   LPDIRECTINPUTDEVICE8 mouse; // TODO.
   const rarch_joypad_driver_t *joypad;
   uint8_t state[256];
   int rk_to_di_lut[RETROK_LAST];
};

struct dinput_joypad
{
   LPDIRECTINPUTDEVICE8 joypad;
   DIJOYSTATE2 joy_state;
};

struct key_map
{
   int di;
   int rk;
};

static const struct key_map rk_to_di[] = {
   { DIK_LEFT, RETROK_LEFT },
   { DIK_RIGHT, RETROK_RIGHT },
   { DIK_UP, RETROK_UP },
   { DIK_DOWN, RETROK_DOWN },
   { DIK_RETURN, RETROK_RETURN },
   { DIK_TAB, RETROK_TAB },
   { DIK_INSERT, RETROK_INSERT },
   { DIK_DELETE, RETROK_DELETE },
   { DIK_RSHIFT, RETROK_RSHIFT },
   { DIK_LSHIFT, RETROK_LSHIFT },
   { DIK_LCONTROL, RETROK_LCTRL },
   { DIK_END, RETROK_END },
   { DIK_HOME, RETROK_HOME },
   { DIK_NEXT, RETROK_PAGEDOWN },
   { DIK_PRIOR, RETROK_PAGEUP },
   { DIK_LALT, RETROK_LALT },
   { DIK_SPACE, RETROK_SPACE },
   { DIK_ESCAPE, RETROK_ESCAPE },
   { DIK_BACKSPACE, RETROK_BACKSPACE },
   { DIK_NUMPADENTER, RETROK_KP_ENTER },
   { DIK_NUMPADPLUS, RETROK_KP_PLUS },
   { DIK_NUMPADMINUS, RETROK_KP_MINUS },
   { DIK_NUMPADSTAR, RETROK_KP_MULTIPLY },
   { DIK_NUMPADSLASH, RETROK_KP_DIVIDE },
   { DIK_GRAVE, RETROK_BACKQUOTE },
   { DIK_PAUSE, RETROK_PAUSE },
   { DIK_NUMPAD0, RETROK_KP0 },
   { DIK_NUMPAD1, RETROK_KP1 },
   { DIK_NUMPAD2, RETROK_KP2 },
   { DIK_NUMPAD3, RETROK_KP3 },
   { DIK_NUMPAD4, RETROK_KP4 },
   { DIK_NUMPAD5, RETROK_KP5 },
   { DIK_NUMPAD6, RETROK_KP6 },
   { DIK_NUMPAD7, RETROK_KP7 },
   { DIK_NUMPAD8, RETROK_KP8 },
   { DIK_NUMPAD9, RETROK_KP9 },
   { DIK_0, RETROK_0 },
   { DIK_1, RETROK_1 },
   { DIK_2, RETROK_2 },
   { DIK_3, RETROK_3 },
   { DIK_4, RETROK_4 },
   { DIK_5, RETROK_5 },
   { DIK_6, RETROK_6 },
   { DIK_7, RETROK_7 },
   { DIK_8, RETROK_8 },
   { DIK_9, RETROK_9 },
   { DIK_F1, RETROK_F1 },
   { DIK_F2, RETROK_F2 },
   { DIK_F3, RETROK_F3 },
   { DIK_F4, RETROK_F4 },
   { DIK_F5, RETROK_F5 },
   { DIK_F6, RETROK_F6 },
   { DIK_F7, RETROK_F7 },
   { DIK_F8, RETROK_F8 },
   { DIK_F9, RETROK_F9 },
   { DIK_F10, RETROK_F10 },
   { DIK_F11, RETROK_F11 },
   { DIK_F12, RETROK_F12 },
   { DIK_A, RETROK_a },
   { DIK_B, RETROK_b },
   { DIK_C, RETROK_c },
   { DIK_D, RETROK_d },
   { DIK_E, RETROK_e },
   { DIK_F, RETROK_f },
   { DIK_G, RETROK_g },
   { DIK_H, RETROK_h },
   { DIK_I, RETROK_i },
   { DIK_J, RETROK_j },
   { DIK_K, RETROK_k },
   { DIK_L, RETROK_l },
   { DIK_M, RETROK_m },
   { DIK_N, RETROK_n },
   { DIK_O, RETROK_o },
   { DIK_P, RETROK_p },
   { DIK_Q, RETROK_q },
   { DIK_R, RETROK_r },
   { DIK_S, RETROK_s },
   { DIK_T, RETROK_t },
   { DIK_U, RETROK_u },
   { DIK_V, RETROK_v },
   { DIK_W, RETROK_w },
   { DIK_X, RETROK_x },
   { DIK_Y, RETROK_y },
   { DIK_Z, RETROK_z },
};

static void init_lut(int *lut)
{
   for (unsigned i = 0; i < sizeof(rk_to_di) / sizeof(rk_to_di[0]); i++)
      lut[rk_to_di[i].rk] = rk_to_di[i].di;
}

static unsigned g_joypad_cnt;
static struct dinput_joypad g_pads[MAX_PLAYERS];

static void dinput_destroy_context(void)
{
   if (g_ctx)
   {
      IDirectInput8_Release(g_ctx);
      g_ctx = NULL;
   }
}

static bool dinput_init_context(void)
{
   if (g_ctx)
      return true;

   if (driver.display_type != RARCH_DISPLAY_WIN32)
   {
      RARCH_ERR("Cannot open DInput as no Win32 window is present.\n");
      return false;
   }

   CoInitialize(NULL);

   // Who said we shouldn't have same call signature in a COM API? <_<
#ifdef __cplusplus
   if (FAILED(DirectInput8Create(
      GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8,
      (void**)&g_ctx, NULL)))
#else
   if (FAILED(DirectInput8Create(
      GetModuleHandle(NULL), DIRECTINPUT_VERSION, &IID_IDirectInput8,
      (void**)&g_ctx, NULL)))
#endif
   {
      RARCH_ERR("Failed to init DirectInput.\n");
      return false;
   }

   return true;
}

static void *dinput_init(void)
{
   if (!dinput_init_context())
      return NULL;

   struct dinput_input *di = (struct dinput_input*)calloc(1, sizeof(*di));
   if (!di)
      return NULL;

#ifdef __cplusplus
   if (FAILED(IDirectInput8_CreateDevice(g_ctx, GUID_SysKeyboard, &di->keyboard, NULL)))
#else
   if (FAILED(IDirectInput8_CreateDevice(g_ctx, &GUID_SysKeyboard, &di->keyboard, NULL)))
#endif
      goto error;

   IDirectInputDevice8_SetDataFormat(di->keyboard, &c_dfDIKeyboard);
   IDirectInputDevice8_SetCooperativeLevel(di->keyboard,
         (HWND)driver.video_window, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
   IDirectInputDevice8_Acquire(di->keyboard);

   init_lut(di->rk_to_di_lut);
   di->joypad = input_joypad_init_first();

   return di;

error:
   dinput_destroy_context();
   free(di);
   return NULL;
}

static void dinput_poll(void *data)
{
   struct dinput_input *di = (struct dinput_input*)data;

   memset(di->state, 0, sizeof(di->state));
   if (FAILED(IDirectInputDevice8_GetDeviceState(di->keyboard,
               sizeof(di->state), di->state)))
   {
      IDirectInputDevice8_Acquire(di->keyboard);
      if (FAILED(IDirectInputDevice8_GetDeviceState(di->keyboard, sizeof(di->state), di->state)))
         memset(di->state, 0, sizeof(di->state));
   }

   input_joypad_poll(di->joypad);
}

static bool dinput_keyboard_pressed(struct dinput_input *di, unsigned key)
{
   if (key >= RETROK_LAST)
      return false;

   return di->state[di->rk_to_di_lut[key]] & 0x80;
}

static bool dinput_is_pressed(struct dinput_input *di, const struct retro_keybind *binds,
      unsigned port, unsigned id)
{
   if (id >= RARCH_BIND_LIST_END)
      return false;

   const struct retro_keybind *bind = &binds[id];
   return dinput_keyboard_pressed(di, bind->key) || input_joypad_pressed(di->joypad, port, bind);
}

static bool dinput_key_pressed(void *data, int key)
{
   return dinput_is_pressed((struct dinput_input*)data, g_settings.input.binds[0], 0, key);
}

static int16_t dinput_input_state(void *data,
      const struct retro_keybind **binds, unsigned port,
      unsigned device, unsigned index, unsigned id)
{
   struct dinput_input *di = (struct dinput_input*)data;
   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return dinput_is_pressed(di, binds[port], port, id);

      case RETRO_DEVICE_KEYBOARD:
         return dinput_keyboard_pressed(di, id);

      case RETRO_DEVICE_ANALOG:
         return input_joypad_analog(di->joypad, port, index, id, g_settings.input.binds[port]);

      case RETRO_DEVICE_MOUSE: // TODO.
      case RETRO_DEVICE_LIGHTGUN:
         return 0;

      default:
         return 0;
   }
}

static void dinput_free(void *data)
{
   struct dinput_input *di = (struct dinput_input*)data;
   LPDIRECTINPUT8 hold_ctx = g_ctx;

   if (di)
   {
      g_ctx = NULL; // Prevent a joypad driver to kill our context prematurely.
      di->joypad->destroy();
      g_ctx = hold_ctx;

      if (di->keyboard)
         IDirectInputDevice8_Release(di->keyboard);

      free(di);
   }

   dinput_destroy_context();
}

const input_driver_t input_dinput = {
   dinput_init,
   dinput_poll,
   dinput_input_state,
   dinput_key_pressed,
   dinput_free,
   "dinput",
};

static void dinput_joypad_destroy(void)
{
   for (unsigned i = 0; i < MAX_PLAYERS; i++)
   {
      if (g_pads[i].joypad)
      {
         IDirectInputDevice8_Unacquire(g_pads[i].joypad);
         IDirectInputDevice8_Release(g_pads[i].joypad);
      }
   }

   g_joypad_cnt = 0;
   memset(g_pads, 0, sizeof(g_pads));

   // Can be blocked by global Dinput context.
   dinput_destroy_context();
}

static BOOL CALLBACK enum_axes_cb(const DIDEVICEOBJECTINSTANCE *inst, void *p)
{
   LPDIRECTINPUTDEVICE8 joypad = (LPDIRECTINPUTDEVICE8)p;

   DIPROPRANGE range;
   memset(&range, 0, sizeof(range));
   range.diph.dwSize = sizeof(DIPROPRANGE);
   range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
   range.diph.dwHow = DIPH_BYID;
   range.diph.dwObj = inst->dwType;
   range.lMin = -0x7fff;
   range.lMax = 0x7fff;
   IDirectInputDevice8_SetProperty(joypad, DIPROP_RANGE, &range.diph);

   return DIENUM_CONTINUE;
}

static BOOL CALLBACK enum_joypad_cb(const DIDEVICEINSTANCE *inst, void *p)
{
   (void)p;
   if (g_joypad_cnt == MAX_PLAYERS)
      return DIENUM_STOP;

   LPDIRECTINPUTDEVICE8 *pad = &g_pads[g_joypad_cnt].joypad;

#ifdef __cplusplus
   if (FAILED(IDirectInput8_CreateDevice(g_ctx, inst->guidInstance, pad, NULL)))
#else
   if (FAILED(IDirectInput8_CreateDevice(g_ctx, &inst->guidInstance, pad, NULL)))
#endif
      return DIENUM_CONTINUE;

   IDirectInputDevice8_SetDataFormat(*pad, &c_dfDIJoystick2);
   IDirectInputDevice8_SetCooperativeLevel(*pad, (HWND)driver.video_window,
         DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);

   IDirectInputDevice8_EnumObjects(*pad, enum_axes_cb, 
         *pad, DIDFT_ABSAXIS);

   g_joypad_cnt++;

   return DIENUM_CONTINUE; 
}

static bool dinput_joypad_init(void)
{
   if (!dinput_init_context())
      return false;

   RARCH_LOG("Enumerating DInput joypads ...\n");
   IDirectInput8_EnumDevices(g_ctx, DI8DEVCLASS_GAMECTRL,
         enum_joypad_cb, NULL, DIEDFL_ATTACHEDONLY);
   RARCH_LOG("Done enumerating DInput joypads ...\n");
   return true;
}

static bool dinput_joypad_button(unsigned port_num, uint16_t joykey)
{
   if (joykey == NO_BTN)
      return false;

   const struct dinput_joypad *pad = &g_pads[port_num];
   if (!pad->joypad)
      return false;

   // Check hat.
   if (GET_HAT_DIR(joykey))
   {
      unsigned hat = GET_HAT(joykey);
      
      unsigned elems = sizeof(pad->joy_state.rgdwPOV) / sizeof(pad->joy_state.rgdwPOV[0]);
      if (hat >= elems)
         return false;

      unsigned pov = pad->joy_state.rgdwPOV[hat];

      // Magic numbers I'm not sure where originate from.
      if (pov < 36000)
      {
         switch (GET_HAT_DIR(joykey))
         {
            case HAT_UP_MASK:
               return (pov >= 31500) || (pov <= 4500);
            case HAT_RIGHT_MASK:
               return (pov >= 4500) && (pov <= 13500);
            case HAT_DOWN_MASK:
               return (pov >= 13500) && (pov <= 22500);
            case HAT_LEFT_MASK:
               return (pov >= 22500) && (pov <= 31500);
         }
      }

      return false;
   }
   else
   {
      unsigned elems = sizeof(pad->joy_state.rgbButtons) / sizeof(pad->joy_state.rgbButtons[0]);

      if (joykey < elems)
         return pad->joy_state.rgbButtons[joykey];
   }

   return false;
}

static int16_t dinput_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   if (joyaxis == AXIS_NONE)
      return 0;

   const struct dinput_joypad *pad = &g_pads[port_num];
   if (!pad->joypad)
      return 0;

   int val = 0;

   int axis    = -1;
   bool is_neg = false;
   bool is_pos = false;

   if (AXIS_NEG_GET(joyaxis) <= 5)
   {
      axis = AXIS_NEG_GET(joyaxis);
      is_neg = true;
   }
   else if (AXIS_POS_GET(joyaxis) <= 5)
   {
      axis = AXIS_POS_GET(joyaxis);
      is_pos = true;
   }

   switch (axis)
   {
      case 0: val = pad->joy_state.lX; break;
      case 1: val = pad->joy_state.lY; break;
      case 2: val = pad->joy_state.lZ; break;
      case 3: val = pad->joy_state.lRx; break;
      case 4: val = pad->joy_state.lRy; break;
      case 5: val = pad->joy_state.lRz; break;
   }

   if (is_neg && val > 0)
      val = 0;
   else if (is_pos && val < 0)
      val = 0;

   return val;
}

static void dinput_joypad_poll(void)
{
   for (unsigned i = 0; i < MAX_PLAYERS; i++)
   {
      struct dinput_joypad *pad = &g_pads[i];

      if (pad->joypad)
      {
         memset(&pad->joy_state, 0, sizeof(pad->joy_state));

         if (FAILED(IDirectInputDevice8_Poll(pad->joypad)))
         {
            if (FAILED(IDirectInputDevice8_Acquire(pad->joypad)))
            {
               memset(&pad->joy_state, 0, sizeof(DIJOYSTATE2));
               continue;
            }

            // If this fails, something *really* bad must have happened.
            if (FAILED(IDirectInputDevice8_Poll(pad->joypad)))
            {
               memset(&pad->joy_state, 0, sizeof(DIJOYSTATE2));
               continue;
            }
         }

         IDirectInputDevice8_GetDeviceState(pad->joypad,
               sizeof(DIJOYSTATE2), &pad->joy_state);
      }
   }
}

static bool dinput_joypad_query_pad(unsigned pad)
{
   return pad < MAX_PLAYERS && g_pads[pad].joypad;
}

const rarch_joypad_driver_t dinput_joypad = {
   dinput_joypad_init,
   dinput_joypad_query_pad,
   dinput_joypad_destroy,
   dinput_joypad_button,
   dinput_joypad_axis,
   dinput_joypad_poll,
   "dinput",
};

