#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <unistd.h>

bool LOG = false;
unsigned int BUFFER_SIZE = 4096; // BIG ASSUMPTION: no datagram is larger than this

void logmsg(std::string msg) {
    if (LOG) std::cout << msg << std::endl;
}

// handle RRQ
void rrq(int cliport, std::string filename) {
    logmsg("RRQ client port " + std::to_string(cliport) + " filename " + filename);
}

void handlepacket(int sockfd) {
    struct sockaddr_in cliaddr;
    memset(&cliaddr, 0, sizeof(cliaddr));
    socklen_t cliaddrlen = sizeof(cliaddr);
    char buffer[BUFFER_SIZE];
    int n = recvfrom(sockfd, &buffer, BUFFER_SIZE, 0, (struct sockaddr *) &cliaddr, &cliaddrlen);
    if (n == -1) {
        perror("recvfrom");
        return;
    } else if (n < 4) {
        // 4 is from 2-byte opcode plus at least one byte filename and NULL char
        logmsg("expected at least 4 bytes, got " + std::to_string(n));
        return;
    } else if (n == BUFFER_SIZE) {
        logmsg("buffer full");
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return;
    }
    // exit parent, child does all the work
    if (pid > 0) {
        logmsg("parent returning");
        return;
    }
    logmsg("child handling packet");

    int opcode = (buffer[0] << 8) | buffer[1];
    int cliport = ntohs(cliaddr.sin_port);

    // read filename out of buffer
    std::string filename;
    for (unsigned int i = 2; i < BUFFER_SIZE; ++i) {
        char c = buffer[i];
        if (c == '\0') {
            break;
        }
        filename += c;
    }

    if (opcode == 1) {
        logmsg("RRQ received");
        rrq(cliport, filename);
    } else if (opcode == 2) {
        logmsg("WRQ received");
        // wrq();
    } else {
        logmsg("unexpected opcode " + std::to_string(opcode));
    }
}

void serve() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(0); // any port; we'll read it later
    socklen_t servaddrlen = sizeof(servaddr);

    if (bind(sockfd, (struct sockaddr *) &servaddr, servaddrlen) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // read and print port that we were assigned
    // https://stackoverflow.com/a/4047837
    if (getsockname(sockfd, (struct sockaddr *)&servaddr, &servaddrlen) == -1) {
        perror("getsockname");
        exit(EXIT_FAILURE);
    }
    printf("%d\n", ntohs(servaddr.sin_port));

    // start the server loop
    logmsg("entering server loop");
    while (true) {
        handlepacket(sockfd);
    }

    // socket();
    // bind();
    // listen();
    // accept();
}


int main(int argc, char** argv) {
    // if we have an argument "log", enable message logging
    if (argc == 2 && argv[1] == std::string("log")) {
        LOG = true;
    }
    logmsg("logging enabled");

    serve();

    return EXIT_SUCCESS;
}
