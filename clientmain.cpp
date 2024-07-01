#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <map>

using namespace std;

#define NOCL 10000
int loopcount = 0;
bool flag = false;
int main(int argc, char *argv[]){
    int sockfd[NOCL]; // Max 100 clients...
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes, numread;
    char buffer[1450];

    int DEBUGv;
    int timeout_in_seconds = 1;

    DEBUGv = 0;

    struct timeval ct1, ct2;

    char **tokens;
    FILE *fptr;

    // Check the number of arguments
    if (argc < 4 || argc > 5) {
        fprintf(stderr, "usage: %s <HOSTNAME:PORT> <CLIENTs> <prob> <resultfile> [debug]\n", argv[0]);
        if (fptr != NULL)
            fprintf(fptr, "ERROR OCCURED");
        exit(1);
    }

    // Split input to each parameter.
    char *str = argv[1];
    char *Desthost = "";
    char *Destport = "";
    char *colon_pos = strrchr(str, ':');
    if (colon_pos != NULL) {
        *colon_pos = '\0';
        Desthost = str;
        Destport = colon_pos + 1;
    }
    int noClients = atoi(argv[2]);
    int prob = atoi(argv[3]);

    // Check the number of clients that connected.
    if (noClients >= NOCL) {
        printf("Too many clients..Max is %d.\n", NOCL);
        printf("If you want more, change NOCL and recompile.\n");
        exit(1);
    }

    printf("Probability = %d \n", prob);

    if (argc == 5) {
        printf("DEBUG ON\n");
        DEBUGv = 1;
    } else {
        printf("DEBUG OFF\n");
        DEBUGv = 0;
    }

    // Store information
    socklen_t addr_len;
    struct sockaddr_storage their_addr;
    addr_len = sizeof(their_addr);

    // Write information into file
    printf("Connecting %d clients %s on port=%s \n", noClients, Desthost, Destport);
    printf("Saving to %s \n", argv[4]);
    fptr = fopen(argv[4], "w+");
    if (fptr == NULL) {
        printf("Can't write to %s, %s.\n", argv[4], strerror(errno));
    }

    // Create a socket descriptor for the number of clients
    memset(&hints, 0, sizeof hints);
    memset(&buffer, 0, sizeof(buffer));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(Desthost, Destport, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // Loop through all the results and make a socket
    for (int i = 0; i < noClients; i++) {
        for (p = servinfo; p != NULL; p = p->ai_next) {
            if ((sockfd[i] = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                perror("socket");
                continue;
            }
            break;
        }
        if (p == NULL) {
            fprintf(stderr, "%s: failed to create socket(%d)\n", argv[0], i);
            return 2;
        }
    }

    int s;
    struct sockaddr_in sa;
    socklen_t sa_len = sizeof(sa);

    char localIP[32];
    const char *myAdd;
    memset(&localIP, 0, sizeof(localIP));

    int bobsMother = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (bobsMother == -1) {
        perror("Socket can't do nr2");
    } else {
        rv = connect(bobsMother, p->ai_addr, p->ai_addrlen);
        if (rv == -1) {
            perror("Can't connect to socket..");
        } else {
            if ((s = getsockname(bobsMother, (struct sockaddr *) &sa, &sa_len) == -1)) {
                perror("getsockname failed.");
            } else {
                myAdd = inet_ntop(sa.sin_family, &sa.sin_addr, localIP, sizeof(localIP));
            }
        }
    }

    close(bobsMother);
    // While starting, send the message to server
    typedef struct calcMessage cMessage;
    cMessage CM;
    CM.type = htons(22);
    CM.message = htons(0);
    CM.major_version = htons(1);
    CM.minor_version = htons(0);
    CM.protocol = htons(17);
    typedef struct calcProtocol cProtocol;

    cProtocol *ptrCM;
    cProtocol CP[NOCL];
    cMessage CMs[NOCL];

    int droppedClient[NOCL];

    for (int i = 0; i < NOCL; i++) {
        droppedClient[i] = 0;
    }

    int myRand;
    int dropped = 0;

    int OKresults = 0;
    int ERRORresults = 0;
    int ERRORsignup = 0;

    printf("Sending Requests.\n");

    int max_retries = 3; // Maximum retries
    int retry_count_send[noClients] = {0};
    map<int, int> records;

    for (int i = 0; i < noClients; i++) {
        records.insert(pair<int, int>(i + 1, 0));
    }

    while (1) {
        for (int i = 0; i < noClients; i++) {
            std::map<int, int>::iterator it = records.find(i + 1);
            if (it != records.end()) {
                if (it->second == 1)
                    continue;
            } else {
                printf("some error in find client.\n");
            }

            if ((numbytes = sendto(sockfd[i], &CM, sizeof(CM), 0, p->ai_addr, p->ai_addrlen)) == -1) {
                perror("talker: sendto");
                if (fptr != NULL)
                    fprintf(fptr, "ERROR OCCURED");
                exit(1);
            } else {
                if ((s = getsockname(sockfd[i], (struct sockaddr *) &sa, &sa_len) == -1)) {
                    perror("getsockname failed.");
                } else {
                    printf("Client[%d] (%s:%d) registered, sent %d bytes\n", i, localIP, ntohs(sa.sin_port), numbytes);
                }
            }
        }

        printf("\n-----RESPONSES to calcMessage (registration) ----- \n");

        for (int i = 0; i < noClients; i++) {
            struct timeval timeout;
            timeout.tv_sec = 2;  // Timeout time
            timeout.tv_usec = 0;

            if (setsockopt(sockfd[i], SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
                perror("setsockopt for receive timeout failed");
                close(sockfd[i]);
                return -1;
            }

            numbytes = recvfrom(sockfd[i], buffer, sizeof(buffer), 0, (struct sockaddr *)&their_addr, &addr_len);
            if (numbytes == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    if (retry_count_send[i] < max_retries) {
                        printf("Receive timeout occurred. Client[%d] Need to retransmit.\n", i);
                        retry_count_send[i]++;
                        continue;
                    } else {
                        printf("Max retries exceeded for Client[%d]. Aborting.\n", i);
                        droppedClient[i] = -1;
                        dropped++;
                        perror("recvfrom");
                        if (fptr != NULL)
                            fprintf(fptr, "ERROR OCCURED");
                    }
                }
            } else {
                printf("Client[%d] received %d bytes \n", i, numbytes);
                records[i + 1] = 1;
                printf("Client[%d] ", i);
            }

            if (numbytes == sizeof(cProtocol)) {
                memcpy(&CP[i], buffer, sizeof(cProtocol));
                printf("| calcProtocol type=%d version=%d.%d id=%d arith=%d ", ntohs(CP[i].type), ntohs(CP[i].major_version), ntohs(CP[i].minor_version), ntohl(CP[i].id), ntohl(CP[i].arith));
                switch (ntohl(CP[i].arith)) {
                    case 1:
                        printf("op=Addition ");
                        break;
                    case 2:
                        printf("op=Subtraction ");
                        break;
                    case 3:
                        printf("op=Multiplication ");
                        break;
                    case 4:
                        printf("op=Division ");
                        break;
                }
                printf("num1=%d num2=%d \n", ntohl(CP[i].num1), ntohl(CP[i].num2));
            } else {
                printf("Client[%d] received incorrect sized message\n", i);
            }
        }
        break;
    }
    // Continue processing the responses, handling errors, and retry logic.
}
