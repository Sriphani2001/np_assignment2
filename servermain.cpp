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

// Constants for OK and NOTOK messages to clients
const struct calcMessage NOTOK_MSG = {htons(2), htonl(2), htonl(17), htons(1), htons(0)};
const struct calcMessage OK_MSG = {htons(2), htonl(1), htonl(17), htons(1), htons(0)};

// Supported arithmetic operations
vector<string> arithmetic_ops = {"add", "sub", "mul", "div", "fadd", "fsub", "fmul", "fdiv"};

// Struct to store client information, combining status, address, and port
struct ClientData {
    int status;       // Tracks client status or timeout count
    string address;   // Client's IP address
    int port;         // Client's port number
};

// Map to store client information using client ID as the key
map<int, ClientData> clients;

int clientID = 1;  // Unique ID for each client

// Function declarations
int getArithIndex(const string& operand);
void *getInAddr(struct sockaddr *sa);
bool isValidProtocol(const char* msg);
void handleClientTimeout();
void sendCalcMessage(int sockfd, struct sockaddr *their_addr, socklen_t addr_len, const struct calcMessage& msg);

int main(int argc, char *argv[]) {
    // Initialize the calculation library
    initCalcLib();

    // Check for valid command-line arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <hostname:port>\n", argv[0]);
        exit(1);
    }

    printf("Server starting...\n");

    // Parse host and port from command line arguments
    char* destHost = strtok(argv[1], ":");
    char* destPort = strtok(NULL, ":");
    int port = atoi(destPort);

    printf("Host %s, and port %d.\n", destHost, port);

    struct addrinfo hints, *servinfo, *p;
    int sockfd, rv, numbytes;
    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len = sizeof(their_addr);

    // Zero out the hints struct for address resolution
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  // IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM; // Use UDP
    hints.ai_flags = AI_PASSIVE; // Any IP address

    // Resolve the server's address and port
    if ((rv = getaddrinfo(destHost, destPort, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // Try binding to the socket with available address info
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

    // If we could not bind to any address, exit
    if (p == NULL) {
        fprintf(stderr, "server: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo); // Free the address info linked list

    // Main loop to handle incoming messages
    struct calcProtocol proto;
    while (1) {
        memset(buf, 0, sizeof(buf));

        // Receive messages from clients
        if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0, (struct sockaddr*)&their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }

        // Convert client IP to a readable string format
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(their_addr.ss_family, getInAddr((struct sockaddr*)&their_addr), clientIP, sizeof(clientIP));
        int clientPort = ntohs(((struct sockaddr_in*)&their_addr)->sin_port);

        printf("Received message from %s:%d\n", clientIP, clientPort);  // Required one line per message

        string operand = randomType();  // Generate a random operation type

        // Handle initial protocol message
        if (numbytes == sizeof(calcMessage)) {
            if (!isValidProtocol(buf)) {  // Validate protocol message
                sendCalcMessage(sockfd, (struct sockaddr*)&their_addr, addr_len, NOTOK_MSG); // Send NOTOK if invalid
                printf("Invalid protocol message from %s:%d\n", clientIP, clientPort);
                continue;
            }

            // Store client information in a single map, using struct for better organization
            clients[clientID] = {0, clientIP, clientPort};

            printf("Client %d connected from %s:%d\n", clientID, clientIP, clientPort);

            // Prepare the calculation task for the client
            if (operand[0] == 'f') {  // Floating-point arithmetic task
                proto.arith = htonl(getArithIndex(operand));
                proto.flValue1 = randomFloat();
                proto.flValue2 = randomFloat();
                proto.inValue1 = htonl(0);
                proto.inValue2 = htonl(0);
                proto.inResult = htonl(0);
                proto.flResult = 0.0f;
            } else {  // Integer arithmetic task
                proto.arith = htonl(getArithIndex(operand));
                proto.inValue1 = htonl(randomInt());
                proto.inValue2 = htonl(randomInt());
                proto.inResult = htonl(0);
                proto.flValue1 = 0.0f;
                proto.flValue2 = 0.0f;
                proto.flResult = 0.0f;
            }

            // Set protocol message properties
            proto.type = htonl(1);
            proto.major_version = htonl(1);
            proto.minor_version = htonl(0);
            proto.id = htonl(clientID);

            clientID++;  // Increment client ID for the next client

            // Send the calculation task to the client
            if ((numbytes = sendto(sockfd, &proto, sizeof(proto), 0, (struct sockaddr*)&their_addr, addr_len)) == -1) {
                perror("sendto");
            } else {
                printf("Sent calculation to client %d\n", ntohl(proto.id));
            }
        }

        // Handle responses from clients with their calculation results
        else if (numbytes == sizeof(calcProtocol)) {
            memcpy(&proto, buf, sizeof(proto));

            int receivedID = ntohl(proto.id);
            if (clients.find(receivedID) == clients.end()) {  // Check if client ID exists
                sendCalcMessage(sockfd, (struct sockaddr*)&their_addr, addr_len, NOTOK_MSG); // Send NOTOK if unknown client
                printf("Unknown client ID %d\n", receivedID);
                continue;
            }

            // Extract values and check the correctness of the result
            int value1 = ntohl(proto.inValue1);
            int value2 = ntohl(proto.inValue2);
            int result = ntohl(proto.inResult);
            double fvalue1 = proto.flValue1;
            double fvalue2 = proto.flValue2;
            double fresult = proto.flResult;

            printf("Received calculation result from client %d\n", receivedID);

            // Determine if the client's result is correct
            bool isEqual = false;
            switch (ntohl(proto.arith)) {
                case 1: isEqual = (value1 + value2 == result); break;  // Integer addition
                case 2: isEqual = (value1 - value2 == result); break;  // Integer subtraction
                case 3: isEqual = (value1 * value2 == result); break;  // Integer multiplication
                case 4: isEqual = (value1 / value2 == result); break;  // Integer division
                case 5: isEqual = (fabs(fvalue1 + fvalue2 - fresult) < 0.0001); break;  // Float addition
                case 6: isEqual = (fabs(fvalue1 - fvalue2 - fresult) < 0.0001); break;  // Float subtraction
                case 7: isEqual = (fabs(fvalue1 * fvalue2 - fresult) < 0.0001); break;  // Float multiplication
                case 8: isEqual = (fabs(fvalue1 / fvalue2 - fresult) < 0.0001); break;  // Float division
            }

            // Send response based on whether the result is correct
            if (isEqual) {
                sendCalcMessage(sockfd, (struct sockaddr*)&their_addr, addr_len, OK_MSG);  // Send OK if result correct
                printf("Client %d calculation correct\n", receivedID);
            } else {
                sendCalcMessage(sockfd, (struct sockaddr*)&their_addr, addr_len, NOTOK_MSG);  // Send NOTOK if result incorrect
                printf("Client %d calculation incorrect\n", receivedID);
            }
            clients.erase(receivedID);  // Remove client from active list after processing
        }
    }

    close(sockfd);  // Close the socket
    return 0;
}

// Function to map arithmetic operation strings to index values
int getArithIndex(const string& operand) {
    static unordered_map<string, int> element_index = {
        {"add", 1}, {"sub", 2}, {"mul", 3}, {"div", 4},
        {"fadd", 5}, {"fsub", 6}, {"fmul", 7}, {"fdiv", 8}
    };
    return element_index[operand];  // Return the index of the operand
}

// Utility function to extract IP address from sockaddr structure
void *getInAddr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {  // IPv4 case
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);  // IPv6 case
}

// Function to validate the incoming protocol message
bool isValidProtocol(const char* msg) {
    struct calcMessage info;
    memcpy(&info, msg, sizeof(info));

    // Convert the fields from network to host byte order
    info.type = ntohs(info.type);
    info.message = ntohl(info.message);
    info.protocol = ntohs(info.protocol);
    info.major_version = ntohs(info.major_version);
    info.minor_version = ntohs(info.minor_version);

    // Check if the message conforms to the expected protocol
    return info.type == PROTOCOL_TYPE && info.message == PROTOCOL_MESSAGE &&
           info.protocol == 17 && info.major_version == PROTOCOL_VERSION_MAJOR &&
           info.minor_version == PROTOCOL_VERSION_MINOR;
}

// Function to handle client timeouts
// Simplified: Periodically checks client status and removes them after 10 loops
void handleClientTimeout() {
    for (auto it = clients.begin(); it != clients.end(); ) {
        it->second.status++;  // Increment timeout counter for each client
        if (it->second.status == 10) {  // If a client has timed out
            printf("Client %d timeout\n", it->first);
            it = clients.erase(it);  // Remove the timed-out client
        } else {
            ++it;
        }
    }
}

// Function to send a calcMessage to a client
void sendCalcMessage(int sockfd, struct sockaddr *their_addr, socklen_t addr_len, const struct calcMessage& msg) {
    if (sendto(sockfd, &msg, sizeof(msg), 0, their_addr, addr_len) == -1) {
        perror("sendto");
    }
}
