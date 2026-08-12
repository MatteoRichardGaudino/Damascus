#ifndef __glad_h_
#define __glad_h_

#ifdef __gl_h_
#error OpenGL header already included, remove this include, glad already provides it
#endif
#define __gl_h_

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#endif

#include <KHR/khrplatform.h>

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYDEF
#define APIENTRYDEF APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif
#ifndef GLAPI
#define GLAPI extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void* (*GLADloadproc)(const char *name);

#define GLAD_GL_CORE_VERSION_1_0 1
#define GLAD_GL_CORE_VERSION_1_1 1
#define GLAD_GL_CORE_VERSION_1_2 1
#define GLAD_GL_CORE_VERSION_1_3 1
#define GLAD_GL_CORE_VERSION_1_4 1
#define GLAD_GL_CORE_VERSION_1_5 1
#define GLAD_GL_CORE_VERSION_2_0 1
#define GLAD_GL_CORE_VERSION_2_1 1
#define GLAD_GL_CORE_VERSION_3_0 1
#define GLAD_GL_CORE_VERSION_3_1 1
#define GLAD_GL_CORE_VERSION_3_2 1
#define GLAD_GL_CORE_VERSION_3_3 1

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef void GLvoid;
typedef int8_t GLbyte;
typedef uint8_t GLubyte;
typedef int16_t GLshort;
typedef uint16_t GLushort;
typedef int GLint;
typedef unsigned int GLuint;
typedef int GLsizei;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef intptr_t GLintptr;
typedef uintptr_t GLsizeiptr;
typedef char GLchar;
typedef int64_t GLint64;
typedef uint64_t GLuint64;
typedef struct __GLsync *GLsync;

#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_POINTS 0x0000
#define GL_LINES 0x0001
#define GL_LINE_LOOP 0x0002
#define GL_LINE_STRIP 0x0003
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN 0x0006
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_TEST 0x0C11
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE0 0x84C0
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_SHORT 0x1403
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_RGBA 0x1908
#define GL_RGB 0x1907
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_FRONT_AND_BACK 0x0408
#define GL_FILL 0x1B02
#define GL_LINE 0x1B01

#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_STREAM_DRAW 0x88E0
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_ACTIVE_UNIFORMS 0x8B86
#define GL_ACTIVE_ATTRIBUTES 0x8B89

typedef void (APIENTRYP PFNGLCLEARPROC)(GLbitfield mask);
typedef void (APIENTRYP PFNGLCLEARCOLORPROC)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (APIENTRYP PFNGLVIEWPORTPROC)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP PFNGLENABLEPROC)(GLenum cap);
typedef void (APIENTRYP PFNGLDISABLEPROC)(GLenum cap);
typedef void (APIENTRYP PFNGLBLENDFUNCPROC)(GLenum sfactor, GLenum dfactor);
typedef void (APIENTRYP PFNGLDEPTHFUNCPROC)(GLenum func);
typedef void (APIENTRYP PFNGLCULLFACEPROC)(GLenum mode);
typedef void (APIENTRYP PFNGLSCISSORPROC)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP PFNGLGENBUFFERSPROC)(GLsizei n, GLuint *buffers);
typedef void (APIENTRYP PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void (APIENTRYP PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void (APIENTRYP PFNGLBUFFERSUBDATAPROC)(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
typedef void (APIENTRYP PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint *buffers);
typedef void (APIENTRYP PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint *arrays);
typedef void (APIENTRYP PFNGLBINDVERTEXARRAYPROC)(GLuint array);
typedef void (APIENTRYP PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint *arrays);
typedef void (APIENTRYP PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void (APIENTRYP PFNGLDISABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void (APIENTRYP PFNGLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef GLuint (APIENTRYP PFNGLCREATESHADERPROC)(GLenum type);
typedef void (APIENTRYP PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
typedef void (APIENTRYP PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void (APIENTRYP PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRYP PFNGLDELETESHADERPROC)(GLuint shader);
typedef GLuint (APIENTRYP PFNGLCREATEPROGRAMPROC)(void);
typedef void (APIENTRYP PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (APIENTRYP PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void (APIENTRYP PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRYP PFNGLUSEPROGRAMPROC)(GLuint program);
typedef void (APIENTRYP PFNGLDELETEPROGRAMPROC)(GLuint program);
typedef GLint (APIENTRYP PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar *name);
typedef GLint (APIENTRYP PFNGLGETATTRIBLOCATIONPROC)(GLuint program, const GLchar *name);
typedef void (APIENTRYP PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void (APIENTRYP PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void (APIENTRYP PFNGLUNIFORM2FPROC)(GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRYP PFNGLUNIFORM3FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (APIENTRYP PFNGLUNIFORM4FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (APIENTRYP PFNGLUNIFORM3FVPROC)(GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORM4FVPROC)(GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORMMATRIX3FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORMMATRIX4FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP PFNGLDRAWARRAYSPROC)(GLenum mode, GLint first, GLsizei count);
typedef void (APIENTRYP PFNGLDRAWELEMENTSPROC)(GLenum mode, GLsizei count, GLenum type, const void *indices);
typedef void (APIENTRYP PFNGLGENTEXTURESPROC)(GLsizei n, GLuint *textures);
typedef void (APIENTRYP PFNGLBINDTEXTUREPROC)(GLenum target, GLuint texture);
typedef void (APIENTRYP PFNGLTEXIMAGE2DPROC)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
typedef void (APIENTRYP PFNGLTEXPARAMETERIPROC)(GLenum target, GLenum pname, GLint param);
typedef void (APIENTRYP PFNGLDELETETEXTURESPROC)(GLsizei n, const GLuint *textures);
typedef void (APIENTRYP PFNGLACTIVETEXTUREPROC)(GLenum texture);
typedef void* (APIENTRYP PFNGLMAPBUFFERPROC)(GLenum target, GLenum access);
typedef GLboolean (APIENTRYP PFNGLUNMAPBUFFERPROC)(GLenum target);

GLAPI PFNGLCLEARPROC glad_glClear;
GLAPI PFNGLCLEARCOLORPROC glad_glClearColor;
GLAPI PFNGLVIEWPORTPROC glad_glViewport;
GLAPI PFNGLENABLEPROC glad_glEnable;
GLAPI PFNGLDISABLEPROC glad_glDisable;
GLAPI PFNGLBLENDFUNCPROC glad_glBlendFunc;
GLAPI PFNGLDEPTHFUNCPROC glad_glDepthFunc;
GLAPI PFNGLCULLFACEPROC glad_glCullFace;
GLAPI PFNGLSCISSORPROC glad_glScissor;
GLAPI PFNGLGENBUFFERSPROC glad_glGenBuffers;
GLAPI PFNGLBINDBUFFERPROC glad_glBindBuffer;
GLAPI PFNGLBUFFERDATAPROC glad_glBufferData;
GLAPI PFNGLBUFFERSUBDATAPROC glad_glBufferSubData;
GLAPI PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers;
GLAPI PFNGLGENVERTEXARRAYSPROC glad_glGenVertexArrays;
GLAPI PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray;
GLAPI PFNGLDELETEVERTEXARRAYSPROC glad_glDeleteVertexArrays;
GLAPI PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray;
GLAPI PFNGLDISABLEVERTEXATTRIBARRAYPROC glad_glDisableVertexAttribArray;
GLAPI PFNGLVERTEXATTRIBPOINTERPROC glad_glVertexAttribPointer;
GLAPI PFNGLCREATESHADERPROC glad_glCreateShader;
GLAPI PFNGLSHADERSOURCEPROC glad_glShaderSource;
GLAPI PFNGLCOMPILESHADERPROC glad_glCompileShader;
GLAPI PFNGLGETSHADERIVPROC glad_glGetShaderiv;
GLAPI PFNGLGETSHADERINFOLOGPROC glad_glGetShaderInfoLog;
GLAPI PFNGLDELETESHADERPROC glad_glDeleteShader;
GLAPI PFNGLCREATEPROGRAMPROC glad_glCreateProgram;
GLAPI PFNGLATTACHSHADERPROC glad_glAttachShader;
GLAPI PFNGLLINKPROGRAMPROC glad_glLinkProgram;
GLAPI PFNGLGETPROGRAMIVPROC glad_glGetProgramiv;
GLAPI PFNGLGETPROGRAMINFOLOGPROC glad_glGetProgramInfoLog;
GLAPI PFNGLUSEPROGRAMPROC glad_glUseProgram;
GLAPI PFNGLDELETEPROGRAMPROC glad_glDeleteProgram;
GLAPI PFNGLGETUNIFORMLOCATIONPROC glad_glGetUniformLocation;
GLAPI PFNGLGETATTRIBLOCATIONPROC glad_glGetAttribLocation;
GLAPI PFNGLUNIFORM1IPROC glad_glUniform1i;
GLAPI PFNGLUNIFORM1FPROC glad_glUniform1f;
GLAPI PFNGLUNIFORM2FPROC glad_glUniform2f;
GLAPI PFNGLUNIFORM3FPROC glad_glUniform3f;
GLAPI PFNGLUNIFORM4FPROC glad_glUniform4f;
GLAPI PFNGLUNIFORM3FVPROC glad_glUniform3fv;
GLAPI PFNGLUNIFORM4FVPROC glad_glUniform4fv;
GLAPI PFNGLUNIFORMMATRIX3FVPROC glad_glUniformMatrix3fv;
GLAPI PFNGLUNIFORMMATRIX4FVPROC glad_glUniformMatrix4fv;
GLAPI PFNGLDRAWARRAYSPROC glad_glDrawArrays;
GLAPI PFNGLDRAWELEMENTSPROC glad_glDrawElements;
GLAPI PFNGLGENTEXTURESPROC glad_glGenTextures;
GLAPI PFNGLBINDTEXTUREPROC glad_glBindTexture;
GLAPI PFNGLTEXIMAGE2DPROC glad_glTexImage2D;
GLAPI PFNGLTEXPARAMETERIPROC glad_glTexParameteri;
GLAPI PFNGLDELETETEXTURESPROC glad_glDeleteTextures;
GLAPI PFNGLACTIVETEXTUREPROC glad_glActiveTexture;

#define glClear glad_glClear
#define glClearColor glad_glClearColor
#define glViewport glad_glViewport
#define glEnable glad_glEnable
#define glDisable glad_glDisable
#define glBlendFunc glad_glBlendFunc
#define glDepthFunc glad_glDepthFunc
#define glCullFace glad_glCullFace
#define glScissor glad_glScissor
#define glGenBuffers glad_glGenBuffers
#define glBindBuffer glad_glBindBuffer
#define glBufferData glad_glBufferData
#define glBufferSubData glad_glBufferSubData
#define glDeleteBuffers glad_glDeleteBuffers
#define glGenVertexArrays glad_glGenVertexArrays
#define glBindVertexArray glad_glBindVertexArray
#define glDeleteVertexArrays glad_glDeleteVertexArrays
#define glEnableVertexAttribArray glad_glEnableVertexAttribArray
#define glDisableVertexAttribArray glad_glDisableVertexAttribArray
#define glVertexAttribPointer glad_glVertexAttribPointer
#define glCreateShader glad_glCreateShader
#define glShaderSource glad_glShaderSource
#define glCompileShader glad_glCompileShader
#define glGetShaderiv glad_glGetShaderiv
#define glGetShaderInfoLog glad_glGetShaderInfoLog
#define glDeleteShader glad_glDeleteShader
#define glCreateProgram glad_glCreateProgram
#define glAttachShader glad_glAttachShader
#define glLinkProgram glad_glLinkProgram
#define glGetProgramiv glad_glGetProgramiv
#define glGetProgramInfoLog glad_glGetProgramInfoLog
#define glUseProgram glad_glUseProgram
#define glDeleteProgram glad_glDeleteProgram
#define glGetUniformLocation glad_glGetUniformLocation
#define glGetAttribLocation glad_glGetAttribLocation
#define glUniform1i glad_glUniform1i
#define glUniform1f glad_glUniform1f
#define glUniform2f glad_glUniform2f
#define glUniform3f glad_glUniform3f
#define glUniform4f glad_glUniform4f
#define glUniform3fv glad_glUniform3fv
#define glUniform4fv glad_glUniform4fv
#define glUniformMatrix3fv glad_glUniformMatrix3fv
#define glUniformMatrix4fv glad_glUniformMatrix4fv
#define glDrawArrays glad_glDrawArrays
#define glDrawElements glad_glDrawElements
#define glGenTextures glad_glGenTextures
#define glBindTexture glad_glBindTexture
#define glTexImage2D glad_glTexImage2D
#define glTexParameteri glad_glTexParameteri
#define glDeleteTextures glad_glDeleteTextures
#define glActiveTexture glad_glActiveTexture

GLAPI int gladLoadGLLoader(GLADloadproc load);

#ifdef __cplusplus
}
#endif

#endif
