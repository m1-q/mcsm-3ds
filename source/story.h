#pragma once

#include <stdbool.h>

typedef struct {
    char video_file[256];
    double total_duration;
    double choice_start;
    double choice_duration;

    char text_y[128]; char target_y[256]; bool has_y;
    char text_x[128]; char target_x[256]; bool has_x;
    char text_a[128]; char target_a[256]; bool has_a;
    char text_b[128]; char target_b[256]; bool has_b;

    char target_default[256];
    int total_choices;
    bool has_choice;

    char set_flag_key[32];
    char set_flag_val[32];
} SceneData;

bool story_load_scene(const char* scene_path, SceneData* out);
