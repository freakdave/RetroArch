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

#ifndef __GL_COMMON_H
#define __GL_COMMON_H

#include "../general.h"
#include "fonts/fonts.h"
#include "math/matrix.h"
#include "gfx_context.h"
#include "scaler/scaler.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <string.h>

#ifdef HAVE_EGL
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#elif defined(HAVE_PSGL)
#include <PSGL/psgl.h>
#include <PSGL/psglu.h>
#include <GLES/glext.h>
#elif defined(HAVE_OPENGL_MODERN)
#include <GL3/gl3.h>
#include <GL3/gl3ext.h>
#elif defined(HAVE_OPENGLES2)
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#elif defined(HAVE_OPENGLES1)
#include <GLES/gl.h>
#include <GLES/glext.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif

static inline bool gl_query_extension(const char *ext)
{
   const char *str = (const char*)glGetString(GL_EXTENSIONS);
   bool ret = str && strstr(str, ext);
   RARCH_LOG("Querying GL extension: %s => %s\n",
         ext, ret ? "exists" : "doesn't exist");

   return ret;
}

static inline bool gl_check_error(void)
{
   int error = glGetError();
   switch (error)
   {
      case GL_INVALID_ENUM:
         RARCH_ERR("GL: Invalid enum.\n");
         break;
      case GL_INVALID_VALUE:
         RARCH_ERR("GL: Invalid value.\n");
         break;
      case GL_INVALID_OPERATION:
         RARCH_ERR("GL: Invalid operation.\n");
         break;
      case GL_OUT_OF_MEMORY:
         RARCH_ERR("GL: Out of memory.\n");
         break;
      case GL_NO_ERROR:
         return true;
      default:
         RARCH_ERR("Non specified GL error.\n");
   }

   return false;
}

static inline unsigned get_alignment(unsigned pitch)
{
   if (pitch & 1)
      return 1;
   if (pitch & 2)
      return 2;
   if (pitch & 4)
      return 4;
   return 8;
}

struct gl_fbo_rect
{
   unsigned img_width;
   unsigned img_height;
   unsigned max_img_width;
   unsigned max_img_height;
   unsigned width;
   unsigned height;
};

enum gl_scale_type
{
   RARCH_SCALE_ABSOLUTE,
   RARCH_SCALE_INPUT,
   RARCH_SCALE_VIEWPORT
};

struct gl_fbo_scale
{
   enum gl_scale_type type_x;
   enum gl_scale_type type_y;
   float scale_x;
   float scale_y;
   unsigned abs_x;
   unsigned abs_y;
   bool valid;
};

struct gl_ortho
{
   GLfloat left;
   GLfloat right;
   GLfloat bottom;
   GLfloat top;
   GLfloat znear;
   GLfloat zfar;
};

struct gl_tex_info
{
   GLuint tex;
   GLfloat input_size[2];
   GLfloat tex_size[2];
   GLfloat coord[8];
};

struct gl_coords
{
   const GLfloat *vertex;
   const GLfloat *color;
   const GLfloat *tex_coord;
   const GLfloat *lut_tex_coord;
};

#define MAX_SHADERS 16

#if (defined(HAVE_GLSL) || defined(HAVE_CG))
#define TEXTURES 8
#else
#define TEXTURES 1
#endif
#define TEXTURES_MASK (TEXTURES - 1)

typedef struct gl
{
   const gfx_ctx_driver_t *ctx_driver;
   bool vsync;
   GLuint texture[TEXTURES];
   unsigned tex_index; // For use with PREV.
   struct gl_tex_info prev_info[TEXTURES];
   GLuint tex_filter;

   void *empty_buf;

   void *conv_buffer;
   struct scaler_ctx scaler;

   unsigned frame_count;

#ifdef HAVE_FBO
   // Render-to-texture, multipass shaders
   GLuint fbo[MAX_SHADERS];
   GLuint fbo_texture[MAX_SHADERS];
   struct gl_fbo_rect fbo_rect[MAX_SHADERS];
   struct gl_fbo_scale fbo_scale[MAX_SHADERS];
   bool render_to_tex;
   int fbo_pass;
   bool fbo_inited;
#endif

   bool should_resize;
   bool quitting;
   bool fullscreen;
   bool keep_aspect;
   unsigned rotation;

   unsigned full_x, full_y;

   unsigned win_width;
   unsigned win_height;
   unsigned vp_width, vp_out_width;
   unsigned vp_height, vp_out_height;
   unsigned last_width[TEXTURES];
   unsigned last_height[TEXTURES];
   unsigned tex_w, tex_h;
   GLfloat tex_coords[8];
   math_matrix mvp;

   struct gl_coords coords;

   GLuint pbo;
   GLenum internal_fmt;
   GLenum texture_type; // RGB565 or ARGB
   GLenum texture_fmt;
   GLenum border_type;
   unsigned base_size; // 2 or 4

#ifdef HAVE_FREETYPE
   font_renderer_t *font;
   GLuint font_tex;
   int font_tex_w, font_tex_h;
   uint16_t *font_tex_buf;
   char font_last_msg[256];
   int font_last_width, font_last_height;
   GLfloat font_color[16];
   GLfloat font_color_dark[16];
#endif

#ifdef RARCH_CONSOLE
   bool block_swap;
#endif
#ifdef HAVE_RMENU
   bool draw_rmenu;
#endif

   bool egl_images;
} gl_t;

// Windows ... <_<
#ifdef _WIN32
extern PFNGLCLIENTACTIVETEXTUREPROC pglClientActiveTexture;
extern PFNGLACTIVETEXTUREPROC pglActiveTexture;
#else
#define pglClientActiveTexture glClientActiveTexture
#define pglActiveTexture glActiveTexture
#endif

#if defined(HAVE_PSGL)
#define RARCH_GL_INTERNAL_FORMAT32 GL_ARGB_SCE
#define RARCH_GL_INTERNAL_FORMAT16 GL_ARGB_SCE
#define RARCH_GL_TEXTURE_TYPE32 GL_BGRA
#define RARCH_GL_TEXTURE_TYPE16 GL_BGRA
#define RARCH_GL_FORMAT32 GL_UNSIGNED_INT_8_8_8_8_REV
#define RARCH_GL_FORMAT16 GL_RGB5
#elif defined(HAVE_OPENGLES)
#define RARCH_GL_INTERNAL_FORMAT32 GL_BGRA_EXT
#define RARCH_GL_INTERNAL_FORMAT16 GL_RGB
#define RARCH_GL_TEXTURE_TYPE32 GL_BGRA_EXT
#define RARCH_GL_TEXTURE_TYPE16 GL_RGB
#define RARCH_GL_FORMAT32 GL_UNSIGNED_BYTE
#define RARCH_GL_FORMAT16 GL_UNSIGNED_SHORT_5_6_5
#else
// On desktop, we always use 32-bit.
#define RARCH_GL_INTERNAL_FORMAT32 GL_RGBA
#define RARCH_GL_INTERNAL_FORMAT16 GL_RGBA
#define RARCH_GL_TEXTURE_TYPE32 GL_BGRA
#define RARCH_GL_TEXTURE_TYPE16 GL_BGRA
#define RARCH_GL_FORMAT32 GL_UNSIGNED_INT_8_8_8_8_REV
#define RARCH_GL_FORMAT16 GL_UNSIGNED_INT_8_8_8_8_REV
#endif

// Platform specific workarounds/hacks.
#if defined(__CELLOS_LV2__)
#define NO_GL_READ_VIEWPORT
#endif

#if defined(HAVE_OPENGL_MODERN) || defined(HAVE_OPENGLES2) || defined(HAVE_PSGL)
#define NO_GL_FF_VERTEX
#endif

#if defined(HAVE_OPENGL_MODERN) || defined(HAVE_OPENGLES2) || defined(HAVE_PSGL)
#define NO_GL_FF_MATRIX
#endif

#if defined(HAVE_OPENGLES2) // TODO: Figure out exactly what.
#define NO_GL_CLAMP_TO_BORDER
#endif

#if defined(HAVE_OPENGLES2) // It's an extension. Don't bother checking for it atm.
#undef GL_UNPACK_ROW_LENGTH
#endif

extern const GLfloat vertexes_flipped[];
extern const GLfloat white_color[];

void gl_shader_use(unsigned index);
void gl_set_projection(gl_t *gl, struct gl_ortho *ortho, bool allow_rotate);
void gl_set_viewport(gl_t *gl, unsigned width, unsigned height, bool force_full, bool allow_rotate);
void gl_shader_set_coords(const struct gl_coords *coords, const math_matrix *mat);

void gl_init_fbo(gl_t *gl, unsigned width, unsigned height);
void gl_deinit_fbo(gl_t *gl);

#endif
