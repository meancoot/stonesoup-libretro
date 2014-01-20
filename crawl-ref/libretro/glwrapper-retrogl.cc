#include "AppHdr.h"

#ifdef USE_TILE_LOCAL
#if defined(USE_RETROGL)

#include "glwrapper-retrogl.h"
#include "options.h"

#ifdef __APPLE__
# ifdef GLES
#  include <OpenGLES/ES2/gl.h>
#  include <OpenGLES/ES2/glext.h>
# else
#  include <OpenGL/gl.h>
#  include <OpenGL/glext.h>
# endif
#else
# ifdef GLES
#  include <GLES2/gl2.h>
#  include <GLES2/gl2ext.h>
# else
#  include <GL/gl.h>
#  include <GL/glext.h>
# endif
#endif

/////////////////////////////////////////////////////////////////////////////
// Static functions from GLStateManager

GLStateManager *glmanager = NULL;

void GLStateManager::init()
{
    if (glmanager)
        return;

    glmanager = new RetroGLStateManager();
}

void GLStateManager::shutdown()
{
    delete glmanager;
    glmanager = NULL;
}

/////////////////////////////////////////////////////////////////////////////
// Static functions from GLShapeBuffer

GLShapeBuffer *GLShapeBuffer::create(bool texture, bool colour,
                                     drawing_modes prim)
{
    return new RetroGLShapeBuffer(texture, colour, prim);
}

unsigned int  generate_texture_id()
{
    unsigned int texid = 0;
    glGenTextures(1, &texid);
    glBindTexture(GL_TEXTURE_2D, texid);
#ifdef GL_CLAMP
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
#else
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#endif
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    Options.tile_filter_scaling ? GL_LINEAR : GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    Options.tile_filter_scaling ? GL_LINEAR : GL_NEAREST);        
    return texid;
}


/////////////////////////////////////////////////////////////////////////////
// RetroGLStateManager

RetroGLStateManager::RetroGLStateManager()
{
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.0, 0.0, 0.0, 1.0f);
    glDepthFunc(GL_LEQUAL);
}

void RetroGLStateManager::enter_frame(unsigned int fb)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glViewport(0, 0, 1024, 768);
    glUseProgram(gl_program);
}

void RetroGLStateManager::exit_frame()
{
    glUseProgram(0);
}

void RetroGLStateManager::context_reset()
{
    build_shaders();
    
    unsigned int pixel = 0xFFFFFFFF;
    glGenTextures(1, & gl_white_texture);
    glBindTexture(GL_TEXTURE_2D, gl_white_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);
    
    for (auto i = m_textures.begin(); i != m_textures.end(); i ++)
    {
        i->second->id = generate_texture_id();
        i->second->dirty = true;
    }
}

void RetroGLStateManager::build_shaders()
{
    static const char *vertex =
        "attribute vec3 a_position;                             \n"
        "attribute vec2 a_texture;                              \n"
        "attribute vec4 a_color;                                \n"
        "uniform vec3 translation;                              \n"
        "uniform vec3 scale;                                    \n"
        "uniform vec2 screen_size;                              \n"
        "varying vec2 texture;                                  \n"
        "varying vec4 color;                                    \n"
        "void main() {                                          \n"
        " gl_Position.xyz = a_position * scale + translation;   \n"
        " gl_Position.xy /= screen_size.xy;                     \n"
        " gl_Position.xy -= .5;                                 \n"
        " gl_Position.xy *= 2.0;                                \n"
        " gl_Position.z /= 100.0;                               \n"
        " gl_Position.w = 1.0;                                  \n"
        " texture = a_texture;                                  \n"
        " color = a_color;                                      \n"
        "}                                                      \n";

    static const char *fragment =
        "uniform sampler2D texture_unit;                        \n"
        "varying vec2 texture;                                  \n"
        "varying vec4 color;                                    \n"
        "void main() {                                          \n"
        " gl_FragColor = texture2D(texture_unit, texture) *     \n"
        "                color;                                 \n"
        "}                                                      \n";

    GLuint vtx = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vtx, 1, &vertex, 0);
    glCompileShader(vtx);
    
    GLuint frg = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frg, 1, &fragment, 0);
    glCompileShader(frg);
    
    gl_program = glCreateProgram();
    glAttachShader(gl_program, vtx);
    glAttachShader(gl_program, frg);
    glBindAttribLocation(gl_program, 0, "a_position");
    glBindAttribLocation(gl_program, 1, "a_texture");
    glBindAttribLocation(gl_program, 2, "a_color");
    glLinkProgram(gl_program);
    glUseProgram(gl_program);

    u_translation = glGetUniformLocation(gl_program, "translation");
    u_scale = glGetUniformLocation(gl_program, "scale");
    u_screen_size = glGetUniformLocation(gl_program, "screen_size");
    u_texture_unit = glGetUniformLocation(gl_program, "texture_unit");
    
    set_transform(m_trans, m_scale);
    reset_view_for_resize(m_window_size);
}

void RetroGLStateManager::set(const GLState& state)
{
    glUseProgram(gl_program);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.0, 0.0, 0.0, 1.0f);
    glDepthFunc(GL_LEQUAL);

    #define WOMP(C, V, E) { if ((C)) glEnable##E(V); else glDisable##E(V); }
    WOMP(state.array_vertex, 0, VertexAttribArray);
    WOMP(state.array_texcoord, 1, VertexAttribArray);
    WOMP(state.array_colour, 2, VertexAttribArray);
    WOMP(state.blend, GL_BLEND, );
    WOMP(state.depthtest, GL_DEPTH_TEST, );

    glVertexAttrib4f(2, state.colour.r / 255.0f, state.colour.g / 255.0f,
                        state.colour.b / 255.0f, state.colour.a / 255.0f);

    if (state.array_texcoord && m_texture)
    {
        glBindTexture(GL_TEXTURE_2D, m_texture->id);
        if (m_texture->dirty)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_texture->width,
                         m_texture->height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         m_texture->pixels);
            m_texture->dirty = false;
        }
    }
    else
        glBindTexture(GL_TEXTURE_2D, gl_white_texture);

/*  if (state.alphatest)
    {
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_NOTEQUAL, state.alpharef);
        glDebug("glAlphaFunc(GL_NOTEQUAL, state.alpharef)");
    }
    else
    {
        glDisable(GL_ALPHA_TEST);
        glDebug("glDisable(GL_ALPHA_TEST)");
    }
*/
}

void RetroGLStateManager::set_transform(const GLW_3VF &trans, const GLW_3VF &scale)
{
    m_trans = trans;
    m_scale = scale;

    glUseProgram(gl_program);
    glUniform3f(u_translation, trans.x, trans.y, trans.z);
    glUniform3f(u_scale, scale.x, scale.y, scale.z);
}

void RetroGLStateManager::reset_view_for_resize(const coord_def &m_windowsz)
{
    m_window_size = m_windowsz;

    glUseProgram(gl_program);
    glUniform2f(u_screen_size, m_windowsz.x, m_windowsz.y);
}

void RetroGLStateManager::reset_transform()
{
    set_transform(GLW_3VF(0, 0, 0), GLW_3VF(1, 1, 1));
}

void RetroGLStateManager::pixelstore_unpack_alignment(unsigned int bpp)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, bpp);
}

void RetroGLStateManager::delete_textures(size_t count, unsigned int *textures)
{
    for (int i = 0; i != count; i ++)
    {
        auto tex = m_textures.find(textures[i]);
        if (tex != m_textures.end())
        {
            if (m_texture == tex->second)
                m_texture = 0;
            glDeleteTextures(1, &tex->second->id);
            delete tex->second;
        }
        m_textures.erase(textures[i]);
    }
}

void RetroGLStateManager::generate_textures(size_t count, unsigned int *textures)
{
    for (int i = 0; i != count; i ++)
    {
        unsigned int id = ++m_next_texture;
        textures[i] = id;
        m_textures[id] = new FBTexture(generate_texture_id());
    }

}

void RetroGLStateManager::bind_texture(unsigned int texture)
{
    auto tex = m_textures.find(texture);
    m_texture = (tex == m_textures.end()) ? 0 : tex->second;
}

void RetroGLStateManager::load_texture(unsigned char *pixels, unsigned int width,
                                   unsigned int height, MipMapOptions mip_opt,
                                   int xoffset, int yoffset)
{
    if (m_texture)
        m_texture->load_pixels((unsigned int*)pixels,
                                width, height, xoffset, yoffset);
}

void RetroGLStateManager::reset_view_for_redraw(float x, float y)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    set_transform(GLW_3VF(x, y, 1), GLW_3VF(1, 1, 1));
}

/////////////////////////////////////////////////////////////////////////////
// RetroGLShapeBuffer

RetroGLShapeBuffer::RetroGLShapeBuffer(bool texture, bool colour, drawing_modes prim) :
    m_prim_type(prim),
    m_texture_verts(texture),
    m_colour_verts(colour)
{
    ASSERT(prim == GLW_RECTANGLE || prim == GLW_LINES);
}

const char *RetroGLShapeBuffer::print_statistics() const
{
    return NULL;
}

unsigned int RetroGLShapeBuffer::size() const
{
    return m_position_buffer.size();
}

void RetroGLShapeBuffer::add(const GLWPrim &rect)
{
    switch (m_prim_type)
    {
    case GLW_RECTANGLE:
        add_rect(rect);
        break;
    case GLW_LINES:
        add_line(rect);
        break;
    default:
        die("Invalid primitive type");
        break;
    }
}

void RetroGLShapeBuffer::add_rect(const GLWPrim &rect)
{
    // Copy vert positions
    size_t last = m_position_buffer.size();
    m_position_buffer.resize(last + 4);
    m_position_buffer[last    ].set(rect.pos_sx, rect.pos_sy, rect.pos_z);
    m_position_buffer[last + 1].set(rect.pos_sx, rect.pos_ey, rect.pos_z);
    m_position_buffer[last + 2].set(rect.pos_ex, rect.pos_sy, rect.pos_z);
    m_position_buffer[last + 3].set(rect.pos_ex, rect.pos_ey, rect.pos_z);

    // Copy texture coords if necessary
    if (m_texture_verts)
    {
        last = m_texture_buffer.size();
        m_texture_buffer.resize(last + 4);
        m_texture_buffer[last    ].set(rect.tex_sx, rect.tex_sy);
        m_texture_buffer[last + 1].set(rect.tex_sx, rect.tex_ey);
        m_texture_buffer[last + 2].set(rect.tex_ex, rect.tex_sy);
        m_texture_buffer[last + 3].set(rect.tex_ex, rect.tex_ey);
    }

    // Copy vert colours if necessary
    if (m_colour_verts)
    {
        last = m_colour_buffer.size();
        m_colour_buffer.resize(last + 4);
        m_colour_buffer[last    ].set(rect.col_s);
        m_colour_buffer[last + 1].set(rect.col_e);
        m_colour_buffer[last + 2].set(rect.col_s);
        m_colour_buffer[last + 3].set(rect.col_e);
    }

    // build indices
    last = m_ind_buffer.size();

    if (last > 3)
    {
        // This is not the first box so make FOUR degenerate triangles
        m_ind_buffer.resize(last + 6);
        unsigned short int val = m_ind_buffer[last - 1];

        // the first three degens finish the previous box and move
        // to the first position of the new one we just added and
        // the fourth degen creates a triangle that is a line from p1 to p3
        m_ind_buffer[last    ] = val++;
        m_ind_buffer[last + 1] = val;

        // Now add as normal
        m_ind_buffer[last + 2] = val++;
        m_ind_buffer[last + 3] = val++;
        m_ind_buffer[last + 4] = val++;
        m_ind_buffer[last + 5] = val;
    }
    else
    {
        // This is the first box so don't bother making any degenerate triangles
        m_ind_buffer.resize(last + 4);
        m_ind_buffer[0] = 0;
        m_ind_buffer[1] = 1;
        m_ind_buffer[2] = 2;
        m_ind_buffer[3] = 3;
    }
}

void RetroGLShapeBuffer::add_line(const GLWPrim &rect)
{
    // Copy vert positions
    size_t last = m_position_buffer.size();
    m_position_buffer.resize(last + 2);
    m_position_buffer[last    ].set(rect.pos_sx, rect.pos_sy, rect.pos_z);
    m_position_buffer[last + 1].set(rect.pos_ex, rect.pos_ey, rect.pos_z);

    // Copy texture coords if necessary
    if (m_texture_verts)
    {
        last = m_texture_buffer.size();
        m_texture_buffer.resize(last + 2);
        m_texture_buffer[last    ].set(rect.tex_sx, rect.tex_sy);
        m_texture_buffer[last + 1].set(rect.tex_ex, rect.tex_ey);
    }

    // Copy vert colours if necessary
    if (m_colour_verts)
    {
        last = m_colour_buffer.size();
        m_colour_buffer.resize(last + 2);
        m_colour_buffer[last    ].set(rect.col_s);
        m_colour_buffer[last + 1].set(rect.col_e);
    }
}

// Draw the buffer
void RetroGLShapeBuffer::draw(const GLState &state)
{
    if (m_position_buffer.empty() || !state.array_vertex)
        return;

    glmanager->set(state);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, &m_position_buffer[0]);

    if (state.array_texcoord && m_texture_verts)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, &m_texture_buffer[0]);

    if (state.array_colour && m_colour_verts)
        glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, &m_colour_buffer[0]);

    switch (m_prim_type)
    {
    case GLW_RECTANGLE:
        glDrawElements(GL_TRIANGLE_STRIP, m_ind_buffer.size(),
                       GL_UNSIGNED_SHORT, &m_ind_buffer[0]);
        break;
    case GLW_LINES:
        glDrawArrays(GL_LINES, 0, m_position_buffer.size());
        break;
    default:
        die("Invalid primitive type");
        break;
    }
}

void RetroGLShapeBuffer::clear()
{
    m_position_buffer.clear();
    m_ind_buffer.clear();
    m_texture_buffer.clear();
    m_colour_buffer.clear();
}

#endif // USE_RETROGL
#endif // USE_TILE_LOCAL
