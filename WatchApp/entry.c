#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include "utils.h"
#include "framebuffer.h"
#include "network_handler.h"
#include "config_handler.h"
#include "ntp_handler.h"
#include "input_handler.h"
#include "screens/charging_screen.h"
#include "screens/shutdown_screen.h"
#include "duck_watchface.h"
#include "watchfaces/uwoslab_serbio.h"
#include "watchfaces/basic_digital.h"

#include <sys/types.h>

#include <linux/futex.h>
#include <sys/syscall.h>
#include <stdatomic.h>
#include <unistd.h>

//extern atomic_int sigPower;

void* monitor(void*) {
    //while (1) {
        //int val = atomic_load(&sigPower);
        //if (val == 1) {
        //    printf("Caught!!\n");
        //};
        //syscall(SYS_futex, &sigPower, FUTEX_WAIT, val, NULL, NULL, 0);
   // }
}

void charger_mode();
void general_mode();

int main(void) {
    printf("%s +\n", __func__);
    printf("Hojuix Watch Software for SM-R860\n");
    printf("Hojuix.org 2024-2025\n");

    // Check boot mode, and launch into specific mode
    int bootmode = utils_check_bootmode();
    if (bootmode == 3) {
        printf("Fatal fault occured in utils_check_bootmode().\n");
        printf("%s -\n", __func__);
        return 1;
    } else if (bootmode == 2) {
        charger_mode();
    } else if (bootmode == 1) {
        general_mode();
    }

    printf("%s -\n", __func__);
    return 0;
}

void charger_mode() {
    printf("%s +\n", __func__);

    // Enable writing to the exynos framebuffer
    if (utils_allow_exynos_fb_write() == 2) {
        printf("%s: Fatal error occured allowing write to Exynos framebuffer.\n", __func__);
        utils_syscall_reboot();
    }

    // Start the framebuffer refresher
    pthread_t thr_fb_refresh;
    pthread_create(&thr_fb_refresh, NULL, framebuffer_refresh_fb, NULL);

    // Initialize the framebuffer (Essentially fetch it's information and memory map it)
    if (framebuffer_init() == 2) {
        printf("%s: Fatal error occured initializing Exynos framebuffer.\n", __func__);
        utils_syscall_reboot();
    }

    // Display the charging screen, and when it exits (Watch disconnected from charger), display the shutdown screen
    charging_screen_display();
    shutdown_screen_display();

    // Unmap the framebuffer
    framebuffer_deinit();

    // Shutdown with a syscall
    utils_syscall_shutdown();
}

void general_mode() {
    printf("%s +\n", __func__);
    //network_handler_wpasupplicant_sta_mode();
    //sleep(2);
    //network_handler_dhclient();
    //sleep(300);
    //


    if (utils_allow_exynos_fb_write() == 2) {
        printf("%s: Fatal error occured allowing write to Exynos framebuffer.\n", __func__);
        utils_syscall_reboot();
    }

    pthread_t thr_fb_refresh;
    pthread_create(&thr_fb_refresh, NULL, framebuffer_refresh_fb, NULL);

    if (framebuffer_init() == 2) {
        printf("%s: Fatal error occured initializing Exynos framebuffer.\n", __func__);
        utils_syscall_reboot();
    }

    // Read config (Specifically read it here because I can display errors to the framebuffer, and user errors can cause these errors easily)

    // Read config file
    // TODO ADD ERROR CHECKING
    config_handler_read();

    // Check network connection
    if (network_handler_check_internet() == 2) {
        printf("Non-fatal error occured, no internet detected.\n");
    }

    // Update system time from NTP
    // TODO ADD ERROR CHECKING AND MAKE SURE THAT DEVICE IS CONNECTED TO WIFI BEFOREHAND
    uint32_t currentNtpTimestamp = ntp_fetchTime();
    if (ntp_setSystemTime(currentNtpTimestamp) == 2) {
        printf("Non-fatal error occured attempting to set system time.\n");
    }

    //char* haha = ntp_getTimeString();
    //printf("time: %s\n", haha);
    printf("CURRENT: %d\n", utils_read_battery_current());

    // Scan event devices
    input_handler_query_input_devices();

    // Start event threads
    pthread_t thr_event_power;
    pthread_create(&thr_event_power, NULL, input_handler_monitor_power_button, NULL);
    pthread_t thr_event_secondary;
    pthread_create(&thr_event_secondary, NULL, input_handler_monitor_secondary_button, NULL);

    pthread_t b;
    pthread_create(&b, NULL, monitor, NULL);

    // Display watchface
    //duck_watchface_display();
    //uwoslab_serbio_screen_display();
    basic_digital_screen_display();
}
