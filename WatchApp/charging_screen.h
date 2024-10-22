#ifndef _CHARGING_SCREEN_H
#define _CHARGING_SCREEN_H

void charging_screen_display();
void charging_screen_draw_outer_circle(double angle, int line_width, double r, double g, double b);
void charging_screen_draw_inner_circle(int percentage);
void charging_screen_clear_inner_circle();
void* charging_screen_draw_outer_circle_animation(void* arg);
void charging_screen_draw_text(int percentage);

#endif
