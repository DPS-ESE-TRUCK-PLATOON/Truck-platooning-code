#pragma once
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>

enum class state_mode {
    standard_platooning,
    temporary_loss,
    permanent_loss,
    park
};

class communication_loss {
private:
    std::thread watchdog_thread;
    std::mutex data_mutex;
    std::atomic<bool> running;
    std::atomic<bool> emergency_triggered;

    bool comm_status;
    double t_loss;
    const double t_max = 4;
    
    using clock_type = std::chrono::steady_clock;
    std::chrono::time_point<clock_type> last_packet_time;
    state_mode current_state;

    void watchdogLoop();

public:
    communication_loss();
    ~communication_loss();

    void connection_recovery();
    
    bool permanentConnection_loss(); 

    double getTimeLost();
    state_mode getCurrentState();
};