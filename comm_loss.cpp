#include "comm_loss.hpp"
#include <iostream>

communication_loss::communication_loss() {
    t_loss = 0.0;
    current_state = state_mode::park;
    last_packet_time = clock_type::now();
    running = true;
    emergency_triggered = false;
    watchdog_thread = std::thread(&communication_loss::watchdogLoop, this);
}

communication_loss::~communication_loss() {
    running = false;
    if (watchdog_thread.joinable()) {
        watchdog_thread.join();
    }
}

void communication_loss::connection_recovery() {
    std::lock_guard<std::mutex> lock(data_mutex);

    if (current_state == state_mode::permanent_loss) {
        return; 
    }

    last_packet_time = clock_type::now();
    
    if (current_state != state_mode::standard_platooning) {
        current_state = state_mode::standard_platooning;
    }
}

void communication_loss::watchdogLoop() {
    while (running) {
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            
            auto now = clock_type::now();
            std::chrono::duration<double> elapsed = now - last_packet_time;
            t_loss = elapsed.count();

            if (current_state != state_mode::park && current_state != state_mode::permanent_loss) {
                
                if (t_loss > 0.1) {
                    current_state = state_mode::temporary_loss;
                }
                
                if (t_loss > t_max) {
                    current_state = state_mode::permanent_loss;
                    emergency_triggered = true; 
                    std::cout << "CRITICAL: Connection Lost!" << std::endl;
                }
            }
        } 

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

bool communication_loss::permanentConnection_loss() {
    return emergency_triggered.load();
}

double communication_loss::getTimeLost() {
    std::lock_guard<std::mutex> lock(data_mutex);
    return t_loss;
}

state_mode communication_loss::getCurrentState() {
    std::lock_guard<std::mutex> lock(data_mutex);
    return current_state;
}