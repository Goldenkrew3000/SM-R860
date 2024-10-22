#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include "utils.h"
#include "framebuffer.h"
#include "charging_screen.h"

void charger_mode();
void general_mode();

int main(void) {
    printf("%s +\n", __func__);
    printf("Malexty Watch Software for SM-R860\n");
    printf("Goldenkrew3000 2024\n");

    // Check boot mode
    int bootmode = utils_check_bootmode();
    if (bootmode == 3) {
        printf("Fatal fault occured in utils_check_bootmode().\n");
        printf("%s -\n", __func__);
        return 1;
    } else if (bootmode == 2) {
        charger_mode();
    } else if (bootmode == 1) {
        // general
    }

    printf("%s -\n", __func__);
    return 0;
}

void charger_mode() {
    printf("%s +\n", __func__);

    // Enable writing to the exynos framebuffer
    utils_allow_exynos_fb_write();

    // Start the framebuffer refresher
    pthread_t thr_fb_refresh;
    pthread_create(&thr_fb_refresh, NULL, framebuffer_refresh_fb, NULL);

    // Initialize the framebuffer (Essentially fetch it's information and memory map it)
    framebuffer_init();

    // Start the charging screen process
    charging_screen_display();

    // Sleep forever TODO
    for(;;){}
}

void general_mode() {

}
