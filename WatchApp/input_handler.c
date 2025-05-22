#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
#include "input_handler.h"

#include <pthread.h>

#include <linux/futex.h>
#include <sys/syscall.h>
#include <stdatomic.h>
#include <unistd.h>

// Event locations (Explicitely stored on the heap)
char* inputHandler_powerKeyEvent = NULL;
char* inputHandler_secondaryKeyEvent = NULL;
char* inputHandler_touchscreenEvent = NULL;
char* inputHandler_motionEvent = NULL;

// Signal Atomics
volatile sig_atomic_t inputHandler_powerKeyAtomic = 0;
volatile sig_atomic_t inputHandler_secondaryKeyAtomic = 0;
volatile sig_atomic_t inputHandler_touchscreenAtomic = 0;
volatile sig_atomic_t inputHandler_motionAtomic = 0;

atomic_int inputHandler_powerSignal;
atomic_int inputHandler_secondarySignal;

static int is_event_device(const struct dirent* dir) {
    return strncmp("event", dir->d_name, 5) == 0;
}

int input_handler_query_input_devices() {
    printf("%s +\n", __func__);

    int ndev = 0; // Number of devices
    struct dirent** namelist;
    ndev = scandir("/dev/input", &namelist, is_event_device, alphasort);
    printf("Number of found devices: %d\n", ndev);
    if (ndev <= 0) {
        // No devices found
        return 1;
    }

    for (size_t i = 0; i < ndev; i++) {
        char fname[64];
        int fd = -1;
        char name[256];

        // Make a string of the location of the event device
        snprintf(fname, sizeof(fname), "/dev/input/%s", namelist[i]->d_name);

        // Open event device
        fd = open(fname, O_RDONLY);
        if (fd < 0) {
            // Could not open event device
            return fd;
        }

        // Fetch the name of the device
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        close(fd);
        printf("Name of device %ld: %s\n", i, name);

        // Sort the found device
        // Note: Not checking lengths here because everything should be good from above
        if (strcmp(name, EVENT_NAME_POWER) == 0) {
            // Found the power key device
            inputHandler_powerKeyEvent = (char*)malloc(strlen(fname) + 1);
            strcpy(inputHandler_powerKeyEvent, fname);
        } else if (strcmp(name, EVENT_NAME_SECONDARY) == 0) {
            // Found the secondary key device
            inputHandler_secondaryKeyEvent = (char*)malloc(strlen(fname) + 1);
            strcpy(inputHandler_secondaryKeyEvent, fname);
        } else if (strcmp(name, EVENT_NAME_TOUCHSCREEN) == 0) {
            // Found the touchscreen device
            inputHandler_touchscreenEvent = (char*)malloc(strlen(fname) + 1);
            strcpy(inputHandler_touchscreenEvent, fname);
        } else if (strcmp(name, EVENT_NAME_MOTION) == 0) {
            // Found the motion device
            inputHandler_motionEvent = (char*)malloc(strlen(fname) + 1);
            strcpy(inputHandler_motionEvent, fname);
        } else {
            printf("Found unknown device.\n");
        }
    }

    // Note: You are meant to perform a test grab to ensure no other process is monitoring the event
    // But since this is the only application running on an embedded device, I think it is fine without.

    // Check that all devices were found
    if (inputHandler_powerKeyEvent == NULL) {
        return 1;
    }
    if (inputHandler_secondaryKeyEvent == NULL) {
        return 1;
    }
    if (inputHandler_touchscreenEvent == NULL) {
        return 1;
    }
    if (inputHandler_motionEvent == NULL) {
        return 1;
    }
    printf("All required devices have been found!\n");

    printf("%s -\n", __func__);
    return 0;
}

void* input_handler_monitor_power_button(void* arg) {
    printf("%s +\n", __func__);

    int fd = open(inputHandler_powerKeyEvent, O_RDONLY);
    if (fd < 0) {
        printf("Could not open power button event.\n");
        return;
    }

    struct input_event ev[64];
    fd_set rfds;
    int rd;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    while (!inputHandler_powerKeyAtomic) {
        select(fd + 1, &rfds, NULL, NULL, NULL);
        if (inputHandler_powerKeyAtomic) {
            break;
        }
        rd = read(fd, ev, sizeof(ev));

        if (rd < (int)sizeof(struct input_event)) {
            printf("Encountered error while monitoring power button event.\n");
            return;
        }

        for (size_t i = 0; i < rd / sizeof(struct input_event); i++) {
            unsigned int type;
            unsigned int code;
            type = ev[i].type;
            code = ev[i].code;

            if (type != EV_SYN) {
                printf("Power button event: ");
                if (code != EVENT_POWER_KEY) {
                    printf("Unknown event.\n");
                } else {
                    printf("Value - ");
                    if (type == EV_MSC && (code == MSC_RAW || code == MSC_SCAN)) {
                        // Value is hex
                        printf("%02x\n", ev[i].value);
                    } else {
                        // Value is int
                        printf("%d\n", ev[i].value);
                    }

                    if (ev[i].value == 1) {
                        atomic_store(&inputHandler_powerSignal, 1);
                        syscall(SYS_futex, &inputHandler_powerSignal, FUTEX_WAKE, 1, NULL, NULL, 0);
                    }
                }
            }
        }
    }
}

void* input_handler_monitor_secondary_button(void* arg) {
    printf("%s +\n", __func__);

    int fd = open(inputHandler_secondaryKeyEvent, O_RDONLY);
    if (fd < 0) {
        printf("Could not open secondary button event.\n");
        return;
    }

    struct input_event ev[64];
    fd_set rfds;
    int rd;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    while (!inputHandler_secondaryKeyAtomic) {
        select(fd + 1, &rfds, NULL, NULL, NULL);
        if (inputHandler_secondaryKeyAtomic) {
            break;
        }
        rd = read(fd, ev, sizeof(ev));

        if (rd < (int)sizeof(struct input_event)) {
            printf("Encountered error while monitoring secondary button event.\n");
            return;
        }

        for (size_t i = 0; i < rd / sizeof(struct input_event); i++) {
            unsigned int type;
            unsigned int code;
            type = ev[i].type;
            code = ev[i].code;

            if (type != EV_SYN) {
                printf("Secondary button event: ");
                if (code != EVENT_SECONDARY_KEY) {
                    printf("Unknown event.\n");
                } else {
                    printf("Value - ");
                    if (type == EV_MSC && (code == MSC_RAW || code == MSC_SCAN)) {
                        // Value is hex
                        printf("%02x\n", ev[i].value);
                    } else {
                        // Value is int
                        printf("%d\n", ev[i].value);
                    }

                    if (ev[i].value == 1) {
                        atomic_store(&inputHandler_secondarySignal, 1);
                        syscall(SYS_futex, &inputHandler_secondarySignal, FUTEX_WAKE, 1, NULL, NULL, 0);
                    }
                }
            }
        }
    }
}
