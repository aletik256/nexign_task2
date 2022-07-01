#pragma once

#include "elevator.h"

#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <queue>
#include <condition_variable>
#include <chrono>
#include <map>
#include <iostream>
#include <atomic>
#include <ctime>

class Elevator::Impl {
 public:
    Impl(int number, float timePerFloor, int capacity);

    void Start();

    static void PushEvent(const Event& event);
    static void StopAll();

 private:
    // Обновление текущего положения.
    void UpdateCurrentPosition();
    // Проверка возможности подобрать пассажира.
    bool CheckForPickUp(const Event& event) const;
    // Обработка события.
    void ProcessEvent(const Event& event);
    // Обновить время до ближайшего события
    void UpdateTimeToAction();
    // Погрузка или выгрузка пассажиров
    void PerformAction();

    // Вывод лога
    static void PrintLog(int number, int floor, const char* info);

 private:
    static constexpr float WAIT_TIME = 5.0;  // Время ожидания перед спуском на 1 этаж

    enum class Direction { None, Up, Down };

    const int m_Number;
    const float m_TimePerFloor;
    const int m_Capacity;

    int m_Passangers;                              // Количество пассажиров в лифте.
    float m_Position;                              // Текущее местоположение лифта.
    Direction m_Direction;                         // Текущее направление движения.
    std::chrono::system_clock::time_point m_Time;  // Время последнего обновления позиции.
    std::chrono::duration<float> m_TimeToAction;  // Время до ближайшего действия.
    bool m_HasToMoveDown;  // Флаг выставляется, если требуется переместиться на 1 этаж

    std::map<int, int> m_Floors;  // Этажи, на которых планируется остановка для загрузки или выгрузки пассажиров.

    static std::condition_variable m_Condition;
    static std::mutex m_EventsMutex;
    static std::mutex m_LogMutex;
    static std::queue<Event> m_Events;
    static bool m_StopFlag;
};

std::condition_variable Elevator::Impl::m_Condition{};
std::mutex Elevator::Impl::m_EventsMutex{};
std::mutex Elevator::Impl::m_LogMutex{};
std::queue<Elevator::Event> Elevator::Impl::m_Events{};
bool Elevator::Impl::m_StopFlag{false};

Elevator::Impl::Impl(int number, float timePerFloor, int capacity)
    : m_Number(number),
      m_TimePerFloor(timePerFloor),
      m_Capacity(capacity),
      m_Passangers(0),
      m_Position(1.0),
      m_Direction(Direction::None),
      m_TimeToAction(std::chrono::duration<float>(0.0)),
      m_HasToMoveDown(false) {}

using namespace std::chrono_literals;

// Считается, что одно событие соответствует одному пассажиру.
// Лифт может подбирать пассажиров по направлению движения до тех пор, пока есть свободное место.
// На любое событие реагирует произвольный лифт.
void Elevator::Impl::Start() {
    const auto checkCondition = [&] -> bool {
        if (m_Events.empty() || m_Passangers == m_Capacity) {
            return m_StopFlag;
        }
        UpdateCurrentPosition();
        return CheckForPickUp(m_Events.front()) || m_StopFlag;
    };

    std::unique_lock<std::mutex> lock(m_EventsMutex);
    while (true) {
        if (m_Condition.wait_for(lock, m_TimeToAction, checkCondition)) {
            if (!m_StopFlag) {
                ProcessEvent(m_Events.front());
                m_Events.pop();
            } else {
                return;
            }
        } else {
            // Сработал один из двух сценариев:
            // 1. лифт доехал до нужного этажа, надо погрузить или выгрузить пассажиров;
            // 3. истекло время ожидания, надо ехать на первый этаж.
            PerformAction();
            if (m_Floors.empty()) {
                // Если все лифты были перегружены или ехали не в ту сторону, могли не обработать события
                while (checkCondition()) {
                    ProcessEvent(m_Events.front());
                    m_Events.pop();
                }
                // Если событий не было, собираемся ехать на первый этаж
                if (m_Floors.empty()) {
                    if (m_HasToMoveDown) {
                        // Перемещаемся на 1 этаж
                        ProcessEvent({1, 0});
                    } else {
                        // Ожидаем перемещения на 1 этаж
                        m_HasToMoveDown = true;
                        m_TimeToAction = std::chrono::duration<float>(WAIT_TIME);
                    }
                }
            }
        }
    }
}

void Elevator::Impl::PushEvent(const Event& event) {
    std::unique_lock<std::mutex> lock(m_EventsMutex);
    m_Events.push(event);
    m_Condition.notify_all();
}

void Elevator::Impl::StopAll() {
    std::unique_lock<std::mutex> lock(m_EventsMutex);
    m_StopFlag = true;
    m_Condition.notify_all();
}

void Elevator::Impl::UpdateCurrentPosition() {
    const auto currentTime = std::chrono::system_clock::now();
    if (m_Direction == Direction::Up) {
        const auto diff = std::chrono::duration_cast<std::chrono::duration<float>>(currentTime - m_Time);
        m_Position += diff.count() / m_TimePerFloor;
    } else if (m_Direction == Direction::Down) {
        const auto diff = std::chrono::duration_cast<std::chrono::duration<float>>(currentTime - m_Time);
        m_Position -= diff.count() / m_TimePerFloor;
    }
    m_Time = currentTime;
}

bool Elevator::Impl::CheckForPickUp(const Event& event) const {
    if (m_Direction == Direction::Up) {
        return m_Position < static_cast<float>(event.floorFrom);
    }
    if (m_Direction == Direction::Down) {
        return m_Position > static_cast<float>(event.floorFrom);
    }
    return true;
}

void Elevator::Impl::ProcessEvent(const Event& event) {
    m_HasToMoveDown = false;
    m_Floors[event.floorFrom]++;
    m_Floors[event.floorTo]--;
    if (m_Direction == Direction::None) {
        if (m_Position < static_cast<float>(event.floorFrom)) {
            m_Direction = Direction::Up;
            PrintLog(m_Number, static_cast<int>(m_Position), "Move up");
        } else {
            m_Direction = Direction::Down;
            PrintLog(m_Number, static_cast<int>(m_Position), "Move down");
        }
    }
    UpdateTimeToAction();
}

void Elevator::Impl::UpdateTimeToAction() {
    if (!m_Floors.empty()) {
        if (m_Direction == Direction::Up) {
            m_TimeToAction = std::chrono::duration<float>((m_Floors.cbegin()->first - m_Position) * m_TimePerFloor);
        } else if (m_Direction == Direction::Down) {
            m_TimeToAction = std::chrono::duration<float>((m_Position - m_Floors.crbegin()->first) * m_TimePerFloor);
        }
    }
}

void Elevator::Impl::PerformAction() {
    if (!m_Floors.empty()) {
        if (m_Direction == Direction::Up) {
            auto iter = m_Floors.begin();
            m_Position = static_cast<float>(iter->first);
            m_Passangers += iter->second;
            m_Floors.erase(iter);
            PrintLog(m_Number, static_cast<int>(m_Position), "Stop");
            if (!m_Floors.empty()) {
                PrintLog(m_Number, static_cast<int>(m_Position), "Move up");
            }
        } else if (m_Direction == Direction::Down) {
            auto iter = m_Floors.rbegin();
            m_Position = static_cast<float>(iter->first);
            m_Passangers += iter->second;
            m_Floors.erase(++iter.base());
            PrintLog(m_Number, static_cast<int>(m_Position), "Stop");
            if (!m_Floors.empty()) {
                PrintLog(m_Number, static_cast<int>(m_Position), "Move down");
            }
        }
        if (m_Floors.empty()) {
            m_Direction = Direction::None;
        }
        UpdateTimeToAction();
    }
}

void Elevator::Impl::PrintLog(int number, int floor, const char* info) {
    std::lock_guard<std::mutex> lock(m_LogMutex);
    const auto currentTime = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(currentTime);
    const auto local_time = *std::localtime(&time);
    std::cout << local_time.tm_hour << ":" << local_time.tm_min << ":" << local_time.tm_sec << " " << number << " "
              << floor << " " << info << std::endl;
}

Elevator::Elevator(int number, float timePerFloor, int capacity)
    : m_Impl(std::make_unique<Impl>(number, timePerFloor, capacity)) {}

Elevator::~Elevator() = default;

void Elevator::Start() {
    m_Impl->Start();
}

void Elevator::PushEvent(const Event& event) {
    Impl::PushEvent(event);
}

void Elevator::StopAll() {
    Impl::StopAll();
}
