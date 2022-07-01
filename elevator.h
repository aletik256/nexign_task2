#pragma once

#include <memory>

/*!
    \brief Класс, реализующий лифт.
 */
class Elevator {
 public:
    /*!
        \brief Структура события вызова лифта.
     */
    struct Event {
        int floorFrom;  // Этаж, откуда надо забрать человека.
        int floorTo;    // Этаж, куда надо доставить человека.
    };

    /*!
        \brief Конструктор объекта.
        \param[in] number        Номер лифта.
        \param[in] timePerFloor  Время, необходимое для перемещения на один этаж.
        \param[in] capacity      Вместимость лифта.
     */
    Elevator(int number, float timePerFloor, int capacity);
    Elevator();
    /*!
        \brief Деструктор объекта.
     */
    ~Elevator();

    /*!
        \brief Начало работы лифта. Вызывается в отдельном потоке.
     */
    void Start();

    /*!
        \brief Добавление нового события вызова лифта.
        \param[in] event  Событие.
     */
    static void PushEvent(const Event& event);

    /*!
        \brief Остановка работы всех лифтов.
     */
    static void StopAll();

    /*!
        \brief Количество этажей в здании.
     */
    static constexpr int FLOORS_NUMBER = 12;

 private:
    class Impl;
    std::unique_ptr<Impl> m_Impl;
};
