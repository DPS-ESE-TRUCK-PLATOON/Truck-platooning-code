#include "comm_loss.hpp"
#include <iostream>

communication_loss::communication_loss() {
    // Initial values
    t_loss = 0.0;
    current_state = state_mode::park;
    last_packet_time = clock_type::now();
    
    // Thread control flags
    running = true;
    emergency_triggered = false;
    
    // Start the background safety thread
    watchdog_thread = std::thread(&communication_loss::watchdogLoop, this);
}

communication_loss::~communication_loss() {
    // Gracefully stop the thread
    running = false;
    if (watchdog_thread.joinable()) {
        watchdog_thread.join();
    }
}

void communication_loss::connection_recovery() {
    std::lock_guard<std::mutex> lock(data_mutex);

    // If permanent loss occurred, we are locked out. No recovery.
    if (current_state == state_mode::permanent_loss) {
        return; 
    }

    // Reset timer
    last_packet_time = clock_type::now();
    
    // Restore state
    if (current_state != state_mode::standard_platooning) {
        current_state = state_mode::standard_platooning;
    }
}

void communication_loss::watchdogLoop() {
    while (running) {
        {
            // CRITICAL: Lock mutex to read shared data safely
            std::lock_guard<std::mutex> lock(data_mutex);
            
            auto now = clock_type::now();
            std::chrono::duration<double> elapsed = now - last_packet_time;
            t_loss = elapsed.count();

            // Only check safety rules if we are active
            if (current_state != state_mode::park && current_state != state_mode::permanent_loss) {
                
                // 1. Temporary Loss (> 0.1s)
                if (t_loss > 0.1) {
                    current_state = state_mode::temporary_loss;
                }
                
                // 2. Permanent Loss (> 4.0s)
                if (t_loss > t_max) {
                    current_state = state_mode::permanent_loss;
                    emergency_triggered = true; 
                    std::cout << "[Comm] CRITICAL: Connection Lost!" << std::endl;
                }
            }
        } // Mutex is released here automatically by the bracket }

        // CRITICAL: Sleep to save CPU (approx 60 checks per second)
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