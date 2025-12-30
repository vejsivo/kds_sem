//Usage: sender.exe <ip> <port> <file>

#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "crc.h"

#define PACKET_MAX 1024
#define DATA_HDR   12
#define PAYLOAD_MAX (PACKET_MAX-DATA_HDR)

//return the size of the file in bytes
static uint32_t fsize(FILE* f){
    fseek(f, 0, SEEK_END);
    long s = ftell(f);
    fseek(f, 0, SEEK_SET);
    return (uint32_t)s;
}

//return the pointer to the last filename separator
static const char* basename_simple(const char* p){
    const char* a = strrchr(p,'\\');
    const char* b = strrchr(p,'/');
    const char* c = (a && b) ? (a>b?a:b) : (a?a:b);
    return c ? c+1 : p;
}

int main(int argc, char** argv){
    //print usage if incorrect number of arguments is passed
    if(argc!=4){ fprintf(stderr,"Usage: %s <ip> <port> <file>\n", argv[0]); return 1; }

    //init the crc table
    crc32_init();

    //assign the program arguments to variables
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    const char* path = argv[3];

    //open file in read binary mode
    FILE* f = fopen(path,"rb");
    if(!f){return 1;}
    uint32_t size = fsize(f);

    //initialize the socket
    WSADATA wsa; 
    WSAStartup(MAKEWORD(2,2), &wsa);
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);

    //set socket timeout
    int timeout_ms = 1000;  
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout_ms, sizeof(timeout_ms));

    //init and assign the ip address structure
    struct sockaddr_in dst = {0};
    dst.sin_family = AF_INET;
    dst.sin_port = htons((unsigned short)port);
    inet_pton(AF_INET, ip, &dst.sin_addr);

    //send the metadata
    char line[512];
    snprintf(line,sizeof(line),"NAME=%s", basename_simple(path));
    sendto(s,line,(int)strlen(line),0,(struct sockaddr*)&dst,sizeof(dst));

    snprintf(line,sizeof(line),"SIZE=%u", (unsigned)size);
    sendto(s,line,(int)strlen(line),0,(struct sockaddr*)&dst,sizeof(dst));

    sendto(s,"START",5,0,(struct sockaddr*)&dst,sizeof(dst));

    //init the packet data structure
    uint8_t pkt[PACKET_MAX];
    memcpy(pkt,"DATA",4);

    //offset in the file
    uint32_t off=0;

    //packet sending loop
    while(off<size){
        //dont send data exceeding file size
        size_t want = PAYLOAD_MAX;
        if(off+want>size) {
            want = size-off;
        }

        //correct endiannes
        uint32_t netoff = htonl(off);
        memcpy(pkt+4,&netoff,4);

        //add data to packet
        fread(pkt+DATA_HDR,1,want,f);

        uint32_t crc = crc32(pkt+DATA_HDR,want);
        uint32_t netcrc = htonl(crc);
        memcpy(pkt+8,&netcrc,4);

        //retransmission loop
        for(;;){
            //send packet
            sendto(s,(char*)pkt,want+DATA_HDR,0,(struct sockaddr*)&dst,sizeof(dst));
            printf("Sent packet offset %u\n", off);

            char reply[128];
            struct sockaddr_in peer;
            int peerlen = sizeof(peer);

            int n = recvfrom(s,reply,sizeof(reply) - 1,0,(struct sockaddr *)&peer,&peerlen);
            if (n < 0) {
                int err = WSAGetLastError();
                if (err == WSAETIMEDOUT) {
                        //resend the same packet
                        continue;
                    } 
                else {
                    printf("recvfrom failed, WSA error = %d\n", err);
                    goto out;
                 }
            }
            reply[n] = 0;

            uint32_t ack_off;
            //break the resend loop if ACK received
            if (sscanf(reply, "ACK %u", &ack_off) == 1 && ack_off == off) {
                off += (uint32_t)want;
                break;
            }

            //repeat the sending if NACK received
            if (sscanf(reply, "NACK %u", &ack_off) == 1 && ack_off == off) {
                continue;
            }
        }
    }

    //send the stop metadata
    sendto(s,"STOP",4,0,(struct sockaddr*)&dst,sizeof(dst));

    
    //cleanups
    out:
    closesocket(s);
    WSACleanup();
    fclose(f);
    return 0;
}
