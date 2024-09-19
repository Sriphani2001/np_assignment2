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

//#define DEBUG // Uncomment for debugging

// Helper function to print server IP metadata
void printServerMetadata(addrinfo *sinfo) {
    char ipstr[INET6_ADDRSTRLEN];
    for (addrinfo *p = sinfo; p != NULL; p = p->ai_next) {
        void *addr;
        string ipver;

        if (p->ai_family == AF_INET) { // Check if it's IPv4
            sockaddr_in *ipv4 = (sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = "IPv4";
        } else { // Else assume IPv6
            sockaddr_in6 *ipv6 = (sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = "IPv6";
        }

        inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr)); // Convert address to string
        cout << "  " << ipver << ": " << ipstr << "\n"; // Print address
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2) { // Check if input is in correct format
        cerr << "Usage: " << argv[0] << " ip:port\n"; // Print usage guide
        return 1;
    }

    char *Desthost = strtok(argv[1], ":"); // Split IP from port
    char *Destport = strtok(NULL, ":"); // Get port

    if (Desthost == NULL || Destport == NULL) { // Check if both parts exist
        cerr << "Invalid input format. Use ip:port\n"; // Invalid input error
        return 1;
    }

    int port = atoi(Destport); // Convert port to integer
    printf("Host %s, and port %d.\n", Desthost, port); // Print host and port

    // Address information setup
    addrinfo hints, *sinfo, *ptr;
    memset(&hints, 0, sizeof hints); // Clear hints
    hints.ai_family = AF_UNSPEC; // Allow both IPv4 and IPv6
    hints.ai_socktype = SOCK_DGRAM; // Use UDP socket

    int result = getaddrinfo(Desthost, Destport, &hints, &sinfo); // Get server info
    if (result != 0) { // Check if getaddrinfo fails
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result)); // Print error
        return 1;
    }

    #ifdef DEBUG
    cout << "Server metadata:\n";
    printServerMetadata(sinfo); // Print server metadata if in debug mode
    #endif

    int soc = -1;
    for (ptr = sinfo; ptr != NULL; ptr = ptr->ai_next) { // Loop through possible addresses
        soc = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol); // Create socket
        if (soc != -1) break; // Break if socket created successfully
    }

    if (soc == -1) { // Check if socket creation failed
        cerr << "Failed to create socket\n"; // Print error
        freeaddrinfo(sinfo); // Free address info
        return 1;
    }

    timeval tout = {2, 0}; // Set timeout of 2 seconds for receiving
    if (setsockopt(soc, SOL_SOCKET, SO_RCVTIMEO, &tout, sizeof(tout)) == -1) { // Set timeout
        perror("setsockopt"); // Print error
        close(soc); // Close socket
        freeaddrinfo(sinfo); // Free address info
        return 1;
    }

    // Initial message to server setup
    calcMessage messg = {
        .type = htons(22), // Message type
        .message = htonl(0), // Message ID
        .protocol = htons(17), // Protocol version
        .major_version = htons(1), // Major version
        .minor_version = htons(0) // Minor version
    };

    if (sendto(soc, &messg, sizeof(messg), 0, ptr->ai_addr, ptr->ai_addrlen) == -1) { // Send message to server
        perror("sendto"); // Print error
        close(soc); // Close socket
        freeaddrinfo(sinfo); // Free address info
        return 1;
    }

    #ifdef DEBUG
    cout << "Initial message sent to server.\n";
    #endif

    calcProtocol msgRecv; // Structure to store the received message
    int attempts = 0;
    bool received = false;

    while (attempts < 3 && !received) { // Retry loop
        memset(&msgRecv, 0, sizeof(msgRecv)); // Clear message buffer
        socklen_t addr_len = ptr->ai_addrlen; // Address length
        int bReceived = recvfrom(soc, &msgRecv, sizeof(msgRecv), 0, ptr->ai_addr, &addr_len); // Receive response
        if (bReceived >= 0) {
            received = true; // Message received successfully
        } else if (bReceived == 0) {
            cerr << "Connection closed by server\n"; // Server closed connection
            break;
        } else {
            perror("recvfrom"); // Receive error
            if (sendto(soc, &messg, sizeof(messg), 0, ptr->ai_addr, ptr->ai_addrlen) == -1) { // Resend message
                perror("sendto"); // Error on sending again
                close(soc); // Close socket
                freeaddrinfo(sinfo); // Free address info
                return 1;
            }
            attempts++;
            cout << "Receive timeout, sending message again.\n"; // Retry message
        }
    }

    if (!received) { // Check if message received
        cerr << "Failed to receive response from server\n"; // No response received
        close(soc); // Close socket
        freeaddrinfo(sinfo); // Free address info
        return 1;
    }

    // Processing received message
    int in1 = ntohl(msgRecv.inValue1); // Get integer 1
    int in2 = ntohl(msgRecv.inValue2); // Get integer 2
    float f1 = msgRecv.flValue1; // Get float 1
    float f2 = msgRecv.flValue2; // Get float 2
    string resultOutput; // To store result string

    // Perform arithmetic operation based on received data
    switch (ntohl(msgRecv.arith)) {
        case 1: msgRecv.inResult = htonl(in1 + in2); cout << "Add " << in1 << " " << in2 << "\n"; resultOutput = to_string(in1 + in2); break;
        case 2: msgRecv.inResult = htonl(in1 - in2); cout << "Sub " << in1 << " " << in2 << "\n"; resultOutput = to_string(in1 - in2); break;
        case 3: msgRecv.inResult = htonl(in1 * in2); cout << "Mul " << in1 << " " << in2 << "\n"; resultOutput = to_string(in1 * in2); break;
        case 4: msgRecv.inResult = htonl(in1 / in2); cout << "Div " << in1 << " " << in2 << "\n"; resultOutput = to_string(in1 / in2); break;
        case 5: msgRecv.flResult = f1 + f2; cout << "Fadd " << f1 << " " << f2 << "\n"; resultOutput = to_string(f1 + f2); break;
        case 6: msgRecv.flResult = f1 - f2; cout << "Fsub " << f1 << " " << f2 << "\n"; resultOutput = to_string(f1 - f2); break;
        case 7: msgRecv.flResult = f1 * f2; cout << "Fmul " << f1 << " " << f2 << "\n"; resultOutput = to_string(f1 * f2); break;
        case 8: msgRecv.flResult = f1 / f2; cout << "Fdiv " << f1 << " " << f2 << "\n"; resultOutput = to_string(f1 / f2); break;
        default: cerr << "Unknown operation\n"; close(soc); freeaddrinfo(sinfo); return 1;
    }

    // Send result back to server
    if (sendto(soc, &msgRecv, sizeof(msgRecv), 0, ptr->ai_addr, ptr->ai_addrlen) == -1) { // Send result
        perror("sendto"); // Error on sending
        close(soc); // Close socket
        freeaddrinfo(sinfo); // Free address info
        return 1;
    }

    cout << "Result sent to server: " << resultOutput << "\n"; // Print result sent

    #ifdef DEBUG
    cout << "Result message sent to server.\n";
    #endif

    // Receive final response from server
    calcMessage resp; // Structure to store final response
    attempts = 0;
    received = false;

    while (attempts < 3 && !received) { // Retry loop for final response
        memset(&resp, 0, sizeof(resp)); // Clear response buffer
        socklen_t addr_len = ptr->ai_addrlen; // Address length
        int bReceived = recvfrom(soc, &resp, sizeof(resp), 0, ptr->ai_addr, &addr_len); // Receive response
        if (bReceived >= 0) {
            received = true; // Message received
        } else if (bReceived == 0) {
            cerr << "Connection closed by server\n"; // Server closed connection
            break;
        } else {
            perror("recvfrom"); // Receive error
            if (sendto(soc, &msgRecv, sizeof(msgRecv), 0, ptr->ai_addr, ptr->ai_addrlen) == -1) { // Resend result
                perror("sendto"); // Error on sending
                close(soc); // Close socket
                freeaddrinfo(sinfo); // Free address info
                return 1;
            }
            attempts++;
            cout << "Receive timeout, sending again.\n"; // Retry receiving
        }
    }

    if (!received) { // Check if final response was received
        cerr << "Failed to receive final response from server\n"; // No response received
        close(soc); // Close socket
        freeaddrinfo(sinfo); // Free address info
        return 1;
    }

    ntohl(resp.message) == 1 ? cout << "Server: OK!\n" : cerr << "Server: NOT OK!\n"; // Check server's OK response

    #ifdef DEBUG
    cout << "Final server response received.\n";
    #endif

    close(soc); // Close socket after operation is complete
    freeaddrinfo(sinfo); // Free address info
    return 0;
}
