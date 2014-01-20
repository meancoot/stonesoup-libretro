#include "AppHdr.h"

#ifdef USE_TILE_LOCAL
#ifdef USE_FB

#include "glwrapper-fb.h"
#include "options.h"


/////////////////////////////////////////////////////////////////////////////
// Rendering helpers

#define BYTEOF(X, Y) (((X) >> ((Y) * 8)) & 0xFF)
#define BYTEOFFLOAT(X, Y) (((float)BYTEOF((X), (Y))) / 255.0f)
#define SETBYTEOF(X, Y, Z) X |= ((Z) << ((Y) * 8))

static bool alpha_test(unsigned int colour, unsigned char ref)
{
    return ((colour >> 24) != ref);
}

static unsigned int modulate(unsigned int c, unsigned int m)
{
    unsigned int result = 0;
    for (int i = 0; i != 4; i ++)
        SETBYTEOF(result, i, ((BYTEOF(c, i) * BYTEOF(m, i)) >> 8));
    return result;
}

static unsigned int blend(unsigned int c, unsigned int m)
{
    if ((c & 0xFF000000UL) == 0xFF00000000UL)
        return c;
    else if ((c & 0xFF000000UL) == 0)
        return m;

    //
    //  
    
    unsigned char src_a = BYTEOF(c, 3);
    unsigned char dst_a = 255 - src_a;    

    unsigned int result = 0;
    for (int i = 0; i != 3; i ++)
    {
        unsigned int src_c = BYTEOF(c, i) * src_a;
        unsigned int dst_c = BYTEOF(m, i) * dst_a;
    
        SETBYTEOF(result, i, (src_c + dst_c) >> 8);
    }

    return result;
}

static unsigned int get_reversed_colour(const VColour& colour)
{
    return (colour.a << 24) | (colour.r << 16) | (colour.g << 8) | colour.b;
}

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
    m_pixels = new unsigned int[m_width * m_height];
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
    if (m_texture)
        m_texture->load_pixels((unsigned int*)pixels,
                                width, height, xoffset, yoffset);

}

void FBStateManager::reset_view_for_redraw(float x, float y)
{
    memset(m_pixels, 0, m_width * m_height * sizeof(unsigned int));

    m_trans = GLW_3VF(x, y, 1);
    m_scale = GLW_3VF(1, 1, 1);
}

template<bool TEST, bool MODULATE, bool BLEND>
void FBStateManager::scan_line(const GLWPrim& p, unsigned int* out,
                               unsigned int width, float u, float v, float step)
{
    unsigned int env_colour = get_reversed_colour(m_state.array_colour ? p.col_s
                                                                       : m_state.colour);

    for (unsigned i = 0; i != width; i ++, u += step)
    {
        unsigned int colour = MODULATE ? m_texture->get_pixel(u, v)
                                       : env_colour;

        if (!TEST || alpha_test(colour, m_state.alpharef))
        {
            if (MODULATE)  colour = modulate(colour, env_colour);
            if (BLEND)     colour = blend(colour, out[i]);
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

    unsigned int* out = m_pixels + (y * m_width) + x;

    float x_step = (p.tex_ex - p.tex_sx) / w;
    float y_step = (p.tex_ey - p.tex_sy) / h;

    const bool have_texture = m_state.texture && m_state.array_texcoord;


#define SCAN(A, B, C) \
    for (float i = 0; i < h && (y + i) < m_height; i ++, out += m_width) \
        scan_line<A, B, C>(p, out, w, p.tex_sx, p.tex_sy + (i * y_step), x_step);

    if (m_state.alphatest)
        if (have_texture)
            if (m_state.blend)    SCAN(1, 1, 1)
            else                  SCAN(1, 1, 0)
        else
            if (m_state.blend)    SCAN(1, 0, 1)
            else                  SCAN(1, 0, 0)
    else
        if (have_texture)
            if (m_state.blend)    SCAN(0, 1, 1)
            else                  SCAN(0, 1, 0)
        else
            if (m_state.blend)    SCAN(0, 0, 1)
            else                  SCAN(0, 0, 0)
}

void FBStateManager::draw_line(const GLWPrim& p)
{
    unsigned int x1 = p.pos_sx * m_scale.x + m_trans.x;
    unsigned int x2 = p.pos_ex * m_scale.x + m_trans.x;
    unsigned int y1 = p.pos_sy * m_scale.y + m_trans.y;
    unsigned int y2 = p.pos_ey * m_scale.y + m_trans.y;
    
    if ((x1 != x2) && (y1 != y2))
    {
        printf("Not straight line.\n");
        return;
    }

    if (m_state.texture && m_state.array_texcoord)
    {
        printf("Textured line.\n");
        return;
    }

    unsigned int env_colour = get_reversed_colour(m_state.array_colour ? p.col_s
                                                                       : m_state.colour);

    if (x1 != x2)
    {
        if (x1 > x2)
            std::swap(x1, x2);

        if (y1 < m_height)
            for (int i = x1; i < x2 && i < m_width; i ++)
                m_pixels[(y1 * m_width) + i] = env_colour;
    }
    else if (y1 != y2)
    {
        if (y1 > y2)
            std::swap(y1, y2);
            
        if (x1 < m_width)
            for (int i = y1; i != y2 && i < m_height; i ++)
                m_pixels[(i * m_width) + x1] = env_colour;
    }
    else /* Just a dot */
        if (x1 < m_width && y1 < m_height)
            m_pixels[(y1 * m_width) + x1] = env_colour;
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
        for (auto p = m_primitives.begin(); p != m_primitives.end(); p ++)
            fbm->draw_rect(*p);
    else if (m_prim_type == GLW_LINES)
        for (auto p = m_primitives.begin(); p != m_primitives.end(); p ++)
            fbm->draw_line(*p);
}

void FBShapeBuffer::clear()
{
    m_primitives.clear();
}

#endif // USE_FB
#endif // USE_TILE_LOCAL
