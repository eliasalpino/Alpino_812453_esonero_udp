#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef int socklen_t;
#else
  #include <unistd.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
#endif

#include "protocol.h"


int platform_init(void) {
#ifdef _WIN32
    WSADATA wsa;
    return (WSAStartup(MAKEWORD(2,2), &wsa) == 0) ? 0 : -1;
#else
    return 0;
#endif
}

void platform_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}


uint32_t float_to_net(float f) {
    uint32_t u;
    memcpy(&u, &f, sizeof(u));
    return htonl(u);
}

float net_to_float(uint32_t v) {
    v = ntohl(v);
    float f;
    memcpy(&f, &v, sizeof(f));
    return f;
}

static float rand_range(float a, float b) {
    return a + (float)rand() / (float)RAND_MAX * (b - a);
}

float get_temperature(void) { return rand_range(-10.0f, 40.0f); }
float get_humidity(void)    { return rand_range(20.0f, 100.0f); }
float get_wind(void)        { return rand_range(0.0f, 100.0f); }
float get_pressure(void)    { return rand_range(950.0f, 1050.0f); }


static const char* supported_cities[] = {
    "Bari","Roma","Milano","Napoli","Torino",
    "Palermo","Genova","Bologna","Firenze","Venezia"
};

static const int supported_count =
    sizeof(supported_cities) / sizeof(supported_cities[0]);

int city_supported(const char* city) {
    for (int i = 0; i < supported_count; i++) {
        if (STRCASECMP(city, supported_cities[i]) == 0)
            return 1;
    }
    return 0;
}


int city_is_valid(const char* city) {
    for (int i = 0; city[i] != '\0'; i++) {
        char c = city[i];

        if (c == '\t')
            return 0;

        if (!((c >= 'A' && c <= 'Z') ||
              (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') ||
              (c == ' ')))
            return 0;
    }
    return 1;
}



int main(int argc, char** argv) {
    int port = DEFAULT_SERVER_PORT;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else {
            fprintf(stderr, "Uso: %s [-p port]\n", argv[0]);
            return 1;
        }
    }

    if (platform_init() != 0) {
        fprintf(stderr, "Errore inizializzazione socket\n");
        return 1;
    }

    srand((unsigned)time(NULL));

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        platform_cleanup();
        return 1;
    }

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = INADDR_ANY;
    srv.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&srv, sizeof(srv)) < 0) {
        perror("bind");
        platform_cleanup();
        return 1;
    }

    printf("[SERVER] In ascolto sulla porta %d\n", port);

    while (1) {
        char buffer[BUFFER_SIZE];
        struct sockaddr_in client;
        socklen_t len = sizeof(client);

        ssize_t n = recvfrom(sock, buffer, sizeof(buffer), 0,
                             (struct sockaddr*)&client, &len);
        if (n <= 0)
            continue;


        weather_request_t req;
        int offset = 0;

        memcpy(&req.type, buffer + offset, sizeof(char));
        offset += sizeof(char);

        memcpy(req.city, buffer + offset, sizeof(req.city));


        char host[NI_MAXHOST];
        char ip[NI_MAXSERV];

        getnameinfo((struct sockaddr*)&client, len,
                    host, sizeof(host),
                    ip, sizeof(ip),
                    NI_NUMERICHOST);

        printf("Richiesta ricevuta da %s (ip %s): type='%c', city='%s'\n",
               host, ip, req.type, req.city);

         weather_response_t resp;

        if (!(req.type=='t' || req.type=='h' ||
              req.type=='w' || req.type=='p')) {
            resp.status = STATUS_INVALID_REQUEST;
        }
        else if (!city_is_valid(req.city)) {
            resp.status = STATUS_INVALID_REQUEST;
        }
        else if (!city_supported(req.city)) {
            resp.status = STATUS_CITY_NOT_FOUND;
        }
        else {
            resp.status = STATUS_SUCCESS;
            resp.type = req.type;

            switch (req.type) {
                case 't': resp.value = get_temperature(); break;
                case 'h': resp.value = get_humidity();    break;
                case 'w': resp.value = get_wind();        break;
                case 'p': resp.value = get_pressure();    break;
            }
        }


        char out[BUFFER_SIZE];
        offset = 0;

        uint32_t net_status = htonl(resp.status);
        memcpy(out + offset, &net_status, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        memcpy(out + offset, &req.type, sizeof(char));
        offset += sizeof(char);

        uint32_t netf = float_to_net(resp.value);
        memcpy(out + offset, &netf, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        sendto(sock, out, offset, 0,
               (struct sockaddr*)&client, len);
    }

    platform_cleanup();
    return 0;
}

