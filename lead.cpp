#include <iostream>
#include <string>
#include <vector>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sstream>
#include <algorithm>
#include <termios.h>
#include <fcntl.h>

using namespace std;

struct TruckInfo {
    string ipv6addr;
    int port;
    int position;
};

class PlatoonLeader {
    vector<TruckInfo> platoon;
    int godotSocket = -1;
    bool godotConnected = false;

public:
    void addTruck(const string& ipv6addr, int port) {

        auto it = find_if(platoon.begin(), platoon.end(),
                          [&ipv6addr, port](const TruckInfo& truck) {
                              return truck.ipv6addr == ipv6addr && truck.port == port;
                          });
        if (it != platoon.end()) {
            cout << "Truck already in platoon." << endl;
            return;
        }  

        int position = platoon.size() + 1;

        platoon.push_back({ipv6addr, port, position});
        sendCommand("LINK", ipv6addr, port, position);
        if (godotConnected) {
            sendToGodot("LINK " + to_string(position));
        }
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
        sendCommand("DELINK", it->ipv6addr, it->port, removedPosition);
        if (godotConnected) {
            sendToGodot("DELINK " + to_string(removedPosition + 1));
        }
        platoon.erase(it);
        for (int i = 0; i < platoon.size(); i++){
            platoon[i].position = i + 1;
        }
        for (const auto& truck : platoon) {
            cout << "Truck: " << truck.ipv6addr << ":" << truck.port << " Position: " << truck.position << endl;
        }
        
        
    }

    bool connectToGodot(const string& ipv6addr, int port) {
        godotSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (godotSocket < 0) {
            cerr << "Godot socket creation failed" << endl;
            return false;
        }

        struct sockaddr_in servaddr;
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(port);
        if(inet_pton(AF_INET, ipv6addr.c_str(), &servaddr.sin_addr) <= 0) {
            cerr << "Invalid Godot IPv4 address: " << ipv6addr << endl;
            close(godotSocket);
            return false;
        }

        if (connect(godotSocket, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
            cerr << "Connection to Godot " << ipv6addr << ":" << port << " failed" << endl;
            close(godotSocket);
            return false;
        }

        godotConnected = true;
        return true;
    }

    void disconnectfromGodot() {
        if (godotSocket >= 0) {
            close(godotSocket);
            godotConnected = false;
        }
    }

    void sendDriveCommand(char key){
        if(!godotConnected) {
            cerr << "Not connected to Godot." << endl;
            return;
        }

        string cmd(1, key);
        sendToGodot(cmd);
    }

    bool isConnectedtoGodot() const{
        return godotConnected;
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

    void sendToGodot(const string& message) {
        if (!godotConnected) {
            cerr << "Not connected to Godot." << endl;
            return;
        }

        string msg = message + "\n";
        ssize_t sent = send(godotSocket, msg.c_str(), msg.size(), 0);
    }

};

void setNonBlockingInput(bool enable) {
    static struct termios oldstate, newstate;

    if (enable) {
        tcgetattr(STDIN_FILENO, &oldstate);
        newstate = oldstate;
        newstate.c_lflag &= ~ICANON; // Disable canonical mode
        newstate.c_lflag &= ~ECHO;   // Disable echo
        tcsetattr(STDIN_FILENO, TCSANOW, &newstate);
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK); // Set non-blocking
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldstate);
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK); // Clear non-blocking
    }
}

void drivingMode(PlatoonLeader& leader) {
    if (!leader.isConnectedtoGodot()) {
        cerr << "Not connected to Godot. Cannot enter driving mode." << endl;
        return;
    }

    cout << "Entering driving mode. Use W/A/S/D to control, Q to quit." << endl;
    setNonBlockingInput(true);

    bool driving = true;
    char ch;
    while (driving) {
        ssize_t n = read(STDIN_FILENO, &ch, 1);

        if (n > 0) {
            ch = tolower(ch);
            switch (ch) {
                case 'W':
                case 'w':
                case 'A':
                case 'a':
                case 'S':
                case 's':
                case 'D':
                case 'd':
                case 'Z':
                case 'z':
                    leader.sendDriveCommand(ch);
                    break;
                case 'Q':
                case 'q':
                    driving = false;
                    leader.sendDriveCommand('z');
                    break;
                default:
                    break;
            }
        }
        usleep(10000); // Sleep for 100ms
    }

    setNonBlockingInput(false);
}

void platoonManagementMode(PlatoonLeader& leader) {
    string line;
    while (true) {
        cout << "Enter command (ADD <ipv6> <port> | REMOVE <ipv6> <port> | DRIVE | EXIT): ";
        getline(cin, line);
        stringstream ss(line);
        string command, ipv6addr;
        int port;

        ss >> command;
        if (command == "ADD") {
            ss >> ipv6addr >> port;
            leader.addTruck(ipv6addr, port);
        } else if (command == "REMOVE") {
            ss >> ipv6addr >> port;
            leader.removeTruck(ipv6addr, port);
        } else if (command == "DRIVE") {
            drivingMode(leader);
        } else if (command == "EXIT") {
            break;
        } else {
            cout << "Invalid command." << endl;
        }
    }
}

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

    if (!leader.connectToGodot(ip, port)) {
        cerr << "Failed to connect to Godot at " << ip << ":" << port << endl;
        return -1;
    }

    while (true) {
        
        cout << "\n===================================" << endl;
        cout << "      Platoon Management Mode      " << endl;
        cout << "===================================" << endl;
        cout << "Select mode:" << endl;
        cout << " 1. Platoon Management" << endl;
        cout << " 2. Driving Mode" << endl;
        cout << " 3. Exit" << endl;
        cout << " Enter choice (1-3): ";
        cout << "> ";
        
        string line;
        getline(cin, line);

        if (line == "1") {
            platoonManagementMode(leader);
        } else if (line == "2") {
            drivingMode(leader);
        } else if (line == "3") {
            break;
        } else {
            cout << "Invalid choice." << endl;
        }
    }

/*    leader.addTruck("2001:db8::1", 8081);
    leader.addTruck("2001:db8::2", 8082);
    leader.removeTruck("2001:db8::1", 8081);*/
    return 0;

}