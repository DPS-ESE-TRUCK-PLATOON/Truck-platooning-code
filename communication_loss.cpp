#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>

using namespace std;

struct TruckInfo {
    enum State { STANDARD, TEMP_LOSS, EMERGENCY, PARKED } state;
    long long Last_message_time;

    TruckInfo() {
        state = STANDARD;
        Last_message_time = 0;
    }
};

TruckInfo myTruck;
bool keepRunning = true;
SOCKET trucksocket[4] = {0, 0, 0, 0}; 

void Communication_Monitor() {

    cout << "Communication state Monitoring Started.\n" << endl;
    
    while (keepRunning) {
        if (myTruck.state == TruckInfo::PARKED) {
            continue;
        }

        auto now = chrono::system_clock::now();
        long long currentTime = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()).count();
        double t_loss = (currentTime - myTruck.Last_message_time) / 1000.0;

        if (t_loss > 1.0 && t_loss < 10.0) {
            if (myTruck.state == TruckInfo::STANDARD) {
                myTruck.state = TruckInfo::TEMP_LOSS;
                cout << "TEMPORARY LOSS: Maintain previous state.\n" << endl;
            }
        }
        else if (t_loss >= 10.0) {
             if (myTruck.state != TruckInfo::EMERGENCY) {
                myTruck.state = TruckInfo::EMERGENCY;
                cout << "PERMANENT LOSS !!! BRAKING AND steering to shoulder...\n" << endl;
                
                myTruck.state = TruckInfo::PARKED; 
            }
        }
    }
}

int main() {
    myTruck.Last_message_time = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
    Communication_Monitor();
    return 0;
}
