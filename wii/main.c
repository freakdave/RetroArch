/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#undef main

#include <stdlib.h>
#include <stddef.h>
#include <sdcard/wiisd_io.h>
#include <sdcard/gcsd.h>
#include <fat.h>

int ssnes_main(int argc, char **argv);

int main(void)
{
   char arg0[] = "ssnes";
   char arg1[] = "sd:/FFIII.smc";
   char arg2[] = "-v";
   char *argv[] = { arg0, arg1, arg2, NULL };

   fatMountSimple("sd", &__io_wiisd);
   int ret = ssnes_main(sizeof(argv) / sizeof(argv[0]) - 1, argv);
   fatUnmount("sd:");
   return ret;
}
