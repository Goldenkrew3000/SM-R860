#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <time.h>
#include "ntp_handler.h"

extern char* config_ntpServer;
extern char* config_ntpPort;
extern char* config_timezone;
extern char* config_timeFormat;

#define NTP_TIMESTAMP_DELTA 2208988800ull
#define LI(packet)   (uint8_t) ((packet.li_vn_mode & 0xC0) >> 6) // (li   & 11 000 000) >> 6
#define VN(packet)   (uint8_t) ((packet.li_vn_mode & 0x38) >> 3) // (vn   & 00 111 000) >> 3
#define MODE(packet) (uint8_t) ((packet.li_vn_mode & 0x07) >> 0) // (mode & 00 000 111) >> 0

typedef struct {
    uint8_t li_vn_mode;
    uint8_t stratum;
    uint8_t poll;
    uint8_t precision;
    uint32_t rootDelay;
    uint32_t rootDispersion;
    uint32_t refId;
    uint32_t refTm_s;
    uint32_t refTm_f;
    uint32_t origTm_s;
    uint32_t origTm_f;
    uint32_t rxTm_s;
    uint32_t rxTm_f;
    uint32_t txTm_s; // Transmit timestamp seconds
    uint32_t txTm_f;
} ntp_packet;

uint32_t ntp_fetchTime() {
    printf("%s +\n", __func__);

    int fd_sock;
    int ret_sock;

    // Create and zero out the NTP packet
    ntp_packet packet = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    memset(&packet, 0, sizeof(ntp_packet));

    // Set the first byte's bits to 00011011, li = 0, vn = 3, mode = 3
    *((char*)&packet + 0) = 0x1b;

    // Create a UDP socket, connect to the server, send the packet, and read the return packet
    struct sockaddr_in serv_addr;
    struct hostent *server;

    fd_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd_sock < 0) {
        printf("NTP: Error opening socket.\n");
        return 0x00000000;
    } else {
        printf("NTP: Opened socket.\n");
    }

    server = gethostbyname(config_ntpServer); // Convert URL to IP
    if (server == NULL) {
        printf("NTP: No such host (%s).\n", config_ntpServer);
        return 0x00000000;
    } else {
        printf("NTP: Resolved %s.\n", config_ntpServer);
    }

    // Zero out the server address structure
    bzero((char*)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;

    // Copy the server IP address to the server address structure
    bcopy((char*)server->h_addr, (char*)&serv_addr.sin_addr.s_addr, server->h_length);
    
    // Copy the port number integer to network big endian style, and save it to the server address structure
    serv_addr.sin_port = htons(atoi(config_ntpPort));

    // Call the server
    if (connect(fd_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("NTP: Could not connect.\n");
        return 0x00000000;
    } else {
        printf("NTP: Connected.\n");
    }

    // Send the NTP packet
    ret_sock = write(fd_sock, (char*)&packet, sizeof(ntp_packet));
    if (ret_sock < 0) {
        printf("NTP: Error writing to socket.\n");
        return 0x00000000;
    } else {
        printf("NTP: Sent NTP packet to socket.\n");
    }

    // Read from the socket
    ret_sock = read(fd_sock, (char*)&packet, sizeof(ntp_packet));
    if (ret_sock < 0) {
        printf("NTP: Could not read NTP packet from socket.\n");
        return 0x00000000;
    } else {
        printf("NTP: Read NTP packet from socket.\n");
    }

    // Convert the byte order endianness
    packet.txTm_s = ntohl(packet.txTm_s);
    packet.txTm_f = ntohl(packet.txTm_f);

    // Convert to UNIX epoch
    time_t txTm = (time_t)(packet.txTm_s - NTP_TIMESTAMP_DELTA);

    printf("%s -\n", __func__);
    return txTm;
}

// Set the system time to a UNIX epoch (1 = Success, 2 = Failure)
int ntp_setSystemTime(uint32_t epoch) {
    printf("%s +\n", __func__);

    struct timeval tv;
    tv.tv_sec = epoch;
    tv.tv_usec = 0;
    if (settimeofday(&tv, NULL) == -1) {
        return 2;
    }

    printf("%s -\n", __func__);
    return 1;
}

char* ntp_getTimeString() {
    printf("%s +\n", __func__);

    // NOTE: Using normal timezones requires 'tzdata' to be installed
    setenv("TZ", config_timezone, 1);
    tzset();

    // Grab system time, and convert to local time
    time_t currentSystemTime;
    time(&currentSystemTime);
    struct tm* localTime = localtime(&currentSystemTime);

    // Get hour and minute string, get AM / PM, and convert if needed (Currently only 'ko' needs converting)
    char hourMinuteString[32];
    strftime(hourMinuteString, sizeof(hourMinuteString), "%I : %M", localTime);
    char AMPMString[8];
    strftime(AMPMString, sizeof(AMPMString), "%p", localTime);
    if (strstr(config_timeFormat, "ko") != NULL) {
        if (strstr(AMPMString, "AM") != NULL) {
            strcpy(AMPMString, "오전");
        } else if (strstr(AMPMString, "PM") != NULL) {
            strcpy(AMPMString, "오후");
        }
    }

    // Set timezone back to UTC to avoid any issues with the NTP implementation
    setenv("TZ", "UTC", 1);
    tzset();

    // Construct final time string
    static char timeString[32];
    if (strstr(config_timeFormat, "ko") != NULL) {
        snprintf(timeString, sizeof(timeString), "%s %s", AMPMString, hourMinuteString);
    } else if (strstr(config_timeFormat, "us") != NULL) {
        snprintf(timeString, sizeof(timeString), "%s %s", hourMinuteString, AMPMString);
    } else {
        printf("Non-fatal error occured, Time Format %s is unknown, defaulting to 'us'.\n", config_timeFormat);
        snprintf(timeString, sizeof(timeString), "%s %s", hourMinuteString, AMPMString);
    }

    printf("%s -\n", __func__);
    return timeString;
}
