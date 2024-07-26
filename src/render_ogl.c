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
static void ogl_build_programs();
static void ogl_create_textures(int width, int height);
static void ogl_init_main_program();
static void ogl_init_shader1_program();
static void ogl_init_shader2_program();
static void ogl_render();
static BOOL ogl_release_resources();
static BOOL ogl_texture_upload_test();
static BOOL ogl_shader_test();
static void ogl_check_error(const char* stmt);

static OGLRENDERER g_ogl;

BOOL ogl_create()
{
    if (g_ogl.hwnd == g_ddraw.hwnd && g_ogl.hdc == g_ddraw.render.hdc && g_ogl.context)
    {
        return TRUE;
    }

    ogl_release();

    g_ogl.context = xwglCreateContext(g_ddraw.render.hdc);
    if (g_ogl.context)
    {
        g_ogl.hwnd = g_ddraw.hwnd;
        g_ogl.hdc = g_ddraw.render.hdc;

        return TRUE;
    }

    g_ogl.hwnd = NULL;
    g_ogl.hdc = NULL;

    return FALSE;
}

DWORD WINAPI ogl_render_main(void)
{
    //Sleep(250);
    g_ogl.got_error = g_ogl.use_opengl = FALSE;
    GLenum err = GL_NO_ERROR;
    BOOL made_current = FALSE;

    for (int i = 0; i < 5; i++)
    {
        if ((made_current = xwglMakeCurrent(g_ogl.hdc, g_ogl.context)))
            break;

        Sleep(50);
    }

    if (made_current && (err = glGetError()) == GL_NO_ERROR)
    {
        GL_CHECK(oglu_init());

        TRACE("+--OpenGL-----------------------------------------\n");
        TRACE("| GL_VERSION:                  %s\n", glGetString(GL_VERSION));
        TRACE("| GL_VENDOR:                   %s\n", glGetString(GL_VENDOR));
        TRACE("| GL_RENDERER:                 %s\n", glGetString(GL_RENDERER));
        TRACE("| GL_SHADING_LANGUAGE_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
        TRACE("+------------------------------------------------\n");

        GL_CHECK(g_ogl.context = ogl_create_core_context(g_ogl.hdc));

        BOOL got_swap_ctrl;
        GL_CHECK(got_swap_ctrl = oglu_ext_exists("WGL_EXT_swap_control", g_ogl.hdc));

        if (got_swap_ctrl && wglSwapIntervalEXT)
            wglSwapIntervalEXT(g_config.vsync ? 1 : 0);

        fpsl_init();
        GL_CHECK(ogl_build_programs());
        GL_CHECK(ogl_create_textures(g_ddraw.width, g_ddraw.height));
        GL_CHECK(ogl_init_main_program());
        GL_CHECK(ogl_init_shader1_program());
        GL_CHECK(ogl_init_shader2_program());

        //GL_CHECK(g_ogl.got_error = g_ogl.got_error || !ogl_texture_upload_test());
        //GL_CHECK(g_ogl.got_error = g_ogl.got_error || !ogl_shader_test());
        g_ogl.got_error = g_ogl.got_error || (err = glGetError()) != GL_NO_ERROR;
        g_ogl.use_opengl = (g_ogl.main_program || g_ddraw.bpp == 16 || g_ddraw.bpp == 32) && !g_ogl.got_error;

        GL_CHECK(ogl_render());

        GL_CHECK(ogl_release_resources());

        while (glGetError() != GL_NO_ERROR);
    }
    else
    {
        TRACE("OpenGL error %08x, GetLastError %lu (xwglMakeCurrent())\n", err, GetLastError());
        ogl_check_error("xwglMakeCurrent()");
    }

    xwglMakeCurrent(NULL, NULL);
    
    if (!g_ogl.use_opengl)
    {
        g_ddraw.show_driver_warning = TRUE;
        g_ddraw.renderer = gdi_render_main;
        gdi_render_main();
    }

    return 0;
}


static void ogl_check_error(const char* stmt)
{
#ifdef _DEBUG
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR)
    {
        g_ogl.got_error = TRUE;
        TRACE("OpenGL error %08x (%s)\n", err, stmt);
    }
#endif
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

        TRACE("+--OpenGL Core-----------------------------------\n");
        TRACE("| GL_VERSION:                  %s\n", glGetString(GL_VERSION));
        TRACE("| GL_VENDOR:                   %s\n", glGetString(GL_VENDOR));
        TRACE("| GL_RENDERER:                 %s\n", glGetString(GL_RENDERER));
        TRACE("| GL_SHADING_LANGUAGE_VERSION: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
        TRACE("+------------------------------------------------\n");

        return context;
    }
    else if (context)
    {
        xwglDeleteContext(context);
    }

    return g_ogl.context;
}

static void ogl_build_programs()
{
    g_ogl.main_program = g_ogl.shader1_program = g_ogl.shader2_program = 0;

    g_ogl.shader2_upscale = FALSE;
    BOOL core_profile = wglCreateContextAttribsARB != NULL;

    if (g_oglu_got_version3)
    {
        if (g_ddraw.bpp == 8)
        {
            g_ogl.main_program = oglu_build_program(PASSTHROUGH_VERT_SHADER, PALETTE_FRAG_SHADER, core_profile);
        }
        else if (g_ddraw.bpp == 16 && g_config.rgb555)
        {
            g_ogl.main_program = oglu_build_program(PASSTHROUGH_VERT_SHADER, RGB555_FRAG_SHADER, core_profile);
        }
        else if (g_ddraw.bpp == 16 || g_ddraw.bpp == 32)
        {
            g_ogl.main_program = oglu_build_program(PASSTHROUGH_VERT_SHADER, PASSTHROUGH_FRAG_SHADER, core_profile);
        }

        BOOL bilinear = FALSE;
        char shader_path[MAX_PATH] = { 0 };

        if (g_ogl.main_program)
        {
            strncpy(shader_path, g_config.shader, sizeof(shader_path));
            shader_path[sizeof(shader_path) - 1] = '\0'; /* strncpy fix */

            if (GetFileAttributes(shader_path) == INVALID_FILE_ATTRIBUTES)
            {
                _snprintf(shader_path, sizeof(shader_path) - 1, "%s%s", g_config.dll_path, g_config.shader);
            }

            /* detect common upscaling shaders and disable them if no upscaling is required */

            BOOL is_upscaler =
                strstr(g_config.shader, "fsr.glsl") != NULL ||
                strstr(g_config.shader, "catmull-rom-bilinear.glsl") != NULL ||
                strstr(g_config.shader, "lanczos2-sharp.glsl") != NULL ||
                strstr(g_config.shader, "xbr-lv2-noblend.glsl") != NULL ||
                strstr(g_config.shader, "xbrz-freescale-multipass.glsl") != NULL ||
                strstr(g_config.shader, "xbrz-freescale.glsl") != NULL;

            if (!is_upscaler ||
                g_ddraw.render.viewport.width != g_ddraw.width ||
                g_ddraw.render.viewport.height != g_ddraw.height ||
                g_config.vhack)
            {
                g_ogl.shader1_program = oglu_build_program_from_file(shader_path, core_profile);

                if (g_ogl.shader1_program && strstr(g_config.shader, "xbrz-freescale-multipass.glsl") != NULL)
                {
                    g_ogl.shader2_upscale = TRUE;
                }

                if (!g_ogl.shader1_program &&
                    (g_ddraw.render.viewport.width != g_ddraw.width ||
                        g_ddraw.render.viewport.height != g_ddraw.height ||
                        g_config.vhack))
                {
                    g_ogl.shader1_program = 
                        oglu_build_program(
                            _stricmp(g_config.shader, "xBR-lv2") == 0 ? XBR_LV2_VERT_SHADER :
                            PASSTHROUGH_VERT_SHADER, 
                            _stricmp(g_config.shader, "Nearest neighbor") == 0 ? PASSTHROUGH_FRAG_SHADER :
                            _stricmp(g_config.shader, "Bilinear") == 0 ? PASSTHROUGH_FRAG_SHADER :
                            _stricmp(g_config.shader, "Lanczos") == 0 ? LANCZOS2_FRAG_SHADER :
                            _stricmp(g_config.shader, "xBR-lv2") == 0 ? XBR_LV2_FRAG_SHADER :
                            CATMULL_ROM_FRAG_SHADER, 
                            core_profile);

                    bilinear =
                        _stricmp(g_config.shader, "Nearest neighbor") != 0 && 
                        _stricmp(g_config.shader, "Lanczos") != 0 &&
                        _stricmp(g_config.shader, "xBR-lv2") != 0;
                }
            }
        }
        else
        {
            g_oglu_got_version3 = FALSE;
        }

        if (g_ogl.shader1_program)
        {
            if (strlen(shader_path) <= sizeof(shader_path) - 8)
            {
                strcat(shader_path, ".pass1");

                g_ogl.shader2_program = oglu_build_program_from_file(shader_path, core_profile);
            }
        }

        g_ogl.filter_bilinear = strstr(g_config.shader, "bilinear.glsl") != NULL || bilinear;
    }

    if (g_oglu_got_version2 && !g_ogl.main_program)
    {
        if (g_ddraw.bpp == 8)
        {
            g_ogl.main_program = oglu_build_program(PASSTHROUGH_VERT_SHADER_110, PALETTE_FRAG_SHADER_110, FALSE);
        }
        else if (g_ddraw.bpp == 16 || g_ddraw.bpp == 32)
        {
            g_ogl.main_program = oglu_build_program(PASSTHROUGH_VERT_SHADER_110, PASSTHROUGH_FRAG_SHADER_110, FALSE);
        }
    }
}

static void ogl_create_textures(int width, int height)
{
    GLenum err = GL_NO_ERROR;

    int w = g_ogl.shader2_program ? max(width, g_ddraw.render.viewport.width) : width;
    int h = g_ogl.shader2_program ? max(height, g_ddraw.render.viewport.height) : height;

    g_ogl.surface_tex_width =
        w <= 1024 ? 1024 : w <= 2048 ? 2048 : w <= 4096 ? 4096 : w <= 8192 ? 8192 : w;

    g_ogl.surface_tex_height =
        h <= 512 ? 512 : h <= 1024 ? 1024 : h <= 2048 ? 2048 : h <= 4096 ? 4096 : h <= 8192 ? 8192 : h;

    g_ogl.scale_w = (float)width / g_ogl.surface_tex_width;
    g_ogl.scale_h = (float)height / g_ogl.surface_tex_height;

    glGenTextures(TEXTURE_COUNT, g_ogl.surface_tex_ids);

    for (int i = 0; i < TEXTURE_COUNT; i++)
    {
        glBindTexture(GL_TEXTURE_2D, g_ogl.surface_tex_ids[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        g_ogl.got_error = g_ogl.got_error || (err = glGetError()) != GL_NO_ERROR;
        
        if (err != GL_NO_ERROR)
        {
            TRACE("OpenGL error %08x (ogl_create_textures())\n", err);
            ogl_check_error("ogl_create_textures()");
        }

        while (glGetError() != GL_NO_ERROR);

        if (g_ddraw.bpp == 32)
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
        else if (g_ddraw.bpp == 16 && g_config.rgb555)
        {
            if (g_oglu_got_version3)
            {
                glTexImage2D(
                    GL_TEXTURE_2D,
                    0,
                    GL_RG8,
                    g_ogl.surface_tex_width,
                    g_ogl.surface_tex_height,
                    0,
                    g_ogl.surface_format = GL_RG,
                    g_ogl.surface_type = GL_UNSIGNED_BYTE,
                    0);
            }
            else
            {
                glTexImage2D(
                    GL_TEXTURE_2D,
                    0,
                    GL_RGBA8,
                    g_ogl.surface_tex_width,
                    g_ogl.surface_tex_height,
                    0,
                    g_ogl.surface_format = GL_BGRA,
                    g_ogl.surface_type = GL_UNSIGNED_SHORT_1_5_5_5_REV,
                    0);
            }
        }
        else if (g_ddraw.bpp == 16)
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
        else if (g_ddraw.bpp == 8)
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

    if (g_ddraw.bpp == 8)
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

    glUniform1i(glGetUniformLocation(g_ogl.main_program, "Texture"), 0);

    if (g_ddraw.bpp == 8)
        glUniform1i(glGetUniformLocation(g_ogl.main_program, "PaletteTexture"), 1);

    if (g_oglu_got_version3)
    {
        g_ogl.main_vertex_coord_attr_loc = glGetAttribLocation(g_ogl.main_program, "VertexCoord");
        g_ogl.main_tex_coord_attr_loc = glGetAttribLocation(g_ogl.main_program, "TexCoord");

        glGenBuffers(3, g_ogl.main_vbos);

        if (g_ogl.shader1_program)
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

static void ogl_init_shader1_program()
{
    if (!g_ogl.shader1_program)
        return;

    glUseProgram(g_ogl.shader1_program);

    GLint vertex_coord_attr_loc = glGetAttribLocation(g_ogl.shader1_program, "VertexCoord");
    g_ogl.shader1_tex_coord_attr_loc = glGetAttribLocation(g_ogl.shader1_program, "TexCoord");
    g_ogl.shader1_frame_count_uni_loc = glGetUniformLocation(g_ogl.shader1_program, "FrameCount");

    glGenBuffers(3, g_ogl.shader1_vbos);

    if (g_ogl.shader2_program)
    {
        glBindBuffer(GL_ARRAY_BUFFER, g_ogl.shader1_vbos[0]);
        static const GLfloat vertext_coord[] = {
            -1.0f,-1.0f,
            -1.0f, 1.0f,
             1.0f, 1.0f,
             1.0f,-1.0f,
        };
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertext_coord), vertext_coord, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindBuffer(GL_ARRAY_BUFFER, g_ogl.shader1_vbos[1]);
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
        glBindBuffer(GL_ARRAY_BUFFER, g_ogl.shader1_vbos[0]);
        static const GLfloat vertext_coord[] = {
            -1.0f, 1.0f,
             1.0f, 1.0f,
             1.0f,-1.0f,
            -1.0f,-1.0f,
        };
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertext_coord), vertext_coord, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glBindBuffer(GL_ARRAY_BUFFER, g_ogl.shader1_vbos[1]);
        GLfloat tex_coord[] = {
            0.0f,           0.0f,
            g_ogl.scale_w,  0.0f,
            g_ogl.scale_w,  g_ogl.scale_h,
            0.0f,           g_ogl.scale_h,
        };
        glBufferData(GL_ARRAY_BUFFER, sizeof(tex_coord), tex_coord, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    glGenVertexArrays(1, &g_ogl.shader1_vao);
    glBindVertexArray(g_ogl.shader1_vao);

    glBindBuffer(GL_ARRAY_BUFFER, g_ogl.shader1_vbos[0]);
    glVertexAttribPointer(vertex_coord_attr_loc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(vertex_coord_attr_loc);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ARRAY_BUFFER, g_ogl.shader1_vbos[1]);
    glVertexAttribPointer(g_ogl.shader1_tex_coord_attr_loc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(g_ogl.shader1_tex_coord_attr_loc);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ogl.shader1_vbos[2]);
    static const GLushort indices[] =
    {
        0, 1, 2,
        0, 2, 3,
    };
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glBindVertexArray(0);

    float input_size[2] = { 0 }, output_size[2] = { 0 }, texture_size[2] = { 0 };

    input_size[0] = (float)g_ddraw.width;
    input_size[1] = (float)g_ddraw.height;
    texture_size[0] = (float)g_ogl.surface_tex_width;
    texture_size[1] = (float)g_ogl.surface_tex_height;
    output_size[0] = (float)g_ddraw.render.viewport.width;
    output_size[1] = (float)g_ddraw.render.viewport.height;

    GLint loc = glGetUniformLocation(g_ogl.shader1_program, "OutputSize");
    if (loc != -1) 
        glUniform2fv(loc, 1, output_size);

    loc = glGetUniformLocation(g_ogl.shader1_program, "TextureSize");
    if (loc != -1) 
        glUniform2fv(loc, 1, texture_size);

    loc = glGetUniformLocation(g_ogl.shader1_program, "InputSize");
    if (loc != -1) 
        glUniform2fv(loc, 1, input_size);

    loc = glGetUniformLocation(g_ogl.shader1_program, "FrameDirection");
    if (loc != -1) 
        glUniform1i(loc, 1);

    loc = glGetUniformLocation(g_ogl.shader1_program, "Texture");
    if (loc != -1) 
        glUniform1i(loc, 0);

    const float mvp_matrix[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1,
    };
    glUniformMatrix4fv(glGetUniformLocation(g_ogl.shader1_program, "MVPMatrix"), 1, GL_FALSE, mvp_matrix);

    glGenFramebuffers(FBO_COUNT, g_ogl.frame_buffer_id);
    glGenTextures(FBO_COUNT, g_ogl.frame_buffer_tex_id);

    int fbo_count = g_ogl.shader2_program ? 2 : 1;

    for (int i = 0; i < fbo_count; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, g_ogl.frame_buffer_id[i]);

        glBindTexture(GL_TEXTURE_2D, g_ogl.frame_buffer_tex_id[i]);
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

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_ogl.frame_buffer_tex_id[i], 0);

        GLenum draw_buffers[1] = { GL_COLOR_ATTACHMENT0 };
        glDrawBuffers(1, draw_buffers);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            glDeleteTextures(FBO_COUNT, g_ogl.frame_buffer_tex_id);

            if (glDeleteFramebuffers)
                glDeleteFramebuffers(FBO_COUNT, g_ogl.frame_buffer_id);

            glUseProgram(0);

            if (glDeleteProgram)
            {
                glDeleteProgram(g_ogl.shader1_program);

                if (g_ogl.shader2_program)
                    glDeleteProgram(g_ogl.shader2_program);
            }  

            g_ogl.shader1_program = 0;
            g_ogl.shader2_program = 0;

            if (glDeleteBuffers)
                glDeleteBuffers(3, g_ogl.shader1_vbos);

            if (glDeleteVertexArrays)
                glDeleteVertexArrays(1, &g_ogl.shader1_vao);

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

            break;
        }
        else
        {
            glClear(GL_COLOR_BUFFER_BIT);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void ogl_init_shader2_program()
{
    if (!g_ogl.shader1_program || !g_ogl.shader2_program)
        return;

    glUseProgram(g_ogl.shader2_program);

    GLint vertex_coord_attr_loc = glGetAttribLocation(g_ogl.shader2_program, "VertexCoord");
    g_ogl.shader2_tex_coord_attr_loc = glGetAttribLocation(g_ogl.shader2_program, "TexCoord");
    g_ogl.shader2_frame_count_uni_loc = glGetUniformLocation(g_ogl.shader2_program, "FrameCount");

    glGenBuffers(3, g_ogl.shader2_vbos);

    glBindBuffer(GL_ARRAY_BUFFER, g_ogl.shader2_vbos[0]);
    GLfloat vertex_coord[] = {
        -1.0f, 1.0f,
         1.0f, 1.0f,
         1.0f,-1.0f,
        -1.0f,-1.0f,
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_coord), vertex_coord, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    float scale_w = g_ogl.shader2_upscale ? g_ogl.scale_w : (float)g_ddraw.render.viewport.width / g_ogl.surface_tex_width;
    float scale_h = g_ogl.shader2_upscale ? g_ogl.scale_h : (float)g_ddraw.render.viewport.height / g_ogl.surface_tex_height;

    glBindBuffer(GL_ARRAY_BUFFER, g_ogl.shader2_vbos[1]);
    GLfloat tex_coord[] = {
         0.0f,     0.0f,
         scale_w,  0.0f,
         scale_w,  scale_h,
         0.0f,     scale_h,
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(tex_coord), tex_coord, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenVertexArrays(1, &g_ogl.shader2_vao);
    glBindVertexArray(g_ogl.shader2_vao);

    glBindBuffer(GL_ARRAY_BUFFER, g_ogl.shader2_vbos[0]);
    glVertexAttribPointer(vertex_coord_attr_loc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(vertex_coord_attr_loc);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ARRAY_BUFFER, g_ogl.shader2_vbos[1]);
    glVertexAttribPointer(g_ogl.shader2_tex_coord_attr_loc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(g_ogl.shader2_tex_coord_attr_loc);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ogl.shader2_vbos[2]);
    static const GLushort indices[] =
    {
        0, 1, 2,
        0, 2, 3,
    };
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glBindVertexArray(0);

    float input_size[2] = { 0 }, output_size[2] = { 0 }, texture_size[2] = { 0 };

    input_size[0] = g_ogl.shader2_upscale ? (float)g_ddraw.width : (float)g_ddraw.render.viewport.width;
    input_size[1] = g_ogl.shader2_upscale ? (float)g_ddraw.height : (float)g_ddraw.render.viewport.height;
    texture_size[0] = (float)g_ogl.surface_tex_width;
    texture_size[1] = (float)g_ogl.surface_tex_height;
    output_size[0] = (float)g_ddraw.render.viewport.width;
    output_size[1] = (float)g_ddraw.render.viewport.height;

    GLint loc = glGetUniformLocation(g_ogl.shader2_program, "OutputSize");
    if (loc != -1)
        glUniform2fv(loc, 1, output_size);

    loc = glGetUniformLocation(g_ogl.shader2_program, "TextureSize");
    if (loc != -1)
        glUniform2fv(loc, 1, texture_size);

    loc = glGetUniformLocation(g_ogl.shader2_program, "InputSize");
    if (loc != -1)
        glUniform2fv(loc, 1, input_size);

    loc = glGetUniformLocation(g_ogl.shader2_program, "FrameDirection");
    if (loc != -1)
        glUniform1i(loc, 1);

    loc = glGetUniformLocation(g_ogl.shader2_program, "Texture");
    if (loc != -1)
        glUniform1i(loc, 0);

    loc = glGetUniformLocation(g_ogl.shader2_program, "PassPrev2Texture");
    if (loc != -1)
        glUniform1i(loc, 1);

    loc = glGetUniformLocation(g_ogl.shader2_program, "PassPrev2TextureSize");
    if (loc != -1)
        glUniform2fv(loc, 1, texture_size);

    const float mvp_matrix[16] = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1,
    };
    glUniformMatrix4fv(glGetUniformLocation(g_ogl.shader2_program, "MVPMatrix"), 1, GL_FALSE, mvp_matrix);
}

static void ogl_render()
{
    BOOL needs_update = FALSE;

    glViewport(
        g_ddraw.render.viewport.x, 
        g_ddraw.render.viewport.y + g_ddraw.render.opengl_y_align,
        g_ddraw.render.viewport.width, 
        g_ddraw.render.viewport.height);

    if (g_ogl.main_program)
    {
        glUseProgram(g_ogl.main_program);
    }
    else if (g_ddraw.bpp == 16 || g_ddraw.bpp == 32)
    {
        glEnable(GL_TEXTURE_2D);
    }

    DWORD timeout = g_config.minfps > 0 ? g_ddraw.minfps_tick_len : INFINITE;

    while (g_ogl.use_opengl && g_ddraw.render.run &&
        (g_config.minfps < 0 || WaitForSingleObject(g_ddraw.render.sem, timeout) != WAIT_FAILED) &&
        g_ddraw.render.run)
    {
        if (InterlockedDecrement(&g_ddraw.zoom.frame_skip) > 0)
            continue;

#if _DEBUG
        dbg_draw_frame_info_start();
#endif

        g_ogl.scale_w = (float)g_ddraw.width / g_ogl.surface_tex_width;
        g_ogl.scale_h = (float)g_ddraw.height / g_ogl.surface_tex_height;

        static int tex_index = 0, pal_index = 0;

        BOOL scale_changed = FALSE;

        fpsl_frame_start();

        EnterCriticalSection(&g_ddraw.cs);

        if (g_ddraw.primary && 
            g_ddraw.primary->bpp == g_ddraw.bpp &&
            g_ddraw.primary->width == g_ddraw.width &&
            g_ddraw.primary->height == g_ddraw.height &&
            (g_ddraw.bpp == 16 || g_ddraw.bpp == 32 || g_ddraw.primary->palette))
        {
            if (g_config.lock_surfaces)
                EnterCriticalSection(&g_ddraw.primary->cs);

            if (g_config.vhack)
            {
                if (util_detect_low_res_screen())
                {
                    g_ogl.scale_w *= (float)g_ddraw.upscale_hack_width / g_ddraw.width;
                    g_ogl.scale_h *= (float)g_ddraw.upscale_hack_height / g_ddraw.height;

                    if (!InterlockedExchange(&g_ddraw.upscale_hack_active, TRUE))
                        scale_changed = TRUE;
                }
                else
                {
                    if (InterlockedExchange(&g_ddraw.upscale_hack_active, FALSE))
                        scale_changed = TRUE;
                }
            }

            if (g_ddraw.bpp == 8 &&
                (InterlockedExchange(&g_ddraw.render.palette_updated, FALSE) || g_config.minfps == -2))
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
                    g_ddraw.primary->palette->data_bgr);
            }

            if (InterlockedExchange(&g_ddraw.render.surface_updated, FALSE) || g_config.minfps == -2)
            {
                if (++tex_index >= TEXTURE_COUNT)
                    tex_index = 0;

                glBindTexture(GL_TEXTURE_2D, g_ogl.surface_tex_ids[tex_index]);

                DWORD row_len = g_ddraw.primary->pitch ? g_ddraw.primary->pitch / g_ddraw.primary->bytes_pp : 0;

                if (row_len != g_ddraw.primary->width)
                    glPixelStorei(GL_UNPACK_ROW_LENGTH, row_len);

                glTexSubImage2D(
                    GL_TEXTURE_2D,
                    0,
                    0,
                    0,
                    g_ddraw.width,
                    g_ddraw.height,
                    g_ogl.surface_format,
                    g_ogl.surface_type,
                    g_ddraw.primary->surface);

                if (row_len != g_ddraw.primary->width)
                    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            }

            static int error_check_count = 0;

            if (error_check_count < 10)
            {
                error_check_count++;

                GLenum err = glGetError();

                if (err != GL_NO_ERROR && err != GL_INVALID_FRAMEBUFFER_OPERATION)
                {
                    g_ogl.use_opengl = FALSE;

                    TRACE("OpenGL error %08x (ogl_render())\n", err);
                    ogl_check_error("ogl_render()");
                }
            }

            if (g_config.fixchilds)
            {
                g_ddraw.child_window_exists = FALSE;
                InterlockedExchangePointer((void*)&g_ddraw.video_window_hwnd, NULL);
                EnumChildWindows(g_ddraw.hwnd, util_enum_child_proc, (LPARAM)g_ddraw.primary);

                if (g_ddraw.render.width != g_ddraw.width || g_ddraw.render.height != g_ddraw.height)
                {
                    if (g_ddraw.child_window_exists)
                    {
                        glClear(GL_COLOR_BUFFER_BIT);

                        if (!needs_update)
                        {
                            glViewport(0, g_ddraw.render.height - g_ddraw.height, g_ddraw.width, g_ddraw.height);
                            needs_update = TRUE;
                        }
                    }
                    else if (needs_update)
                    {
                        glViewport(
                            g_ddraw.render.viewport.x, 
                            g_ddraw.render.viewport.y + g_ddraw.render.opengl_y_align,
                            g_ddraw.render.viewport.width, 
                            g_ddraw.render.viewport.height);

                        needs_update = FALSE;
                    }
                }
            }

            if (g_config.lock_surfaces)
                LeaveCriticalSection(&g_ddraw.primary->cs);
        }

        LeaveCriticalSection(&g_ddraw.cs);

        if (g_ddraw.render.viewport.x != 0 || g_ddraw.render.viewport.y != 0)
        {
            glClear(GL_COLOR_BUFFER_BIT);
        }

        if (scale_changed)
        {
            if (g_ogl.shader2_upscale && g_ogl.shader2_program && g_ogl.shader1_program && g_ogl.main_program)
            {
                glBindVertexArray(g_ogl.shader2_vao);
                glBindBuffer(GL_ARRAY_BUFFER, g_ogl.shader2_vbos[1]);
                GLfloat tex_coord[] = {
                    0.0f,           0.0f,
                    g_ogl.scale_w,  0.0f,
                    g_ogl.scale_w,  g_ogl.scale_h,
                    0.0f,           g_ogl.scale_h,
                };
                glBufferData(GL_ARRAY_BUFFER, sizeof(tex_coord), tex_coord, GL_STATIC_DRAW);
                glVertexAttribPointer(g_ogl.shader2_tex_coord_attr_loc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                glEnableVertexAttribArray(g_ogl.shader2_tex_coord_attr_loc);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glBindVertexArray(0);
            }
            else if (g_ogl.shader2_program && g_ogl.shader1_program && g_ogl.main_program)
            {
                glBindVertexArray(g_ogl.shader1_vao);
                glBindBuffer(GL_ARRAY_BUFFER, g_ogl.shader1_vbos[1]);
                GLfloat tex_coord[] = {
                    0.0f,          0.0f,
                    0.0f,          g_ogl.scale_h,
                    g_ogl.scale_w, g_ogl.scale_h,
                    g_ogl.scale_w, 0.0f,
                };
                glBufferData(GL_ARRAY_BUFFER, sizeof(tex_coord), tex_coord, GL_STATIC_DRAW);
                glVertexAttribPointer(g_ogl.shader1_tex_coord_attr_loc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                glEnableVertexAttribArray(g_ogl.shader1_tex_coord_attr_loc);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glBindVertexArray(0);
            }
            else if (g_ogl.shader1_program && g_ogl.main_program)
            {
                glBindVertexArray(g_ogl.shader1_vao);
                glBindBuffer(GL_ARRAY_BUFFER, g_ogl.shader1_vbos[1]);
                GLfloat tex_coord[] = {
                    0.0f,           0.0f,
                    g_ogl.scale_w,  0.0f,
                    g_ogl.scale_w,  g_ogl.scale_h,
                    0.0f,           g_ogl.scale_h,
                };
                glBufferData(GL_ARRAY_BUFFER, sizeof(tex_coord), tex_coord, GL_STATIC_DRAW);
                glVertexAttribPointer(g_ogl.shader1_tex_coord_attr_loc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                glEnableVertexAttribArray(g_ogl.shader1_tex_coord_attr_loc);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glBindVertexArray(0);
            }
            else if (g_oglu_got_version3 && g_ogl.main_program)
            {
                glBindVertexArray(g_ogl.main_vao);
                glBindBuffer(GL_ARRAY_BUFFER, g_ogl.main_vbos[1]);
                GLfloat tex_coord[] = {
                    0.0f,           0.0f,
                    g_ogl.scale_w,  0.0f,
                    g_ogl.scale_w,  g_ogl.scale_h,
                    0.0f,           g_ogl.scale_h,
                };
                glBufferData(GL_ARRAY_BUFFER, sizeof(tex_coord), tex_coord, GL_STATIC_DRAW);
                glVertexAttribPointer(g_ogl.main_tex_coord_attr_loc, 2, GL_FLOAT, GL_FALSE, 0, NULL);
                glEnableVertexAttribArray(g_ogl.main_tex_coord_attr_loc);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glBindVertexArray(0);
            }
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_ogl.surface_tex_ids[tex_index]);

        if (g_ddraw.bpp == 8)
        {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, g_ogl.palette_tex_ids[pal_index]);

            glActiveTexture(GL_TEXTURE0);
        }

        if (g_ogl.shader1_program && g_ogl.shader2_program && g_ogl.main_program)
        {
            static int frames = 0;
            frames++;

            /* draw surface into framebuffer */
            glUseProgram(g_ogl.main_program);

            glViewport(0, 0, g_ddraw.width, g_ddraw.height);

            glBindFramebuffer(GL_FRAMEBUFFER, g_ogl.frame_buffer_id[0]);

            glBindVertexArray(g_ogl.main_vao);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
            glBindVertexArray(0);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);

            /* apply shader1 */

            if (!g_ogl.shader2_upscale)
            {
                glViewport(0, 0, g_ddraw.render.viewport.width, g_ddraw.render.viewport.height);
            }

            glUseProgram(g_ogl.shader1_program);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, g_ogl.frame_buffer_tex_id[0]);

            glBindFramebuffer(GL_FRAMEBUFFER, g_ogl.frame_buffer_id[1]);

            if (g_ogl.shader1_frame_count_uni_loc != -1)
                glUniform1i(g_ogl.shader1_frame_count_uni_loc, frames);

            glBindVertexArray(g_ogl.shader1_vao);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
            glBindVertexArray(0);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            /* apply shader2 */

            if (g_ddraw.child_window_exists)
            {
                glViewport(0, g_ddraw.render.height - g_ddraw.height, g_ddraw.width, g_ddraw.height);
            }
            else
            {
                glViewport(
                    g_ddraw.render.viewport.x,
                    g_ddraw.render.viewport.y + g_ddraw.render.opengl_y_align,
                    g_ddraw.render.viewport.width,
                    g_ddraw.render.viewport.height);
            }

            glUseProgram(g_ogl.shader2_program);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, g_ogl.frame_buffer_tex_id[1]);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, g_ogl.frame_buffer_tex_id[0]);

            if (g_ogl.shader2_frame_count_uni_loc != -1)
                glUniform1i(g_ogl.shader2_frame_count_uni_loc, frames);

            glBindVertexArray(g_ogl.shader2_vao);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
            glBindVertexArray(0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        else if (g_ogl.shader1_program && g_ogl.main_program)
        {
            /* draw surface into framebuffer */
            glUseProgram(g_ogl.main_program);

            glViewport(0, 0, g_ddraw.width, g_ddraw.height);

            glBindFramebuffer(GL_FRAMEBUFFER, g_ogl.frame_buffer_id[0]);

            glBindVertexArray(g_ogl.main_vao);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
            glBindVertexArray(0);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);

            if (g_ddraw.child_window_exists)
            {
                glViewport(0, g_ddraw.render.height - g_ddraw.height, g_ddraw.width, g_ddraw.height);
            }
            else
            {
                glViewport(
                    g_ddraw.render.viewport.x, 
                    g_ddraw.render.viewport.y + g_ddraw.render.opengl_y_align,
                    g_ddraw.render.viewport.width, 
                    g_ddraw.render.viewport.height);
            }

            /* apply filter */

            glUseProgram(g_ogl.shader1_program);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, g_ogl.frame_buffer_tex_id[0]);

            static int frames = 1;
            if (g_ogl.shader1_frame_count_uni_loc != -1)
                glUniform1i(g_ogl.shader1_frame_count_uni_loc, frames++);

            glBindVertexArray(g_ogl.shader1_vao);
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

        if (g_ddraw.bnet_active)
            glClear(GL_COLOR_BUFFER_BIT);

        SwapBuffers(g_ogl.hdc);

        if (!g_ddraw.render.run)
            break;

#if _DEBUG
        dbg_draw_frame_info_end();
#endif

        fpsl_frame_end();
    }

    if (g_config.vhack)
        InterlockedExchange(&g_ddraw.upscale_hack_active, FALSE);
}

static BOOL ogl_release_resources()
{
    glDeleteTextures(TEXTURE_COUNT, g_ogl.surface_tex_ids);

    if (g_ddraw.bpp == 8)
        glDeleteTextures(TEXTURE_COUNT, g_ogl.palette_tex_ids);

    if (glUseProgram)
        glUseProgram(0);

    if (g_ogl.shader1_program)
    {
        glDeleteTextures(FBO_COUNT, g_ogl.frame_buffer_tex_id);

        if (glDeleteBuffers)
            glDeleteBuffers(3, g_ogl.shader1_vbos);

        if (glDeleteFramebuffers)
            glDeleteFramebuffers(FBO_COUNT, g_ogl.frame_buffer_id);

        if (glDeleteVertexArrays)
            glDeleteVertexArrays(1, &g_ogl.shader1_vao);
    }

    if (g_ogl.shader2_program)
    {
        if (glDeleteBuffers)
            glDeleteBuffers(3, g_ogl.shader2_vbos);

        if (glDeleteVertexArrays)
            glDeleteVertexArrays(1, &g_ogl.shader2_vao);
    }

    if (glDeleteProgram)
    {
        if (g_ogl.main_program)
            glDeleteProgram(g_ogl.main_program);

        if (g_ogl.shader1_program)
            glDeleteProgram(g_ogl.shader1_program);

        if (g_ogl.shader2_program)
            glDeleteProgram(g_ogl.shader2_program);
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

    return TRUE;
}

BOOL ogl_release()
{
    if (g_ddraw.render.thread)
        return FALSE;

    if (g_ogl.context)
    {
        xwglMakeCurrent(NULL, NULL);
        xwglDeleteContext(g_ogl.context);
        g_ogl.context = NULL;
    }

    return TRUE;
}

static BOOL ogl_texture_upload_test()
{
    if (g_ogl.surface_tex_width > 4096 || g_ogl.surface_tex_height > 4096)
        return TRUE;

    int* surface_tex =
        HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, g_ogl.surface_tex_width * g_ogl.surface_tex_height * sizeof(int));

    if (!surface_tex)
        return TRUE;

    static char test_data[] = { 0,1,2,0,0,2,3,0,0,4,5,0,0,6,7,0,0,8,9,0 };

    int i;
    for (i = 0; i < TEXTURE_COUNT; i++)
    {
        memcpy(surface_tex, test_data, sizeof(test_data));

        glBindTexture(GL_TEXTURE_2D, g_ogl.surface_tex_ids[i]);

        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0,
            0,
            g_ddraw.width,
            g_ddraw.height,
            g_ogl.surface_format,
            g_ogl.surface_type,
            surface_tex);

        memset(surface_tex, 0, sizeof(test_data));

        glGetTexImage(GL_TEXTURE_2D, 0, g_ogl.surface_format, g_ogl.surface_type, surface_tex);

        if (memcmp(surface_tex, test_data, sizeof(test_data)) != 0)
        {
            HeapFree(GetProcessHeap(), 0, surface_tex);
            return FALSE;
        }
    }

    if (g_ddraw.bpp == 8)
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
                surface_tex);

            memset(surface_tex, 0, sizeof(test_data));

            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface_tex);

            if (memcmp(surface_tex, test_data, sizeof(test_data)) != 0)
            {
                HeapFree(GetProcessHeap(), 0, surface_tex);
                return FALSE;
            }
                
        }
    }

    HeapFree(GetProcessHeap(), 0, surface_tex);
    return TRUE;
}

static BOOL ogl_shader_test()
{
    BOOL result = TRUE;

    if (g_ddraw.bpp != 8 || g_ogl.surface_tex_width > 4096 || g_ogl.surface_tex_height > 4096)
        return result;

    int* surface_tex =
        HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, g_ogl.surface_tex_width * g_ogl.surface_tex_height * sizeof(int));

    if (!surface_tex)
        return TRUE;

    if (g_oglu_got_version3 && g_ogl.main_program)
    {
        memset(surface_tex, 0, g_ogl.surface_tex_height * g_ogl.surface_tex_width * sizeof(int));

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
            surface_tex);

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
                surface_tex);

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
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface_tex);

            int i;
            for (i = 0; i < g_ogl.surface_tex_height * g_ogl.surface_tex_width; i++)
            {
                if (surface_tex[i] != 0x80808080)
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

    HeapFree(GetProcessHeap(), 0, surface_tex);
    return result;
}
