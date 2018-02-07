#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <unistd.h>

bool LOG = false;

void logmsg(std::string msg) {
    if (LOG) std::cout << msg << std::endl;
}

void handlepacket(int sockfd) {
    struct sockaddr_in cliaddr;
    memset(&cliaddr, 0, sizeof(cliaddr));
    socklen_t cliaddrlen = sizeof(cliaddr);
    char buffer[2];
    int n = recvfrom(sockfd, &buffer, 2, 0, (struct sockaddr *) &cliaddr, &cliaddrlen);
    if (n == -1) {
        perror("recvfrom");
        return;
    } else if (n != 2) {
        logmsg("expected 2 bytes, got " + std::to_string(n));
        return;
    }

    int opcode = (buffer[0] << 8) | buffer[1];

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return;
    }
    // exit parent, child does all the work
    if (pid > 0) {
        logmsg("parent exiting");
        return;
    }
    logmsg("child handling packet");

    if (opcode == 1) {
        logmsg("RRQ received");
        // rrq();
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
