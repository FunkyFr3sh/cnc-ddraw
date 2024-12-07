#include <windows.h>
#include <stdio.h>
#include "config.h"
#include "fps_limiter.h"
#include "opengl_utils.h"
#include "dd.h"
#include "ddsurface.h"
#include "openglshader.h"
#include "render_gdi.h"
#include "render_ogl.h"
#include "utils.h"
#include "debug.h"


static HGLRC ogl_create_core_context(HDC hdc);
static HGLRC ogl_create_context(HDC hdc);
static void ogl_build_programs();
static void ogl_create_textures(int width, int height);
static void ogl_init_main_program();
static void ogl_init_scale_program();
static void ogl_render();
static void ogl_delete_context(HGLRC context);
static BOOL ogl_texture_upload_test();
static BOOL ogl_shader_test();

static OGLRENDERER g_ogl;

DWORD WINAPI ogl_render_main(void)
{
    Sleep(500);
    g_ogl.got_error = g_ogl.use_opengl = FALSE;

    g_ogl.context = ogl_create_context(g_ddraw->render.hdc);
    if (g_ogl.context)
    {
        oglu_init();

        g_ogl.context = ogl_create_core_context(g_ddraw->render.hdc);

        if (oglu_ext_exists("WGL_EXT_swap_control", g_ddraw->render.hdc) && wglSwapIntervalEXT)
            wglSwapIntervalEXT(g_ddraw->vsync ? 1 : 0);

        fpsl_init();
        ogl_build_programs();
        ogl_create_textures(g_ddraw->width, g_ddraw->height);
        ogl_init_main_program();
        ogl_init_scale_program();

        g_ogl.got_error = g_ogl.got_error || !ogl_texture_upload_test();
        g_ogl.got_error = g_ogl.got_error || !ogl_shader_test();
        g_ogl.got_error = g_ogl.got_error || glGetError() != GL_NO_ERROR;
        g_ogl.use_opengl = (g_ogl.main_program || g_ddraw->bpp == 16 || g_ddraw->bpp == 32) && !g_ogl.got_error;

        ogl_render();

        ogl_delete_context(g_ogl.context);
    }

    if (!g_ogl.use_opengl)
    {
        g_ddraw->show_driver_warning = TRUE;
        g_ddraw->renderer = gdi_render_main;
        gdi_render_main();
    }

    return 0;
}

static HGLRC ogl_create_core_context(HDC hdc)
{
    if (!wglCreateContextAttribsARB)
        return g_ogl.context;

    int attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 2,
        WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0 };

    HGLRC context = wglCreateContextAttribsARB(hdc, 0, attribs);
    BOOL made_current = context && xwglMakeCurrent(hdc, context);

    if (made_current)
    {
        xwglDeleteContext(g_ogl.context);
        oglu_init();
        return context;
    }
    else if (context)
    {
        xwglDeleteContext(context);
    }

    return g_ogl.context;
}

static HGLRC ogl_create_context(HDC hdc)
{
    HGLRC context = xwglCreateContext(hdc);
    BOOL made_current = context && xwglMakeCurrent(hdc, context);

    if (!made_current || glGetError() != GL_NO_ERROR)
    {
        if (made_current)
        {
            xwglMakeCurrent(NULL, NULL);
            xwglDeleteContext(context);
        }

        context = 0;
    }

    return context;
}

static void ogl_build_programs()
{
    g_ogl.main_program = g_ogl.scale_program = 0;

    if (g_oglu_got_version3)
    {
        if (g_ddraw->bpp == 8)
        {
            g_ogl.main_program = oglu_build_program(PASSTHROUGH_VERT_SHADER, PALETTE_FRAG_SHADER);

            if (!g_ogl.main_program)
            {
                g_ogl.main_program = oglu_build_program(PASSTHROUGH_VERT_SHADER_CORE, PALETTE_FRAG_SHADER_CORE);
            }
        }
        else if (g_ddraw->bpp == 16 || g_ddraw->bpp == 32)
        {
            g_ogl.main_program = oglu_build_program(PASSTHROUGH_VERT_SHADER, PASSTHROUGH_FRAG_SHADER);

            if (!g_ogl.main_program)
            {
                g_ogl.main_program = oglu_build_program(PASSTHROUGH_VERT_SHADER_CORE, PASSTHROUGH_FRAG_SHADER_CORE);
            }
        }

        if (g_ogl.main_program)
        {
            char shader_path[MAX_PATH] = { 0 };

            strncpy(shader_path, g_ddraw->shader, sizeof(shader_path) - 1);

            if (GetFileAttributes(shader_path) == INVALID_FILE_ATTRIBUTES)
            {
                _snprintf(shader_path, sizeof(shader_path) - 1, "%s%s", g_config.game_path, g_ddraw->shader);
            }

            BOOL is_upscaler =
                strstr(g_ddraw->shader, "fsr.glsl") != NULL ||
                strstr(g_ddraw->shader, "catmull-rom-bilinear.glsl") != NULL ||
                strstr(g_ddraw->shader, "lanczos2-sharp.glsl") != NULL ||
                strstr(g_ddraw->shader, "xbr-lv2-noblend.glsl") != NULL ||
                strstr(g_ddraw->shader, "xbrz-freescale-multipass.glsl") != NULL ||
                strstr(g_ddraw->shader, "xbrz-freescale.glsl") != NULL;

            if (!is_upscaler ||
                g_ddraw->render.viewport.width != g_ddraw->width ||
                g_ddraw->render.viewport.height != g_ddraw->height ||
                g_ddraw->vhack)
            {
                g_ogl.scale_program = oglu_build_program_from_file(shader_path, wglCreateContextAttribsARB != NULL);
            }
        }
        else
        {
            g_oglu_got_version3 = FALSE;
        }

        g_ogl.filter_bilinear = strstr(g_ddraw->shader, "bilinear.glsl") != 0;
    }

    if (g_oglu_got_version2 && !g_ogl.main_program)
    {
        if (g_ddraw->bpp == 8)
        {
            g_ogl.main_program = oglu_build_program(PASSTHROUGH_VERT_SHADER_110, PALETTE_FRAG_SHADER_110);
        }
        else if (g_ddraw->bpp == 16 || g_ddraw->bpp == 32)
        {
            g_ogl.main_program = oglu_build_program(PASSTHROUGH_VERT_SHADER_110, PASSTHROUGH_FRAG_SHADER_110);
        }
    }
}

static void ogl_create_textures(int width, int height)
{
    g_ogl.surface_tex_width =
        width <= 1024 ? 1024 : width <= 2048 ? 2048 : width <= 4096 ? 4096 : width;

    g_ogl.surface_tex_height =
        height <= 512 ? 512 : height <= 1024 ? 1024 : height <= 2048 ? 2048 : height <= 4096 ? 4096 : height;

    g_ogl.surface_tex =
        HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, g_ogl.surface_tex_width * g_ogl.surface_tex_height * sizeof(int));

    g_ogl.adjust_alignment = (width % 4) != 0;

    g_ogl.scale_w = (float)width / g_ogl.surface_tex_width;
    g_ogl.scale_h = (float)height / g_ogl.surface_tex_height;

    glGenTextures(TEXTURE_COUNT, g_ogl.surface_tex_ids);

    for (int i = 0; i < TEXTURE_COUNT; i++)
    {
        glBindTexture(GL_TEXTURE_2D, g_ogl.surface_tex_ids[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        g_ogl.got_error = g_ogl.got_error || glGetError() != GL_NO_ERROR;

        while (glGetError() != GL_NO_ERROR);

        if (g_ddraw->bpp == 32)
        {
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RGBA8,
                g_ogl.surface_tex_width,
                g_ogl.surface_tex_height,
                0,
                g_ogl.surface_format = GL_BGRA,
                g_ogl.surface_type = GL_UNSIGNED_BYTE,
                0);
        }
        else if (g_ddraw->bpp == 16)
        {
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RGB565,
                g_ogl.surface_tex_width,
                g_ogl.surface_tex_height,
                0,
                g_ogl.surface_format = GL_RGB,
                g_ogl.surface_type = GL_UNSIGNED_SHORT_5_6_5,
                0);


            if (glGetError() != GL_NO_ERROR)
            {
                glTexImage2D(
                    GL_TEXTURE_2D,
                    0,
                    GL_RGB5,
                    g_ogl.surface_tex_width,
                    g_ogl.surface_tex_height,
                    0,
                    g_ogl.surface_format = GL_RGB,
                    g_ogl.surface_type = GL_UNSIGNED_SHORT_5_6_5,
                    0);
            }
        }
        else if (g_ddraw->bpp == 8)
        {
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_LUMINANCE8,
                g_ogl.surface_tex_width,
                g_ogl.surface_tex_height,
                0,
                g_ogl.surface_format = GL_LUMINANCE,
                g_ogl.surface_type = GL_UNSIGNED_BYTE,
                0);


            if (glGetError() != GL_NO_ERROR)
            {
                glTexImage2D(
                    GL_TEXTURE_2D,
                    0,
                    GL_R8,
                    g_ogl.surface_tex_width,
                    g_ogl.surface_tex_height,
                    0,
                    g_ogl.surface_format = GL_RED,
                    g_ogl.surface_type = GL_UNSIGNED_BYTE,
                    0);
            }

            if (glGetError() != GL_NO_ERROR)
            {
                glTexImage2D(
                    GL_TEXTURE_2D,
                    0,
                    GL_RED,
                    g_ogl.surface_tex_width,
                    g_ogl.surface_tex_height,
                    0,
                    g_ogl.surface_format = GL_RED,
                    g_ogl.surface_type = GL_UNSIGNED_BYTE,
                    0);
            }
        }
    }

    if (g_ddraw->bpp == 8)
    {
        glGenTextures(TEXTURE_COUNT, g_ogl.palette_tex_ids);

        for (int i = 0; i < TEXTURE_COUNT; i++)
        {
            glBindTexture(GL_TEXTURE_2D, g_ogl.palette_tex_ids[i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        }
    }
}

static void ogl_init_main_program()
{
    if (!g_ogl.main_program)
        return;

    glUseProgram(g_ogl.main_program);

    glUniform1i(glGetUniformLocation(g_ogl.main_program, "SurfaceTex"), 0);

    if (g_ddraw->bpp == 8)
        glUniform1i(glGetUniformLocation(g_ogl.main_program, "PaletteTex"), 1);

    if (g_oglu_got_version3)
    {
        g_ogl.main_vertex_coord_attr_loc = glGetAttribLocation(g_ogl.main_program, "VertexCoord");
        g_ogl.main_tex_coord_attr_loc = glGetAttribLocation(g_ogl.main_program, "TexCoord");

        glGenBuffers(3, g_ogl.main_vbos);

        if (g_ogl.scale_program)
        {
            glBindBuffer(GL_ARRAY_BUFFER, g_ogl.main_vbos[0]);
            static const GLfloat vertex_coord[] = {
                -1.0f,-1.0f,
                -1.0f, 1.0f,
                 1.0f, 1.0f,
                 1.0f,-1.0f,
            };
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_coord), vertex_coord, GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            glBindBuffer(GL_ARRAY_BUFFER, g_ogl.main_vbos[1]);
            GLfloat tex_coord[] = {
                0.0f,          0.0f,
                0.0f,          g_ogl.scale_h,
                g_ogl.scale_w, g_ogl.scale_h,
                g_ogl.scale_w, 0.0f,
            };
            glBufferData(GL_ARRAY_BUFFER, sizeof(tex_coord), tex_coord, GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        else
        {
            glBindBuffer(GL_ARRAY_BUFFER, g_ogl.main_vbos[0]);
            static const GLfloat vertex_coord[] = {
                -1.0f, 1.0f,
                 1.0f, 1.0f,
                 1.0f,-1.0f,
                -1.0f,-1.0f,
            };
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_coord), vertex_coord, GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            glBindBuffer(GL_ARRAY_BUFFER, g_ogl.main_vbos[1]);
            GLfloat tex_coord[] = {
                0.0f,          0.0f,
                g_ogl.scale_w, 0.0f,
                g_ogl.scale_w, g_ogl.scale_h,
                0.0f,          g_ogl.scale_h,
            };
            glBufferData(GL_ARRAY_BUFFER, sizeof(tex_coord), tex_coord, GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        glGenVertexArrays(1, &g_ogl.main_vao);
        glBindVertexArray(g_ogl.main_vao);

        glBindBuffer(GL_ARRAY_BUFFER, g_ogl.main_vbos[0]);
        glVertexAttribPointer(g_ogl.main_vertex_coord_attr_loc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
        glEnableVertexAttribArray(g_ogl.main_vertex_coord_attr_loc);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindBuffer(GL_ARRAY_BUFFER, g_ogl.main_vbos[1]);
        glVertexAttribPointer(g_ogl.main_tex_coord_attr_loc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
        glEnableVertexAttribArray(g_ogl.main_tex_coord_attr_loc);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ogl.main_vbos[2]);
        static const GLushort indices[] =
        {
            0, 1, 2,
            0, 2, 3,
        };
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        glBindVertexArray(0);

        const float mvp_matrix[16] = {
            1,0,0,0,
            0,1,0,0,
            0,0,1,0,
            0,0,0,1,
        };
        glUniformMatrix4fv(glGetUniformLocation(g_ogl.main_program, "MVPMatrix"), 1, GL_FALSE, mvp_matrix);

    }
}

static void ogl_init_scale_program()
{
    if (!g_ogl.scale_program)
        return;

    glUseProgram(g_ogl.scale_program);

    GLint vertex_coord_attr_loc = glGetAttribLocation(g_ogl.scale_program, "VertexCoord");
    g_ogl.scale_tex_coord_attr_loc = glGetAttribLocation(g_ogl.scale_program, "TexCoord");
    g_ogl.frame_count_uni_loc = glGetUniformLocation(g_ogl.scale_program, "FrameCount");

    glGenBuffers(3, g_ogl.scale_vbos);

    glBindBuffer(GL_ARRAY_BUFFER, g_ogl.scale_vbos[0]);
    static const GLfloat vertext_coord[] = {
        -1.0f, 1.0f,
         1.0f, 1.0f,
         1.0f,-1.0f,
        -1.0f,-1.0f,
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertext_coord), vertext_coord, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ARRAY_BUFFER, g_ogl.scale_vbos[1]);
    GLfloat tex_coord[] = {
        0.0f,           0.0f,
        g_ogl.scale_w,  0.0f,
        g_ogl.scale_w,  g_ogl.scale_h,
        0.0f,           g_ogl.scale_h,
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(tex_coord), tex_coord, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenVertexArrays(1, &g_ogl.scale_vao);
    glBindVertexArray(g_ogl.scale_vao);

    glBindBuffer(GL_ARRAY_BUFFER, g_ogl.scale_vbos[0]);
    glVertexAttribPointer(vertex_coord_attr_loc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(vertex_coord_attr_loc);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ARRAY_BUFFER, g_ogl.scale_vbos[1]);
    glVertexAttribPointer(g_ogl.scale_tex_coord_attr_loc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(g_ogl.scale_tex_coord_attr_loc);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ogl.scale_vbos[2]);
    static const GLushort indices[] =
    {
        0, 1, 2,
        0, 2, 3,
    };
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glBindVertexArray(0);

    float input_size[2], output_size[2], texture_size[2];

    input_size[0] = (float)g_ddraw->width;
    input_size[1] = (float)g_ddraw->height;
    texture_size[0] = (float)g_ogl.surface_tex_width;
    texture_size[1] = (float)g_ogl.surface_tex_height;
    output_size[0] = (float)g_ddraw->render.viewport.width;
    output_size[1] = (float)g_ddraw->render.viewport.height;

    glUniform2fv(glGetUniformLocation(g_ogl.scale_program, "OutputSize"), 1, output_size);
    glUniform2fv(glGetUniformLocation(g_ogl.scale_program, "TextureSize"), 1, texture_size);
    glUniform2fv(glGetUniformLocation(g_ogl.scale_program, "InputSize"), 1, input_size);
    glUniform1i(glGetUniformLocation(g_ogl.scale_program, "FrameDirection"), 1);
    glUniform1i(glGetUniformLocation(g_ogl.scale_program, "Texture"), 0);

    const float mvp_matrix[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1,
    };
    glUniformMatrix4fv(glGetUniformLocation(g_ogl.scale_program, "MVPMatrix"), 1, GL_FALSE, mvp_matrix);

    glGenFramebuffers(1, &g_ogl.frame_buffer_id);
    glBindFramebuffer(GL_FRAMEBUFFER, g_ogl.frame_buffer_id);

    glGenTextures(1, &g_ogl.frame_buffer_tex_id);
    glBindTexture(GL_TEXTURE_2D, g_ogl.frame_buffer_tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, g_ogl.filter_bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, g_ogl.filter_bilinear ? GL_LINEAR : GL_NEAREST);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        g_ogl.surface_tex_width,
        g_ogl.surface_tex_height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        0);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_ogl.frame_buffer_tex_id, 0);

    GLenum draw_buffers[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, draw_buffers);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        glDeleteTextures(1, &g_ogl.frame_buffer_tex_id);

        if (glDeleteFramebuffers)
            glDeleteFramebuffers(1, &g_ogl.frame_buffer_id);

        if (glDeleteProgram)
            glDeleteProgram(g_ogl.scale_program);

        g_ogl.scale_program = 0;

        if (glDeleteBuffers)
            glDeleteBuffers(3, g_ogl.scale_vbos);

        if (glDeleteVertexArrays)
            glDeleteVertexArrays(1, &g_ogl.scale_vao);

        if (g_ogl.main_program)
        {
            glBindVertexArray(g_ogl.main_vao);
            glBindBuffer(GL_ARRAY_BUFFER, g_ogl.main_vbos[0]);
            static const GLfloat vertex_coord_pal[] = {
                -1.0f, 1.0f,
                 1.0f, 1.0f,
                 1.0f,-1.0f,
                -1.0f,-1.0f,
            };
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_coord_pal), vertex_coord_pal, GL_STATIC_DRAW);
            glVertexAttribPointer(g_ogl.main_vertex_coord_attr_loc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
            glEnableVertexAttribArray(g_ogl.main_vertex_coord_attr_loc);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);

            glBindVertexArray(g_ogl.main_vao);
            glBindBuffer(GL_ARRAY_BUFFER, g_ogl.main_vbos[1]);
            GLfloat tex_coord_pal[] = {
                0.0f,          0.0f,
                g_ogl.scale_w, 0.0f,
                g_ogl.scale_w, g_ogl.scale_h,
                0.0f,          g_ogl.scale_h,
            };
            glBufferData(GL_ARRAY_BUFFER, sizeof(tex_coord_pal), tex_coord_pal, GL_STATIC_DRAW);
            glVertexAttribPointer(g_ogl.main_tex_coord_attr_loc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
            glEnableVertexAttribArray(g_ogl.main_tex_coord_attr_loc);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void ogl_render()
{
    BOOL needs_update = FALSE;
    LONG clear_count = 0;

    glViewport(
        g_ddraw->render.viewport.x, g_ddraw->render.viewport.y,
        g_ddraw->render.viewport.width, g_ddraw->render.viewport.height);

    if (g_ogl.main_program)
    {
        glUseProgram(g_ogl.main_program);
    }
    else if (g_ddraw->bpp == 16 || g_ddraw->bpp == 32)
    {
        glEnable(GL_TEXTURE_2D);
    }

    DWORD timeout = g_ddraw->render.minfps > 0 ? g_ddraw->render.minfps_tick_len : INFINITE;

    while (g_ogl.use_opengl && g_ddraw->render.run &&
        (g_ddraw->render.minfps < 0 || WaitForSingleObject(g_ddraw->render.sem, timeout) != WAIT_FAILED))
    {
#if _DEBUG
        dbg_draw_frame_info_start();
#endif

        g_ogl.scale_w = (float)g_ddraw->width / g_ogl.surface_tex_width;
        g_ogl.scale_h = (float)g_ddraw->height / g_ogl.surface_tex_height;

        static int tex_index = 0, pal_index = 0;

        BOOL scale_changed = FALSE;

        if (InterlockedExchange(&g_ddraw->render.clear_screen, FALSE))
            clear_count = 10;

        fpsl_frame_start();

        EnterCriticalSection(&g_ddraw->cs);

        if (g_ddraw->primary && 
            g_ddraw->primary->bpp == g_ddraw->bpp &&
            (g_ddraw->bpp == 16 || g_ddraw->bpp == 32 || g_ddraw->primary->palette))
        {
            if (g_ddraw->vhack)
            {
                if (util_detect_low_res_screen())
                {
                    g_ogl.scale_w *= (float)g_ddraw->upscale_hack_width / g_ddraw->width;
                    g_ogl.scale_h *= (float)g_ddraw->upscale_hack_height / g_ddraw->height;

                    if (!InterlockedExchange(&g_ddraw->upscale_hack_active, TRUE))
                        scale_changed = TRUE;
                }
                else
                {
                    if (InterlockedExchange(&g_ddraw->upscale_hack_active, FALSE))
                        scale_changed = TRUE;
                }
            }

            if (g_ddraw->bpp == 8 &&
                (InterlockedExchange(&g_ddraw->render.palette_updated, FALSE) || g_ddraw->render.minfps == -2))
            {
                if (++pal_index >= TEXTURE_COUNT)
                    pal_index = 0;

                glBindTexture(GL_TEXTURE_2D, g_ogl.palette_tex_ids[pal_index]);

                glTexSubImage2D(
                    GL_TEXTURE_2D,
                    0,
                    0,
                    0,
                    256,
                    1,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    g_ddraw->primary->palette->data_bgr);
            }

            if (InterlockedExchange(&g_ddraw->render.surface_updated, FALSE) || g_ddraw->render.minfps == -2)
            {
                if (++tex_index >= TEXTURE_COUNT)
                    tex_index = 0;

                glBindTexture(GL_TEXTURE_2D, g_ogl.surface_tex_ids[tex_index]);

                if (g_ogl.adjust_alignment)
                    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

                glTexSubImage2D(
                    GL_TEXTURE_2D,
                    0,
                    0,
                    0,
                    g_ddraw->width,
                    g_ddraw->height,
                    g_ogl.surface_format,
                    g_ogl.surface_type,
                    g_ddraw->primary->surface);

                if (g_ogl.adjust_alignment)
                    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            }

            static int error_check_count = 0;

            if (error_check_count < 10)
            {
                error_check_count++;

                GLenum err = glGetError();

                if (err != GL_NO_ERROR && err != GL_INVALID_FRAMEBUFFER_OPERATION)
                    g_ogl.use_opengl = FALSE;
            }

            if (g_ddraw->fixchilds)
            {
                g_ddraw->child_window_exists = g_ddraw->bnet_active && GetSystemMetrics(SM_CMONITORS) > 1;
                EnumChildWindows(g_ddraw->hwnd, util_enum_child_proc, (LPARAM)g_ddraw->primary);

                if (g_ddraw->render.width != g_ddraw->width || g_ddraw->render.height != g_ddraw->height)
                {
                    if (g_ddraw->child_window_exists)
                    {
                        glClear(GL_COLOR_BUFFER_BIT);

                        if (!needs_update)
                        {
                            glViewport(0, g_ddraw->render.height - g_ddraw->height, g_ddraw->width, g_ddraw->height);
                            needs_update = TRUE;
                        }
                    }
                    else if (needs_update)
                    {
                        glViewport(
                            g_ddraw->render.viewport.x, g_ddraw->render.viewport.y,
                            g_ddraw->render.viewport.width, g_ddraw->render.viewport.height);

                        needs_update = FALSE;
                    }
                }
            }

            if (g_ddraw->bnet_active)
            {
                glClear(GL_COLOR_BUFFER_BIT);

                glBindTexture(GL_TEXTURE_2D, g_ogl.surface_tex_ids[tex_index]);

                if (g_ogl.adjust_alignment)
                    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

                glTexSubImage2D(
                    GL_TEXTURE_2D,
                    0,
                    0,
                    0,
                    g_ddraw->width,
                    g_ddraw->height,
                    g_ogl.surface_format,
                    g_ogl.surface_type,
                    g_ddraw->primary->bnet_surface);

                if (g_ogl.adjust_alignment)
                    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            }
        }

        LeaveCriticalSection(&g_ddraw->cs);

        if (g_ddraw->wine)
        {
            glClear(GL_COLOR_BUFFER_BIT);
        }
        else if (clear_count > 0)
        {
            clear_count--;
            glClear(GL_COLOR_BUFFER_BIT);
        }

        if (scale_changed)
        {
            if (g_ogl.scale_program && g_ogl.main_program)
            {
                glBindVertexArray(g_ogl.scale_vao);
                glBindBuffer(GL_ARRAY_BUFFER, g_ogl.scale_vbos[1]);
                GLfloat texCoord[] = {
                    0.0f,           0.0f,
                    g_ogl.scale_w,  0.0f,
                    g_ogl.scale_w,  g_ogl.scale_h,
                    0.0f,           g_ogl.scale_h,
                };
                glBufferData(GL_ARRAY_BUFFER, sizeof(texCoord), texCoord, GL_STATIC_DRAW);
                glVertexAttribPointer(g_ogl.scale_tex_coord_attr_loc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                glEnableVertexAttribArray(g_ogl.scale_tex_coord_attr_loc);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glBindVertexArray(0);
            }
            else if (g_oglu_got_version3 && g_ogl.main_program)
            {
                glBindVertexArray(g_ogl.main_vao);
                glBindBuffer(GL_ARRAY_BUFFER, g_ogl.main_vbos[1]);
                GLfloat texCoord[] = {
                    0.0f,           0.0f,
                    g_ogl.scale_w,  0.0f,
                    g_ogl.scale_w,  g_ogl.scale_h,
                    0.0f,           g_ogl.scale_h,
                };
                glBufferData(GL_ARRAY_BUFFER, sizeof(texCoord), texCoord, GL_STATIC_DRAW);
                glVertexAttribPointer(g_ogl.main_tex_coord_attr_loc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                glEnableVertexAttribArray(g_ogl.main_tex_coord_attr_loc);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glBindVertexArray(0);
            }
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_ogl.surface_tex_ids[tex_index]);

        if (g_ddraw->bpp == 8)
        {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, g_ogl.palette_tex_ids[pal_index]);

            glActiveTexture(GL_TEXTURE0);
        }

        if (g_ogl.scale_program && g_ogl.main_program)
        {
            /* draw surface into framebuffer */
            glUseProgram(g_ogl.main_program);

            glViewport(0, 0, g_ddraw->width, g_ddraw->height);

            glBindFramebuffer(GL_FRAMEBUFFER, g_ogl.frame_buffer_id);

            glBindVertexArray(g_ogl.main_vao);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
            glBindVertexArray(0);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);

            if (g_ddraw->child_window_exists)
            {
                glViewport(0, g_ddraw->render.height - g_ddraw->height, g_ddraw->width, g_ddraw->height);
            }
            else
            {
                glViewport(
                    g_ddraw->render.viewport.x, 
                    g_ddraw->render.viewport.y,
                    g_ddraw->render.viewport.width, 
                    g_ddraw->render.viewport.height);
            }

            /* apply filter */

            glUseProgram(g_ogl.scale_program);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, g_ogl.frame_buffer_tex_id);

            static int frames = 1;
            if (g_ogl.frame_count_uni_loc != -1)
                glUniform1i(g_ogl.frame_count_uni_loc, frames++);

            glBindVertexArray(g_ogl.scale_vao);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
            glBindVertexArray(0);
        }
        else if (g_oglu_got_version3 && g_ogl.main_program)
        {
            glBindVertexArray(g_ogl.main_vao);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
            glBindVertexArray(0);
        }
        else
        {
            glBegin(GL_TRIANGLE_FAN);
            glTexCoord2f(0, 0);                          glVertex2f(-1, 1);
            glTexCoord2f(g_ogl.scale_w, 0);              glVertex2f(1, 1);
            glTexCoord2f(g_ogl.scale_w, g_ogl.scale_h);  glVertex2f(1, -1);
            glTexCoord2f(0, g_ogl.scale_h);              glVertex2f(-1, -1);
            glEnd();
        }

        SwapBuffers(g_ddraw->render.hdc);

#if _DEBUG
        dbg_draw_frame_info_end();
#endif

        fpsl_frame_end();
    }

    if (g_ddraw->vhack)
        InterlockedExchange(&g_ddraw->upscale_hack_active, FALSE);
}

static void ogl_delete_context(HGLRC context)
{
    HeapFree(GetProcessHeap(), 0, g_ogl.surface_tex);
    glDeleteTextures(TEXTURE_COUNT, g_ogl.surface_tex_ids);

    if (g_ddraw->bpp == 8)
        glDeleteTextures(TEXTURE_COUNT, g_ogl.palette_tex_ids);

    if (glUseProgram)
        glUseProgram(0);

    if (g_ogl.scale_program)
    {
        glDeleteTextures(1, &g_ogl.frame_buffer_tex_id);

        if (glDeleteBuffers)
            glDeleteBuffers(3, g_ogl.scale_vbos);

        if (glDeleteFramebuffers)
            glDeleteFramebuffers(1, &g_ogl.frame_buffer_id);

        if (glDeleteVertexArrays)
            glDeleteVertexArrays(1, &g_ogl.scale_vao);
    }

    if (glDeleteProgram)
    {
        if (g_ogl.main_program)
            glDeleteProgram(g_ogl.main_program);

        if (g_ogl.scale_program)
            glDeleteProgram(g_ogl.scale_program);
    }

    if (g_oglu_got_version3)
    {
        if (g_ogl.main_program)
        {
            if (glDeleteBuffers)
                glDeleteBuffers(3, g_ogl.main_vbos);

            if (glDeleteVertexArrays)
                glDeleteVertexArrays(1, &g_ogl.main_vao);
        }
    }

    xwglMakeCurrent(NULL, NULL);
    xwglDeleteContext(context);
}

static BOOL ogl_texture_upload_test()
{
    static char test_data[] = { 0,1,2,0,0,2,3,0,0,4,5,0,0,6,7,0,0,8,9,0 };

    int i;
    for (i = 0; i < TEXTURE_COUNT; i++)
    {
        memcpy(g_ogl.surface_tex, test_data, sizeof(test_data));

        glBindTexture(GL_TEXTURE_2D, g_ogl.surface_tex_ids[i]);

        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0,
            0,
            g_ddraw->width,
            g_ddraw->height,
            g_ogl.surface_format,
            g_ogl.surface_type,
            g_ogl.surface_tex);

        memset(g_ogl.surface_tex, 0, sizeof(test_data));

        glGetTexImage(GL_TEXTURE_2D, 0, g_ogl.surface_format, g_ogl.surface_type, g_ogl.surface_tex);

        if (memcmp(g_ogl.surface_tex, test_data, sizeof(test_data)) != 0)
            return FALSE;
    }

    if (g_ddraw->bpp == 8)
    {
        for (i = 0; i < TEXTURE_COUNT; i++)
        {
            glBindTexture(GL_TEXTURE_2D, g_ogl.palette_tex_ids[i]);

            glTexSubImage2D(
                GL_TEXTURE_2D,
                0,
                0,
                0,
                256,
                1,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                g_ogl.surface_tex);

            memset(g_ogl.surface_tex, 0, sizeof(test_data));

            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, g_ogl.surface_tex);

            if (memcmp(g_ogl.surface_tex, test_data, sizeof(test_data)) != 0)
                return FALSE;
        }
    }
    return TRUE;
}

static BOOL ogl_shader_test()
{
    BOOL result = TRUE;

    if (g_ddraw->bpp != 8)
        return result;

    if (g_oglu_got_version3 && g_ogl.main_program)
    {
        memset(g_ogl.surface_tex, 0, g_ogl.surface_tex_height * g_ogl.surface_tex_width * sizeof(int));

        GLuint fbo_id = 0;
        glGenFramebuffers(1, &fbo_id);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);

        GLuint fbo_tex_id = 0;
        glGenTextures(1, &fbo_tex_id);
        glBindTexture(GL_TEXTURE_2D, fbo_tex_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            g_ogl.surface_tex_width,
            g_ogl.surface_tex_height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            g_ogl.surface_tex);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_tex_id, 0);

        GLenum draw_buffers[1] = { GL_COLOR_ATTACHMENT0 };
        glDrawBuffers(1, draw_buffers);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
        {
            static char gray0pal[] = { 128,128,128,128 };

            glBindTexture(GL_TEXTURE_2D, g_ogl.palette_tex_ids[0]);

            glTexSubImage2D(
                GL_TEXTURE_2D,
                0,
                0,
                0,
                1,
                1,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                gray0pal);

            glBindTexture(GL_TEXTURE_2D, g_ogl.surface_tex_ids[0]);

            glTexSubImage2D(
                GL_TEXTURE_2D,
                0,
                0,
                0,
                g_ogl.surface_tex_width,
                g_ogl.surface_tex_height,
                g_ogl.surface_format,
                GL_UNSIGNED_BYTE,
                g_ogl.surface_tex);

            glViewport(0, 0, g_ogl.surface_tex_width, g_ogl.surface_tex_height);

            glUseProgram(g_ogl.main_program);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, g_ogl.palette_tex_ids[0]);
            glActiveTexture(GL_TEXTURE0);

            glBindVertexArray(g_ogl.main_vao);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
            glBindVertexArray(0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE0);

            glBindTexture(GL_TEXTURE_2D, fbo_tex_id);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, g_ogl.surface_tex);

            int i;
            for (i = 0; i < g_ogl.surface_tex_height * g_ogl.surface_tex_width; i++)
            {
                if (g_ogl.surface_tex[i] != 0x80808080)
                {
                    result = FALSE;
                    break;
                }
            }
        }

        glBindTexture(GL_TEXTURE_2D, 0);

        if (glDeleteFramebuffers)
            glDeleteFramebuffers(1, &fbo_id);

        glDeleteTextures(1, &fbo_tex_id);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    return result;
}
