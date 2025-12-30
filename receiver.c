    //Usage: receiver.exe <port>

    #define _CRT_SECURE_NO_WARNINGS

    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <stdint.h>
    #include <stdio.h>
    #include <string.h>
    #include <stdlib.h>
    #include "crc.h"
    #define PACKET_MAX 1024
    #define DATA_HDR 12

    int main(int argc, char ** argv) {
        if (argc != 2) {
            fprintf(stderr, "Usage: %s <port>\n", argv[0]);
            return 1;
        }

        printf("receiver started\n");

        int port = atoi(argv[1]);

        //init the crc table
        crc32_init();

        //socket init
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), & wsa);
        SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);

        struct sockaddr_in local = {
            0
        };
        local.sin_family = AF_INET;
        local.sin_port = htons((unsigned short) port);
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        bind(s, (struct sockaddr * ) & local, sizeof(local));

        char filename[256] = "received.bin";
        uint32_t expected_size = 0;
        FILE * out = NULL;

        uint8_t buf[PACKET_MAX];
        //declare the sender ip address
        struct sockaddr_in peer;
        int peerlen = sizeof(peer);

        for (;;) {
            int n = recvfrom(s, (char * ) buf, PACKET_MAX, 0, (struct sockaddr * ) & peer, & peerlen);
            if (n <= 0) continue;

            //check if packet is a control message
            if (n < DATA_HDR || memcmp(buf, "DATA", 4) != 0) {
                char txt[PACKET_MAX + 1];
                memcpy(txt, buf, n);
                txt[n] = 0;

                if (!strncmp(txt, "NAME=", 5)) {
                    strncpy(filename, txt + 5, sizeof(filename) - 1);
                    filename[sizeof(filename) - 1] = 0;
                } else if (!strncmp(txt, "SIZE=", 5)) {
                    expected_size = (uint32_t) strtoul(txt + 5, NULL, 10);
                } else if (!strcmp(txt, "START")) {
                    out = fopen("received.pdf", "wb+");
                    if (!out) {
                        perror("fopen");
                        break;
                    }
                    printf("Receiving %s (%u bytes)\n", filename, (unsigned) expected_size);
                } else if (!strcmp(txt, "STOP")) {
                    printf("STOP\n");
                    break;
                }
                continue;
            }

            //"DATA" + offset + crc32 + payload
            uint32_t netoff, netcrc;
            memcpy( & netoff, buf + 4, 4);
            memcpy( & netcrc, buf + 8, 4);

            uint32_t off = ntohl(netoff);
            uint32_t recv_crc = ntohl(netcrc);

            int payload = n - DATA_HDR;
            uint8_t * data = buf + DATA_HDR;

            uint32_t calc_crc = crc32(data, payload);

            char reply[64];

            if (calc_crc != recv_crc) {
                //failed crc - NACK
                snprintf(reply, sizeof(reply), "NACK %u", off);
                sendto(s, reply, (int) strlen(reply), 0,
                    (struct sockaddr * ) & peer, peerlen);
                printf("NACK %u\n", off);
                continue;
            }
            
            if (out) {
                fseek(out, (long) off, SEEK_SET);
                fwrite(data, 1, payload, out);
            }

            //send ACK
            snprintf(reply, sizeof(reply), "ACK %u", off);
            sendto(s, reply, (int) strlen(reply), 0,
                (struct sockaddr * ) & peer, peerlen);
            printf("ACK %u\n", off);


        }

        if (out) fclose(out);
        closesocket(s);
        WSACleanup();
        return 0;
    }