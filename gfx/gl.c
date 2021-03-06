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

#include "../driver.h"
#include "../benchmark.h"
#include "scaler/scaler.h"

#include <stdint.h>
#include "../libretro.h"
#include <stdio.h>
#include <string.h>
#include "../general.h"
#include <math.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gl_common.h"
#include "gl_font.h"
#include "gfx_common.h"
#include "gfx_context.h"
#include "../compat/strl.h"

#ifdef HAVE_CG
#include "shader_cg.h"
#endif

#ifdef HAVE_GLSL
#include "shader_glsl.h"
#endif

// Used for the last pass when rendering to the back buffer.
const GLfloat vertexes_flipped[] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

// Used when rendering to an FBO.
// Texture coords have to be aligned with vertex coordinates.
static const GLfloat vertexes[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const GLfloat tex_coords[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static inline void set_texture_coords(GLfloat *coords, GLfloat xamt, GLfloat yamt)
{
   coords[2] = xamt;
   coords[6] = xamt;
   coords[5] = yamt;
   coords[7] = yamt;
}

const GLfloat white_color[] = {
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
   1, 1, 1, 1,
};

const GLfloat *vertex_ptr = vertexes_flipped;
const GLfloat *default_vertex_ptr = vertexes_flipped;

#undef LOAD_GL_SYM
#define LOAD_GL_SYM(SYM) if (!pgl##SYM) { \
   gfx_ctx_proc_t sym = gl->ctx_driver->get_proc_address("gl" #SYM); \
   memcpy(&(pgl##SYM), &sym, sizeof(sym)); \
}

#if defined(HAVE_EGL) && defined(HAVE_OPENGLES2)
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC pglEGLImageTargetTexture2DOES;

static bool load_eglimage_proc(gl_t *gl)
{
   LOAD_GL_SYM(EGLImageTargetTexture2DOES);
   return pglEGLImageTargetTexture2DOES;
}
#endif

#ifdef HAVE_FBO
#if defined(_WIN32) && !defined(RARCH_CONSOLE)
static PFNGLGENFRAMEBUFFERSPROC pglGenFramebuffers;
static PFNGLBINDFRAMEBUFFERPROC pglBindFramebuffer;
static PFNGLFRAMEBUFFERTEXTURE2DPROC pglFramebufferTexture2D;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC pglCheckFramebufferStatus;
static PFNGLDELETEFRAMEBUFFERSPROC pglDeleteFramebuffers;

static bool load_fbo_proc(gl_t *gl)
{
   LOAD_GL_SYM(GenFramebuffers);
   LOAD_GL_SYM(BindFramebuffer);
   LOAD_GL_SYM(FramebufferTexture2D);
   LOAD_GL_SYM(CheckFramebufferStatus);
   LOAD_GL_SYM(DeleteFramebuffers);

   return pglGenFramebuffers && pglBindFramebuffer && pglFramebufferTexture2D && 
      pglCheckFramebufferStatus && pglDeleteFramebuffers;
}
#elif defined(HAVE_OPENGLES2)
#define pglGenFramebuffers glGenFramebuffers
#define pglBindFramebuffer glBindFramebuffer
#define pglFramebufferTexture2D glFramebufferTexture2D
#define pglCheckFramebufferStatus glCheckFramebufferStatus
#define pglDeleteFramebuffers glDeleteFramebuffers
#define load_fbo_proc(gl) (true)
#elif defined(HAVE_OPENGLES)
#define pglGenFramebuffers glGenFramebuffersOES
#define pglBindFramebuffer glBindFramebufferOES
#define pglFramebufferTexture2D glFramebufferTexture2DOES
#define pglCheckFramebufferStatus glCheckFramebufferStatusOES
#define pglDeleteFramebuffers glDeleteFramebuffersOES
#define GL_FRAMEBUFFER GL_FRAMEBUFFER_OES
#define GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#define GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_OES
#define load_fbo_proc(gl) (true)
#else
#define pglGenFramebuffers glGenFramebuffers
#define pglBindFramebuffer glBindFramebuffer
#define pglFramebufferTexture2D glFramebufferTexture2D
#define pglCheckFramebufferStatus glCheckFramebufferStatus
#define pglDeleteFramebuffers glDeleteFramebuffers
#define load_fbo_proc(gl) (true)
#endif
#endif

#ifdef _WIN32
PFNGLCLIENTACTIVETEXTUREPROC pglClientActiveTexture;
PFNGLACTIVETEXTUREPROC pglActiveTexture;
static PFNGLBINDBUFFERPROC pglBindBuffer;
static PFNGLBUFFERSUBDATAPROC pglBufferSubData;
static PFNGLBUFFERDATAPROC pglBufferData;
static PFNGLMAPBUFFERPROC pglMapBuffer;
static PFNGLUNMAPBUFFERPROC pglUnmapBuffer;
static inline bool load_gl_proc_win32(gl_t *gl)
{
   LOAD_GL_SYM(ClientActiveTexture);
   LOAD_GL_SYM(ActiveTexture);
   LOAD_GL_SYM(BindBuffer);
   LOAD_GL_SYM(BufferSubData);
   LOAD_GL_SYM(BufferData);
   LOAD_GL_SYM(MapBuffer);
   LOAD_GL_SYM(UnmapBuffer);
   return pglClientActiveTexture && pglActiveTexture && pglBindBuffer &&
      pglBufferSubData && pglBufferData && pglMapBuffer && pglUnmapBuffer;
}
#else
#define pglBindBuffer glBindBuffer
#define pglBufferSubData glBufferSubData
#define pglBufferData glBufferData
#define pglMapBuffer glMapBuffer
#define pglUnmapBuffer glUnmapBuffer
#endif

////////////////// Shaders

#ifdef HAVE_OPENGLES2
static bool gl_shader_init(void) // We always need a shader alive in GLES2.
{
   const char *shader_path = NULL;
   if ((g_settings.video.shader_type == RARCH_SHADER_AUTO || g_settings.video.shader_type == RARCH_SHADER_BSNES)
         && *g_settings.video.bsnes_shader_path)
      shader_path = g_settings.video.bsnes_shader_path;

   return gl_glsl_init(shader_path);
}
#else
static bool gl_shader_init(void)
{
   switch (g_settings.video.shader_type)
   {
      case RARCH_SHADER_AUTO:
      {
         if (*g_settings.video.cg_shader_path && *g_settings.video.bsnes_shader_path)
            RARCH_WARN("Both Cg and bSNES XML shader are defined in config file. Cg shader will be selected by default.\n");

#ifdef HAVE_CG
         if (*g_settings.video.cg_shader_path)
            return gl_cg_init(g_settings.video.cg_shader_path);
#endif

#ifdef HAVE_XML
         if (*g_settings.video.bsnes_shader_path)
            return gl_glsl_init(g_settings.video.bsnes_shader_path);
#endif
         break;
      }

#ifdef HAVE_CG
      case RARCH_SHADER_CG:
         return gl_cg_init(g_settings.video.cg_shader_path);
#endif

#ifdef HAVE_XML
      case RARCH_SHADER_BSNES:
         return gl_glsl_init(g_settings.video.bsnes_shader_path);
#endif

      default:
         break;
   }

   return true;
}
#endif

void gl_shader_use(unsigned index)
{
#ifdef HAVE_CG
   gl_cg_use(index);
#endif

#ifdef HAVE_GLSL
   gl_glsl_use(index);
#endif
}

static inline void gl_shader_deinit(void)
{
#ifdef HAVE_CG
   gl_cg_deinit();
#endif

#ifdef HAVE_GLSL
   gl_glsl_deinit();
#endif
}

#ifndef NO_GL_FF_VERTEX
static void gl_set_coords(const struct gl_coords *coords)
{
   pglClientActiveTexture(GL_TEXTURE0);

   glVertexPointer(2, GL_FLOAT, 0, coords->vertex);
   glEnableClientState(GL_VERTEX_ARRAY);

   glColorPointer(4, GL_FLOAT, 0, coords->color);
   glEnableClientState(GL_COLOR_ARRAY);

   glTexCoordPointer(2, GL_FLOAT, 0, coords->tex_coord);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);

   pglClientActiveTexture(GL_TEXTURE1);
   glTexCoordPointer(2, GL_FLOAT, 0, coords->lut_tex_coord);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   pglClientActiveTexture(GL_TEXTURE0);
}
#endif

#ifndef NO_GL_FF_MATRIX
static void gl_set_mvp(const math_matrix *mat)
{
   glMatrixMode(GL_PROJECTION);
   glLoadMatrixf(mat->data);
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
}
#endif

void gl_shader_set_coords(const struct gl_coords *coords, const math_matrix *mat)
{
   bool ret_coords = false;
   bool ret_mvp    = false;

   (void)ret_coords;
   (void)ret_mvp;

#ifdef HAVE_GLSL
   if (!ret_coords)
      ret_coords |= gl_glsl_set_coords(coords);
   if (!ret_mvp)
      ret_mvp    |= gl_glsl_set_mvp(mat);
#endif

#ifdef HAVE_CG
   if (!ret_coords)
      ret_coords |= gl_cg_set_coords(coords);
   if (!ret_mvp)
      ret_mvp    |= gl_cg_set_mvp(mat);
#endif

   // Fall back to FF-style if needed and possible.
#ifndef NO_GL_FF_VERTEX
   if (!ret_coords)
      gl_set_coords(coords);
#endif

#ifndef NO_GL_FF_MATRIX
   if (!ret_mvp)
      gl_set_mvp(mat);
#endif
}

static inline void gl_shader_set_params(unsigned width, unsigned height, 
      unsigned tex_width, unsigned tex_height, 
      unsigned out_width, unsigned out_height,
      unsigned frame_count,
      const struct gl_tex_info *info,
      const struct gl_tex_info *prev_info,
      const struct gl_tex_info *fbo_info, unsigned fbo_info_cnt)
{
#ifdef HAVE_CG
   gl_cg_set_params(width, height, 
         tex_width, tex_height, 
         out_width, out_height, 
         frame_count, info, prev_info, fbo_info, fbo_info_cnt);
#endif

#ifdef HAVE_GLSL
   gl_glsl_set_params(width, height, 
         tex_width, tex_height, 
         out_width, out_height, 
         frame_count, info, prev_info, fbo_info, fbo_info_cnt);
#endif
}

static unsigned gl_shader_num(void)
{
#ifdef HAVE_CG
   unsigned cg_num = gl_cg_num();
   if (cg_num)
      return cg_num;
#endif

#ifdef HAVE_GLSL
   unsigned glsl_num = gl_glsl_num();
   if (glsl_num)
      return glsl_num;
#endif

   return 0;
}

static bool gl_shader_filter_type(unsigned index, bool *smooth)
{
   bool valid = false;

#ifdef HAVE_CG
   if (!valid)
      valid = gl_cg_filter_type(index, smooth);
#endif

#ifdef HAVE_GLSL
   if (!valid)
      valid = gl_glsl_filter_type(index, smooth);
#endif

   return valid;
}

#ifdef HAVE_FBO
static void gl_shader_scale(unsigned index, struct gl_fbo_scale *scale)
{
   scale->valid = false;

#ifdef HAVE_CG
   if (!scale->valid)
      gl_cg_shader_scale(index, scale);
#endif

#ifdef HAVE_GLSL
   if (!scale->valid)
      gl_glsl_shader_scale(index, scale);
#endif
}
#endif
///////////////////


#ifdef HAVE_FBO
static void gl_compute_fbo_geometry(gl_t *gl, unsigned width, unsigned height,
      unsigned vp_width, unsigned vp_height)
{
   unsigned last_width = width;
   unsigned last_height = height;
   unsigned last_max_width = gl->tex_w;
   unsigned last_max_height = gl->tex_h;

   GLint max_size = 0;
   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
   bool size_modified = false;

   // Calculate viewports for FBOs.
   for (int i = 0; i < gl->fbo_pass; i++)
   {
      switch (gl->fbo_scale[i].type_x)
      {
         case RARCH_SCALE_INPUT:
            gl->fbo_rect[i].img_width = last_width * gl->fbo_scale[i].scale_x;
            gl->fbo_rect[i].max_img_width = last_max_width * gl->fbo_scale[i].scale_x;
            break;

         case RARCH_SCALE_ABSOLUTE:
            gl->fbo_rect[i].img_width = gl->fbo_rect[i].max_img_width = gl->fbo_scale[i].abs_x;
            break;

         case RARCH_SCALE_VIEWPORT:
            gl->fbo_rect[i].img_width = gl->fbo_rect[i].max_img_width = gl->fbo_scale[i].scale_x * vp_width;
            break;

         default:
            break;
      }

      switch (gl->fbo_scale[i].type_y)
      {
         case RARCH_SCALE_INPUT:
            gl->fbo_rect[i].img_height = last_height * gl->fbo_scale[i].scale_y;
            gl->fbo_rect[i].max_img_height = last_max_height * gl->fbo_scale[i].scale_y;
            break;

         case RARCH_SCALE_ABSOLUTE:
            gl->fbo_rect[i].img_height = gl->fbo_rect[i].max_img_height = gl->fbo_scale[i].abs_y;
            break;

         case RARCH_SCALE_VIEWPORT:
            gl->fbo_rect[i].img_height = gl->fbo_rect[i].max_img_height = gl->fbo_scale[i].scale_y * vp_height;
            break;

         default:
            break;
      }

      if (gl->fbo_rect[i].img_width > (unsigned)max_size)
      {
         size_modified = true;
         gl->fbo_rect[i].img_width = max_size;
      }

      if (gl->fbo_rect[i].img_height > (unsigned)max_size)
      {
         size_modified = true;
         gl->fbo_rect[i].img_height = max_size;
      }

      if (gl->fbo_rect[i].max_img_width > (unsigned)max_size)
      {
         size_modified = true;
         gl->fbo_rect[i].max_img_width = max_size;
      }

      if (gl->fbo_rect[i].max_img_height > (unsigned)max_size)
      {
         size_modified = true;
         gl->fbo_rect[i].max_img_height = max_size;
      }

      if (size_modified)
         RARCH_WARN("FBO textures exceeded maximum size of GPU (%ux%u). Resizing to fit.\n", max_size, max_size);

      last_width = gl->fbo_rect[i].img_width;
      last_height = gl->fbo_rect[i].img_height;
      last_max_width = gl->fbo_rect[i].max_img_width;
      last_max_height = gl->fbo_rect[i].max_img_height;
   }
}

static void gl_create_fbo_textures(gl_t *gl)
{
   glGenTextures(gl->fbo_pass, gl->fbo_texture);

   GLuint base_filt = g_settings.video.second_pass_smooth ? GL_LINEAR : GL_NEAREST;
   for (int i = 0; i < gl->fbo_pass; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i]);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl->border_type);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl->border_type);

      GLuint filter_type = base_filt;
      bool smooth = false;
      if (gl_shader_filter_type(i + 2, &smooth))
         filter_type = smooth ? GL_LINEAR : GL_NEAREST;

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_type);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_type);

      glTexImage2D(GL_TEXTURE_2D,
            0, RARCH_GL_INTERNAL_FORMAT32, gl->fbo_rect[i].width, gl->fbo_rect[i].height,
            0, RARCH_GL_TEXTURE_TYPE32,
            RARCH_GL_FORMAT32, NULL);
   }

   glBindTexture(GL_TEXTURE_2D, 0);
}

static bool gl_create_fbo_targets(gl_t *gl)
{
   pglGenFramebuffers(gl->fbo_pass, gl->fbo);
   for (int i = 0; i < gl->fbo_pass; i++)
   {
      pglBindFramebuffer(GL_FRAMEBUFFER, gl->fbo[i]);
      pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->fbo_texture[i], 0);

      GLenum status = pglCheckFramebufferStatus(GL_FRAMEBUFFER);
      if (status != GL_FRAMEBUFFER_COMPLETE)
         goto error;
   }

   return true;

error:
   pglDeleteFramebuffers(gl->fbo_pass, gl->fbo);
   RARCH_ERR("Failed to set up frame buffer objects. Multi-pass shading will not work.\n");
   return false;
}

void gl_deinit_fbo(gl_t *gl)
{
   if (gl->fbo_inited)
   {
      glDeleteTextures(gl->fbo_pass, gl->fbo_texture);
      pglDeleteFramebuffers(gl->fbo_pass, gl->fbo);
      memset(gl->fbo_texture, 0, sizeof(gl->fbo_texture));
      memset(gl->fbo, 0, sizeof(gl->fbo));
      gl->fbo_inited = false;
      gl->render_to_tex = false;
      gl->fbo_pass = 0;
   }
}

void gl_init_fbo(gl_t *gl, unsigned width, unsigned height)
{
   // No need to use FBOs.
   if (!g_settings.video.render_to_texture && gl_shader_num() == 0)
      return;

   struct gl_fbo_scale scale, scale_last;
   gl_shader_scale(1, &scale);
   gl_shader_scale(gl_shader_num(), &scale_last);

   // No need to use FBOs.
   if (gl_shader_num() == 1 && !scale.valid && !g_settings.video.render_to_texture)
      return;

   if (!load_fbo_proc(gl))
   {
      RARCH_ERR("Failed to locate FBO functions. Won't be able to use render-to-texture.\n");
      return;
   }

   gl->fbo_pass = gl_shader_num() - 1;
   if (scale_last.valid)
      gl->fbo_pass++;

   if (gl->fbo_pass <= 0)
      gl->fbo_pass = 1;

   if (!scale.valid)
   {
      scale.scale_x = g_settings.video.fbo.scale_x;
      scale.scale_y = g_settings.video.fbo.scale_y;
      scale.type_x  = scale.type_y = RARCH_SCALE_INPUT;
      scale.valid   = true;
   }

   gl->fbo_scale[0] = scale;

   for (int i = 1; i < gl->fbo_pass; i++)
   {
      gl_shader_scale(i + 1, &gl->fbo_scale[i]);

      if (!gl->fbo_scale[i].valid)
      {
         gl->fbo_scale[i].scale_x = gl->fbo_scale[i].scale_y = 1.0f;
         gl->fbo_scale[i].type_x  = gl->fbo_scale[i].type_y  = RARCH_SCALE_INPUT;
         gl->fbo_scale[i].valid   = true;
      }
   }

   gl_compute_fbo_geometry(gl, width, height, gl->win_width, gl->win_height);

   for (int i = 0; i < gl->fbo_pass; i++)
   {
      gl->fbo_rect[i].width  = next_pow2(gl->fbo_rect[i].img_width);
      gl->fbo_rect[i].height = next_pow2(gl->fbo_rect[i].img_height);
      RARCH_LOG("Creating FBO %d @ %ux%u\n", i, gl->fbo_rect[i].width, gl->fbo_rect[i].height);
   }

   gl_create_fbo_textures(gl);
   if (!gl_create_fbo_targets(gl))
   {
      glDeleteTextures(gl->fbo_pass, gl->fbo_texture);
      return;
   }

   gl->fbo_inited = true;
}
#endif

////////////

void gl_set_projection(gl_t *gl, struct gl_ortho *ortho, bool allow_rotate)
{
#ifdef RARCH_CONSOLE
   if (g_extern.console.screen.state.overscan.enable)
   {
      ortho->left = -g_extern.console.screen.overscan_amount / 2;
      ortho->right = 1 + g_extern.console.screen.overscan_amount / 2;
      ortho->bottom = -g_extern.console.screen.overscan_amount / 2;
   }
#endif

   // Calculate projection.
   math_matrix proj;
   matrix_ortho(&proj, ortho->left, ortho->right,
         ortho->bottom, ortho->top, ortho->znear, ortho->zfar);

   if (allow_rotate)
   {
      math_matrix rot;
      matrix_rotate_z(&rot, M_PI * gl->rotation / 180.0f);
      matrix_multiply(&proj, &rot, &proj);
   }

   gl->mvp = proj;
   gl_shader_set_coords(&gl->coords, &gl->mvp);
}

void gl_set_viewport(gl_t *gl, unsigned width, unsigned height, bool force_full, bool allow_rotate)
{
   unsigned x = 0, y = 0;
   struct gl_ortho ortho = {0, 1, 0, 1, -1, 1};

   if (gl->keep_aspect && !force_full)
   {
      float desired_aspect = g_settings.video.aspect_ratio;

      float device_aspect;
      if (gl->ctx_driver->translate_aspect)
         device_aspect = gl->ctx_driver->translate_aspect(width, height);
      else
         device_aspect = (float)width / height;

      float delta;

#ifdef RARCH_CONSOLE
      if (g_settings.video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         x      = g_extern.console.screen.viewports.custom_vp.x;
         y      = g_extern.console.screen.viewports.custom_vp.y;
         width  = g_extern.console.screen.viewports.custom_vp.width;
         height = g_extern.console.screen.viewports.custom_vp.height;
      }
      else
#endif
      {
         if (fabs(device_aspect - desired_aspect) < 0.0001)
         {
            // If the aspect ratios of screen and desired aspect ratio are sufficiently equal (floating point stuff), 
            // assume they are actually equal.
         }
         else if (device_aspect > desired_aspect)
         {
            delta = (desired_aspect / device_aspect - 1.0) / 2.0 + 0.5;
            x     = (unsigned)(width * (0.5 - delta));
            width = (unsigned)(2.0 * width * delta);
         }
         else
         {
            delta  = (device_aspect / desired_aspect - 1.0) / 2.0 + 0.5;
            y      = (unsigned)(height * (0.5 - delta));
            height = (unsigned)(2.0 * height * delta);
         }
      }
   }

   glViewport(x, y, width, height);

   gl_set_projection(gl, &ortho, allow_rotate);

   gl->vp_width  = width;
   gl->vp_height = height;

   // Set last backbuffer viewport.
   if (!force_full)
   {
      gl->vp_out_width  = width;
      gl->vp_out_height = height;
   }

   //RARCH_LOG("Setting viewport @ %ux%u\n", width, height);
}

static void gl_set_rotation(void *data, unsigned rotation)
{
   struct gl_ortho ortho = {0, 1, 0, 1, -1, 1};

   gl_t *gl = (gl_t*)driver.video_data;
   gl->rotation = 90 * rotation;
   gl_set_projection(gl, &ortho, true);
}

#ifdef HAVE_FBO

static inline void gl_start_frame_fbo(gl_t *gl)
{
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
   pglBindFramebuffer(GL_FRAMEBUFFER, gl->fbo[0]);
   gl->render_to_tex = true;
   gl_set_viewport(gl, gl->fbo_rect[0].img_width, gl->fbo_rect[0].img_height, true, false);

   // Need to preserve the "flipped" state when in FBO as well to have 
   // consistent texture coordinates.
   // We will "flip" it in place on last pass.
   if (gl->render_to_tex)
      gl->coords.vertex = vertexes;
}

static void gl_check_fbo_dimensions(gl_t *gl)
{
   // Check if we have to recreate our FBO textures.
   for (int i = 0; i < gl->fbo_pass; i++)
   {
      // Check proactively since we might suddently get sizes of tex_w width or tex_h height.
      if (gl->fbo_rect[i].max_img_width > gl->fbo_rect[i].width ||
            gl->fbo_rect[i].max_img_height > gl->fbo_rect[i].height)
      {
         unsigned img_width = gl->fbo_rect[i].max_img_width;
         unsigned img_height = gl->fbo_rect[i].max_img_height;
         unsigned max = img_width > img_height ? img_width : img_height;
         unsigned pow2_size = next_pow2(max);
         gl->fbo_rect[i].width = gl->fbo_rect[i].height = pow2_size;

         pglBindFramebuffer(GL_FRAMEBUFFER, gl->fbo[i]);
         glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i]);

         glTexImage2D(GL_TEXTURE_2D,
               0, RARCH_GL_INTERNAL_FORMAT32, gl->fbo_rect[i].width, gl->fbo_rect[i].height,
               0, RARCH_GL_TEXTURE_TYPE32,
               RARCH_GL_FORMAT32, NULL);

         pglFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->fbo_texture[i], 0);

         GLenum status = pglCheckFramebufferStatus(GL_FRAMEBUFFER);
         if (status != GL_FRAMEBUFFER_COMPLETE)
            RARCH_WARN("Failed to reinit FBO texture.\n");

         RARCH_LOG("Recreating FBO texture #%d: %ux%u\n", i, gl->fbo_rect[i].width, gl->fbo_rect[i].height);
      }
   }
}

static void gl_frame_fbo(gl_t *gl, const struct gl_tex_info *tex_info)
{
   GLfloat fbo_tex_coords[8] = {0.0f};

   // Render the rest of our passes.
   gl->coords.tex_coord = fbo_tex_coords;

   // It's kinda handy ... :)
   const struct gl_fbo_rect *prev_rect;
   const struct gl_fbo_rect *rect;
   struct gl_tex_info *fbo_info;

   struct gl_tex_info fbo_tex_info[MAX_SHADERS];
   unsigned fbo_tex_info_cnt = 0;

   // Calculate viewports, texture coordinates etc, and render all passes from FBOs, to another FBO.
   for (int i = 1; i < gl->fbo_pass; i++)
   {
      prev_rect = &gl->fbo_rect[i - 1];
      rect = &gl->fbo_rect[i];
      fbo_info = &fbo_tex_info[i - 1];

      GLfloat xamt = (GLfloat)prev_rect->img_width / prev_rect->width;
      GLfloat yamt = (GLfloat)prev_rect->img_height / prev_rect->height;

      set_texture_coords(fbo_tex_coords, xamt, yamt);

      fbo_info->tex = gl->fbo_texture[i - 1];
      fbo_info->input_size[0] = prev_rect->img_width;
      fbo_info->input_size[1] = prev_rect->img_height;
      fbo_info->tex_size[0] = prev_rect->width;
      fbo_info->tex_size[1] = prev_rect->height;
      memcpy(fbo_info->coord, fbo_tex_coords, sizeof(fbo_tex_coords));

      pglBindFramebuffer(GL_FRAMEBUFFER, gl->fbo[i]);
      gl_shader_use(i + 1);
      glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[i - 1]);

      glClear(GL_COLOR_BUFFER_BIT);

      // Render to FBO with certain size.
      gl_set_viewport(gl, rect->img_width, rect->img_height, true, false);
      gl_shader_set_params(prev_rect->img_width, prev_rect->img_height, 
            prev_rect->width, prev_rect->height, 
            gl->vp_width, gl->vp_height, gl->frame_count, 
            tex_info, gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

      gl_shader_set_coords(&gl->coords, &gl->mvp);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

      fbo_tex_info_cnt++;
   }

   // Render our last FBO texture directly to screen.
   prev_rect = &gl->fbo_rect[gl->fbo_pass - 1];
   GLfloat xamt = (GLfloat)prev_rect->img_width / prev_rect->width;
   GLfloat yamt = (GLfloat)prev_rect->img_height / prev_rect->height;

   set_texture_coords(fbo_tex_coords, xamt, yamt);

   // Render our FBO texture to back buffer.
   pglBindFramebuffer(GL_FRAMEBUFFER, 0);
   gl_shader_use(gl->fbo_pass + 1);

   glBindTexture(GL_TEXTURE_2D, gl->fbo_texture[gl->fbo_pass - 1]);

   glClear(GL_COLOR_BUFFER_BIT);
   gl->render_to_tex = false;
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
   gl_shader_set_params(prev_rect->img_width, prev_rect->img_height, 
         prev_rect->width, prev_rect->height, 
         gl->vp_width, gl->vp_height, gl->frame_count, 
         tex_info, gl->prev_info, fbo_tex_info, fbo_tex_info_cnt);

   gl->coords.vertex = vertex_ptr;

   gl_shader_set_coords(&gl->coords, &gl->mvp);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

   gl->coords.tex_coord = gl->tex_coords;
}
#endif

static void gl_update_resize(gl_t *gl)
{
#ifdef HAVE_FBO
   if (!gl->render_to_tex)
      gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
   else
   {
      gl_check_fbo_dimensions(gl);

      // Go back to what we're supposed to do, render to FBO #0 :D
      gl_start_frame_fbo(gl);
   }
#else
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
#endif
}

static void gl_update_input_size(gl_t *gl, unsigned width, unsigned height, unsigned pitch)
{
   // Res change. Need to clear out texture.
   if ((width != gl->last_width[gl->tex_index] || height != gl->last_height[gl->tex_index]) && gl->empty_buf)
   {
      gl->last_width[gl->tex_index] = width;
      gl->last_height[gl->tex_index] = height;

#if defined(HAVE_PSGL)
      glBufferSubData(GL_TEXTURE_REFERENCE_BUFFER_SCE,
		      gl->tex_w * gl->tex_h * gl->tex_index * gl->base_size,
		      gl->tex_w * gl->tex_h * gl->base_size,
		      gl->empty_buf);
#else
      glPixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(width * sizeof(uint32_t)));

      glTexSubImage2D(GL_TEXTURE_2D,
            0, 0, 0, gl->tex_w, gl->tex_h, gl->texture_type,
            gl->texture_fmt, gl->empty_buf);
#endif

      GLfloat xamt = (GLfloat)width / gl->tex_w;
      GLfloat yamt = (GLfloat)height / gl->tex_h;

      set_texture_coords(gl->tex_coords, xamt, yamt);
   }
   // We might have used different texture coordinates last frame. Edge case if resolution changes very rapidly.
   else if (width != gl->last_width[(gl->tex_index - 1) & TEXTURES_MASK] ||
         height != gl->last_height[(gl->tex_index - 1) & TEXTURES_MASK])
   {
      GLfloat xamt = (GLfloat)width / gl->tex_w;
      GLfloat yamt = (GLfloat)height / gl->tex_h;
      set_texture_coords(gl->tex_coords, xamt, yamt);
   }
}

// It is *much* faster (order of mangnitude on my setup) to use a custom SIMD-optimized conversion routine than letting GL do it :(
#if !defined(HAVE_PSGL) && !defined(HAVE_OPENGLES2)
static inline void gl_convert_frame_rgb16_32(gl_t *gl, void *output, const void *input, int width, int height, int in_pitch)
{
   if (width != gl->scaler.in_width || height != gl->scaler.in_height)
   {
      gl->scaler.in_width    = width;
      gl->scaler.in_height   = height;
      gl->scaler.out_width   = width;
      gl->scaler.out_height  = height;
      gl->scaler.in_fmt      = SCALER_FMT_RGB565;
      gl->scaler.out_fmt     = SCALER_FMT_ARGB8888;
      gl->scaler.scaler_type = SCALER_TYPE_POINT;
      scaler_ctx_gen_filter(&gl->scaler);
   }

   gl->scaler.in_stride  = in_pitch;
   gl->scaler.out_stride = width * sizeof(uint32_t);
   scaler_ctx_scale(&gl->scaler, output, input);
}
#endif

#if defined(HAVE_PSGL)
static inline void gl_copy_frame(gl_t *gl, const void *frame, unsigned width, unsigned height, unsigned pitch)
{
   if (!gl->fbo_inited)
      gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);

   size_t buffer_addr        = gl->tex_w * gl->tex_h * gl->tex_index * gl->base_size;
   size_t buffer_stride      = gl->tex_w * gl->base_size;
   const uint8_t *frame_copy = frame;
   size_t frame_copy_size    = width * gl->base_size;

   uint8_t *buffer = (uint8_t*)glMapBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, GL_WRITE_ONLY) + buffer_addr;
   for (unsigned h = 0; h < height; h++, buffer += buffer_stride, frame_copy += pitch)
      memcpy(buffer, frame_copy, frame_copy_size);

   glUnmapBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE);
}

static void gl_init_textures(gl_t *gl, const video_info_t *video)
{
   glGenTextures(TEXTURES, gl->texture);

   for (unsigned i = 0; i < TEXTURES; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->texture[i]);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl->border_type);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl->border_type);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl->tex_filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl->tex_filter);

      glTextureReferenceSCE(GL_TEXTURE_2D, 1,
            gl->tex_w, gl->tex_h, 0, 
            gl->texture_fmt,
            gl->tex_w * gl->base_size,
            gl->tex_w * gl->tex_h * i * gl->base_size);
   }
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}
#else
static inline void gl_copy_frame(gl_t *gl, const void *frame, unsigned width, unsigned height, unsigned pitch)
{
#ifdef HAVE_OPENGLES2
#ifdef HAVE_EGL
   if (gl->egl_images)
   {
      EGLImageKHR img = 0;
      bool new_egl = gl->ctx_driver->write_egl_image(frame, width, height, pitch, (gl->base_size == 4), gl->tex_index, &img);

      if (img == EGL_NO_IMAGE_KHR)
      {
         RARCH_ERR("[GL]: Failed to create EGL image.\n");
         return;
      }

      if (new_egl)
         pglEGLImageTargetTexture2DOES(GL_TEXTURE_2D, (GLeglImageOES)img);
   }
   else
#endif
   {
      // No GL_UNPACK_ROW_LENGTH ;(
      unsigned pitch_width = pitch / gl->base_size;
      if (width == pitch_width) // Happy path :D
      {
         glTexSubImage2D(GL_TEXTURE_2D,
               0, 0, 0, width, height, gl->texture_type,
               gl->texture_fmt, frame);
      }
      else // Slower path.
      {
         const uint8_t *src = (const uint8_t*)frame;
         for (unsigned h = 0; h < height; h++, src += pitch)
         {
            glTexSubImage2D(GL_TEXTURE_2D,
                  0, 0, h, width, 1, gl->texture_type,
                  gl->texture_fmt, src);
         }
      }
   }
#else
   glPixelStorei(GL_UNPACK_ALIGNMENT, get_alignment(pitch));
   if (gl->base_size == 2)
   {
      // Always use 32-bit textures on desktop GL.
      gl_convert_frame_rgb16_32(gl, gl->conv_buffer, frame, width, height, pitch);
      glTexSubImage2D(GL_TEXTURE_2D,
            0, 0, 0, width, height, gl->texture_type,
            gl->texture_fmt, gl->conv_buffer);
   }
   else
   {
      glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / gl->base_size);
      glTexSubImage2D(GL_TEXTURE_2D,
            0, 0, 0, width, height, gl->texture_type,
            gl->texture_fmt, frame);

      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
   }
#endif
}

static void gl_init_textures(gl_t *gl, const video_info_t *video)
{
#if defined(HAVE_EGL) && defined(HAVE_OPENGLES2)
   gl->egl_images = load_eglimage_proc(gl) && gl->ctx_driver->init_egl_image_buffer(video);
#else
   (void)video;
#endif

   glGenTextures(TEXTURES, gl->texture);
   for (unsigned i = 0; i < TEXTURES; i++)
   {
      glBindTexture(GL_TEXTURE_2D, gl->texture[i]);

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl->border_type);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl->border_type);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl->tex_filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl->tex_filter);

      if (!gl->egl_images)
      {
         glTexImage2D(GL_TEXTURE_2D,
               0, gl->internal_fmt, gl->tex_w, gl->tex_h, 0, gl->texture_type,
               gl->texture_fmt, gl->empty_buf ? gl->empty_buf : NULL);
      }
   }
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
}
#endif

static inline void gl_next_texture_index(gl_t *gl, const struct gl_tex_info *tex_info)
{
   memmove(gl->prev_info + 1, gl->prev_info, sizeof(*tex_info) * (TEXTURES - 1));
   memcpy(&gl->prev_info[0], tex_info, sizeof(*tex_info));
   gl->tex_index = (gl->tex_index + 1) & TEXTURES_MASK;
}

static inline void gl_set_shader_viewport(gl_t *gl, unsigned shader)
{
   gl_shader_use(shader);
   gl_set_viewport(gl, gl->win_width, gl->win_height, false, true);
}

static bool gl_frame(void *data, const void *frame, unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   RARCH_PERFORMANCE_INIT(frame_run);
   RARCH_PERFORMANCE_START(frame_run);

   gl_t *gl = (gl_t*)data;

   gl_shader_use(1);
   gl->frame_count++;

   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);

#ifdef HAVE_FBO
   // Render to texture in first pass.
   if (gl->fbo_inited)
   {
      // Recompute FBO geometry.
      // When width/height changes or window sizes change, we have to recalcuate geometry of our FBO.
      gl_compute_fbo_geometry(gl, width, height, gl->vp_out_width, gl->vp_out_height);
      gl_start_frame_fbo(gl);
   }
#endif

   if (gl->should_resize)
   {
      gl->should_resize = false;
      gl->ctx_driver->set_resize(gl->win_width, gl->win_height);

      // On resize, we might have to recreate our FBOs due to "Viewport" scale, and set a new viewport.
      gl_update_resize(gl);
   }

   if (frame) // Can be NULL for frame dupe / NULL render.
   {
      gl_update_input_size(gl, width, height, pitch);

      RARCH_PERFORMANCE_INIT(copy_frame);
      RARCH_PERFORMANCE_START(copy_frame);
      gl_copy_frame(gl, frame, width, height, pitch);
      RARCH_PERFORMANCE_STOP(copy_frame);
      RARCH_PERFORMANCE_LOG("gl_copy_frame", copy_frame);
   }

   struct gl_tex_info tex_info = {0};
   tex_info.tex           = gl->texture[gl->tex_index];
   tex_info.input_size[0] = width;
   tex_info.input_size[1] = height;
   tex_info.tex_size[0]   = gl->tex_w;
   tex_info.tex_size[1]   = gl->tex_h;

   memcpy(tex_info.coord, gl->tex_coords, sizeof(gl->tex_coords));

   glClear(GL_COLOR_BUFFER_BIT);
   gl_shader_set_params(width, height,
         gl->tex_w, gl->tex_h,
         gl->vp_width, gl->vp_height,
         gl->frame_count, 
         &tex_info, gl->prev_info, NULL, 0);

   gl_shader_set_coords(&gl->coords, &gl->mvp);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

#ifdef HAVE_FBO
   if (gl->fbo_inited)
      gl_frame_fbo(gl, &tex_info);
#endif

   gl_next_texture_index(gl, &tex_info);

   if (msg)
      gl_render_msg(gl, msg);

#ifndef RARCH_CONSOLE
   gl->ctx_driver->update_window_title(false);
#endif

   RARCH_PERFORMANCE_STOP(frame_run);
   RARCH_PERFORMANCE_LOG("gl_frame", frame_run);

#ifdef RARCH_CONSOLE
   if (!gl->block_swap)
#endif
      gl->ctx_driver->swap_buffers();

#ifdef HAVE_RMENU
   if (gl->draw_rmenu)
      gl->ctx_driver->rmenu_frame(gl);
#endif

   return true;
}

#ifndef NO_GL_FF_VERTEX
static void gl_disable_client_arrays(void)
{
   glDisableClientState(GL_VERTEX_ARRAY);
   glDisableClientState(GL_TEXTURE_COORD_ARRAY);
   glDisableClientState(GL_COLOR_ARRAY);
}
#endif

static void gl_free(void *data)
{
#ifdef RARCH_CONSOLE
   if (driver.video_data)
      return;
#endif

   gl_t *gl = (gl_t*)data;

   gl_deinit_font(gl);
   gl_shader_deinit();

#ifndef NO_GL_FF_VERTEX
   gl_disable_client_arrays();
#endif

   glDeleteTextures(TEXTURES, gl->texture);

#if defined(HAVE_PSGL)
   glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, 0);
   glDeleteBuffers(1, &gl->pbo);
#endif

#ifdef HAVE_FBO
   gl_deinit_fbo(gl);
#endif

   gl->ctx_driver->destroy();

   free(gl->empty_buf);
   free(gl->conv_buffer);

   free(gl);
}

static void gl_set_nonblock_state(void *data, bool state)
{
   gl_t *gl = (gl_t*)data;

   RARCH_LOG("GL VSync => %s\n", state ? "off" : "on");
   gl->ctx_driver->swap_interval(state ? 0 : 1);
}

static bool resolve_extensions(gl_t *gl)
{
#ifdef _WIN32
   // Win32 GL lib doesn't have some elementary functions needed.
   // Need to load dynamically :(
   if (!load_gl_proc_win32(gl))
      return false;
#endif

#ifdef NO_GL_CLAMP_TO_BORDER
   // NOTE: This will be a serious problem for some shaders.
   gl->border_type = GL_CLAMP_TO_EDGE;
#else
   gl->border_type = GL_CLAMP_TO_BORDER;
#endif

#ifdef HAVE_OPENGLES2
   if (!gl_query_extension("BGRA8888"))
   {
      RARCH_ERR("[GL]: GLES implementation does not have BGRA8888 extension.\n");
      return false;
   }
#endif

#if 0
   // Useful for debugging, but kinda obnoxious.
   const char *ext = (const char*)glGetString(GL_EXTENSIONS);
   if (ext)
      RARCH_LOG("[GL] Supported extensions: %s\n", ext);
#endif

   return true;
}


static void *gl_init(const video_info_t *video, const input_driver_t **input, void **input_data)
{
#ifdef _WIN32
   gfx_set_dwm();
#endif

#ifdef RARCH_CONSOLE
   if (driver.video_data)
      return driver.video_data;
#endif

   gl_t *gl = (gl_t*)calloc(1, sizeof(gl_t));
   if (!gl)
      return NULL;

#ifdef HAVE_OPENGLES
   gl->ctx_driver = gfx_ctx_init_first(GFX_CTX_OPENGL_ES_API);
#else
   gl->ctx_driver = gfx_ctx_init_first(GFX_CTX_OPENGL_API);
#endif
   if (!gl->ctx_driver)
   {
      free(gl);
      return NULL;
   }

   RARCH_LOG("Found GL context: %s\n", gl->ctx_driver->ident);

   gl->ctx_driver->get_video_size(&gl->full_x, &gl->full_y);
   RARCH_LOG("Detecting screen resolution %ux%u.\n", gl->full_x, gl->full_y);

   gl->ctx_driver->swap_interval(video->vsync ? 1 : 0);

   unsigned win_width  = video->width;
   unsigned win_height = video->height;
   if (video->fullscreen && (win_width == 0) && (win_height == 0))
   {
      win_width  = gl->full_x;
      win_height = gl->full_y;
   }

   if (!gl->ctx_driver->set_video_mode(win_width, win_height,
            g_settings.video.force_16bit ? 15 : 0, video->fullscreen))
   {
      free(gl);
      return NULL;
   }

#ifndef RARCH_CONSOLE
   gl->ctx_driver->update_window_title(true);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif

   if (!resolve_extensions(gl))
   {
      gl->ctx_driver->destroy();
      free(gl);
      return NULL;
   }

   gl->vsync      = video->vsync;
   gl->fullscreen = video->fullscreen;
   
   // Get real known video size, which might have been altered by context.
   gl->ctx_driver->get_video_size(&gl->win_width, &gl->win_height);
   RARCH_LOG("GL: Using resolution %ux%u\n", gl->win_width, gl->win_height);

   if (gl->full_x || gl->full_y) // We got bogus from gfx_ctx_get_video_size. Replace.
   {
      gl->full_x = gl->win_width;
      gl->full_y = gl->win_height;
   }

#if defined(HAVE_RMENU) && defined(HAVE_CG)
   RARCH_LOG("Initializing menu shader ...\n");
   gl_cg_set_menu_shader(default_paths.menu_shader_file);
#endif

#ifdef HAVE_GLSL
   gl_glsl_set_get_proc_address(gl->ctx_driver->get_proc_address);
#endif

   if (!gl_shader_init())
   {
      RARCH_ERR("Shader init failed.\n");
      gl->ctx_driver->destroy();
      free(gl);
      return NULL;
   }

   RARCH_LOG("GL: Loaded %u program(s).\n", gl_shader_num());

#ifdef HAVE_FBO
   // Set up render to texture.
   gl_init_fbo(gl, RARCH_SCALE_BASE * video->input_scale,
         RARCH_SCALE_BASE * video->input_scale);
#endif

   gl->keep_aspect = video->force_aspect;

   // Apparently need to set viewport for passes when we aren't using FBOs.
   gl_set_shader_viewport(gl, 0);
   gl_set_shader_viewport(gl, 1);

   bool force_smooth = false;
   if (gl_shader_filter_type(1, &force_smooth))
      gl->tex_filter = force_smooth ? GL_LINEAR : GL_NEAREST;
   else
      gl->tex_filter = video->smooth ? GL_LINEAR : GL_NEAREST;

   gl->internal_fmt = video->rgb32 ? RARCH_GL_INTERNAL_FORMAT32 : RARCH_GL_INTERNAL_FORMAT16;
   gl->texture_type = video->rgb32 ? RARCH_GL_TEXTURE_TYPE32 : RARCH_GL_TEXTURE_TYPE16;
   gl->texture_fmt  = video->rgb32 ? RARCH_GL_FORMAT32 : RARCH_GL_FORMAT16;
   gl->base_size    = video->rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);

#ifndef HAVE_OPENGLES
   glEnable(GL_TEXTURE_2D);
#endif

   glDisable(GL_DEPTH_TEST);
   glDisable(GL_DITHER);

   memcpy(gl->tex_coords, tex_coords, sizeof(tex_coords));
   gl->coords.vertex         = vertex_ptr;
   gl->coords.tex_coord      = gl->tex_coords;
   gl->coords.color          = white_color;
   gl->coords.lut_tex_coord  = tex_coords;
   gl_shader_set_coords(&gl->coords, &gl->mvp);

   gl->tex_w = RARCH_SCALE_BASE * video->input_scale;
   gl->tex_h = RARCH_SCALE_BASE * video->input_scale;

#if defined(HAVE_PSGL)
   glGenBuffers(1, &gl->pbo);
   glBindBuffer(GL_TEXTURE_REFERENCE_BUFFER_SCE, gl->pbo);
   glBufferData(GL_TEXTURE_REFERENCE_BUFFER_SCE,
         gl->tex_w * gl->tex_h * gl->base_size * TEXTURES, NULL, GL_STREAM_DRAW);
#endif

   // Empty buffer that we use to clear out the texture with on res change.
   gl->empty_buf = calloc(sizeof(uint32_t), gl->tex_w * gl->tex_h);

#if !defined(HAVE_PSGL)
   gl->conv_buffer = calloc(sizeof(uint32_t), gl->tex_w * gl->tex_h);
   if (!gl->conv_buffer)
   {
      gl->ctx_driver->destroy();
      free(gl);
      return NULL;
   }
#endif

   gl_init_textures(gl, video);

   for (unsigned i = 0; i < TEXTURES; i++)
   {
      gl->last_width[i] = gl->tex_w;
      gl->last_height[i] = gl->tex_h;
   }

   for (unsigned i = 0; i < TEXTURES; i++)
   {
      gl->prev_info[i].tex           = gl->texture[(gl->tex_index - (i + 1)) & TEXTURES_MASK];
      gl->prev_info[i].input_size[0] = gl->tex_w;
      gl->prev_info[i].tex_size[0]   = gl->tex_w;
      gl->prev_info[i].input_size[1] = gl->tex_h;
      gl->prev_info[i].tex_size[1]   = gl->tex_h;
      memcpy(gl->prev_info[i].coord, tex_coords, sizeof(tex_coords)); 
   }

   gl->ctx_driver->input_driver(input, input_data);
   gl_init_font(gl, g_settings.video.font_path, g_settings.video.font_size);

   if (!gl_check_error())
   {
      gl->ctx_driver->destroy();
      free(gl);
      return NULL;
   }

   return gl;
}

static bool gl_alive(void *data)
{
   gl_t *gl = (gl_t*)data;
   bool quit, resize;

   gl->ctx_driver->check_window(&quit,
         &resize, &gl->win_width, &gl->win_height,
         gl->frame_count);

   if (quit)
      gl->quitting = true;
   else if (resize)
      gl->should_resize = true;

   return !gl->quitting;
}

static bool gl_focus(void *data)
{
   gl_t *gl = (gl_t*)data;
   return gl->ctx_driver->has_focus();
}

#if defined(HAVE_XML) || defined(HAVE_CG)
static bool gl_set_shader(void *data, enum rarch_shader_type type, const char *path)
{
   gl_t *gl = (gl_t*)data;

#ifdef HAVE_FBO
   gl_deinit_fbo(gl);
   glBindTexture(GL_TEXTURE_2D, gl->texture[gl->tex_index]);
#endif

   gl_shader_deinit();

   switch (type)
   {
#ifdef HAVE_XML
      case RARCH_SHADER_BSNES:
         if (!gl_glsl_init(path))
            return false;
         break;
#endif

#ifdef HAVE_CG
      case RARCH_SHADER_CG:
         if (!gl_cg_init(path))
            return false;
         break;
#endif

      default:
         RARCH_ERR("Invalid shader type in gl_set_shader().\n");
         return false;
   }

#ifdef HAVE_FBO
   // Set up render to texture again.
   gl_init_fbo(gl, gl->tex_w, gl->tex_h);
#endif

   // Apparently need to set viewport for passes when we aren't using FBOs.
   gl_set_shader_viewport(gl, 0);
   gl_set_shader_viewport(gl, 1);

   return true;
}
#endif

#ifndef NO_GL_READ_VIEWPORT
static void gl_viewport_size(void *data, unsigned *width, unsigned *height)
{
   (void)data;

   GLint vp[4];
   glGetIntegerv(GL_VIEWPORT, vp);

   *width  = vp[2];
   *height = vp[3];
}

static bool gl_read_viewport(void *data, uint8_t *buffer)
{
   (void)data;

   GLint vp[4];
   glGetIntegerv(GL_VIEWPORT, vp);
   glPixelStorei(GL_PACK_ALIGNMENT, get_alignment(vp[2] * 3));

#ifdef HAVE_OPENGLES
   glReadPixels(vp[0], vp[1],
         vp[2], vp[3],
         GL_RGB, GL_UNSIGNED_BYTE, buffer);

   uint8_t *pixels = (uint8_t*)buffer;
   unsigned num_pixels = vp[2] * vp[3];
   // Convert RGB to BGR. Formats are byte ordered, so just swap 1st and 3rd byte.
   for (unsigned i = 0; i <= num_pixels; pixels += 3, i++)
   {
      uint8_t tmp = pixels[2];
      pixels[2] = pixels[0];
      pixels[0] = tmp;
   }
#else
   glPixelStorei(GL_PACK_ROW_LENGTH, vp[2]);

   glReadPixels(vp[0], vp[1],
         vp[2], vp[3],
         GL_BGR, GL_UNSIGNED_BYTE, buffer);
#endif

   return true;
}
#endif

#ifdef HAVE_RMENU
static void gl_init_menu(void *data)
{
   gl_t *gl = (gl_t*)data;

   RARCH_LOG("Initializing menu shader...\n");
   if (gl->ctx_driver->rmenu_init)
      gl->ctx_driver->rmenu_init();
}
#endif

#ifdef RARCH_CONSOLE
static void gl_start(void)
{
   video_info_t video_info = {0};

   // Might have to supply correct values here.
   video_info.vsync = g_settings.video.vsync;
   video_info.force_aspect = false;
   video_info.smooth = g_settings.video.smooth;
   video_info.input_scale = 2;
   video_info.fullscreen = true;
   if (g_settings.video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
   {
      video_info.width  = g_extern.console.screen.viewports.custom_vp.width;
      video_info.height = g_extern.console.screen.viewports.custom_vp.height;
   }
   driver.video_data = gl_init(&video_info, NULL, NULL);

   gl_t *gl = (gl_t*)driver.video_data;

   gl->ctx_driver->set_fbo(g_settings.video.fbo.enable);
   gl->ctx_driver->get_available_resolutions();

   gl_init_menu(gl);

#ifdef HAVE_FBO
// FBO mode has to be enabled once even if FBO mode has to be 
// turned off
   if (!g_settings.video.fbo.enable)
   {
      gl->ctx_driver->apply_fbo_state_changes(FBO_DEINIT);
      gl->ctx_driver->apply_fbo_state_changes(FBO_INIT);
      gl->ctx_driver->apply_fbo_state_changes(FBO_DEINIT);
   }
#endif
}

static void gl_stop(void)
{
   void *data = driver.video_data;
   driver.video_data = NULL;
   gl_free(data);
}

static void gl_restart(void)
{
   gl_t *gl = driver.video_data;

   if (!gl)
	   return;

#ifdef RARCH_CONSOLE
   bool should_block_swap = gl->block_swap;
#endif
#ifdef HAVE_RMENU
   bool should_draw_rmenu = gl->draw_rmenu;
#endif

   gl_stop();
#ifdef HAVE_CG
   gl_cg_invalidate_context();
#endif
   gl_start();

#ifdef HAVE_RMENU
   gl->draw_rmenu = should_draw_rmenu;
#endif

   gl->frame_count = 0;

#ifdef RARCH_CONSOLE
   gl->block_swap = should_block_swap;
   SET_TIMER_EXPIRATION(gl, 30);
#endif
}

static void gl_apply_state_changes(void)
{
   gl_t *gl = (gl_t*)driver.video_data;
   gl->should_resize = true;
}

static void gl_set_aspect_ratio(void *data, unsigned aspectratio_index)
{
   (void)data;
   gl_t *gl = driver.video_data;

   if (g_settings.video.aspect_ratio_idx == ASPECT_RATIO_AUTO)
      rarch_set_auto_viewport(g_extern.frame_cache.width, g_extern.frame_cache.height);
   else if(g_settings.video.aspect_ratio_idx == ASPECT_RATIO_CORE)
      rarch_set_core_viewport();

   g_settings.video.aspect_ratio = aspectratio_lut[g_settings.video.aspect_ratio_idx].value;
   g_settings.video.force_aspect = false;
   gl->keep_aspect = true;
   gl->should_resize = true;
}

#endif

const video_driver_t video_gl = {
   gl_init,
   gl_frame,
   gl_set_nonblock_state,
   gl_alive,
   gl_focus,

#if defined(HAVE_XML) || defined(HAVE_CG)
   gl_set_shader,
#else
   NULL,
#endif

   gl_free,
   "gl",

#ifdef RARCH_CONSOLE
   gl_start,
   gl_stop,
   gl_restart,
   gl_apply_state_changes,
   gl_set_aspect_ratio,
#endif

   gl_set_rotation,

#ifndef NO_GL_READ_VIEWPORT
   gl_viewport_size,
   gl_read_viewport,
#else
   NULL,
   NULL,
#endif
};

