#ifndef RETRO_WINDOWMANAGER_H
#define RETRO_WINDOWMANAGER_H

#ifdef USE_TILE_LOCAL
#ifdef __LIBRETRO__

#include "../libretro/libretro.h"

#include "windowmanager.h"

class RetroWrapper : public WindowManager
{
public:
    RetroWrapper();
    ~RetroWrapper();

    // Class functions
    virtual int init(coord_def *m_windowsz);

    // Environment state functions
    virtual void set_window_title(const char *title);
    virtual bool set_window_icon(const char* icon_name);

    virtual key_mod get_mod_state() const;
    virtual void set_mod_state(key_mod mod);

    // System time functions
    virtual void set_timer(unsigned int interval,
                           wm_timer_callback callback);
    virtual unsigned int get_ticks() const;
    virtual void delay(unsigned int ms);

    // Event functions
    virtual int raise_custom_event();
    virtual int wait_event(wm_event *event);
    virtual unsigned int get_event_count(wm_event_type type);

    // Display functions
    virtual void resize(coord_def &m_windowsz);
    virtual void swap_buffers();
    virtual int screen_width() const;
    virtual int screen_height() const;
    virtual int desktop_width() const;
    virtual int desktop_height() const;

    // Texture loading
    virtual bool load_texture(GenericTexture *tex, const char *filename,
                              MipMapOptions mip_opt, unsigned int &orig_width,
                              unsigned int &orig_height,
                              tex_proc_func proc = NULL,
                              bool force_power_of_two = true);

    // Event functions
    void push_mouse_movement(int x, int y);
    void push_mouse_button(unsigned int button, bool state, int x, int y);
    void push_key(bool down, enum retro_key keycode, enum retro_mod mods, uint32_t unicode);
    
private:
    std::deque<wm_event> queue;
};

#endif // __LIBRETRO__
#endif // USE_TILE_LOCAL

#endif // RETRO_WINDOWMANAGER_H
