#ifndef RETROGL_GL_WRAPPER_H
#define RETROGL_GL_WRAPPER_H

#ifdef USE_TILE_LOCAL
#ifdef USE_RETROGL

#include "glwrapper.h"
#include "glwrapper-texture.h"

class RetroGLStateManager : public GLStateManager
{
public:
    RetroGLStateManager();

    void enter_frame(unsigned int fb);
    void exit_frame();
    void context_reset();
    void build_shaders();

    // State Manipulation
    virtual void set(const GLState& state);
    virtual void pixelstore_unpack_alignment(unsigned int bpp);
    virtual void reset_view_for_redraw(float x, float y);
    virtual void reset_view_for_resize(const coord_def &m_windowsz);
    virtual void set_transform(const GLW_3VF &trans, const GLW_3VF &scale);
    virtual void reset_transform();

    // Texture-specific functinos
    virtual void delete_textures(size_t count, unsigned int *textures);
    virtual void generate_textures(size_t count, unsigned int *textures);
    virtual void bind_texture(unsigned int texture);
    virtual void load_texture(unsigned char *pixels, unsigned int width,
                              unsigned int height, MipMapOptions mip_opt,
                              int xoffset=-1, int yoffset=-1);

protected:
    unsigned int gl_white_texture;
    unsigned int gl_program;
    unsigned int u_translation;
    unsigned int u_scale;
    unsigned int u_screen_size;
    unsigned int u_texture_unit;
    
    GLW_3VF m_trans;
    GLW_3VF m_scale;
    coord_def m_window_size;
    
    std::map<unsigned int, FBTexture*> m_textures;
    unsigned int m_next_texture;
    FBTexture* m_texture;
};

class RetroGLShapeBuffer : public GLShapeBuffer
{
public:
    RetroGLShapeBuffer(bool texture = false, bool colour = false,
                   drawing_modes prim = GLW_RECTANGLE);

    virtual const char *print_statistics() const;
    virtual unsigned int size() const;

    virtual void add(const GLWPrim &rect);
    virtual void draw(const GLState &state);
    virtual void clear();

protected:
    // Helper methods for adding specific primitives.
    void add_rect(const GLWPrim &rect);
    void add_line(const GLWPrim &rect);

    drawing_modes m_prim_type;
    bool m_texture_verts;
    bool m_colour_verts;

    vector<GLW_3VF> m_position_buffer;
    vector<GLW_2VF> m_texture_buffer;
    vector<VColour> m_colour_buffer;
    vector<unsigned short int> m_ind_buffer;
};

#endif // USE_RETROGL
#endif // USE_TILE_LOCAL
#endif // RETROGL_GL_WRAPPER_H
