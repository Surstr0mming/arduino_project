#include <Arduino.h>

#include "print3x5.h"

#define PART_AMOUNT 58  // загальна кількість піщинок на полі
#define BTN1_PIN 2
#define BTN2_PIN 3
#define CS_PIN 6
#define DT_PIN 4
#define CK_PIN 5
#define SOUND_PIN 9

#define MaxVolume 1  // максимальний рівень гучності ефектів
#define MaxMelody 5  // кількість наявних мелодій (для коректної роботи меню)

#define battery_min 3000  // мінімальний рівень заряду батареї для відображення 3000
#define battery_max 3800  // максимальний рівень заряду батареї для відображення 3800

// Структура конфігурації для зберігання в EEPROM
struct Data {
  int16_t sec = 60;  // час
  int8_t bri = 2;    // яскравість
  int8_t vol = 1;    // гучність ефектів
  int8_t mel = 1;    // мелодія завершення часу
  int8_t ani = 0;    // анімація завершення часу
  int8_t tmpo = 4;   // темп відтворення мелодій
  int8_t bat = 1;
};
Data data;

#include <EEManager.h>
EEManager memory(data);

// button
#include <EncButton.h>
Button up(BTN1_PIN);
Button down(BTN2_PIN);
VirtButton dbl;

// matrix
#include <GyverMAX7219.h>
MAX7219<2, 1, CS_PIN, DT_PIN, CK_PIN> mtrx;

// mpu
#include "mini6050.h"
Mini6050 mpu;

// sandbox
#define BOX_W 16
#define BOX_H 16

#include "sand.h"
Sand<BOX_W, BOX_H> box;

// timer
#include "Timer.h"
Timer fall_tmr, disp_tmr, menu_tmr;

// ============== ВАШ КОД ==============
// функція викликається при кожному "проштовхуванні" піщинки
void onSandPush() {
#ifdef SOUND_PIN
  StopMelody();  // щоб мелодія не накладалася на звук падаючого піску
  if (data.vol > 0) PlaySandPushTone();
#endif
}

// todo
//    складність у тому, що подія onSandEnd повинна спрацьовувати саме тоді, коли весь пісок пересипався
//    з урахуванням можливих пауз через нахили
//    можливо, тут можна зробити таймер, який буде відлічувати налаштовані секунди, робити паузу за потреби та автоматично обнулятися, коли весь пісок пересипеться
//    або потрібно відстежити кількість частинок на другому екрані
// функція викликається при завершенні відведеного часу
// void onSandEnd() {

// }

// функція викликається, коли пісок перестав сипатися
// це відбувається також через нахил годинника, коли ще не весь пісок пересипався
void onSandStop() {
#ifdef SOUND_PIN
  if (data.mel > 0) PlayMelody(data.mel);
#endif
}

// функція викликається, коли заряд батареї нижче порога
void onBatteryEmpty() {
  mtrx.setBright(0);
  while (true) {
    mtrx.clear();
    printIcon(&mtrx, 0, 0, 6);
    mtrx.update();
    delay(1000);
    mtrx.clear();
    printIcon(&mtrx, 0, 0, 4);
    mtrx.update();
    delay(1000);
  }
}

uint8_t inMenu = 0;
uint8_t lastUsedMenu = 0;    // останнє використане меню
unsigned long voltage;       // напруга акумулятора
float my_vcc_const = 1.080;  // константа вольтметра

// =====================================

bool runFlag = 1;
bool checkBound(int8_t x, int8_t y) {
  if (y >= 8 && x < 8) return 0;
  if (y < 8 && x >= 8) return 0;
  if (mpu.getDir() > 0) {
    if (x == 8 && y == 8) return 0;
  } else {
    if (x == 7 && y == 7) return 0;
  }
  return (x >= 0 && y >= 0 && x < BOX_W && y < BOX_H);
}

void setXY(int8_t x, int8_t y, bool value) {
  if (y >= 8) y -= 8;
  mtrx.dot(x, y, value);
}

void resetSand() {
  box.buf.clear();
  mtrx.clear();
  mtrx.update();
  for (uint8_t n = 0; n < PART_AMOUNT; n++) {
    box.buf.set(n % 8, n / 8, 1);
  }
}

void setup() {
  // Serial.begin(115200);
  Wire.begin();
  mpu.begin();
  memory.begin(0, 'a');
  mtrx.begin();
  mtrx.setBright(data.bri);

  mpu.setX({ 1, -1 });
  mpu.setY({ 2, 1 });
  mpu.setZ({ 0, 1 });

  box.attachBound(checkBound);
  box.attachSet(setXY);

#ifdef SOUND_PIN
  pinMode(SOUND_PIN, OUTPUT);
  SetTempo(data.tmpo);
#endif

  voltage = readVcc();  // зчитати напругу живлення
  // Serial.println(voltage);
  if (voltage <= battery_min) onBatteryEmpty();
  showBattLevel(voltage);
  delay(2000);  // час відображення заряду після увімкнення

  resetSand();
  fall_tmr.setInterval(data.sec * 1000ul / PART_AMOUNT);
}

void changeTime(int8_t dir) {
  disp_tmr.setTimeout(3000);
  mtrx.clear();
  data.sec += dir;
  if (data.sec < 0) data.sec = 0;
  uint8_t min = data.sec / 60;
  uint8_t sec = data.sec % 60;

  printDig(&mtrx, 0, 1, min / 10);
  printDig(&mtrx, 4, 1, min % 10);
  printDig(&mtrx, 8 + 0, 1, sec / 10);
  printDig(&mtrx, 8 + 4, 1, sec % 10);

  fall_tmr.setInterval(data.sec * 1000ul / PART_AMOUNT);
  memory.update();
  mtrx.update();
}

// зміна темпу
void changeTempo(int8_t dir) {
  data.tmpo += dir;
  data.tmpo = constrain(data.tmpo, 1, 7);
  SetTempo(data.tmpo);
  PlayMelody(data.mel);
  memory.update();
}

// зміна анімації
void changeAni(int8_t dir) {
  data.ani += dir;
  data.ani = constrain(data.ani, 0, 1);
  memory.update();
}
// зміна яскравості
void changeBri(int8_t dir) {
  data.bri += dir;
  data.bri = constrain(data.bri, 0, 15);
  mtrx.setBright(data.bri);
  memory.update();
}

// зміна гучності
void changeVol(int8_t dir) {
  data.vol += dir;
  data.vol = constrain(data.vol, 0, MaxVolume);
  memory.update();
}

// зміна мелодії
void changeMel(int8_t dir) {
  data.mel += dir;
  data.mel = constrain(data.mel, 0, MaxMelody);
  if (data.mel) PlayMelody(data.mel);
  else StopMelody();
  memory.update();
}

// зміна стилю відображення заряду
void changeBat(int8_t dir) {
  data.bat += dir;
  data.bat = constrain(data.bat, 0, 2);
  memory.update();
}

void enterMenu(int8_t menu) {
  disp_tmr.setTimeout(returnFromMenu, 5000);
  mtrx.clear();

  // тут налаштовується число доступних пунктів меню
  if (menu > 5) menu = 1;
  if (menu < 1) menu = 5;

  inMenu = menu;
  menu_tmr.setTimeout(forgetLastMenu, 30000);

  showMenu(menu);
}

void forgetLastMenu() {
  lastUsedMenu = 0;
}

void showMenu(int8_t menu) {
  // виводимо іконку меню на перший екран
  // і поточне значення параметра на другий
  switch (menu) {
    case 1:  // яскравість
      printIcon(&mtrx, 0, 0, 2);
      printDig(&mtrx, 8 + 0, 1, data.bri / 10);
      printDig(&mtrx, 8 + 4, 1, data.bri % 10);
      break;

    case 2:  // гучність звуків
      printIcon(&mtrx, 0, 0, 0);
      printDig(&mtrx, 8 + 0, 1, data.vol / 10);
      printDig(&mtrx, 8 + 4, 1, data.vol % 10);
      break;

    case 3:  // мелодія завершення
      printIcon(&mtrx, 0, 0, 1);
      printDig(&mtrx, 8 + 0, 1, data.mel / 10);
      printDig(&mtrx, 8 + 4, 1, data.mel % 10);
      break;

    case 4:  // темп мелодій
      printIcon(&mtrx, 0, 0, 5);
      printDig(&mtrx, 8 + 0, 1, data.tmpo / 10);
      printDig(&mtrx, 8 + 4, 1, data.tmpo % 10);
      break;

    case 5:  // стиль відображення заряду при увімкненні
      printIcon(&mtrx, 0, 0, 6);
      printDig(&mtrx, 8 + 0, 1, data.bat / 10);
      printDig(&mtrx, 8 + 4, 1, data.bat % 10);
      break;

    case 6:  // анімації
      printIcon(&mtrx, 0, 0, 3);
      printDig(&mtrx, 8 + 0, 1, data.ani / 10);
      printDig(&mtrx, 8 + 4, 1, data.ani % 10);
      break;
  }
  mtrx.update();
}

// Відображення рівня заряду у відсотках або вольтах
void showBattLevel(long voltage) {
  int8_t level = 0;
  switch (data.bat) {
    case 1:
      level = map(voltage, battery_min, battery_max, 0, 99);
      level = constrain(level, 0, 99);
      mtrx.clear();
      printIcon(&mtrx, 0, 0, 4);
      printDig(&mtrx, 8 + 0, 1, level / 10);
      printDig(&mtrx, 8 + 4, 1, level % 10);
      mtrx.update();
      break;
    case 2:
      mtrx.clear();
      printDig(&mtrx, 0, 1, voltage / 1000);
      printDig(&mtrx, 4, 1, (voltage % 1000) / 100);
      printDig(&mtrx, 8 + 0, 1, (voltage % 100) / 10);
      printDig(&mtrx, 8 + 4, 1, (voltage % 100) % 10);
      mtrx.update();
      delay(2000);
      mtrx.clear();
      break;
  }
}
// зміна параметра меню
void changeMenuParam(int8_t menu, int8_t val) {
  disp_tmr.setTimeout(returnFromMenu, 5000);  // оновлюємо таймер
  mtrx.clear();                               // очищуємо екран

  switch (menu) {
    case 1:            // яскравість
      changeBri(val);  // зберігаємо нове значення яскравості
      break;
    case 2:  // гучність звуків
      changeVol(val);
      break;
    case 3:  // мелодія завершення
      changeMel(val);
      break;

    case 4:              // темп мелодій
      changeTempo(val);  // зберігаємо нове значення темпу
      break;

    case 5:  // відображення заряду
      changeBat(val);
      break;

    case 6:  // анімація завершення
      changeAni(val);
      // todo
      // Варіанти:
      // - мигання піску (увімк./вимк.)
      // - плавне мигання піску (гра з яскравістю)
      break;
  }
  showMenu(menu);
}

// колбек для виходу з меню по тайм-ауту
void returnFromMenu() {
  lastUsedMenu = inMenu;  // запам'ятовуємо останнє використане меню
  inMenu = 0;
}

// обробник кнопок
void buttons() {
  up.tick();
  down.tick();
  dbl.tick(up, down);

  if (!inMenu) {
    if (dbl.hold()) enterMenu(lastUsedMenu ? lastUsedMenu : 1);
    if (dbl.click()) resetSand();

    // зупиняємо відтворення мелодії при натисканні на будь-яку кнопку
    if (up.click()) isMelodyPlaying() ? StopMelody() : changeTime(1);
    if (up.step()) changeTime(10);
    if (up.hold()) changeTime(10);

    // зупиняємо відтворення мелодії при натисканні на будь-яку кнопку
    if (down.click()) isMelodyPlaying() ? StopMelody() : changeTime(-1);
    if (down.step()) changeTime(-10);
    if (down.hold()) changeTime(-10);
  } else {
    if (dbl.click()) returnFromMenu();
    if (up.hold()) enterMenu(inMenu + 1);
    if (down.hold()) enterMenu(inMenu - 1);
    if (up.click()) changeMenuParam(inMenu, 1);
    if (down.click()) changeMenuParam(inMenu, -1);
  }
}

void step() {
  uint16_t prd = 255 - mpu.getMag();
  prd = constrain(prd, 15, 90);
  if (mpu.update(prd)) {
    mtrx.clear();
    box.step(mpu.getAngle() + 45);
    mtrx.update();
  }
}

void fall() {
  if (fall_tmr) {
    bool pushed = 0;
    if (mpu.getDir() > 0) {
      if (box.buf.get(7, 7) && !box.buf.get(8, 8)) {
        box.buf.set(7, 7, 0);
        box.buf.set(8, 8, 1);
        box.setCallback(7, 7, 0);
        box.setCallback(8, 8, 1);
        pushed = 1;
      }
    } else {
      if (box.buf.get(8, 8) && !box.buf.get(7, 7)) {
        box.buf.set(8, 8, 0);
        box.buf.set(7, 7, 1);
        box.setCallback(8, 8, 0);
        box.setCallback(7, 7, 1);
        pushed = 1;
      }
    }

    if (pushed) {
      runFlag = 0;
      mtrx.update();
      onSandPush();
    } else {
      if (!runFlag) {
        runFlag = 1;
        onSandStop();
      }
    }
  }
}

// Головний цикл
void loop() {
  memory.tick();
  disp_tmr.tick();
#ifdef SOUND_PIN
  soundsTick();
#endif
  buttons();

  if (!disp_tmr.state()) {
    step();
    fall();
  }
}

unsigned long readVcc() {  // функція читання внутрішньої опорної напруги, універсальна (для всіх Arduino)
#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#elif defined(__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
  ADMUX = _BV(MUX5) | _BV(MUX0);
#elif defined(__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
  ADMUX = _BV(MUX3) | _BV(MUX2);
#else
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#endif
  delay(2);             // Зачекайте для стабілізації Vref
  ADCSRA |= _BV(ADSC);  // Початок конверсії
  while (bit_is_set(ADCSRA, ADSC))
    ;                   // вимірювання
  uint8_t low = ADCL;   // спершу потрібно прочитати ADCL - тоді він блокує ADCH
  uint8_t high = ADCH;  // розблоковує обидва
  long result = (high << 8) | low;
  result = my_vcc_const * 1023 * 1000 / result;  // розрахунок реального VCC
  return result;                                 // повертає VCC
}
