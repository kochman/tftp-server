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

void concatstr(char* first, char* second) {
    for (int i = 0; i < strlen(second); ++i) {
        first[i] = second[i];
    }
}

// handle WRQ
void wrq(struct sockaddr_in* cliaddr, std::string filename) {
    logmsg("WRQ client port " + std::to_string(cliaddr->sin_port) + " filename " + filename);

    // set up another socket for this transfer
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    // set socket timeout to 1 sec
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*) &tv, sizeof(tv));

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

    socklen_t cliaddrlen = sizeof(*cliaddr);

    // https://stackoverflow.com/a/4047837
    if (getsockname(sockfd, (struct sockaddr *)&servaddr, &servaddrlen) == -1) {
        perror("getsockname");
        exit(EXIT_FAILURE);
    }

    // try to open the file
    FILE* file = fopen(filename.c_str(), "wx");
    if (file == NULL) {
        // reply with error if exists
        logmsg("file already exists");
        // char msg[128];
        char msg[32] = { '\0' };
        msg[0] = 0; // ERROR opcode
        msg[1] = 5;
        msg[2] = 0; // File not found error value
        msg[3] = 6;

        char humanmsg[] = "File already exists.";
        concatstr(&(msg[4]), humanmsg);

        int sendn = sendto(sockfd, &msg, 4 + strlen(humanmsg) + 1, 0, (struct sockaddr *) cliaddr, cliaddrlen);
        if (sendn == -1) {
            perror("sendto");
            exit(EXIT_FAILURE);
        }
        return;
    }

    bool done = false;
    short int blocknum = -1;
    // start receiving data
    while (true) {
	    // create message
	    ++blocknum;
	    int msglen = 4; // opcode + block number + bytes read
	    char msg[msglen];

	    msg[0] = 0; // ACK opcode
	    msg[1] = 4; // ACK opcode
	    msg[2] = blocknum >> 8;
	    msg[3] = blocknum;

	    // loop until we receive data or time out
	    char buffer[516];
	    int recvn;
	    while (true) {
		    // send ack
		    int sendn = sendto(sockfd, &msg, msglen, 0, (struct sockaddr *) cliaddr, cliaddrlen);
		    if (sendn == -1) {
			    perror("sendto");
			    exit(EXIT_FAILURE);
		    }
		    if (done) goto done;

		    // wait for data
receive:
		    recvn = recvfrom(sockfd, &buffer, 516, 0, (struct sockaddr *) cliaddr, &cliaddrlen);
		    if (recvn == -1) {
			    if (errno == EAGAIN) {
				    goto receive;
			    }
			    perror("recvfrom");
			    exit(EXIT_FAILURE);
		    }

		    if (recvn < 512) {
			    // end of file
			    done = true;
		    }

		    break;
	    }
	    // write data
	    int bufn = fwrite(&buffer[4], sizeof(char), recvn - 4, file);
	    if (bufn == -1) {
		    perror("fwrite");
		    exit(EXIT_FAILURE);
	    }
    }
done:

    fclose(file);
}

// handle RRQ
void rrq(struct sockaddr_in* cliaddr, std::string filename) {
    logmsg("RRQ client port " + std::to_string(cliaddr->sin_port) + " filename " + filename);

    // set up another socket for this transfer
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    // set socket timeout to 1 sec
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*) &tv, sizeof(tv));

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

    socklen_t cliaddrlen = sizeof(*cliaddr);

    // https://stackoverflow.com/a/4047837
    if (getsockname(sockfd, (struct sockaddr *)&servaddr, &servaddrlen) == -1) {
        perror("getsockname");
        exit(EXIT_FAILURE);
    }

    // try to open the file
    FILE* file = fopen(filename.c_str(), "r");
    if (file == NULL) {
        // reply with error if it doesn't exist
        logmsg("file not found");
        // char msg[128];
        char msg[32] = { '\0' };
        msg[0] = 0; // ERROR opcode
        msg[1] = 5;
        msg[2] = 0; // File not found error value
        msg[3] = 1;

        char humanmsg[] = "File not found.";
        concatstr(&(msg[4]), humanmsg);

        int sendn = sendto(sockfd, &msg, 4 + strlen(humanmsg) + 1, 0, (struct sockaddr *) cliaddr, cliaddrlen);
        if (sendn == -1) {
            perror("sendto");
            exit(EXIT_FAILURE);
        }
        return;
    }

    short int blocknum = 0;
    // start sending data
    while (true) {
        // read data
        char buffer[512];
        int bufn = fread(buffer, sizeof(char), 512, file);

        // create message
        ++blocknum;
        int msglen = 2 + 2 + bufn; // opcode + block number + bytes read
        char msg[msglen];

        msg[0] = 0; // DATA opcode
        msg[1] = 3; // DATA opcode
        msg[2] = blocknum >> 8;
        msg[3] = blocknum;

        for (int i = 0; i < bufn; ++i) {
            msg[4+i] = buffer[i];
        }

        // loop until we receive an ack or time out
        while (true) {
            int sendn = sendto(sockfd, &msg, msglen, 0, (struct sockaddr *) cliaddr, cliaddrlen);
            if (sendn == -1) {
                perror("sendto");
                exit(EXIT_FAILURE);
            }


            // wait for ack
            char ackbuffer[4];
            int ackn = recvfrom(sockfd, &ackbuffer, 4, 0, (struct sockaddr *) cliaddr, &cliaddrlen);
            if (ackn == -1) {
                perror("recvfrom");
                exit(EXIT_FAILURE);
            } else if (ackn < 4) {
                // 4 is from 2-byte opcode plus 2-byte block number
                logmsg("expected 4 bytes, got " + std::to_string(ackn));
                exit(EXIT_FAILURE);
            }
            break;
        }


        if (bufn < 512) {
            // end of file
            break;
        }
    }

    fclose(file);
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
        rrq(&cliaddr, filename);
    } else if (opcode == 2) {
        logmsg("WRQ received");
        wrq(&cliaddr, filename);
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
    //servaddr.sin_port        = htons(7000); // for testing
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
}


int main(int argc, char** argv) {
    // if we have an argument "log", enable message logging
    if (argc == 2 && argv[1] == std::string("--log")) {
        LOG = true;
    }
    logmsg("logging enabled");

    serve();

    return EXIT_SUCCESS;
}
