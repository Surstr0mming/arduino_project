#pragma once
#include <Arduino.h>

typedef void (*TimerCallback)();

class Timer {
   public:
    // конструктор

    // пустий
    Timer() {}

    // вказати час. Таймер сам запуститься в режимі інтервалу!
    Timer(uint32_t ms, uint32_t seconds = 0, uint32_t minutes = 0, uint32_t hours = 0, uint32_t days = 0) {
        setTime(ms, seconds, minutes, hours, days);
        start();
    }

    // ========= JAVASCRIPT =========
    // запуск в режимі інтервалу з вказанням часу в мс
    void setInterval(uint32_t ms) {
        prd = ms;
        start();
    }

    // запуск в режимі інтервалу з вказанням обробника і часу в мс
    void setInterval(TimerCallback callback, uint32_t ms) {
        prd = ms;
        this->callback = callback;
        start();
    }

    // запуск в режимі таймауту з вказанням часу в мс
    void setTimeout(uint32_t ms) {
        prd = ms;
        start(true);
    }

    // запуск в режимі таймауту з вказанням обробника і часу в мс
    void setTimeout(TimerCallback callback, uint32_t ms) {
        prd = ms;
        this->callback = callback;
        start(true);
    }

    // =========== MANUAL ===========
    // вказати час
    void setTime(uint32_t ms, uint32_t seconds = 0, uint32_t minutes = 0, uint32_t hours = 0, uint32_t days = 0) {
        prd = seconds;
        if (minutes) prd += minutes * 60ul;
        if (hours) prd += hours * 3600ul;
        if (days) prd += days * 86400ul;
        if (prd) prd *= 1000ul;
        prd += ms;
    }

    // запустити в режимі інтервалу. Передати true для одноразового спрацьовування (режим таймауту)
    void start(bool once = false) {
        if (prd) {
            tmr = millis();
            mode = once ? 2 : 1;
        }
    }

    // зупинити
    void stop() {
        mode = 0;
    }

    // стан (запущений?)
    bool state() {
        return mode;
    }

    // підключити функцію-обробник виду void f()
    void attach(TimerCallback callback) {
        this->callback = callback;
    }

    // відключити функцію-обробник
    void detach() {
        callback = nullptr;
    }

    // тикер, викликати в loop. Поверне true, якщо спрацював
    bool tick() {
        if (mode && millis() - tmr >= prd) {
            if (callback) callback();
            if (mode == 1) start();
            else stop();
            return 1;
        }
        return 0;
    }
    operator bool() {
        return tick();
    }

   private:
    uint8_t mode = 0;  // 0 зупинено, 1 період, 2 таймаут
    uint32_t tmr = 0, prd = 0;
    TimerCallback callback = nullptr;
};
