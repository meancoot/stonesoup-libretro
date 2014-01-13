#ifdef USE_TILE_LOCAL
#ifdef __LIBRETRO__

#include <png.h>
#include "AppHdr.h"
#include "windowmanager-retro.h"

#include "cio.h"
#include "files.h"
#include "glwrapper.h"
#include "libutil.h"
#include "options.h"
#include "syscalls.h"
#include "version.h"
#include "windowmanager.h"

WindowManager *wm = NULL;
extern void retro_return(bool flip);

static int _translate_keysym(enum retro_key sym, enum retro_mod mods, uint32_t unicode)
{
    // This function returns the key that was hit.  Returning zero implies that
    // the keypress (e.g. hitting shift on its own) should be eaten and not
    // handled.

    const int shift_offset = CK_SHIFT_UP - CK_UP;
    const int ctrl_offset  = CK_CTRL_UP - CK_UP;

    int mod = 0;
    if (mods & RETROKMOD_SHIFT)
        mod |= MOD_SHIFT;
    if (mods & RETROKMOD_CTRL)
        mod |= MOD_CTRL;
    if (mods & RETROKMOD_ALT)
        mod |= MOD_ALT;

    // This is arbitrary, but here's the current mappings.
    // 0-256: ASCII, Crawl arrow keys
    // 0-1k : Other SDL keys (F1, Windows keys, etc...) and modifiers
    // 1k-3k: Non-ASCII with modifiers other than just shift or just ctrl.
    // 3k+  : ASCII with the left alt modifier.

    int offset = mod ? 1000 + 256 * mod : 0;
    int numpad_offset = 0;
    if (mod == MOD_CTRL)
        numpad_offset = ctrl_offset;
    else if (mod == MOD_SHIFT)
        numpad_offset = shift_offset;
    else
        numpad_offset = offset;

    switch (sym)
    {
    case RETROK_RETURN:
    case RETROK_KP_ENTER:
        return CK_ENTER + offset;
    case RETROK_BACKSPACE:
        return CK_BKSP + offset;
    case RETROK_ESCAPE:
        return CK_ESCAPE + offset;
    case RETROK_DELETE:
        return CK_DELETE + offset;

#ifdef __ANDROID__
    // i think android's SDL port treats these differently? they certainly
    // shouldn't be interpreted as unicode characters!
    case RETROK_LSHIFT:
    case RETROK_RSHIFT:
    case RETROK_LALT:
    case RETROK_RALT:
    case RETROK_LCTRL:
    case RETROK_RCTRL:
#endif
    case RETROK_NUMLOCK:
    case RETROK_CAPSLOCK:
    case RETROK_SCROLLOCK:
    case RETROK_RMETA:
    case RETROK_LMETA:
    case RETROK_LSUPER:
    case RETROK_RSUPER:
    case RETROK_MODE:
    case RETROK_COMPOSE:
        // Don't handle these.
        return 0;

    case RETROK_F1:
    case RETROK_F2:
    case RETROK_F3:
    case RETROK_F4:
    case RETROK_F5:
    case RETROK_F6:
    case RETROK_F7:
    case RETROK_F8:
    case RETROK_F9:
    case RETROK_F10:
    case RETROK_F11:
    case RETROK_F12:
    case RETROK_F13:
    case RETROK_F14:
    case RETROK_F15:
    case RETROK_HELP:
    case RETROK_PRINT:
    case RETROK_SYSREQ:
    case RETROK_BREAK:
    case RETROK_MENU:
    case RETROK_POWER:
    case RETROK_EURO:
    case RETROK_UNDO:
        ASSERT_RANGE(sym, RETROK_F1, RETROK_UNDO + 1);
        return -(sym + (RETROK_UNDO - RETROK_F1 + 1) * mod);

        // Hack.  libw32c overloads clear with '5' too.
    case RETROK_KP5:
        return CK_CLEAR + numpad_offset;

    case RETROK_KP8:
    case RETROK_UP:
        return CK_UP + numpad_offset;
    case RETROK_KP2:
    case RETROK_DOWN:
        return CK_DOWN + numpad_offset;
    case RETROK_KP4:
    case RETROK_LEFT:
        return CK_LEFT + numpad_offset;
    case RETROK_KP6:
    case RETROK_RIGHT:
        return CK_RIGHT + numpad_offset;
    case RETROK_KP0:
    case RETROK_INSERT:
        return CK_INSERT + numpad_offset;
    case RETROK_KP7:
    case RETROK_HOME:
        return CK_HOME + numpad_offset;
    case RETROK_KP1:
    case RETROK_END:
        return CK_END + numpad_offset;
    case RETROK_CLEAR:
        return CK_CLEAR + numpad_offset;
    case RETROK_KP9:
    case RETROK_PAGEUP:
        return CK_PGUP + numpad_offset;
    case RETROK_KP3:
    case RETROK_PAGEDOWN:
        return CK_PGDN + numpad_offset;
    case RETROK_TAB:
        if (numpad_offset) // keep tab a tab
            return CK_TAB_TILE + numpad_offset;
#ifdef TOUCH_UI
        break;
    // used for zoom in/out
    case RETROK_KP_PLUS:
        return CK_NUMPAD_PLUS;
    case RETROK_KP_MINUS:
        return CK_NUMPAD_MINUS;
#endif
    default:
        break;
    }

    // Alt does not get baked into keycodes like shift and ctrl, so handle it.
    const int key_offset = (mod & MOD_ALT) ? -3000 : 0;
    return (unicode < 127) ? (unicode & 0x7F) + key_offset : unicode;
}

void WindowManager::create()
{
    if (wm)
        return;

    wm = new RetroWrapper();
}

void WindowManager::shutdown()
{
    delete wm;
    wm = NULL;
}

RetroWrapper::RetroWrapper()                              { }
RetroWrapper::~RetroWrapper()                             { }
int RetroWrapper::screen_width() const                    { return 1024; }
int RetroWrapper::screen_height() const                   { return 768;  }
int RetroWrapper::desktop_width() const                   { return 1024; }
int RetroWrapper::desktop_height() const                  { return 768;  }
void RetroWrapper::set_window_title(const char *title)    { }
bool RetroWrapper::set_window_icon(const char* icon_name) { return true; }
void RetroWrapper::set_mod_state(key_mod mod)             { /* (EMPTY) Functionality not needed */ }
void RetroWrapper::delay(unsigned int ms)                 { /* (EMPTY) Never called */ }

int RetroWrapper::init(coord_def *m_windowsz)
{
    m_windowsz->x = 1024;
    m_windowsz->y = 768;

/*
    if (Options.tile_key_repeat_delay > 0)
    {
        const int repdelay    = Options.tile_key_repeat_delay;
        const int interval = SDL_DEFAULT_REPEAT_INTERVAL;
        if (SDL_EnableKeyRepeat(repdelay, interval) != 0)
            printf("Failed to set key repeat mode: %s\n", SDL_GetError());
    }
*/

    return true;
}


void RetroWrapper::resize(coord_def &m_windowsz)
{
    glmanager->reset_view_for_resize(m_windowsz);
}

key_mod RetroWrapper::get_mod_state() const
{
    // (EMPTY) Never called
    return (key_mod)0;
}


int RetroWrapper::wait_event(wm_event *event)
{
    while (queue.empty())
        retro_return(false);
    
    *event = queue.front();    
    queue.pop_front();
    return 1;
}

unsigned int RetroWrapper::get_ticks() const
{
    // TODO: Needed for tooltips
    static int t = 0;
    return t+=10;
}

void RetroWrapper::set_timer(unsigned int interval, wm_timer_callback callback)
{
    // (EMPTY) TODO: Needed for tooltips
}

int RetroWrapper::raise_custom_event()
{
    // (EMPTY) TODO: Needed for tooltips
    return 0;
}

void RetroWrapper::swap_buffers()
{
   retro_return(true);
}



unsigned int RetroWrapper::get_event_count(wm_event_type type)
{
    unsigned int result = 0;

    for (auto i = queue.begin(); i != queue.end(); i ++)
        result += (i->type == type) ? 1 : 0;
    
    return result;
}

struct ImageBuf
{
    unsigned int width;
    unsigned int height;
    unsigned int *pixels;
    unsigned int **rows;
    
    ImageBuf(unsigned int width_, unsigned int height_) :
        width(width_), height(height_), 
        pixels(new unsigned int[width * height]),
        rows(new unsigned int*[height])
    {
        for (unsigned int i = 0; i != height; i ++)
        {
            rows[i] = pixels + (i * width);
        }
    }
    
    ~ImageBuf()
    {
        delete[] pixels;
        delete[] rows;  
    }
};

static unsigned int next_power_of_two(unsigned int x)
{
    unsigned int result = 1;
    while (result < x)
        result *= 2;
    return result;
}

bool RetroWrapper::load_texture(GenericTexture *tex, const char *filename,
                              MipMapOptions mip_opt, unsigned int &orig_width,
                              unsigned int &orig_height, tex_proc_func proc,
                              bool force_power_of_two)
{
    string tex_path = datafile_path(filename);

    if (tex_path.c_str()[0] == 0)
    {
        fprintf(stderr, "Couldn't find texture '%s'.\n", filename);
        return false;
    }

    FILE* fp = 0;
    unsigned char header[8];    
    png_structp png_ptr = 0;
    png_infop info_ptr = 0;

    if (   !(fp = fopen(tex_path.c_str(), "rb"))
        || fread(header, 1, 8, fp) != 8
        || png_sig_cmp(header, 0, 8) != 0
        || !(png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
                                              NULL, NULL))
        || !(info_ptr = png_create_info_struct(png_ptr))
        || setjmp(png_jmpbuf(png_ptr)) )
    {
        if (fp) fclose(fp);
        if (png_ptr) png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return false;        
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, info_ptr);    
    orig_width = png_get_image_width(png_ptr, info_ptr);
    orig_height = png_get_image_height(png_ptr, info_ptr);
    png_read_update_info(png_ptr, info_ptr);

    /* read file */
    int new_width  = force_power_of_two ? next_power_of_two(orig_width)
                                        : orig_width;
    int new_height = force_power_of_two ? next_power_of_two(orig_height)
                                        : orig_height;

    ImageBuf image(new_width, new_height);
    png_read_image(png_ptr, (png_bytepp)image.rows);

    fclose(fp);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);    

    //
    glmanager->pixelstore_unpack_alignment(4);

    if (!proc || proc((unsigned char*)image.pixels, new_width, new_height))
        return tex->load_texture((unsigned char*)image.pixels, new_width, new_height, mip_opt);
    return false;
}

void RetroWrapper::push_mouse_movement(int x, int y)
{
    wm_event event;
    event.type = WME_MOUSEMOTION;    
    event.mouse_event.held   = MouseEvent::NONE;
    event.mouse_event.event  = MouseEvent::MOVE;
    event.mouse_event.button = MouseEvent::NONE;
    event.mouse_event.px     = x;
    event.mouse_event.py     = y;
    queue.push_back(event);
}

void RetroWrapper::push_mouse_button(unsigned int button, bool state,
                                     int x, int y)
{
    static const MouseEvent::mouse_event_button buttons[2] =
        { MouseEvent::LEFT, MouseEvent::RIGHT };

    if (button > 1)
        return;

    wm_event event;
    event.type                  = state ? WME_MOUSEBUTTONDOWN
                                        : WME_MOUSEBUTTONUP;
    event.mouse_event.held      = MouseEvent::NONE;
    event.mouse_event.event     = state ? MouseEvent::PRESS 
                                        : MouseEvent::RELEASE;
    event.mouse_event.button    = buttons[button];
    event.mouse_event.px        = x;
    event.mouse_event.py        = y;
    queue.push_back(event);
}

void RetroWrapper::push_key(bool down, enum retro_key keycode, enum retro_mod mods, uint32_t unicode)
{
    wm_event event;
    event.type = down ? WME_KEYDOWN : WME_KEYUP;
    event.key.state = down;
    event.key.keysym.scancode = 0; // < Unused
    event.key.keysym.key_mod = (key_mod)0;
    event.key.keysym.unicode = 0; // < Unused
    event.key.keysym.sym = _translate_keysym(keycode, mods, unicode);
    queue.push_back(event);
}


#endif // __LIBRETRO__
#endif // USE_TILE_LOCAL
