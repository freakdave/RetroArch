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

#ifndef __RARCH_LOGGER_OVERRIDE_H
#define __RARCH_LOGGER_OVERRIDE_H

#if defined(HAVE_LOGGER)
#include "logger.h"

#ifdef IS_SALAMANDER

#define RARCH_LOG(...) do { \
   logger_send("RetroArch Salamander: " __VA_ARGS__); \
} while(0)

#define RARCH_LOG_OUTPUT(...) do { \
   logger_send("RetroArch Salamander [OUTPUT] :: " __VA_ARGS__); \
} while(0)

#define RARCH_ERR(...) do { \
   logger_send("RetroArch Salamander [ERROR] :: " __VA_ARGS__); \
} while(0)

#define RARCH_WARN(...) do { \
   logger_send("RetroArch Salamander [WARN] :: " __VA_ARGS__); \
} while(0)

#else

#define RARCH_LOG(...) do { \
   logger_send("RetroArch: " __VA_ARGS__); \
} while(0)

#define RARCH_ERR(...) do { \
   logger_send("RetroArch [ERROR] :: " __VA_ARGS__); \
} while(0)

#define RARCH_WARN(...) do { \
   logger_send("RetroArch [WARN] :: " __VA_ARGS__); \
} while(0)

#define RARCH_LOG_OUTPUT(...) do { \
   logger_send("RetroArch [OUTPUT] :: " __VA_ARGS__); \
} while(0)

#endif

#elif defined(HAVE_FILE_LOGGER)
extern FILE * log_fp;

#ifdef IS_SALAMANDER

#ifndef RARCH_LOG
#define RARCH_LOG(...) do { \
      fprintf(log_fp, "RetroArch Salamander: " __VA_ARGS__); \
      fflush(log_fp); \
   } while (0)
#endif

#ifndef RARCH_ERR
#define RARCH_ERR(...) do { \
      fprintf(log_fp, "RetroArch Salamander [ERROR] :: " __VA_ARGS__); \
      fflush(log_fp); \
   } while (0)
#endif

#ifndef RARCH_WARN
#define RARCH_WARN(...) do { \
      fprintf(log_fp, "RetroArch Salamander [WARN] :: " __VA_ARGS__); \
      fflush(log_fp); \
   } while (0)
#endif

#ifndef RARCH_LOG_OUTPUT
#define RARCH_LOG_OUTPUT(...) do { \
      fprintf(log_fp, "RetroArch Salamander [OUTPUT] :: " __VA_ARGS__); \
      fflush(log_fp); \
   } while (0)
#endif

#else

#ifndef RARCH_LOG
#define RARCH_LOG(...) do { \
      fprintf(log_fp, "RetroArch: " __VA_ARGS__); \
      fflush(log_fp); \
   } while (0)
#endif

#ifndef RARCH_ERR
#define RARCH_ERR(...) do { \
      fprintf(log_fp, "RetroArch [ERROR] :: " __VA_ARGS__); \
      fflush(log_fp); \
   } while (0)
#endif

#ifndef RARCH_WARN
#define RARCH_WARN(...) do { \
      fprintf(log_fp, "RetroArch [WARN] :: " __VA_ARGS__); \
      fflush(log_fp); \
   } while (0)
#endif

#ifndef RARCH_LOG_OUTPUT
#define RARCH_LOG_OUTPUT(...) do { \
      fprintf(log_fp, "RetroArch [OUTPUT] :: " __VA_ARGS__); \
      fflush(log_fp); \
   } while (0)
#endif

#endif

#endif

#endif
