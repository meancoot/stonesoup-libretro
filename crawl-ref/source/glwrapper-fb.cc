#include "AppHdr.h"

#ifdef USE_TILE_LOCAL
#ifdef USE_FB

#include "glwrapper-fb.h"
#include "options.h"

/////////////////////////////////////////////////////////////////////////////
// Static functions from GLStateManager

GLStateManager *glmanager = NULL;

void GLStateManager::init()
{
    if (glmanager)
        return;

    glmanager = new FBStateManager();
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
    return new FBShapeBuffer(texture, colour, prim);
}

/////////////////////////////////////////////////////////////////////////////
// FBStateManager

FBStateManager::FBStateManager() :
    m_pixels(0), m_width(0), m_height(0), m_next_texture(0), m_texture(0)
{
}

void FBStateManager::set(const GLState& state)
{
    m_state = state;
}

void FBStateManager::set_transform(const GLW_3VF &trans, const GLW_3VF &scale)
{
    m_trans = trans;
    m_scale = scale;
}

void FBStateManager::reset_view_for_resize(const coord_def &m_windowsz)
{
    if (m_width == m_windowsz.x && m_height == m_windowsz.y)
        return;

    m_width = m_windowsz.x;
    m_height = m_windowsz.y;
    
    delete[] m_pixels;
    m_pixels = new FBColour[m_width * m_height];
}

void FBStateManager::reset_transform()
{
    m_trans = GLW_3VF(0, 0, 0);
    m_scale = GLW_3VF(1, 1, 1);
}

void FBStateManager::pixelstore_unpack_alignment(unsigned int bpp)
{
}

void FBStateManager::delete_textures(size_t count, unsigned int *textures)
{
    for (int i = 0; i != count; i ++)
    {
        auto tex = m_textures.find(textures[i]);
        if (tex != m_textures.end())
        {
            if (m_texture == tex->second)
                m_texture = 0;
            delete tex->second;
        }
        m_textures.erase(textures[i]);
    }
}

void FBStateManager::generate_textures(size_t count, unsigned int *textures)
{
    for (int i = 0; i != count; i ++)
    {
        unsigned int id = ++m_next_texture;
        textures[i] = id;
        m_textures[id] = new FBTexture();
    }
}

void FBStateManager::bind_texture(unsigned int texture)
{
    auto tex = m_textures.find(texture);
    m_texture = (tex == m_textures.end()) ? 0 : tex->second;
}

void FBStateManager::load_texture(unsigned char *pixels, unsigned int width,
                                   unsigned int height, MipMapOptions mip_opt,
                                   int xoffset, int yoffset)
{   
    const unsigned int* const px = (unsigned int*)pixels;
        
    if (xoffset < 0 || yoffset < 0)
    {
        m_texture->set_size(width, height);
        
        if (pixels)
            for (unsigned i = 0; i != width * height; i ++)
                m_texture->pixels[i] = px[i];
        else
            memset(m_texture->pixels, 0, width * height * 4);
    }
    else
    {
        if (m_texture->pixels == 0)
            abort();
    
        for (unsigned int i = 0; i != height; i ++)
        {
            auto ty = yoffset + i;
            if (ty >= m_texture->height)
                break;
                
            for (unsigned int j = 0; j != width; j ++)
            {
                auto tx = xoffset + j;
                if (tx >= m_texture->width)
                    break;
                    
                m_texture->pixels[ty * m_texture->width + tx] = px[i * width + j];
            }
        }
    }
}

void FBStateManager::reset_view_for_redraw(float x, float y)
{
    memset(m_pixels, 0, m_width * m_height * sizeof(unsigned int));

    m_trans = GLW_3VF(x, y, 1);
    m_scale = GLW_3VF(1, 1, 1);
}

bool FBStateManager::alpha_test(const FBColour& colour) const
{
    return !m_state.alphatest || colour.a != m_state.alpharef;

}

void FBStateManager::scan_line(const GLWPrim& p, FBColour* out,
                               unsigned int width, float u, float v, float step)
{
    FBColour env_colour = m_state.array_colour ? p.col_s
                                               : m_state.colour;
    env_colour.reverse();

    for (unsigned i = 0; i != width; i ++, u += step)
    {
        const bool have_texture = m_state.texture && m_state.array_texcoord;
        FBColour colour = have_texture ? m_texture->get_pixel(u, v)
                                       : env_colour;
                                       
        if (alpha_test(colour))
        {
            if (have_texture)     colour.modulate(env_colour);
            if (m_state.blend)    colour.blend(out[i]);
            out[i] = colour;
        }
    }
}

void FBStateManager::draw_rect(const GLWPrim& p)
{
    unsigned int x = p.pos_sx * m_scale.x + m_trans.x;
    unsigned int y = p.pos_sy * m_scale.y + m_trans.y;
    unsigned int w = (p.pos_ex * m_scale.x + m_trans.x) - x;
    unsigned int h = (p.pos_ey * m_scale.y + m_trans.y) - y;

    if ((x + w) > m_width)
        w = m_width - x;

    FBColour* out = m_pixels + (y * m_width) + x;

    float x_step = (p.tex_ex - p.tex_sx) / w;
    float y_step = (p.tex_ey - p.tex_sy) / h;

    for (float i = 0; i < h && (y + i) < m_height; i ++, out += m_width)
        scan_line(p, out, w, p.tex_sx, p.tex_sy + (i * y_step), x_step);
}


/////////////////////////////////////////////////////////////////////////////
// FBShapeBuffer

FBShapeBuffer::FBShapeBuffer(bool texture, bool colour, drawing_modes prim) :
    m_prim_type(prim),
    m_texture_verts(texture),
    m_colour_verts(colour)
{
    ASSERT(prim == GLW_RECTANGLE || prim == GLW_LINES);
}

const char *FBShapeBuffer::print_statistics() const
{
    return NULL;
}

unsigned int FBShapeBuffer::size() const
{
    return m_primitives.size();
}

void FBShapeBuffer::add(const GLWPrim &rect)
{
    m_primitives.push_back(rect);
}

// Draw the buffer
void FBShapeBuffer::draw(const GLState &state)
{
    if (m_primitives.empty())
        return;

    glmanager->set(state);

    FBStateManager* fbm = (FBStateManager*)glmanager;
    
    if (m_prim_type == GLW_RECTANGLE)
    {
        for (auto p = m_primitives.begin(); p != m_primitives.end(); p ++)
        {
            fbm->draw_rect(*p);
        }
    }

//    float pos_sx, pos_sy, pos_ex, pos_ey, pos_z;
//    float tex_sx, tex_sy, tex_ex, tex_ey;
//    VColour col_s, col_e;
}

void FBShapeBuffer::clear()
{
    m_primitives.clear();
}

#endif // USE_FB
#endif // USE_TILE_LOCAL
