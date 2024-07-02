#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <string>
#include <iostream>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <map>
#include <calcLib.h>
#include "protocol.h"

using namespace std;

#define MAXBUFLEN 100
#define PROTOCOL_TYPE 22
#define PROTOCOL_MESSAGE 0
#define PROTOCOL_VERSION_MAJOR 1
#define PROTOCOL_VERSION_MINOR 0

const struct calcMessage PROTOCOL = {htons(PROTOCOL_TYPE), htonl(PROTOCOL_MESSAGE), htonl(17), htons(PROTOCOL_VERSION_MAJOR), htons(PROTOCOL_VERSION_MINOR)};
const struct calcMessage NOTOK_MSG = {htons(2), htonl(2), htonl(17), htons(1), htons(0)};
const struct calcMessage OK_MSG = {htons(2), htonl(1), htonl(17), htons(1), htons(0)};

vector<string> arithmetic_ops = {"add", "sub", "mul", "div", "fadd", "fsub", "fmul", "fdiv"};
map<int, int> clientStatus;
map<int, string> clientAddresses;
map<int, int> clientPorts;

int clientID = 1;
int loopCount = 0;

typedef struct {
    char* addr;
    int port;
} ClientInfo;

map<int, ClientInfo> clients;

int getArithIndex(const string& operand);
void *getInAddr(struct sockaddr *sa);
bool isValidProtocol(const char* msg);
void handleClientTimeout(int signum);
void sendCalcMessage(int sockfd, struct sockaddr *their_addr, socklen_t addr_len, const struct calcMessage& msg);

int main(int argc, char *argv[]) {
    initCalcLib();

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <hostname:port>\n", argv[0]);
        exit(1);
    }

    char* destHost = strtok(argv[1], ":");
    char* destPort = strtok(NULL, ":");

    struct addrinfo hints, *servinfo, *p;
    int sockfd, rv, numbytes;
    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len = sizeof(their_addr);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(destHost, destPort, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo);

    struct itimerval alarmTime;
    alarmTime.it_interval.tv_sec = 1;
    alarmTime.it_interval.tv_usec = 0;
    alarmTime.it_value.tv_sec = 1;
    alarmTime.it_value.tv_usec = 0;

    signal(SIGALRM, handleClientTimeout);
    setitimer(ITIMER_REAL, &alarmTime, NULL);

    struct calcProtocol proto;
    while (1) {
        memset(buf, 0, sizeof(buf));

        if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0, (struct sockaddr*)&their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(their_addr.ss_family, getInAddr((struct sockaddr*)&their_addr), clientIP, sizeof(clientIP));
        int clientPort = ntohs(((struct sockaddr_in*)&their_addr)->sin_port);

        string operand = randomType();

        if (numbytes == sizeof(calcMessage)) {
            if (!isValidProtocol(buf)) {
                sendCalcMessage(sockfd, (struct sockaddr*)&their_addr, addr_len, NOTOK_MSG);
                continue;
            }

            clientStatus[clientID] = 0;
            clientAddresses[clientID] = clientIP;
            clientPorts[clientID] = clientPort;

            if (operand[0] == 'f') {
                proto.arith = htonl(getArithIndex(operand));
                proto.flValue1 = randomFloat();
                proto.flValue2 = randomFloat();
                proto.inValue1 = htonl(0);
                proto.inValue2 = htonl(0);
                proto.inResult = htonl(0);
                proto.flResult = 0.0f;
            } else {
                proto.arith = htonl(getArithIndex(operand));
                proto.inValue1 = htonl(randomInt());
                proto.inValue2 = htonl(randomInt());
                proto.inResult = htonl(0);
                proto.flValue1 = 0.0f;
                proto.flValue2 = 0.0f;
                proto.flResult = 0.0f;
            }

            proto.type = htonl(1);
            proto.major_version = htonl(1);
            proto.minor_version = htonl(0);
            proto.id = htonl(clientID);

            clientID++;

            if ((numbytes = sendto(sockfd, &proto, sizeof(proto), 0, (struct sockaddr*)&their_addr, addr_len)) == -1) {
                perror("sendto");
                printf("Sent calculation to client %d\n", ntohl(proto.id));
            }
        } else if (numbytes == sizeof(calcProtocol)) {
            memcpy(&proto, buf, sizeof(proto));

            int receivedID = ntohl(proto.id);
            if (clientStatus.find(receivedID) == clientStatus.end()) {
                sendCalcMessage(sockfd, (struct sockaddr*)&their_addr, addr_len, NOTOK_MSG);
                continue;
            }

            int value1 = ntohl(proto.inValue1);
            int value2 = ntohl(proto.inValue2);
            int result = ntohl(proto.inResult);
            double fvalue1 = proto.flValue1;
            double fvalue2 = proto.flValue2;
            double fresult = proto.flResult;

            bool isEqual = false;

            switch (ntohl(proto.arith)) {
                case 1: isEqual = (value1 + value2 == result); break;
                case 2: isEqual = (value1 - value2 == result); break;
                case 3: isEqual = (value1 * value2 == result); break;
                case 4: isEqual = (value1 / value2 == result); break;
                case 5: isEqual = (fabs(fvalue1 + fvalue2 - fresult) < 0.0001); break;
                case 6: isEqual = (fabs(fvalue1 - fvalue2 - fresult) < 0.0001); break;
                case 7: isEqual = (fabs(fvalue1 * fvalue2 - fresult) < 0.0001); break;
                case 8: isEqual = (fabs(fvalue1 / fvalue2 - fresult) < 0.0001); break;
            }

            if (isEqual) {
                sendCalcMessage(sockfd, (struct sockaddr*)&their_addr, addr_len, OK_MSG);
                printf("Client %d calculation correct\n", receivedID);
            } else {
                sendCalcMessage(sockfd, (struct sockaddr*)&their_addr, addr_len, NOTOK_MSG);
                printf("Client %d calculation incorrect\n", receivedID);
            }
            clientStatus.erase(receivedID);
        }
    }

    close(sockfd);
    return 0;
}

int getArithIndex(const string& operand) {
    unordered_map<string, int> element_index;
    for (int i = 0; i < arithmetic_ops.size(); i++) {
        element_index[arithmetic_ops[i]] = i + 1;
    }
    return element_index[operand];
}

void *getInAddr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

bool isValidProtocol(const char* msg) {
    struct calcMessage info;
    memcpy(&info, msg, sizeof(info));

    info.type = ntohs(info.type);
    info.message = ntohl(info.message);
    info.protocol = ntohs(info.protocol);
    info.major_version = ntohs(info.major_version);
    info.minor_version = ntohs(info.minor_version);

    return info.type == PROTOCOL_TYPE && info.message == PROTOCOL_MESSAGE &&
           info.protocol == 17 && info.major_version == PROTOCOL_VERSION_MAJOR &&
           info.minor_version == PROTOCOL_VERSION_MINOR;
}

void handleClientTimeout(int signum) {
    for (auto it = clientStatus.begin(); it != clientStatus.end(); ) {
        it->second++;
        if (it->second == 10) {
            printf("Client %d timeout\n", it->first);
            it = clientStatus.erase(it);
        } else {
            ++it;
        }
    }
    loopCount++;
}

void sendCalcMessage(int sockfd, struct sockaddr *their_addr, socklen_t addr_len, const struct calcMessage& msg) {
    if (sendto(sockfd, &msg, sizeof(msg), 0, their_addr, addr_len) == -1) {
        perror("sendto");
    }
}
