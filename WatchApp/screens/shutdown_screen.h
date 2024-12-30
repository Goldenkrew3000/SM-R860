#ifndef _SHUTDOWN_SCREEN_H
#define _SHUTDOWN_SCREEN_H

void shutdown_screen_display();
void shutdown_screen_draw_text(int second);
void* shutdown_screen_draw_circle_animation(void* arg);
void shutdown_screen_draw_circle(double angle);

#endif
