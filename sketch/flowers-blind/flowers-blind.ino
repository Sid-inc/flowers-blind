#include <EncButton.h>                                // Библиотека для работы с кнопкой.
#include <GyverStepper.h>                             // Библиотека для работы с шаговыми двигателями.
#include <EEPROM.h>                                   // Библиотека для работы с EEPROM памятью.

// Настройка работы.
const uint8_t BTN_PIN = 2;                            // Пин к которому подключена кнопка (второй вывод кнопки подключен к GND).
const uint16_t HOLD_TIME_OUT = 3000;                  // Время удержания кнопки для переключения режима настройки.
const uint8_t LED_PIN = 13;                           // Пин к которому подключен светодиод.
const uint32_t ledInterval = 1500;                    // Интервал мигания калибровочного светодиода.
const uint32_t ledStepInterval = 150;                 // Интервал мигания калибровочного светодиода для индикации этапа настройки. (1 вспышка - режим 1, 2 вспышки - режим 2).

const uint8_t STEPPER1_EEPROM_ADDR = 0;               // Адрес для сохранения в EEPROM целевого положения первого мотора.
const uint8_t STEPPER2_EEPROM_ADDR = 4;               // Адрес для сохранения в EEPROM целевого положения второго мотора.

// Сервисные переменные.
uint8_t settingMode = 0;                              // Режим калибровки штор. (0 - выключен, 1 - калибровка первой шторы, 2 - калибровка второй шторы).
bool debugMode = true;                                // Режим отладки. При включении режима отладки выводятся отладочные сообщения в COM порт.

uint32_t ledMillis = 0;                               // Переменная для задержки мигания калибровочного светодиода.
uint32_t ledStepMillis = 0;                           // Переменная для задержки мигания калибровочного светодиода для индикации этапа настройки.
uint8_t ledFlashCount = 0;                            // Колчиство миганий светодиода за цикл.
bool ledState = false;                                // Текущий режим светодиода. true - горит, false - не горит.

bool setupRotationDirection = true;                   // Направление вращения мотора в режиме калибровки, true - прямое, false - реверс.
float setupStepperMaxPosition = 0;                    // Верхнее калибровочное положение мотора.
float setupStepperMinPosition = 0;                    // Нижнее калибровочное положение мотора.

float targetPositionStepper1 = 0;                     // Целевое положение первого мотора.
float targetPositionStepper2 = 0;                     // Целевое положение второго мотора.

Button settingButton(BTN_PIN);                        // Кнопка настройки.

GStepper<STEPPER4WIRE> stepper1(2048, 11, 9, 10, 8);  // Моторы с драйвером ULN2003 подключается по порядку пинов, но крайние нужно поменять местами
GStepper<STEPPER4WIRE> stepper2(2048, 7, 5, 6, 4);    // подключено D2-IN1, D3-IN2, D4-IN3, D5-IN4, но в программе 5 и 2 меняются местами.

void setup() {
  // Начало инициализации.
  Serial.begin(115200);
  log("Start init");

  log("Init led");
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  log("Init button");
  settingButton.setHoldTimeout(HOLD_TIME_OUT);

  log("Init stepper 1");
  stepper1.autoPower(true);  // Отключать мотор при достижении цели.
  EEPROM.get(STEPPER1_EEPROM_ADDR, targetPositionStepper1);

  log("Init stepper 2");
  stepper2.autoPower(true);  // Отключать мотор при достижении цели.
  EEPROM.get(STEPPER2_EEPROM_ADDR, targetPositionStepper2);

  // Конец инициализации.
  log("End init");
  digitalWrite(LED_PIN, LOW);
}

void loop() {
  settingButton.tick();
  stepper1.tick();
  stepper2.tick();

  if(settingButton.hold()) {
    changeDebugMode();
  }

  if (settingMode != 0) {
    flashLed();

    if (settingButton.click()) {
      setupStepperRotater();
    }
  }
}

// Переключение режима калибровки.
void changeDebugMode() {
  log("Btn settings holded");
  saveSetup();

  if (settingMode != 2) {
    log("next setting mode");
    settingMode++;
  } else {
    log("exit setting mode");
    settingMode = 0;

    digitalWrite(LED_PIN, LOW);
    ledState = false;
  }
}

// Сохранить настройки калибровки моторов.
void saveSetup() {
  if (settingMode == 1) {
    log("save stepper 1 dist");
    EEPROM.put(STEPPER1_EEPROM_ADDR, setupStepperMaxPosition - setupStepperMinPosition);
    resetSetupVars();
  }
  if (settingMode == 2) {
    log("save stepper 2 dist");
    EEPROM.put(STEPPER2_EEPROM_ADDR, setupStepperMaxPosition - setupStepperMinPosition);
    resetSetupVars();
  }
}

// Сброс калибровочных перменных.
void resetSetupVars() {
  setupRotationDirection = true;
  setupStepperMaxPosition = 0;
  setupStepperMinPosition = 0;   
}

// Вывод лога в порт.
void log(String text) {
  if (debugMode) {
    Serial.println(text);
  }
}

// Мигание светодиода для индикации режима калибровки.
void flashLed() {
  if (millis() - ledMillis > ledInterval) {
    ledMillis = millis();
    resetFlashCount();
  }

  if (ledFlashCount == 0) 
    return;

  if (millis() - ledStepMillis > ledStepInterval) {
    ledStepMillis = millis();

    changeLedState();
    if (!ledState) {
      ledFlashCount = ledFlashCount -1;
    }
  }
}

// Сброс количества вспыщек светодиода.
void resetFlashCount() {
  switch (settingMode) {
    case 0:
      ledFlashCount = 0;
      break;
    case 1:
      ledFlashCount = 1;
      break;
    case 2:
      ledFlashCount = 2;
      break;
  }
}

// Изменить статус светодиода.
void changeLedState() {
  if (ledState) {
    digitalWrite(LED_PIN, LOW);
    ledState = false;
  } else {
    digitalWrite(LED_PIN, HIGH);
    ledState = true;
  }
}

// Калибровочное вращение мотора.
void setupStepperRotater() {
  if (settingMode == 1) {
    setupStepper1();
  }

  if (settingMode == 2) {
    setupStepper2();
  }
}

void setupStepper1() {
  if (!stepper1.getState()) {
    log("start 1 rotate");
    stepper1.setRunMode(KEEP_SPEED);

    if (setupRotationDirection) {
      stepper1.setSpeedDeg(400);
    } else {
      stepper1.setSpeedDeg(-400);
    }
  } else {
    log("stepper 1 stop");
    if (setupRotationDirection) {
      setupStepperMaxPosition = stepper1.getCurrentDeg();
    } else {
      setupStepperMinPosition = stepper1.getCurrentDeg();
    }

    stepper1.brake();
    setupRotationDirection = !setupRotationDirection;
  }
}

void setupStepper2() {
  if (!stepper2.getState()) {
    log("start 2 rotate");
    stepper2.setRunMode(KEEP_SPEED);

    if (setupRotationDirection) {
      stepper2.setSpeedDeg(400);
    } else {
      stepper2.setSpeedDeg(-400);
    }
  } else {
    log("stepper 2 stop");
    if (setupRotationDirection) {
      setupStepperMaxPosition = stepper2.getCurrentDeg();
    } else {
      setupStepperMinPosition = stepper2.getCurrentDeg();
    }

    stepper2.brake();
    setupRotationDirection = !setupRotationDirection;
  }
}
