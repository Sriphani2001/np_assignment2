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

//#define DEBUG
//#define SERVERPORT "5000"
#include "protocol.h"
using namespace std;

//#define DEBUG

int main(int argc, char *argv[])
{
    // Server address input. Should be given in the order of ip:port....
    sockaddr servadrs;
    char delim[] = ":";
    char *Desthost = strtok(argv[1], delim);
    char *Destport = strtok(NULL, delim);
    int port = atoi(Destport);
    printf("Host %s, and port %d.\n", Desthost, port);

    #ifdef DEBUG
        //printf("Host %s, and port %d.\n",Desthost,port);
    #endif

    if (Desthost == NULL || Destport == NULL) {
        cout << "Entered input is false\n";
        exit(1);
    }

    char buffer[1024];

    // Structs of address information and address size (server data)
    socklen_t addr_size;
    addrinfo hints, *sinfo, *ptr;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // Support both IPv4 and IPv6
    hints.ai_socktype = SOCK_DGRAM;

    // Get address info
    int variable;
    if ((variable = getaddrinfo(Desthost, Destport, &hints, &sinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(variable));
        return 1;
    }

    // Create socket
    int soc = 0;
    for (ptr = sinfo; ptr != NULL; ptr = ptr->ai_next) {
        soc = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (soc == -1) {
            cout << "Error in creating socket\n";
            continue;
        } else {
            #ifdef DEBUG
                cout << "Socket created\n";
            #endif
            break;
        }
    }

    if (ptr == NULL) {
        cout << "Failed to create socket\n";
        return 1;
    }

    // Client first message server
    calcMessage messg;
    messg.type = htons(22); // Client-to-server, binary protocol
    messg.message = htonl(0); // Not applicable/available (N/A or NA)
    messg.protocol = htons(17); // UDP connection
    messg.major_version = htons(1);
    messg.minor_version = htons(0);

    freeaddrinfo(sinfo);

    // Connection timeout
    timeval tout;
    tout.tv_sec = 2;
    tout.tv_usec = 0;

    if (setsockopt(soc, SOL_SOCKET, SO_RCVTIMEO, &tout, sizeof(tout)) < 0) {
        perror("setsockopt failed\n");
    }

    if (sendto(soc, &messg, sizeof(messg), 0, ptr->ai_addr, ptr->ai_addrlen) == -1) {
        cout << "Unable to send message\n";
        close(soc);
        exit(1);
    }

    int nSent = 0;
    int bReceived = 0;
    calcProtocol msgRecv;

    while (nSent < 3) {
        memset(&msgRecv, 0, sizeof(msgRecv));
        bReceived = recvfrom(soc, &msgRecv, sizeof(msgRecv), 0, ptr->ai_addr, &ptr->ai_addrlen);
        if (bReceived < 0) {
            cout << "Receive timeout, sending message again.\n";
            nSent++;
            int send = sendto(soc, &messg, sizeof(messg), 0, ptr->ai_addr, ptr->ai_addrlen);
            if (send == -1) {
                cout << "Message cannot be sent\n";
                close(soc);
                exit(1);
            }
            if (nSent < 3) continue;
            else {
                cout << "Message cannot be sent\n";
                close(soc);
                exit(1);
            }
        } else {
            nSent = 0;
            break;
        }
    }

    if (bReceived < (int)sizeof(msgRecv)) {
        calcMessage *clcMsg = (calcMessage *)&msgRecv;
        int t = 0;
        if (ntohs(clcMsg->type) == 2 && ntohs(clcMsg->message) == 2 && ntohs(clcMsg->major_version) == 1 && ntohs(clcMsg->minor_version) == 0) {
            cout << "stop";
            t = -1;
        }
        return t;

        if (clcMsg) {
            cout << "NOT OK message is given.\nClosing socket\n";
            close(soc);
            exit(1);
        }
    }

    #ifdef DEBUG
        cout << "Assignment received ";
    #endif

    int in1 = ntohl(msgRecv.inValue1), in2 = ntohl(msgRecv.inValue2);
    int inRes = ntohl(msgRecv.inResult);
    float f1 = msgRecv.flValue1, f2 = msgRecv.flValue2;
    float fRes = msgRecv.flResult;
    cout << "Assignment: ";
    string output = "";

    // Calculations of given assignment begin here....
    if (ntohl(msgRecv.arith) == 1) { // integer add
        cout << "Add " << in1 << " " << in2 << "\n";
        inRes = in1 + in2;
        msgRecv.inResult = htonl(inRes);
        output = "i";
    } else if (ntohl(msgRecv.arith) == 2) { // integer sub
        cout << "Sub " << in1 << " " << in2 << "\n";
        inRes = in1 - in2;
        msgRecv.inResult = htonl(inRes);
        output = "i";
    } else if (ntohl(msgRecv.arith) == 3) { // integer mul
        cout << "Mul " << in1 << " " << in2 << "\n";
        inRes = in1 * in2;
        msgRecv.inResult = htonl(inRes);
        output = "i";
    } else if (ntohl(msgRecv.arith) == 4) { // integer div
        cout << "Div " << in1 << " " << in2 << "\n";
        inRes = in1 / in2;
        msgRecv.inResult = htonl(inRes);
        output = "i";
    } else if (ntohl(msgRecv.arith) == 5) { // float add
        cout << "Fadd " << f1 << " " << f2 << "\n";
        fRes = f1 + f2;
        msgRecv.flResult = fRes;
        output = "f";
    } else if (ntohl(msgRecv.arith) == 6) { // float sub
        cout << "Fsub " << f1 << " " << f2 << "\n";
        fRes = f1 - f2;
        msgRecv.flResult = fRes;
        output = "f";
    } else if (ntohl(msgRecv.arith) == 7) { // float mul
        cout << "Fmul " << f1 << " " << f2 << "\n";
        fRes = f1 * f2;
        msgRecv.flResult = fRes;
        output = "f";
    } else if (ntohl(msgRecv.arith) == 8) { // float div
        cout << "Fdiv " << f1 << " " << f2 << "\n";
        fRes = f1 / f2;
        msgRecv.flResult = fRes;
        output = "f";
    } else {
        cout << "Can't do that operation.\n";
    }

    // Sending answer to server
    int send = sendto(soc, &msgRecv, sizeof(msgRecv), 0, ptr->ai_addr, ptr->ai_addrlen);
    if (send == -1) {
        cout << "Message cannot be sent\n";
        close(soc);
        exit(1);
    }

    if (output == "i") {
        cout << "My result: " << ntohl(msgRecv.inResult) << "\n";
    } else {
        cout << "My result: " << msgRecv.flResult << "\n";
    }

    #ifdef DEBUG
        cout << "Message sent successfully\n";
    #endif

    calcMessage resp;
    while (nSent < 3) {
        memset(&resp, 0, sizeof(resp));
        bReceived = recvfrom(soc, &resp, sizeof(resp), 0, ptr->ai_addr, &ptr->ai_addrlen);
        if (bReceived < 0) {
            cout << "Receive timeout, sending again.\n";
            nSent++;
            int send = sendto(soc, &msgRecv, sizeof(msgRecv), 0, ptr->ai_addr, ptr->ai_addrlen);
            if (send == -1) {
                cout << "Message cannot be sent\n";
                close(soc);
                exit(1);
            }
            if (nSent < 3) continue;
            else {
                cout << "Could not send message\n";
                close(soc);
                exit(1);
            }
        } else {
            nSent = 0;
            break;
        }
    }

    // Verification from server whether our problem is correct or not
    ntohl(resp.message) == 1 ? cout << "Server: OK!\n" : cerr << "Server: NOT OK!\n";

    // Close the socket
    close(soc);
    return 0;
}
