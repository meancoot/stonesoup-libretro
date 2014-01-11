#ifndef OGL_FB_WRAPPER_H
#define OGL_FB_WRAPPER_H

#ifdef USE_TILE_LOCAL
#ifdef USE_FB

#include "glwrapper.h"

struct FBColour : public VColour
{
    FBColour() : VColour() { }
    FBColour(const VColour& c) : VColour(c) { }

    FBColour(unsigned int value)
    {
        b =  value        & 0xFF;
        g = (value >> 8)  & 0xFF;
        r = (value >> 16) & 0xFF;
        a = (value >> 24) & 0xFF;
    }
    
    void reverse()
    {
        char t = r;
        r = b;
        b = t;
    }
    
    void modulate(const FBColour& other)
    {
        for (int i = 0; i != 4; i ++)
            (*this)[i] = ((*this)(i) * other(i)) * 255;
    }
    
    void blend(const FBColour& other)
    {
        float src = (*this)(3);
        float dst = 1.0f - src;
    
        for (int i = 0; i != 4; i ++)
            (*this)[i] = (((*this)(i) * src) + (other(i) * dst)) * 255;
    }
    
    unsigned char& operator[](unsigned int index)
    {
        switch (index)
        {
            case 0:  return r;
            case 1:  return g;
            case 2:  return b;
            default: return a;
        }
    }

    const unsigned char& operator[](unsigned int index) const
    {
        switch (index)
        {
            case 0:  return r;
            case 1:  return g;
            case 2:  return b;
            default: return a;
        }
    }
    
    float operator()(unsigned int index) const
    {
        return (*this)[index] / 255.0f;
    }
};

struct FBTexture
{
    unsigned int width;
    unsigned int height;
    FBColour* pixels;

    FBTexture() : width(0), height(0), pixels(0)
    {
    
    }
    
    ~FBTexture()
    {
        delete[] pixels;
    }
    
    void set_size(unsigned int width_, unsigned int height_)
    {
        if (width == width_ && height == height_)
            return;
    
        width = width_;
        height = height_;
        
        delete[] pixels;
        pixels = new FBColour[width * height];
    }
    
    FBColour get_pixel(float u, float v) const
    {
        if (!pixels) return 0;
       
        if (u < 0) u = 0;
        if (u > 1) u = 1;
        if (v < 0) v = 0;
        if (v > 1) v = 1;
        
        unsigned int x = (float)width * u;
        unsigned int y = (float)height * v;
        
        if (x == width) x -= 1;
        if (y == height) y -= 1;

        return pixels[y * width + x];
    }
};

#include "glwrapper.h"
class FBStateManager : public GLStateManager
{
public:
    FBStateManager();

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
public:
    bool alpha_test(const FBColour& colour) const;

    void scan_line(const GLWPrim& p, FBColour* out, unsigned int width,
                   float u, float v, float step);

    void set_pixel(unsigned int x, unsigned int y, const FBColour& color)
    {
        if (x < m_width && y < m_height)
            m_pixels[y * m_width + x] = color;
    }

    void draw_rect(const GLWPrim& p);

    FBColour *m_pixels;
    unsigned int m_width;
    unsigned int m_height;
    
protected:
    GLState m_state;
    GLW_3VF m_trans;
    GLW_3VF m_scale;

    std::map<unsigned int, FBTexture*> m_textures;
    unsigned int m_next_texture;
    FBTexture* m_texture;

private:
    void glDebug(const char* msg);
};

class FBShapeBuffer : public GLShapeBuffer
{
public:
    FBShapeBuffer(bool texture = false, bool colour = false,
                   drawing_modes prim = GLW_RECTANGLE);

    virtual const char *print_statistics() const;
    virtual unsigned int size() const;

    virtual void add(const GLWPrim &rect);
    virtual void draw(const GLState &state);
    virtual void clear();

protected:
    drawing_modes m_prim_type;
    bool m_texture_verts;
    bool m_colour_verts;

    vector<GLWPrim> m_primitives;
};

#endif // USE_FB
#endif // USE_TILE_LOCAL
#endif // OGL_FB_WRAPPER_H
