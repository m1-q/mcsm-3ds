#pragma once

#include <3ds.h>
#include <stdbool.h>

bool player_init(void);
void player_cleanup(void);

bool player_start_stream(const char* filename, bool loop);
void player_stop_stream(void);
void player_pause(bool pause);
bool player_is_playing(void);

void player_draw_frame(float center_x, float center_y);

// Получение времени и общей длины текущего видео
double player_get_duration(void);
double player_get_time(void);
