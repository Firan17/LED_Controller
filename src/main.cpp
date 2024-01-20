/*
    LED_Controller
    Программа управления подсветкой.
    Используется 62.5 кГц - 8 bit - ШИМ (15.6 кГц - 10 bit - ШИМ) на пинах D9 и D10 (в связке - один уровень ==> опционально --> раскомментировать нужные блоки кода).
    Платформа: Atmeg328 (ARDUINO NANO)
    GitHub: https://github.com/Firan17/LED_Controller

    Данное программное обеспечение использует библиотеки других авторов, а именно:

        - GyverButton v3.8 от AlexGyver <alex@alexgyver.ru> распространяемую под лицензией MIT
        URL: https://github.com/GyverLibs/GyverButton
        для получения дополнительной информации обратитесь к файлу: lib\GyverButton\LICENSE

        - GyverPower v2.2 от AlexGyver <alex@alexgyver.ru> распространяемую под лицензией MIT
        URL: https://github.com/GyverLibs/GyverPower
        для получения дополнительной информации обратитесь к файлу: lib\GyverPower\LICENSE

        - EEManager v2.0 от AlexGyver <alex@alexgyver.ru> распространяемую под лицензией MIT
        URL: https://github.com/GyverLibs/EEManager
        для получения дополнительной информации обратитесь к файлу: lib\EEManager\LICENSE

        - GyverBlinker v1.0 от AlexGyver <alex@alexgyver.ru> распространяемую под лицензией MIT
        URL: https://github.com/GyverLibs/GyverBlinker
        для получения дополнительной информации обратитесь к файлу: lib\GyverBlinker\LICENSE

        - EasyingLib v1.0.0 от Luis Llamas <luisllamas.es> распространяемую под лицензией Apache Version 2.0
        URL: https://github.com/luisllamasbinaburo/Arduino-Easing
        для получения дополнительной информации обратитесь к файлу: lib\EasyingLib\LICENSE

    Все библиотеки используются без внесения изменений в их исходный код.

    Возможности:
    - Включение/выключение подсветки 1 кликом (индикация - 1 мигание светодиода):
        - При выключении МК погружается в сон (POWERDOWN_SLEEP)
        - При включении исп. последний из используемых активных режимов
    - Переключение активных режимов работы двойным кликом (индикация - 2 мигания светодиода)
    - Настройка текущего активного режима работы после 3 кликов (исп. потенциометр) (индикация - 3 мигания светодиода):
        - 2 клика для смены активного режима (индикация - 2 мигания светодиода)
        - 3 клика для завершения настройки (текущая яркость сохраняется)
        - 4 клика для завершения настройки (текущая яркость не сохраняется)
        - Индикация завершения настройки - 3 мигания светодиода
    - Запись текущих настроек в EEPROM после 5 кликов (индикация - 4 мигания светодиода)
    - Сброс настроек к "заводским" после 10 кликов (индикация - 5 миганий светодиода)

    by Firan17
    MIT License

    ---------- ВЕРСИИ ----------
    v1.0 от 03.01.2024

*/

#include <Arduino.h>
#include "GyverButton.h"
#include "GyverPower.h"
#include "EEManager.h"
#include "EasingLib.h"
#include "Blinker.h"

#define OFF 0                   // значение яркости в режиме ВЫКЛ.
#define MAX 1023                // максимальное значение яркости
#define ADDR 512                // адрес хранения настроек в EEPROM
#define INIT_KEY 's'            // ключ первого запуска 
#define BTN_PIN 3               // пин кнопки (D3)
#define VOL_PIN 14              // пин потенциометра (A0)
#define LED_PIN_1 9             // пин управления лентой 1 (D9)
//#define LED_PIN_2 10            // пин управлени лентой 2 (D10)
#define TIME_CHANGE 800         // время изменения яркости

// объявляем структуру данных, в которой будут храниться настройки (необходимо сохранение в EEPROM)
struct Data
{
    int FIRST_LVL = 512;                        // первый активный режим
    int SECOND_LVL = 1023;                      // второй активный режим
    bool NUM_MODE = 0;                          // номер используемого режима (последнего)
};
Data settings;                                  // создаём структуру
EEManager memory(settings);                     // передаём созданную структуру (фактически её адрес)

Easing easing(LINEAR, TIME_CHANGE);             // конструктор функций easing

GButton btn(BTN_PIN, LOW_PULL, NORM_OPEN);      // объявление кнопки (подтяжка к GNG, нормально открытая)

Blinker led(LED_BUILTIN);                       // настройка встроенного светодиода на мигание

int NOW_LVL = 0;                                // текущий уровень яркости (линейная зав.)
int NOW_LVL_CRT = 0;                            // текущий уровень яркости, скорректированный по CRT
int newValue = 0;                               // новое значение уровня яркости

/*
---------- НОМЕР ТЕКУЩЕГО РЕЖИМА РАБОТЫ ----------
    0 - FIRST_LVL   - ПЕРВЫЙ АКТИВНЫЙ РЕЖИМ
    1 - SECOND_LVL  - ВТОРОЙ АКТИВНЫЙ РЕЖИМ
    2 - OFF         - ВЫКЛ.
    3 - MAX         - МАКС. ЯРКОСТЬ (1023)
*/
int NUM_MODE_ = 2;


// ---------- ПРОТОТИПЫ ФУНКЦИЙ ----------

void isr();                     // обработка прерывания (МК просыпается)
void set();                     // установка нового значения яркости для текущего режима
void transit();                 // плавный переход яркости (жёсткая ф-ция) 
void setNewValue();             // установка нового значения яркости ленты
int getBrightCRT(int val);      // функция возвращает скорректированное по CRT значение текущей яркости (для 10-bit ШИМ)


// ---------- НАСТРОЙКИ ----------
void setup() 
{
    /*
    // 15.6 кГц - 10 bit - ШИМ на пинах D9 и D10
    TCCR1A = 0b00000011;
    TCCR1B = 0b00001001;
    */

    // Пины D9 и D10 - 62.5 кГц
    TCCR1A = 0b00000001;  // 8bit
    TCCR1B = 0b00001001;  // x1 fast pwm
    
    // настройка используемых пинов
    pinMode(VOL_PIN, INPUT);
    pinMode(LED_PIN_1, OUTPUT);
    //pinMode(LED_PIN_2, OUTPUT);

    attachInterrupt(1, isr, RISING);            // прерывание на D3 (при получении высокого сигнала)

    // отключение ненужной периферии
    power.hardwareDisable(PWR_I2C | PWR_UART0 | PWR_UART1| PWR_UART2 | PWR_UART3 | PWR_SPI | PWR_TIMER2 | PWR_TIMER3 | PWR_TIMER4 | PWR_TIMER5);

    power.setSleepMode(POWERDOWN_SLEEP);        // режим сна (по умолчанию POWERDOWN_SLEEP)

    memory.begin(ADDR, INIT_KEY);               // запускаем EEManager
    memory.setTimeout(5000);
    
    //btn.setClickTimeout(600);                 // установка таймаута между кликами (по умолчанию 500 мс)
    //btn.setDebounce(100);                     // установка времени антидребезга (по умолчанию 80 мс)
    //btn.setTimeout(500);                      // установка таймаута удержания (по умолчанию 300 мс)

    led.blink(1, 500, 100);                     // мигаем 1 раз (маркер включения)
}


// ---------- ОСНОВНОЙ КОД ПРОГРАММЫ ----------
void loop() 
{
    memory.tick();                              // обновление данных в EEPROM
    led.tick();                                 // мигаем светодиодом (проверка на необх. изм. сост.)
    btn.tick();                                 // опрос кнопки

    if (btn.hasClicks())                        // проверка на наличие нажатий
    {
        int val = btn.getClicks();              // получаем число нажатий

        switch (val)
        {
        case 1:                                                         // ---------- 1 клик ----------
            led.blink(1, 100, 100);                                     // мигнуть 1 раз
            if (NUM_MODE_ == 2) NUM_MODE_ = settings.NUM_MODE;              // если выкл., то последний активный режим
            else                                                        // если активный режим, то спим
            {
                NUM_MODE_ = 2;                                          // установка режима выкл.
                transit();                                              // установка нового значения яркости
                power.sleep(SLEEP_FOREVER);                             // погружаем МК в сон
                led.blink(1, 100, 100);                                 // мигнуть 1 раз
                NUM_MODE_ = settings.NUM_MODE;                          // установка последнего активного режима
            }
            break;
        
        case 2:                                                         // ---------- 2 клика ----------
            led.blink(2, 100, 100);                                     // мигнуть 2 раза
            if (NUM_MODE_ == 0) NUM_MODE_ = 1;                          // меняем активный режим
            else if (NUM_MODE_ == 1) NUM_MODE_ = 0;
            break;

        case 3:                                                         // ---------- 3 клика ----------
            led.blink(3, 100, 100);                                     // мигнуть 3 раза
            if (NUM_MODE_ == 0 || NUM_MODE_ == 1) set();                // если активный режим, то настройка
            break;

        case 5:                                                         // ---------- 5 кликов ----------
            if (NUM_MODE_ == 0 || NUM_MODE_ == 1)
            {
                led.blink(4, 100, 100);                                 // мигнуть 4 раза
                memory.update();                                        // сообщаем, что данные нужно обновить
            }
            break;

        case 10:                                                        // ---------- 10 кликов ----------
            if (NUM_MODE_ == 0 || NUM_MODE_ == 1)
            {
                led.blink(5, 100, 100);                                 // мигнуть 5 раз
                memory.reset();                                         // сброс ключа запуска
                memory.begin(ADDR, INIT_KEY);                           // новый старт работы EEManager (читаем данные в переменную)
            }
            break;

        default:
            break;
        }

        transit();                                                      // установка нового значения яркости
    }
}


// ---------- ВЫЗЫВАЕМЫЕ ФУНКЦИИ ----------

// функция устанавливает новое значение яркости ленты
void setNewValue()
{
    switch (NUM_MODE_)                          // проверяем режим ленты
    {
    case 0:                                     // ---------- ПЕРВЫЙ АКТИВНЫЙ РЕЖИМ ----------
        newValue = settings.FIRST_LVL;
        settings.NUM_MODE = 0;
        break;

    case 1:                                     // ---------- ВТОРОЙ АКТИВНЫЙ РЕЖИМ ----------
        newValue = settings.SECOND_LVL;
        settings.NUM_MODE = 1;
        break;
    
    case 2:                                     // ---------- ВЫКЛ. ----------
        newValue = OFF;
        break;
        
    case 3:                                     // ---------- МАКС. ----------
        newValue = MAX;
        break;

    default:
        break;
    }
}

// прерывание по кнопке управления (исп. для пробуждения МК)
void isr() {}

// плавный переход яркости (жёсткая ф-ция)
void transit()
{
    setNewValue();                                                      // уст. новое значение яркости
    uint32_t time_trans = millis();
    while (millis() - time_trans <= (TIME_CHANGE + 50))
    {
        led.tick();                                                     // мигаем светодиодом (проверка на необх. изм. сост.)
        NOW_LVL = constrain(easing.SetSetpoint(newValue), 0, 1023);     // вычисляем значение, которое следует установить сейчас
        //NOW_LVL_CRT = getBrightCRT(NOW_LVL);                            // корректируем текущее значение по CRT
        NOW_LVL_CRT = getBrightCRT(map(NOW_LVL, 0, 1023, 0, 255));      // корректируем текущее значение по CRT (8 бит ШИМ)
        analogWrite(LED_PIN_1, NOW_LVL_CRT);                            // выводим текущее значение (скорректирование по CRT)
        //analogWrite(LED_PIN_2, NOW_LVL_CRT);
    }
}

// функция устанавливает новое значение яркости для текущего режима
void set()
{
    int now_lvl = constrain(analogRead(VOL_PIN), 0, 1023);              // читаем текущее значение с потенциометра
    bool restart = true;                                                // флаг выполнения настройки

    uint32_t time_trans = millis();
    while (millis() - time_trans <= (TIME_CHANGE + 50))                 // плавный переход яркости
    {
        led.tick();                                                     // мигаем светодиодом (проверка на необх. изм. сост.)
        int lvl = constrain(easing.SetSetpoint(now_lvl), 0, 1023);      // вычисляем значение, которое следует установить сейчас
        //int lvl_crt = getBrightCRT(lvl);                                // корректируем текущее значение по CRT
        int lvl_crt = getBrightCRT(map(lvl, 0, 1023, 0, 255));          // корректируем текущее значение по CRT (8 бит ШИМ)
        analogWrite(LED_PIN_1, lvl_crt);                                // выводим текущее значение (скорректирование по CRT)
        //analogWrite(LED_PIN_2, lvl_crt);
    }

    while (restart)                                                     // установка нового значения яркости (исп. потенциометр)
    {
        led.tick();                                                     // мигаем светодиодом (проверка на необх. изм. сост.)
        btn.tick();                                                     // опрос кнопки
        if (btn.hasClicks())                                            // проверка на наличие нажатий
        {
            int val = btn.getClicks();                                  // получаем число нажатий
            if (val == 2)                                               // ---------- 2 клика ----------
            {
                led.blink(2, 100, 100);                                 // мигнуть 2 раза
                if (NUM_MODE_ == 0) NUM_MODE_ = 1;                      // меняем активный режим
                else if (NUM_MODE_ == 1) NUM_MODE_ = 0;
            }
            if (val == 3)                                               // ---------- 3 клика ----------
            {                                                           // соранение текущего значения яркости
                restart = false;                                        // выход из ф-ции настройки (по завершению цикла)
                if (NUM_MODE_ == 0) settings.FIRST_LVL = now_lvl;       // сохраняем новое значение яркости
                if (NUM_MODE_ == 1) settings.SECOND_LVL = now_lvl;
            }
            if (val == 4)                                               // ---------- 4 клика ----------
            {
                restart = false;                                        // выход из ф-ции настройки без сохранения значения (по завершению цикла)
            }
        }

        now_lvl = constrain(analogRead(VOL_PIN), 0, 1023);              // вычисляем значение, которое следует установить сейчас
        //int lvl_crt = getBrightCRT(now_lvl);                            // корректируем по CRT
        int lvl_crt = getBrightCRT(map(now_lvl, 0, 1023, 0, 225));      // корректируем по CRT (8 бит ШИМ)
        analogWrite(LED_PIN_1, lvl_crt);                                // выводим текущее значение (скорректирование по CRT)
        //analogWrite(LED_PIN_2, lvl_crt);
    }
    led.blink(3, 100, 100);                                             // мигнуть 3 раза
}

// функция возвращает скорректированное по CRT значение уровня яркости для 10 бит ШИМ / 8 бит ШИМ
int getBrightCRT(int val) {
    //return ((long)val * val * val + 2094081) >> 20;                     // 10 бит
    return ((long)val * val * val + 130305) >> 16;                      // 8 бит
}