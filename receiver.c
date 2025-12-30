
//Usage: receiver.exe <port>

#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PACKET_MAX 1024
#define DATA_HDR   8

int main(int argc, char** argv){
    if(argc!=2){ fprintf(stderr,"Usage: %s <port>\n", argv[0]); return 1; }
    int port = atoi(argv[1]);

    //socket init
    WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);


    struct sockaddr_in local = {0};
    local.sin_family = AF_INET;
    local.sin_port = htons((unsigned short)port);
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(s,(struct sockaddr*)&local,sizeof(local));

    char filename[256] = "received.bin";
    uint32_t expected_size = 0;
    FILE* out = NULL;

    uint8_t buf[PACKET_MAX];
    //declare the sender ip address
    struct sockaddr_in peer; int peerlen = sizeof(peer);

    for(;;){
        int n = recvfrom(s,(char*)buf,PACKET_MAX,0,(struct sockaddr*)&peer,&peerlen);
        if(n<=0) continue;

        //check if packet is a control message
        if(n<DATA_HDR || memcmp(buf,"DATA",4)!=0){
            char txt[PACKET_MAX+1];
            memcpy(txt,buf,n); 
            txt[n]=0;

            if(!strncmp(txt,"NAME=",5)){ strncpy(filename,txt+5,sizeof(filename)-1); filename[sizeof(filename)-1]=0; }
            else if(!strncmp(txt,"SIZE=",5)){ expected_size = (uint32_t)strtoul(txt+5,NULL,10); }
            else if(!strcmp(txt,"START")){
                out = fopen("received.jpg","wb+");
                if(!out){ perror("fopen"); break; }
                printf("Receiving %s (%u bytes)\n", filename, (unsigned)expected_size);
            }
            else if(!strcmp(txt,"STOP")){
                printf("STOP\n");
                break;
            }
            continue;
        }

        // DATA packet: "DATA" + offset(uint32) + payload
        uint32_t netoff; memcpy(&netoff, buf+4, 4);
        uint32_t off = ntohl(netoff);

        int payload = n - DATA_HDR;
        if(out){
            fseek(out, (long)off, SEEK_SET);
            fwrite(buf+DATA_HDR, 1, payload, out);
        }
    }

    if(out) fclose(out);
    closesocket(s);
    WSACleanup();
    return 0;
}
