#pragma once

#include <3ds.h>
#include <stdbool.h>

#define MAX_FLAGS 32

typedef enum {
    TIMER_NORMAL = 0,
    TIMER_EXTENDED,
    TIMER_INFINITE
} TimerMode;

typedef enum {
    LANG_ENGLISH = 0,
    LANG_RUSSIAN
} GameLang;

typedef enum {
    SUBS_OFF = 0,
    SUBS_ENGLISH,
    SUBS_RUSSIAN
} SubtitlesMode;

typedef struct {
    TimerMode timer_mode;
    GameLang text_lang;
    GameLang voice_lang;
    SubtitlesMode subs_mode;
    bool debug_unlocked;
    bool skip_scenes_enabled;
} GameConfig;

typedef struct {
    char key[32];
    char val[32];
} StoryFlag;

typedef struct {
    bool has_save;
    char current_scene[128];
    int episode;
    int flags_count;
    StoryFlag flags[MAX_FLAGS];
} SaveData;

extern GameConfig g_config;
extern SaveData g_save;

void save_system_init(void);
void save_config_write(void);
void save_game_write(const char* scene_name);
bool save_game_exists(void);

// Работа с сюжетными флагами
void flag_set(const char* key, const char* val);
const char* flag_get(const char* key);
bool flag_is(const char* key, const char* expected_val);
void flags_clear(void);
