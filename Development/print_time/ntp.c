#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

int ntp_port = 123;
char* ntp_server = "au.pool.ntp.org";

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

uint32_t ntp_fetch() {
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

    server = gethostbyname(ntp_server); // Convert URL to IP
    if (server == NULL) {
        printf("NTP: No such host (%s).\n", ntp_server);
        return 0x00000000;
    } else {
        printf("NTP: Resolved %s.\n", ntp_server);
    }

    // Zero out the server address structure
    bzero((char*)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;

    // Copy the server IP address to the server address structure
    bcopy((char*)server->h_addr, (char*)&serv_addr.sin_addr.s_addr, server->h_length);
    
    // Copy the port number integer to network big endian style, and save it to the server address structure
    serv_addr.sin_port = htons(ntp_port);

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

    return txTm;
}
