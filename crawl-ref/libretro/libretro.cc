#include <stdio.h>
#include <string.h>
#include "libco/libco.h"
#include "libretro.h"

#include "../source/AppHdr.h"
#include "../source/windowmanager-retro.h"
#include "../source/glwrapper-fb.h"

extern WindowManager *wm;
static RetroWrapper *retrowm;

extern GLStateManager *glmanager;
static FBStateManager *fbmanager;

static cothread_t main_thread;
static cothread_t game_thread;

static retro_log_printf_t log_cb = NULL;
static retro_video_refresh_t video_cb = NULL;
static retro_input_poll_t poll_cb = NULL;
static retro_input_state_t input_cb = NULL;
static retro_audio_sample_batch_t audio_batch_cb = NULL;
static retro_environment_t environ_cb = NULL;

extern int ss_main(int argc, char* argv[]);
static char rc_path[PATH_MAX];

void main_wrap()
{
    static char content_dir[PATH_MAX];
    strlcpy(content_dir, rc_path, PATH_MAX);

    char* slash = strrchr(content_dir, '/');
    if (slash) *slash = '\0';

#define MAKE_AND_BAKE(X) \
    static char X[PATH_MAX]; snprintf(X, PATH_MAX, #X"=%s", content_dir);
    
    MAKE_AND_BAKE(crawl_dir);
    MAKE_AND_BAKE(morgue_dir);
    MAKE_AND_BAKE(save_dir);
    MAKE_AND_BAKE(macro_dir);

    const char* argv[] = { "crawl", 
                           "-rc", rc_path,
                           "--extra-opt-first", crawl_dir,
                           "--extra-opt-first", morgue_dir,
                           "--extra-opt-first", save_dir,
                           "--extra-opt-first", macro_dir,                                                      
                           0 };

    ss_main(9, (char**)argv);
    
    while (true)
    {
        if (log_cb)
            log_cb(RETRO_LOG_ERROR, "Game thread has finished.\n");
        else
            printf("Game thread has finished.\n");
            
        co_switch(main_thread);
    }
}

static void core_gl_context_reset()
{
}

static bool have_frame;
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
   info->library_version = "0"; // < TODO
   info->valid_extensions = "crawlrc";
   info->need_fullpath = true;
   info->block_extract = true;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   // TODO
   info->geometry.base_width = 1024;
   info->geometry.base_height = 768;
   info->geometry.max_width = 1024;
   info->geometry.max_height = 768;
   info->geometry.aspect_ratio = 0.0;
   info->timing.fps = 60;
   info->timing.sample_rate = 44100.0;
}

static void keyboard_event(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers)
{
    if (retrowm)
        retrowm->push_key(down, (enum retro_key)keycode, (enum retro_mod)key_modifiers, character);
}

void retro_init(void)
{
    struct retro_log_callback log;
    if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
        log_cb = log.log;   
   
    unsigned colorMode = RETRO_PIXEL_FORMAT_XRGB8888;
    environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &colorMode);

    struct retro_keyboard_callback callback = { keyboard_event };
    environ_cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &callback);
}

void retro_deinit(void)
{
}

bool retro_load_game(const struct retro_game_info *game)
{    
    strlcpy(rc_path, game->path, PATH_MAX);

    main_thread = co_active();
    game_thread = co_create(65536 * sizeof(void*) * 16, main_wrap);
    return true;
}

void retro_run (void)
{
    retrowm = wm ? (RetroWrapper*)wm : 0;
    fbmanager = glmanager ? (FBStateManager*)glmanager : 0;

    poll_cb();
    co_switch(game_thread);
    
    if (fbmanager && fbmanager->m_pixels)
        video_cb(fbmanager->m_pixels, fbmanager->m_width, fbmanager->m_height, 1024 * 4);
    else
        video_cb(0, 1024, 768, 1024 * 4);
}

// Stubs
unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_set_controller_port_device(unsigned in_port, unsigned device) { }

void retro_cheat_reset(void) { }
void retro_cheat_set(unsigned unused, bool unused1, const char* unused2) { }
unsigned retro_get_region (void) { return RETRO_REGION_NTSC; }

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info) { return false; }
void retro_unload_game(void) { }

size_t retro_serialize_size (void) { return 0; }
bool retro_serialize(void *data, size_t size) { return false; }
bool retro_unserialize(const void * data, size_t size) { return false; }

void *retro_get_memory_data(unsigned type) { return 0; }
size_t retro_get_memory_size(unsigned type) { return 0; }

void retro_reset (void) { /* EMPTY */ }
