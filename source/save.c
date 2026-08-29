#include "save.h"
#include <stdio.h>
#include <string.h>

GameConfig g_config;
SaveData g_save;

void save_system_init(void) {
    g_config.timer_mode = TIMER_NORMAL;
    g_config.text_lang = LANG_ENGLISH;
    g_config.voice_lang = LANG_ENGLISH;
    g_config.subs_mode = SUBS_ENGLISH;
    g_config.debug_unlocked = false;
    g_config.skip_scenes_enabled = false;

    memset(&g_save, 0, sizeof(SaveData));

    FILE* fp = fopen("sdmc:/assets/settings.dat", "rb");
    if (fp) {
        fread(&g_config, sizeof(GameConfig), 1, fp);
        fclose(fp);
    }

    FILE* sf = fopen("sdmc:/assets/save.dat", "rb");
    if (sf) {
        fread(&g_save, sizeof(SaveData), 1, sf);
        g_save.has_save = true;
        fclose(sf);
    }
}

void save_config_write(void) {
    FILE* fp = fopen("sdmc:/assets/settings.dat", "wb");
    if (fp) {
        fwrite(&g_config, sizeof(GameConfig), 1, fp);
        fclose(fp);
    }
}

void save_game_write(const char* scene_name) {
    if (!scene_name || strlen(scene_name) == 0) return;

    g_save.has_save = true;
    strncpy(g_save.current_scene, scene_name, sizeof(g_save.current_scene) - 1);
    g_save.episode = 1;

    FILE* sf = fopen("sdmc:/assets/save.dat", "wb");
    if (sf) {
        fwrite(&g_save, sizeof(SaveData), 1, sf);
        fclose(sf);
    }
}

bool save_game_exists(void) {
    return g_save.has_save && strlen(g_save.current_scene) > 0;
}

void flag_set(const char* key, const char* val) {
    if (!key || !val) return;

    // Ищем существующий флаг
    for (int i = 0; i < g_save.flags_count; i++) {
        if (!strcmp(g_save.flags[i].key, key)) {
            strncpy(g_save.flags[i].val, val, sizeof(g_save.flags[i].val) - 1);
            return;
        }
    }

    if (g_save.flags_count < MAX_FLAGS) {
        strncpy(g_save.flags[g_save.flags_count].key, key, 31);
        strncpy(g_save.flags[g_save.flags_count].val, val, 31);
        g_save.flags_count++;
    }
}

const char* flag_get(const char* key) {
    if (!key) return "";
    for (int i = 0; i < g_save.flags_count; i++) {
        if (!strcmp(g_save.flags[i].key, key)) {
            return g_save.flags[i].val;
        }
    }
    return "";
}

bool flag_is(const char* key, const char* expected_val) {
    return !strcmp(flag_get(key), expected_val);
}

void flags_clear(void) {
    g_save.flags_count = 0;
    memset(g_save.flags, 0, sizeof(g_save.flags));
}
