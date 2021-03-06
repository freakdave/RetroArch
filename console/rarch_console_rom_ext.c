/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#include <stdint.h>
#include <ctype.h>

#ifdef _WIN32
#include "../compat/posix_string.h"
#endif

#include "../general.h"
#include "../file.h"

#include "rarch_console_settings.h"
#include "rarch_console.h"
#include "rarch_console_rom_ext.h"

#ifdef HAVE_ZLIB
#include "rarch_console_rzlib.h"
#endif

static void rarch_console_load_game(const char *path)
{
   snprintf(g_extern.file_state.rom_path, sizeof(g_extern.file_state.rom_path), path);
   rarch_settings_change(S_START_RARCH);
}

void rarch_console_load_game_wrap(const char *path, unsigned extract_zip_mode, unsigned delay)
{
   const char *game_to_load;
   char first_file_inzip[PATH_MAX];
   char rom_path_temp[PATH_MAX];
   char dir_path_temp[PATH_MAX];
   struct retro_system_info info;
   bool block_zip_extract = false;
   bool extract_zip_cond = false;
   bool extract_zip_and_load_game_cond = false;
   bool load_game = !extract_zip_cond;

   retro_get_system_info(&info);
   block_zip_extract = info.block_extract;

   snprintf(rom_path_temp, sizeof(rom_path_temp), path);

#ifdef HAVE_ZLIB
   extract_zip_cond = (strstr(rom_path_temp, ".zip") || strstr(rom_path_temp, ".ZIP"))
   && !block_zip_extract;

   if(extract_zip_cond)
   {
      fill_pathname_basedir(dir_path_temp, rom_path_temp, sizeof(dir_path_temp));
      rarch_extract_zipfile(rom_path_temp, dir_path_temp, first_file_inzip, sizeof(first_file_inzip), extract_zip_mode);

      if(g_extern.console.rmenu.state.msg_info.enable)
         rarch_settings_msg(S_MSG_EXTRACTED_ZIPFILE, S_DELAY_180);
   }

   extract_zip_and_load_game_cond = (extract_zip_cond && 
   g_extern.file_state.zip_extract_mode == ZIP_EXTRACT_TO_CURRENT_DIR_AND_LOAD_FIRST_FILE);
   load_game = (extract_zip_and_load_game_cond) || (!extract_zip_cond);

   if(extract_zip_and_load_game_cond)
      game_to_load = first_file_inzip;
   else
#endif
      game_to_load = path;

   if(load_game)
   {
      rarch_console_load_game(game_to_load);

      if(g_extern.console.rmenu.state.msg_info.enable)
         rarch_settings_msg(S_MSG_LOADING_ROM, delay);
   }
}

const char *rarch_console_get_rom_ext(void)
{
   const char *retval = NULL;

   struct retro_system_info info;
   retro_get_system_info(&info);

   if (info.valid_extensions)
      retval = info.valid_extensions;
   else
      retval = "ZIP|zip";

   return retval;
}
