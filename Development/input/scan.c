#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>

#define DEV_INPUT_EVENT "/dev/input" //DEV_INPUT_EVENT
#define EVENT_DEV_NAME "event"

volatile sig_atomic_t stop = 0;

// function from evtest
static int is_event_device(const struct dirent* dir) {
    return strncmp(EVENT_DEV_NAME, dir->d_name, 5) == 0;
}

// function from evtest
static int test_grab(int fd, int grab_flag) {
    int rc;
    rc = ioctl(fd, EVIOCGRAB, (void*)1);
    if (rc == 0 && !grab_flag) {
        ioctl(fd, EVIOCGRAB, (void*)0);
    }
    return rc;
}

int main() {
    printf("Input scan test\n");

    int ndev = 0;
    struct dirent** namelist;
    ndev = scandir(DEV_INPUT_EVENT, &namelist, is_event_device, alphasort); // Allocates memory automatically, but you need to free it manually

    if (ndev <= 0) {
        printf("fucky wucky\n");
    }
    printf("Devices found: %d\n", ndev);


    // In the prod version - I would put the info into a struct loaded onto the heap here
    for(size_t i = 0; i < ndev; i++) {
        char fname[64];
        int fd = -1;
        char name[256];

        // Make a string of the location of the device
        snprintf(fname, sizeof(fname), "%s/%s", DEV_INPUT_EVENT, namelist[i]->d_name);

        // Open file as read only
        fd = open(fname, O_RDONLY);
        if (fd < 0) {
            printf("Failure: %d\n", fd); // P.S. -1 is EPERM
            continue; // It fails here
        }

        // Fetch the name of the event
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        close(fd);
        printf("Name: %s\n", name);

        free(namelist[i]);
    }




    // Perform a test grab to make sure that the event is not grabbed by another process
    int fd_b = open("/dev/input/event0", O_RDONLY);
    if (fd_b < 0) { exit(1); }
    //test_grab(fd_b, grab_flag); // Think this should return 0


    // Now actually do events
    struct input_event ev[64];
    fd_set rfds;
    int rd;

    FD_ZERO(&rfds);
    FD_SET(fd_b, &rfds);

    while (!stop) {
        select(fd_b + 1, &rfds, NULL, NULL, NULL);
        if (stop) {
            break;
        }
        rd = read(fd_b, ev, sizeof(ev));

        if (rd < (int)sizeof(struct input_event)) {
            printf("Fucked up.\n");
            exit(1);
        }

        for (size_t i = 0; i < rd / sizeof(struct input_event); i++) {
            unsigned int type, code;

            type = ev[i].type;
            code = ev[i].code;

            if (type != EV_SYN) {
                printf("Event received: %ld.%06ld\n", ev[i].time.tv_sec, ev[i].time.tv_usec);
                printf("Type %d - Code %d - ", type, code);
                if (type == EV_MSC && (code == MSC_RAW || code == MSC_SCAN)) {
                    printf("Value %02x\n", ev[i].value);
                } else {
                    printf("Value %d\n", ev[i].value);
                }
            }
        }
    }

    // Always ungrab the device before exiting
    ioctl(fd_b, EVIOCGRAB, (void*)0);
}
