#include <iostream>
#include <string>
#include <vector>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sstream>
#include <algorithm>

using namespace std;

struct TruckInfo {
    string ipv6addr;
    int port;
    int position;
};

class PlatoonLeader {
    vector<TruckInfo> platoon;

public:
    void addTruck(const string& ipv6addr, int port, int position) {
        if (position < 1 || position > platoon.size() + 1) {
            cout << "Invalid position to add truck." << endl;
            return;
        }

        auto it = find_if(platoon.begin(), platoon.end(),
                          [&ipv6addr, port](const TruckInfo& truck) {
                              return truck.ipv6addr == ipv6addr && truck.port == port;
                          });
        if (it != platoon.end()) {
            cout << "Truck already in platoon." << endl;
            return;
        }  
        if ( platoon.size() >= position ) {
            cout << "Position already occupied. Next available position is " << platoon.size() + 1 << endl;
            return;
        }

        platoon.push_back({ipv6addr, port, position});
        sendCommand("LINK", ipv6addr, port, position);
        for (const auto& truck : platoon) {
            cout << "Truck: " << truck.ipv6addr << ":" << truck.port << " Position: " << truck.position << endl;
        }
    }

    void removeTruck(const string& ipv6addr, int port) {
        auto it = find_if(platoon.begin(), platoon.end(),
                          [&ipv6addr, port](const TruckInfo& truck) {
                              return truck.ipv6addr == ipv6addr && truck.port == port;
                          });

        if (it == platoon.end()) {
            cout << "Truck not found in platoon." << endl;
            return;
        }

        int removedPosition = it->position;
        //sendCommand("DELINK", it->ipv6addr, it->port, removedPosition);
        it -> position = -1;
        sendCommand("DELINK", it->ipv6addr, it->port, removedPosition);
        platoon.erase(it);
        for (auto& truck : platoon) {
            if (truck.position > removedPosition) {
                truck.position--;
            } else {
                continue;
            }
        }
        for (const auto& truck : platoon) {
            cout << "Truck: " << truck.ipv6addr << ":" << truck.port << " Position: " << truck.position << endl;
        }
        
        
    }

private: 
    bool sendCommand(const string& command, const string& ipv6addr, int port, int position) {
        int sockfd = socket(AF_INET6, SOCK_STREAM, 0);
        if (sockfd < 0) {
            cerr << "Socket creation failed" << endl;
            return false;
        }

        struct sockaddr_in6 servaddr;
        servaddr.sin6_family = AF_INET6;
        servaddr.sin6_port = htons(port);
        if(inet_pton(AF_INET6, ipv6addr.c_str(), &servaddr.sin6_addr) <= 0) {
            cerr << "Invalid IPv6 address: " << ipv6addr << endl;
            close(sockfd);
            return false;
        }

        if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
            cerr << "Connection to " << ipv6addr << ":" << port << " failed" << endl;
            close(sockfd);
            return false;
        }

        stringstream ss;
        ss << command << " " << position;
        string msg = ss.str();
        send(sockfd, msg.c_str(), msg.size(), 0);

        char buffer[1024];
        int n = recv(sockfd, buffer, sizeof(buffer)-1, 0);
        if (n > 0) {
            buffer[n] = '\0';
            cout << "Response from " << ipv6addr << ":" << port << " - " << buffer << endl;
        } else {
            cerr << "No response from " << ipv6addr << ":" << port << endl;
        }

        close(sockfd);
        return true;
    }

};

int main(int argc, char** argv) {
    PlatoonLeader leader;
    string line;
    string ip;
    int port;
    int leadposition = 0;

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--ip" && i + 1 < argc) {
            ip = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = stoi(argv[++i]);
        }
    }

    while (true) {
        cout << "Enter command (ADD <ipv6> <port> <position> | REMOVE <ipv6> <port> | EXIT): ";
        getline(cin, line);
        stringstream ss(line);
        string command, ipv6addr;
        int port, position;

        ss >> command;
        if (command == "ADD") {
            ss >> ipv6addr >> port >> position;
            leader.addTruck(ipv6addr, port, position);
            
        } else if (command == "REMOVE") {
            ss >> ipv6addr >> port;
            leader.removeTruck(ipv6addr, port);
        } else if (command == "EXIT") {
            break;
        } else {
            cout << "Invalid command." << endl;
        }

    }

/*    leader.addTruck("2001:db8::1", 8081);
    leader.addTruck("2001:db8::2", 8082);
    leader.removeTruck("2001:db8::1", 8081);*/
    return 0;

}