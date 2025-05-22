#ifndef _INPUT_HANDLER_H
#define _INPUT_HANDLER_H
#include <signal.h>

// Event names
#define EVENT_NAME_POWER            "sec-pmic-key"
#define EVENT_NAME_SECONDARY        "gpio_keys"
#define EVENT_NAME_TOUCHSCREEN      "sec_touchscreen"
#define EVENT_NAME_MOTION           "LPM_MOTION"

// Power button (EV_KEY / Type 1, Code 116 (KEY_POWER), 1 or 0)
#define EVENT_POWER_KEY             116

// Secondary button (EV_KEY / Type 1, Code 580 (KEY_APPSELECT), 1 or 0)
#define EVENT_SECONDARY_KEY         580

// TODO Touchscreen and motion

int input_handler_query_input_devices();
void* input_handler_monitor_power_button(void* arg);
void* input_handler_monitor_secondary_button(void* arg);

#endif

