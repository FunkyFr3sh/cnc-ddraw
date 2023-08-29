#ifndef OPENGLSHADER_H
#define OPENGLSHADER_H

/* OpenGL 2.0 */

static char PASSTHROUGH_VERT_SHADER_110[] =
    "#version 110\n"
    "varying vec2 TEX0; \n"
    "\n"
    "void main()\n"
    "{\n"
    "    gl_Position = ftransform(); \n"
    "    TEX0 = gl_MultiTexCoord0.xy; \n"
    "}\n";


static char PALETTE_FRAG_SHADER_110[] =
    "#version 110\n"
    "uniform sampler2D Texture; \n"
    "uniform sampler2D PaletteTexture; \n"
    "varying vec2 TEX0; \n"
    "\n"
    "void main()\n"
    "{\n"
    "   vec4 pIndex = texture2D(Texture, TEX0); \n"
    "   gl_FragColor = texture2D(PaletteTexture, vec2(pIndex.r * (255.0/256.0) + (0.5/256.0), 0)); \n"
    "}\n";


static char PASSTHROUGH_FRAG_SHADER_110[] =
    "#version 110\n"
    "uniform sampler2D Texture; \n"
    "varying vec2 TEX0; \n"
    "\n"
    "void main()\n"
    "{\n"
    "   vec4 texel = texture2D(Texture, TEX0); \n"
    "   gl_FragColor = texel; \n"
    "}\n";

/* OpenGL 3.0 */

static char PASSTHROUGH_VERT_SHADER[] =
    "#version 130\n"
    "in vec4 VertexCoord;\n"
    "in vec4 COLOR;\n"
    "in vec4 TexCoord;\n"
    "out vec4 COL0;\n"
    "out vec4 TEX0;\n"
    "uniform mat4 MVPMatrix;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    gl_Position = MVPMatrix * VertexCoord;\n"
    "    COL0 = COLOR;\n"
    "    TEX0.xy = TexCoord.xy;\n"
    "}\n";


static char PALETTE_FRAG_SHADER[] =
    "#version 130\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D Texture;\n"
    "uniform sampler2D PaletteTexture;\n"
    "in vec4 TEX0;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    vec4 pIndex = texture(Texture, TEX0.xy);\n"
    "    FragColor = texture(PaletteTexture, vec2(pIndex.r * (255.0/256.0) + (0.5/256.0), 0));\n"
    "}\n";


static char PASSTHROUGH_FRAG_SHADER[] =
    "#version 130\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D Texture;\n"
    "in vec4 TEX0;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    vec4 texel = texture(Texture, TEX0.xy);\n"
    "    FragColor = texel;\n"
    "}\n";


/*   
//    The following code is licensed under the MIT license: https://gist.github.com/TheRealMJP/bc503b0b87b643d3505d41eab8b332ae
//    Ported from code: https://gist.github.com/TheRealMJP/c83b8c0f46b63f3a88a5986f4fa982b1
//    Samples a texture with Catmull-Rom filtering, using 9 texture fetches instead of 16.
//    See http://vec3.ca/bicubic-filtering-in-fewer-taps/ for more details
//    Modified to use 5 texture fetches
*/

static char CATMULL_ROM_FRAG_SHADER[] =
    "#version 130\n"
    "out mediump vec4 FragColor;\n"
    "uniform int FrameDirection;\n"
    "uniform int FrameCount;\n"
    "uniform vec2 OutputSize;\n"
    "uniform vec2 TextureSize;\n"
    "uniform vec2 InputSize;\n"
    "uniform sampler2D Texture;\n"
    "in vec4 TEX0;\n"
    "\n"
    "#define Source Texture\n"
    "#define vTexCoord TEX0.xy\n"
    "\n"
    "#define SourceSize vec4(TextureSize, 1.0 / TextureSize)\n"
    "#define outsize vec4(OutputSize, 1.0 / OutputSize)\n"
    "\n"
    "void main()\n"
    "{\n"
    "    vec2 samplePos = vTexCoord * SourceSize.xy;\n"
    "    vec2 texPos1 = floor(samplePos - 0.5) + 0.5;\n"
    "\n"
    "    vec2 f = samplePos - texPos1;\n"
    "\n"
    "    vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));\n"
    "    vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);\n"
    "    vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));\n"
    "    vec2 w3 = f * f * (-0.5 + 0.5 * f);\n"
    "\n"
    "    vec2 w12 = w1 + w2;\n"
    "    vec2 offset12 = w2 / (w1 + w2);\n"
    "\n"
    "    vec2 texPos0  = texPos1 - 1.;\n"
    "    vec2 texPos3  = texPos1 + 2.;\n"
    "    vec2 texPos12 = texPos1 + offset12;\n"
    "\n"
    "    texPos0  *= SourceSize.zw;\n"
    "    texPos3  *= SourceSize.zw;\n"
    "    texPos12 *= SourceSize.zw;\n"
    "\n"
    "    float wtm = w12.x * w0.y;\n"
    "    float wml = w0.x * w12.y;\n"
    "    float wmm = w12.x * w12.y;\n"
    "    float wmr = w3.x * w12.y;\n"
    "    float wbm = w12.x * w3.y;\n"
    "\n"
    "    vec3 result = vec3(0.0f);\n"
    "\n"
    "    result += texture(Source, vec2(texPos12.x, texPos0.y)).rgb * wtm;\n"
    "    result += texture(Source, vec2(texPos0.x, texPos12.y)).rgb * wml;\n"
    "    result += texture(Source, vec2(texPos12.x, texPos12.y)).rgb * wmm;\n"
    "    result += texture(Source, vec2(texPos3.x, texPos12.y)).rgb * wmr;\n"
    "    result += texture(Source, vec2(texPos12.x, texPos3.y)).rgb * wbm;\n"
    "\n"
    "    FragColor = vec4(result * (1. / (wtm + wml + wmm + wmr + wbm)), 1.0);\n"
    "}\n";


/*
// The following code is licensed under the MIT license:
// Hyllian's jinc windowed-jinc 2-lobe sharper with anti-ringing Shader
// Copyright (C) 2011-2016 Hyllian/Jararaca - sergiogdb@gmail.com
// https://github.com/libretro/glsl-shaders/blob/09e2942efbab2f51b60ff0b93b7761b0b0570910/windowed/shaders/lanczos2-sharp.glsl
*/

static char LANCZOS2_FRAG_SHADER[] =
    "#version 130\n"
    "#define JINC2_WINDOW_SINC 0.5\n"
    "#define JINC2_SINC 1.0\n"
    "#define JINC2_AR_STRENGTH 0.8\n"
    "\n"
    "out vec4 FragColor;\n"
    "uniform int FrameDirection;\n"
    "uniform int FrameCount;\n"
    "uniform vec2 OutputSize;\n"
    "uniform vec2 TextureSize;\n"
    "uniform vec2 InputSize;\n"
    "uniform sampler2D Texture;\n"
    "in vec4 TEX0;\n"
    "\n"
    "const   float pi                = 3.1415926535897932384626433832795;\n"
    "const   float wa                = JINC2_WINDOW_SINC*pi;\n"
    "const   float wb                = JINC2_SINC*pi;\n"
    "\n"
    "// Calculates the distance between two points\n"
    "float d(vec2 pt1, vec2 pt2)\n"
    "{\n"
    "  vec2 v = pt2 - pt1;\n"
    "  return sqrt(dot(v,v));\n"
    "}\n"
    "\n"
    "vec3 min4(vec3 a, vec3 b, vec3 c, vec3 d)\n"
    "{\n"
    "    return min(a, min(b, min(c, d)));\n"
    "}\n"
    "\n"
    "vec3 max4(vec3 a, vec3 b, vec3 c, vec3 d)\n"
    "{\n"
    "    return max(a, max(b, max(c, d)));\n"
    "}\n"
    "\n"
    "vec4 resampler(vec4 x)\n"
    "{\n"
    "   vec4 res;\n"
    "\n"
    "   res.x = (x.x==0.0) ?  wa*wb  :  sin(x.x*wa)*sin(x.x*wb)/(x.x*x.x);\n"
    "   res.y = (x.y==0.0) ?  wa*wb  :  sin(x.y*wa)*sin(x.y*wb)/(x.y*x.y);\n"
    "   res.z = (x.z==0.0) ?  wa*wb  :  sin(x.z*wa)*sin(x.z*wb)/(x.z*x.z);\n"
    "   res.w = (x.w==0.0) ?  wa*wb  :  sin(x.w*wa)*sin(x.w*wb)/(x.w*x.w);\n"
    "\n"
    "   return res;\n"
    "}\n"
    "\n"
    "void main()\n"
    "{\n"
    "    vec3 color;\n"
    "    vec4 weights[4];\n"
    "\n"
    "    vec2 dx = vec2(1.0, 0.0);\n"
    "    vec2 dy = vec2(0.0, 1.0);\n"
    "\n"
    "    vec2 pc = TEX0.xy*TextureSize;\n"
    "\n"
    "    vec2 tc = (floor(pc-vec2(0.5,0.5))+vec2(0.5,0.5));\n"
    "     \n"
    "    weights[0] = resampler(vec4(d(pc, tc    -dx    -dy), d(pc, tc           -dy), d(pc, tc    +dx    -dy), d(pc, tc+2.0*dx    -dy)));\n"
    "    weights[1] = resampler(vec4(d(pc, tc    -dx       ), d(pc, tc              ), d(pc, tc    +dx       ), d(pc, tc+2.0*dx       )));\n"
    "    weights[2] = resampler(vec4(d(pc, tc    -dx    +dy), d(pc, tc           +dy), d(pc, tc    +dx    +dy), d(pc, tc+2.0*dx    +dy)));\n"
    "    weights[3] = resampler(vec4(d(pc, tc    -dx+2.0*dy), d(pc, tc       +2.0*dy), d(pc, tc    +dx+2.0*dy), d(pc, tc+2.0*dx+2.0*dy)));\n"
    "\n"
    "    dx = dx/TextureSize;\n"
    "    dy = dy/TextureSize;\n"
    "    tc = tc/TextureSize;\n"
    "\n"
    "    vec3 c00 = texture(Texture, tc    -dx    -dy).xyz;\n"
    "    vec3 c10 = texture(Texture, tc           -dy).xyz;\n"
    "    vec3 c20 = texture(Texture, tc    +dx    -dy).xyz;\n"
    "    vec3 c30 = texture(Texture, tc+2.0*dx    -dy).xyz;\n"
    "    vec3 c01 = texture(Texture, tc    -dx       ).xyz;\n"
    "    vec3 c11 = texture(Texture, tc              ).xyz;\n"
    "    vec3 c21 = texture(Texture, tc    +dx       ).xyz;\n"
    "    vec3 c31 = texture(Texture, tc+2.0*dx       ).xyz;\n"
    "    vec3 c02 = texture(Texture, tc    -dx    +dy).xyz;\n"
    "    vec3 c12 = texture(Texture, tc           +dy).xyz;\n"
    "    vec3 c22 = texture(Texture, tc    +dx    +dy).xyz;\n"
    "    vec3 c32 = texture(Texture, tc+2.0*dx    +dy).xyz;\n"
    "    vec3 c03 = texture(Texture, tc    -dx+2.0*dy).xyz;\n"
    "    vec3 c13 = texture(Texture, tc       +2.0*dy).xyz;\n"
    "    vec3 c23 = texture(Texture, tc    +dx+2.0*dy).xyz;\n"
    "    vec3 c33 = texture(Texture, tc+2.0*dx+2.0*dy).xyz;\n"
    "	\n"
    "    //  Get min/max samples\n"
    "    vec3 min_sample = min4(c11, c21, c12, c22);\n"
    "    vec3 max_sample = max4(c11, c21, c12, c22);\n"
    "	\n"
    "    color = vec3(dot(weights[0], vec4(c00.x, c10.x, c20.x, c30.x)), dot(weights[0], vec4(c00.y, c10.y, c20.y, c30.y)), dot(weights[0], vec4(c00.z, c10.z, c20.z, c30.z)));\n"
    "    color+= vec3(dot(weights[1], vec4(c01.x, c11.x, c21.x, c31.x)), dot(weights[1], vec4(c01.y, c11.y, c21.y, c31.y)), dot(weights[1], vec4(c01.z, c11.z, c21.z, c31.z)));\n"
    "    color+= vec3(dot(weights[2], vec4(c02.x, c12.x, c22.x, c32.x)), dot(weights[2], vec4(c02.y, c12.y, c22.y, c32.y)), dot(weights[2], vec4(c02.z, c12.z, c22.z, c32.z)));\n"
    "    color+= vec3(dot(weights[3], vec4(c03.x, c13.x, c23.x, c33.x)), dot(weights[3], vec4(c03.y, c13.y, c23.y, c33.y)), dot(weights[3], vec4(c03.z, c13.z, c23.z, c33.z)));\n"
    "    color = color/(dot(weights[0], vec4(1,1,1,1)) + dot(weights[1], vec4(1,1,1,1)) + dot(weights[2], vec4(1,1,1,1)) + dot(weights[3], vec4(1,1,1,1)));\n"
    "\n"
    "    // Anti-ringing\n"
    "    vec3 aux = color;\n"
    "    color = clamp(color, min_sample, max_sample);\n"
    "    color = mix(aux, color, JINC2_AR_STRENGTH);\n"
    "\n"
    "    // final sum and weight normalization\n"
    "    FragColor.xyz = color;\n"
    "}\n";


#endif
