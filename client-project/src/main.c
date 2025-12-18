#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
#else
  #include <unistd.h>
  #include <arpa/inet.h>
  #include <netdb.h>
#endif

#include "protocol.h"

int platform_init(void) {
#ifdef _WIN32
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2,2), &wsa);
#else
    return 0;
#endif
}

void platform_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

int main(int argc, char* argv[]) {
    char server[128] = "localhost";
    int port = DEFAULT_SERVER_PORT;
    char type = 0;
    char city[64] = {0};

    for (int i=1;i<argc;i++) {
        if (!strcmp(argv[i],"-s") && i+1<argc)
            strcpy(server, argv[++i]);
        else if (!strcmp(argv[i],"-p") && i+1<argc)
            port = atoi(argv[++i]);
        else if (!strcmp(argv[i],"-r") && i+1<argc) {
            char* space = strchr(argv[++i],' ');
            if (!space || space==argv[i] || space-argv[i]!=1) {
                fprintf(stderr,"Errore richiesta\n");
                return 1;
            }
            type = argv[i][0];
            strncpy(city, space+1, 63);
        }
    }

    if (!type) {
        fprintf(stderr,"Richiesta mancante\n");
        return 1;
    }

    if (platform_init()!=0) return 1;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    struct addrinfo hints={}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    getaddrinfo(server, NULL, &hints, &res);
    struct sockaddr_in srv = *(struct sockaddr_in*)res->ai_addr;
    srv.sin_port = htons(port);

    char buffer[sizeof(char)+64];
    int offset=0;
    memcpy(buffer+offset,&type,1);
    offset+=1;
    memcpy(buffer+offset,city,64);

    sendto(sock,buffer,sizeof(buffer),0,
           (struct sockaddr*)&srv,sizeof(srv));

    char respbuf[512];
    socklen_t len = sizeof(srv);
    recvfrom(sock,respbuf,sizeof(respbuf),0,
             (struct sockaddr*)&srv,&len);

    weather_response_t resp;
    offset=0;
    uint32_t net_status;
    memcpy(&net_status,respbuf,sizeof(uint32_t));
    resp.status = ntohl(net_status);
    offset+=sizeof(uint32_t);

    memcpy(&resp.type,respbuf+offset,1);
    offset+=1;

    uint32_t temp;
    memcpy(&temp,respbuf+offset,sizeof(float));
    temp = ntohl(temp);
    memcpy(&resp.value,&temp,sizeof(float));

    char host[NI_MAXHOST], ip[NI_MAXSERV];
    getnameinfo((struct sockaddr*)&srv,len,
                host,sizeof(host),
                ip,sizeof(ip),
                NI_NUMERICHOST);

    if (resp.status==STATUS_SUCCESS) {
        const char* label =
            resp.type=='t'?"Temperatura":
            resp.type=='h'?"Umidità":
            resp.type=='w'?"Vento":"Pressione";

        const char* unit =
            resp.type=='t'?"°C":
            resp.type=='h'?"%":
            resp.type=='w'?" km/h":" hPa";

        printf("Ricevuto risultato dal server %s (ip %s). %s: %s = %.1f%s\n",
               host,ip,city,label,resp.value,unit);
    } else if (resp.status==STATUS_CITY_NOT_FOUND) {
        printf("Ricevuto risultato dal server %s (ip %s). Città non disponibile\n",
               host,ip);
    } else {
        printf("Ricevuto risultato dal server %s (ip %s). Richiesta non valida\n",
               host,ip);
    }

#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif

    platform_cleanup();
    return 0;
}
