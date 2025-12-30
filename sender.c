//Usage: sender.exe <ip> <port> <file>

#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PACKET_MAX 1024
#define DATA_HDR   8                 // "DATA"(4) + offset(4)
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
        fread(pkt+8,1,want,f);

        //send packet
        sendto(s,(char*)pkt,(int)(8+want),0,(struct sockaddr*)&dst,sizeof(dst));

        //move the offset
        off += (uint32_t)want;
    }

    //send the stop metadata
    sendto(s,"STOP",4,0,(struct sockaddr*)&dst,sizeof(dst));

    //cleanups
    closesocket(s);
    WSACleanup();
    fclose(f);
    return 0;
}
