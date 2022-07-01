#include "elevator.h"

#include <thread>
#include <random>
#include <functional>
#include <chrono>

constexpr int ELEVATORS_NUMBER = 5;
constexpr int EVENTS_NUMBER = 10;

int main() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> tpf_dist(1000, 5000);
    std::uniform_int_distribution<int> cap_dist(5, 10);

    std::vector<std::thread> threads;
    std::vector<std::unique_ptr<Elevator>> elevators;
    for (int i = 0; i < ELEVATORS_NUMBER; i++) {
        const auto timePerFloor = static_cast<float>(tpf_dist(gen)) / 1000.0;
        const auto capacity = cap_dist(gen);
        auto elevator = std::make_unique<Elevator>(i + 1, timePerFloor, capacity);
        threads.emplace_back(std::bind(&Elevator::Start, elevator.get()));
        elevators.push_back(std::move(elevator));
    }

    std::uniform_int_distribution<int> floor_dist(1, Elevator::FLOORS_NUMBER);
    std::uniform_int_distribution<int> time_dist(1000, 5000);
    for (int i = 0; i < EVENTS_NUMBER; i++) {
        const auto floorFrom = floor_dist(gen);
        auto floorTo = floor_dist(gen);
        while (floorTo == floorFrom) {
            floorTo = floor_dist(gen);
        }
        Elevator::PushEvent({floorFrom, floorTo});
        const auto delay = static_cast<float>(time_dist(gen)) / 1000.0;
        std::this_thread::sleep_for(std::chrono::duration<float>(delay));
    }

    Elevator::StopAll();
    for (auto &thread : threads) {
        if (thread.joinable()) {
            thread.join();
        } else {
            thread.detach();
        }
    }

    return 0;
}
