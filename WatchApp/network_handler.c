#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

extern char** environ;
pid_t pid_wpasupplicant_sta;
pid_t pid_dhclient;

int network_handler_wpasupplicant_sta_mode() {
    printf("%s +\n", __func__);
    char* argv_wpasupplicant_sta[] = {"wpa_supplicant", "-Dnl80211", "-iwlan0", "-c/root/wpa.conf", NULL};

    if (posix_spawn(&pid_wpasupplicant_sta, "/sbin/wpa_supplicant", NULL, NULL, argv_wpasupplicant_sta, environ) != 0) {
        printf("%s: Error spawning /sbin/wpa_supplicant\n", __func__);
        return 0; // TODO
    }

    //int status;
    //waitpid(&pid, &status, 0);
    //if (WIFEXITED(status)) {
    //    printf("weee %d\n", WEXITSTATUS(status));
   // }

    printf("%s -\n", __func__);
    return 0; // TODO
}

int network_handler_dhclient() {
    printf("%s +\n", __func__);
    char* argv_dhclient[] = {"/usr/sbin/dhclient", "-v", "wlan0", NULL};

    if (posix_spawn(&pid_dhclient, "/usr/sbin/dhclient", NULL, NULL, argv_dhclient, environ) != 0) {
        printf("%s: Error spawning /usr/sbin/dhclient.\n", __func__);
        return 0; // TODO
    }

    //int status_dhclient;
    //
    printf("%s -\n", __func__);
    return 0; // TODO
}

// Checks whether the device has an internet connection by attempting to contact the DNS server (1 = Success, 2 = Failure)
int network_handler_check_internet() {
    printf("%s +\n", __func__);

    struct hostent* hostinfo;
    hostinfo = gethostbyname("google.com");
    if (hostinfo == NULL) {
        printf("%s -\n", __func__);
        return 2;
    } else {
        struct in_addr** addr_list;
        addr_list = (struct in_addr**)hostinfo->h_addr_list;
        printf("Network test IP returned for address google.com: %s\n", inet_ntoa(*addr_list[0]));
        printf("%s -\n", __func__);
        return 1;
    }

    // Code should never reach here, return error
    printf("%s -\n", __func__);
    return 2;
}

