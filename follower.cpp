#include <iostream>
#include <string>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <truck.cpp>

using namespace std;


int main(int argc, char** argv) {

    string ip;
    int port;

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--ip" && i + 1 < argc) {
            ip = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = stoi(argv[++i]);
        }
    }
    TruckInfo truck(ip, port);
    
    int sockfd = socket(AF_INET6, SOCK_STREAM, 0);
    if (sockfd < 0) {
        cerr << "Socket creation failed" << endl;
        return -1;
    }

    struct sockaddr_in6 servaddr;
    servaddr.sin6_family = AF_INET6;
    servaddr.sin6_port = htons(truck.port);
    servaddr.sin6_addr = in6addr_any;

    if (bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        cerr << "Bind failed" << endl;
        return -1;
    }

    if (listen(sockfd, 5) < 0) {
        cerr << "Listen failed" << endl;
        return -1;
    }

    cout << "Truck listening on [" << truck.ipv6addr << "]:" << truck.port << endl;

    while (true) {
        struct sockaddr_in6 cliaddr;
        socklen_t len = sizeof(cliaddr);
        int connfd = accept(sockfd, (struct sockaddr*)&cliaddr, &len);
        if (connfd < 0) {
            cerr << "Accept failed" << endl;
            continue;
        }

        char buffer[1024];
        int n = recv(connfd, buffer, sizeof(buffer)-1, 0);
        if (n > 0) {
            buffer[n] = '\0';
            string command(buffer);
            string response = truck.processCmd(command);
            send(connfd, response.c_str(), response.size(), 0);
        }
        close(connfd);
    }
    close(sockfd);

    return 0;
}
