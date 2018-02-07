#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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
    };

    // read and print port that we were assigned
    // https://stackoverflow.com/a/4047837
    if (getsockname(sockfd, (struct sockaddr *)&servaddr, &servaddrlen) == -1) {
        perror("getsockname");
        exit(EXIT_FAILURE);
    }
    printf("%d\n", ntohs(servaddr.sin_port));

    // socket();
    // bind();
    // listen();
    // accept();
}

int main(void) {
    serve();
    return EXIT_SUCCESS;
}
