#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* input_handler_power_button_event;
//char* input_handler_secondary_button_event;

int* input_handler_screen_on;

int input_handler_query_input_devices() {
    printf("%s +\n", __func__);

    // Fetch all the files in /dev/input


    printf("%s -\n", __func__);
}

void* input_handler_monitor_power_button(void* arg) {
    printf("%s +\n", __func__);

}
