#include <stdio.h>
#include <string.h>
#include <fcntl.h>

int main() {
    char cmdline_buffer[4096];

    FILE *cmdline_fp = fopen("/proc/cmdline", "r");
    if (!cmdline_fp) {
        printf("Failed to open /proc/cmdline\n");
        return 1;
    }

    if (fgets(cmdline_buffer, sizeof(cmdline_buffer), cmdline_fp) == NULL) {
        printf("Failed to read from /proc/cmdline\n");
    }

    if (strstr(cmdline_buffer, "androidboot.mode=charger") != NULL) {
        printf("Device is in charging mode.\n");
    } else {
        printf("Device is not in charging mode.\n");
    }

    return 0;
}
