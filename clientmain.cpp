#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <iostream>
#include <inttypes.h>
#include "protocol.h"

using namespace std;

#define DEBUG

void printServerMetadata(addrinfo *sinfo) {
    char ipstr[INET6_ADDRSTRLEN];
    for (addrinfo *p = sinfo; p != NULL; p = p->ai_next) {
        void *addr;
        string ipver;

        if (p->ai_family == AF_INET) {
            sockaddr_in *ipv4 = (sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = "IPv4";
        } else {
            sockaddr_in6 *ipv6 = (sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = "IPv6";
        }

        inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
        cout << "  " << ipver << ": " << ipstr << "\n";
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " ip:port\n";
        return 1;
    }

    char *Desthost = strtok(argv[1], ":");
    char *Destport = strtok(NULL, ":");

    if (Desthost == NULL || Destport == NULL) {
        cerr << "Invalid input format. Use ip:port\n";
        return 1;
    }

    int port = atoi(Destport);
    printf("Host %s, and port %d.\n", Desthost, port);

    // Address information setup
    addrinfo hints, *sinfo, *ptr;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // Support both IPv4 and IPv6
    hints.ai_socktype = SOCK_DGRAM;

    int variable = getaddrinfo(Desthost, Destport, &hints, &sinfo);
    if (variable != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(variable));
        return 1;
    }

    #ifdef DEBUG
    cout << "Server metadata:\n";
    printServerMetadata(sinfo);
    #endif

    int soc;
    for (ptr = sinfo; ptr != NULL; ptr = ptr->ai_next) {
        soc = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (soc != -1) break;
    }

    if (ptr == NULL) {
        cerr << "Failed to create socket\n";
        freeaddrinfo(sinfo);
        return 1;
    }

    // Initial message to server
    calcMessage messg = {
        .type = htons(22),
        .message = htonl(0),
        .protocol = htons(17),
        .major_version = htons(1),
        .minor_version = htons(0)
    };

    freeaddrinfo(sinfo);

    timeval tout = {2, 0};
    setsockopt(soc, SOL_SOCKET, SO_RCVTIMEO, &tout, sizeof(tout));

    if (sendto(soc, &messg, sizeof(messg), 0, ptr->ai_addr, ptr->ai_addrlen) == -1) {
        perror("sendto");
        close(soc);
        return 1;
    }

    #ifdef DEBUG
    cout << "Initial message sent to server.\n";
    #endif

    calcProtocol msgRecv;
    int attempts = 0;
    bool received = false;

    while (attempts < 3 && !received) {
        memset(&msgRecv, 0, sizeof(msgRecv));
        int bReceived = recvfrom(soc, &msgRecv, sizeof(msgRecv), 0, ptr->ai_addr, &ptr->ai_addrlen);
        if (bReceived >= 0) {
            received = true;
        } else {
            cout << "Receive timeout, sending message again.\n";
            if (sendto(soc, &messg, sizeof(messg), 0, ptr->ai_addr, ptr->ai_addrlen) == -1) {
                perror("sendto");
                close(soc);
                return 1;
            }
            attempts++;
        }
    }

    if (!received) {
        cerr << "Failed to receive response from server\n";
        close(soc);
        return 1;
    }

    // Process received message
    int in1 = ntohl(msgRecv.inValue1);
    int in2 = ntohl(msgRecv.inValue2);
    float f1 = msgRecv.flValue1;
    float f2 = msgRecv.flValue2;
    string resultOutput;

    switch (ntohl(msgRecv.arith)) {
        case 1: msgRecv.inResult = htonl(in1 + in2); cout << "Add " << in1 << " " << in2 << "\n"; resultOutput = to_string(in1 + in2); break;
        case 2: msgRecv.inResult = htonl(in1 - in2); cout << "Sub " << in1 << " " << in2 << "\n"; resultOutput = to_string(in1 - in2); break;
        case 3: msgRecv.inResult = htonl(in1 * in2); cout << "Mul " << in1 << " " << in2 << "\n"; resultOutput = to_string(in1 * in2); break;
        case 4: msgRecv.inResult = htonl(in1 / in2); cout << "Div " << in1 << " " << in2 << "\n"; resultOutput = to_string(in1 / in2); break;
        case 5: msgRecv.flResult = f1 + f2; cout << "Fadd " << f1 << " " << f2 << "\n"; resultOutput = to_string(f1 + f2); break;
        case 6: msgRecv.flResult = f1 - f2; cout << "Fsub " << f1 << " " << f2 << "\n"; resultOutput = to_string(f1 - f2); break;
        case 7: msgRecv.flResult = f1 * f2; cout << "Fmul " << f1 << " " << f2 << "\n"; resultOutput = to_string(f1 * f2); break;
        case 8: msgRecv.flResult = f1 / f2; cout << "Fdiv " << f1 << " " << f2 << "\n"; resultOutput = to_string(f1 / f2); break;
        default: cerr << "Unknown operation\n"; close(soc); return 1;
    }

    // Sending result to server
    if (sendto(soc, &msgRecv, sizeof(msgRecv), 0, ptr->ai_addr, ptr->ai_addrlen) == -1) {
        perror("sendto");
        close(soc);
        return 1;
    }

    cout << "Result sent to server: " << resultOutput << "\n";

    #ifdef DEBUG
    cout << "Result message sent to server.\n";
    #endif

    // Receive final server response
    calcMessage resp;
    attempts = 0;
    received = false;

    while (attempts < 3 && !received) {
        memset(&resp, 0, sizeof(resp));
        int bReceived = recvfrom(soc, &resp, sizeof(resp), 0, ptr->ai_addr, &ptr->ai_addrlen);
        if (bReceived >= 0) {
            received = true;
        } else {
            cout << "Receive timeout, sending again.\n";
            if (sendto(soc, &msgRecv, sizeof(msgRecv), 0, ptr->ai_addr, ptr->ai_addrlen) == -1) {
                perror("sendto");
                close(soc);
                return 1;
            }
            attempts++;
        }
    }

    if (!received) {
        cerr << "Failed to receive final response from server\n";
        close(soc);
        return 1;
    }

    ntohl(resp.message) == 1 ? cout << "Server: OK!\n" : cerr << "Server: NOT OK!\n";

    #ifdef DEBUG
    cout << "Final server response received.\n";
    #endif

    close(soc);
    return 0;
}
