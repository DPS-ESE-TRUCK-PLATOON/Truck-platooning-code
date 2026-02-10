#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "../truck.cpp"
#include "../network.hpp"

// Test Suite: Truck Physics

void test_truck_speed_clamping() {
  Truck truck;
  
  // Test max speed clamping
  truck.setAccel(100); // Request excessive acceleration
  truck.simulateFrame(10.0f);
  CU_ASSERT(truck.getSpeed() <= 70.0f);
  
  // Test that max accel clamps to 2 m/s^2
  truck.setAccel(999);
  CU_ASSERT(truck.getAccel() == 4.0f);
}

void test_truck_deceleration() {
  Truck truck;
  truck.setAccel(2.0f);
  truck.simulateFrame(10.0f); // Accelerate to 20 m/s
  
  float speed_after_accel = truck.getSpeed();
  CU_ASSERT(speed_after_accel > 10.0f);
  
  // Now decelerate
  truck.setAccel(-5.0f);
  truck.simulateFrame(2.0f);
  
  float speed_after_decel = truck.getSpeed();
  CU_ASSERT(speed_after_decel < speed_after_accel);
  CU_ASSERT(speed_after_decel >= 0.0f);
}

void test_truck_heading_conversion() {
  Truck truck;
  
  // Set heading in degrees
  truck.setHeading(45.0f);
  float heading = truck.getHeading();
  CU_ASSERT(heading > 44.0f && heading < 46.0f);
  
  // Set 360 degrees (should wrap to 0)
  truck.setHeading(360.0f);
  heading = truck.getHeading();
  CU_ASSERT(heading < 1.0f || heading > 359.0f);
  
  // Negative angles should wrap correctly
  truck.setHeading(-45.0f);
  heading = truck.getHeading();
  CU_ASSERT(heading > 314.0f && heading < 316.0f); // Should be ~315
}

void test_truck_position_update() {
  Truck truck;
  truck.setLocation(0.0, 0.0);
  truck.setHeading(0.0f); // East direction
  truck.setAccel(2.0f);
  
  // Simulate forward movement
  truck.simulateFrame(5.0f); // Accelerate for 5 seconds
  
  double x = truck.getX();
  double y = truck.getY();
  
  // X should increase (moving east)
  CU_ASSERT(x > 0.0);
  // Y should stay near 0 (moving horizontally)
  CU_ASSERT(y < 0.1 && y > -0.1);
}

void test_truck_braking_distance() {
  Truck truck;
  
  // At rest, minimum distance
  double dist = truck.brakingDistance();
  CU_ASSERT(dist == 2.0); // absolute_min_distance
  
  // Accelerate truck
  truck.setAccel(2.0f);
  truck.simulateFrame(10.0f); // Speed up
  
  float speed = truck.getSpeed();
  double braking_dist = truck.brakingDistance();
  
  // Braking distance should increase with speed
  CU_ASSERT(braking_dist > 2.0);
  // Formula: v^2 / (2*|a|) where a = -5
  double expected = std::pow(speed * 1000.0 / 3600.0, 2) / 10.0;
  CU_ASSERT(std::abs(braking_dist - expected) < 0.1);
}

void test_truck_zero_speed_threshold() {
  Truck truck;
  truck.setAccel(2.0f);
  truck.simulateFrame(1.0f);
  
  // Set acceleration that would result in very small positive speed
  truck.setAccel(-2.0f);
  truck.simulateFrame(1.0f - 0.000001f);
  
  // Very small speeds should be clamped to 0
  float speed = truck.getSpeed();
  CU_ASSERT(speed == 0.0f);
}

// Test Suite: Network Queue Operations

void test_queue_back_state_fifo() {
  // Clear queues first
  proto::StatePayload temp;
  while (network::pop_back_state(temp)) {}
  
  proto::StatePayload state1;
  state1.truck_id = 1;
  state1.speed = 10.0f;
  
  proto::StatePayload state2;
  state2.truck_id = 2;
  state2.speed = 20.0f;
  
  network::queue_back_state(state1);
  network::queue_back_state(state2);
  
  proto::StatePayload retrieved1, retrieved2;
  bool got1 = network::pop_back_state(retrieved1);
  bool got2 = network::pop_back_state(retrieved2);
  
  CU_ASSERT(got1 && got2);
  CU_ASSERT(retrieved1.truck_id == 1);
  CU_ASSERT(retrieved1.speed == 10.0f);
  CU_ASSERT(retrieved2.truck_id == 2);
  CU_ASSERT(retrieved2.speed == 20.0f);
}

void test_queue_empty_pop() {
  // Clear queue first
  proto::StatePayload temp;
  while (network::pop_back_state(temp)) {}
  
  proto::StatePayload state;
  bool result = network::pop_back_state(state);
  
  CU_ASSERT(!result);
}

// Main Test Runner

int main() {
  CU_initialize_registry();
  
  // Create test suite for Truck physics
  CU_pSuite truck_suite = CU_add_suite("Truck Physics", NULL, NULL);
  CU_add_test(truck_suite, "Speed Clamping", test_truck_speed_clamping);
  CU_add_test(truck_suite, "Deceleration", test_truck_deceleration);
  CU_add_test(truck_suite, "Heading Conversion", test_truck_heading_conversion);
  CU_add_test(truck_suite, "Position Update", test_truck_position_update);
  CU_add_test(truck_suite, "Braking Distance", test_truck_braking_distance);
  CU_add_test(truck_suite, "Zero Speed Threshold", test_truck_zero_speed_threshold);
  
  // Create test suite for Network queues
  CU_pSuite network_suite = CU_add_suite("Network Queues", NULL, NULL);
  CU_add_test(network_suite, "Back State FIFO Order", test_queue_back_state_fifo);
  CU_add_test(network_suite, "Empty Queue Pop", test_queue_empty_pop);
  
  // Run tests
  CU_basic_run_tests();
  
  int failures = CU_get_number_of_tests_run() - CU_get_number_of_successes();
  CU_cleanup_registry();
  
  return failures > 0 ? 1 : 0;
}
