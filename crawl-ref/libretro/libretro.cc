#include <stdio.h>
#include <string.h>
#include "libco/libco.h"
#include "libretro.h"

#include "../source/AppHdr.h"
#include "windowmanager-retro.h"
#include "glwrapper-fb.h"
#include "glwrapper-retrogl.h"

static void retro_log_printf_default(enum retro_log_level level, const char *fmt, ...);
extern int ss_main(int argc, char* argv[]);

static retro_log_printf_t log_cb = retro_log_printf_default;
static retro_video_refresh_t video_cb;
static retro_input_poll_t poll_cb;
static retro_input_state_t input_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;

static bool init_failed;
static cothread_t main_thread;
static cothread_t game_thread;

static char rc_path[PATH_MAX];

extern WindowManager *wm;
extern GLStateManager *glmanager;

static RetroWrapper *retrowm;

static bool have_frame;

#ifdef USE_FB
#define RETROGLSTATEMANAGER FBStateManager
static RETROGLSTATEMANAGER *retromanager;
#else
#define RETROGLSTATEMANAGER RetroGLStateManager
static RETROGLSTATEMANAGER *retromanager;

static struct retro_hw_render_callback render_iface;

static void core_gl_context_reset()
{
    if (retromanager)
        retromanager->context_reset();
}

#endif

// 
static void retro_log_printf_default(enum retro_log_level level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

// Input helpers
struct touch_detail
{
    bool pressed;
    int x;
    int y;
    
    touch_detail() : pressed(false), x(0), y(0) { }
    
    touch_detail(int index)
    {
        pressed = input_cb(0, RETRO_DEVICE_POINTER, index, RETRO_DEVICE_ID_POINTER_PRESSED);
        x = input_cb(0, RETRO_DEVICE_POINTER, index, RETRO_DEVICE_ID_POINTER_X) + 32768;
        y = input_cb(0, RETRO_DEVICE_POINTER, index, RETRO_DEVICE_ID_POINTER_Y) + 32768;
        
        x = (x * 1024) >> 16;
        y = (y * 768 ) >> 16;
    }
    
    operator void*() { return pressed ? this : 0; }
};

void process_touches()
{
    static bool down;
    static unsigned int touch_stamp;
    static unsigned int double_timeout;
    static touch_detail last_touch;
    
    
    // PREMOVE is used to set the mouse move event twice.
    // Doing this helps ensure the help messages are updated.
    enum touch_state_t { NONE, PREMOVE, MOVED, WAIT, CLICK };
    static touch_state_t state = NONE;
    
    touch_stamp ++;
    
    touch_detail touch1(0);
    touch_detail touch2(1);
    bool pressed = touch1 && !touch2;
    
    if (state == NONE || state == PREMOVE)
    {
        if (pressed)
        {
            state = (state == NONE) ? PREMOVE : MOVED;
            double_timeout = touch_stamp + 30;
            retrowm->push_mouse_movement(touch1.x, touch1.y);
            last_touch = touch1;
        }
    }
    else if (state == MOVED)
    {
        if (touch_stamp > double_timeout)
            state = NONE;
        else if (!pressed)
            state = WAIT;
    }
    else if (state == WAIT)
    {
        if (touch_stamp > double_timeout)
            state = NONE;
        else if (pressed)
        {
            state = CLICK;
            retrowm->push_mouse_button(0, true, last_touch.x, last_touch.y);        
        }
    }
    else if (state == CLICK)
    {
        if (!pressed)
        {
            retrowm->push_mouse_button(0, false, last_touch.x, last_touch.y);
            state = NONE;
        }
    }
}

static void keyboard_event(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers)
{
    if (retrowm)
        retrowm->push_key(down, (enum retro_key)keycode, (enum retro_mod)key_modifiers, character);
}

// Game Wrapper
void main_wrap()
{
    static char content_dir[PATH_MAX];
    strlcpy(content_dir, rc_path, PATH_MAX);

    char* slash = strrchr(content_dir, '/');
    if (slash) slash[1] = '\0';

    const char* argv[] = { "crawl", 
                           "-rc", rc_path,
                           "-dir", content_dir,
                           "-rcdir", content_dir,
                           "-morgue", content_dir,
                           "-macro", content_dir,
                           0 };

    ss_main(11, (char**)argv);
    
    while (true)
    {
        if (log_cb)
            log_cb(RETRO_LOG_ERROR, "Game thread has finished.\n");
        else
            printf("Game thread has finished.\n");
            
        co_switch(main_thread);
    }
}

void retro_return(bool flip)
{
   have_frame = flip;
   co_switch(main_thread);
}

//

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb)   { }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_cb = cb; }
void retro_set_environment(retro_environment_t cb) { environ_cb = cb; }

void retro_get_system_info(struct retro_system_info *info)
{
   info->library_name = "Dungeon Crawl: Stone Soup";
   info->library_version = "git"; // < TODO
   info->valid_extensions = "crawlrc";
   info->need_fullpath = true;
   info->block_extract = true;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   memset(info, 0, sizeof(*info));
   info->geometry.base_width = 1024;
   info->geometry.base_height = 768;
   info->geometry.max_width = 1024;
   info->geometry.max_height = 768;
   info->geometry.aspect_ratio = 0.0;
   info->timing.fps = 60;
   info->timing.sample_rate = 44100.0;
}

void retro_init(void)
{
    struct retro_log_callback log;
    if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
        log_cb = log.log;   
   
    unsigned colorMode = RETRO_PIXEL_FORMAT_XRGB8888;
    if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &colorMode))
    {
        log_cb(RETRO_LOG_ERROR, "stonesoup: Needs XRGB8888 support in frontend.\n");
        init_failed = true;
        return;
    }

    struct retro_keyboard_callback callback = { keyboard_event };
    if (!environ_cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &callback))
    {
        log_cb(RETRO_LOG_ERROR, "stonesoup: Needs keyboard callback support in frontend.\n");
    }
}

void retro_deinit(void)
{
}

bool retro_load_game(const struct retro_game_info *game)
{
    if (init_failed)
        return false;

// TODO: Fallback to FB if RetroGL fails.
#ifndef USE_FB
    memset(&render_iface, 0, sizeof(render_iface));
#ifndef GLES
    render_iface.context_type = RETRO_HW_CONTEXT_OPENGL;
#else
    render_iface.context_type = RETRO_HW_CONTEXT_OPENGLES2;
#endif
    render_iface.context_reset = core_gl_context_reset;
    render_iface.depth = true;
    render_iface.bottom_left_origin = false;
    render_iface.cache_context = true;
    
    if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &render_iface))
    {
       log_cb(RETRO_LOG_ERROR, "stonesoup: Needs OpenGL support in frontend.");
       return false;
    }
#endif

    strlcpy(rc_path, game->path, PATH_MAX);

    main_thread = co_active();
    game_thread = co_create(65536 * sizeof(void*) * 16, main_wrap);

    WindowManager::create();
    GLStateManager::init();
    
    retrowm = (RetroWrapper*)wm;
    retromanager = (RETROGLSTATEMANAGER*)glmanager;
    
    return true;
}

void retro_unload_game(void)
{
    // TODO
}


void retro_run (void)
{
    poll_cb();
    process_touches();
    
#ifdef USE_FB    
    co_switch(game_thread);
    
    if (retromanager->m_pixels)
        video_cb(have_frame ? retromanager->m_pixels : 0, retromanager->m_width,
                 retromanager->m_height, 1024 * 4);
    else
        video_cb(0, 1024, 768, 1024 * 4);
#else
    retromanager->enter_frame(render_iface.get_current_framebuffer());
    co_switch(game_thread);
    retromanager->exit_frame();
    video_cb(have_frame ? RETRO_HW_FRAME_BUFFER_VALID : 0, 1024, 768, 0);
#endif
}

// Stubs
unsigned retro_api_version(void) { return RETRO_API_VERSION; }
void retro_set_controller_port_device(unsigned in_port, unsigned device) { }
void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned unused, bool unused1, const char* unused2) { }
unsigned retro_get_region (void) { return RETRO_REGION_NTSC; }
bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info) { return false; }
size_t retro_serialize_size (void) { return 0; }
bool retro_serialize(void *data, size_t size) { return false; }
bool retro_unserialize(const void * data, size_t size) { return false; }
void *retro_get_memory_data(unsigned type) { return 0; }
size_t retro_get_memory_size(unsigned type) { return 0; }
void retro_reset (void) { /* EMPTY */ }
