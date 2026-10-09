
// распиновка
// A0 - термометр
// A1 - кнопки влево/вправо
// A2 - тензодатчик (coming soon)
// A3 - тензодатчик (coming soon)
// A4 - дисплей (i2c)
// A5 - дисплей (i2c)
// 0 - блютуз модуль
// 1 - блютуз модуль
// 2 - концевик
// 3 - кнопка выключения экрана (coming soon)
// 4 - кнопка выбора (центральная)
// 5 - кнопка выхода
// 6 - модуль реального времени
// 7 - модуль реального времени
// 8 - модуль реального времени
// 9 - шаговый двигатель 
// 10 - шаговый двигатель
// 11 - шаговый двигатель
// 12 - шаговый двигатель (питание от батарейки)
// 13 - реле (насос) (насос питается от батарейки)

#include <SoftwareSerial.h>
#include <Wire.h>
#include <iarduino_RTC.h> 
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <OneWire.h>

#include "polivalka_include.h"

Servo servo1;
LiquidCrystal_I2C lcd(0x27,16,2);  // Устанавливаем дисплей

const int amount_of_mods = 2;
bool isChoicePressed = false;

const int relayPin = 13;

const int ButtonPin = 4;
const int ExitButtonPin = 5;
const int ButtonsPin = A1; //для кнопок вправо, влево и выкл

const int inputWiFiPin = 10;  //передача на ардуино от esp
const int outputWiFiPin = 11; //передача от ардуино на esp

// Определение пинов для подключения DS1302
const int RST_PIN = 8;  // Пин для управления
const int DAT_PIN = 7;  // Пин для передачи данных
const int CLK_PIN = 6;  // Пин для синхронизации тактов

int prev_analogBut = 0, current_analogBut = 0;
int prev_But = 0, current_But = 0; //кнопка выбора
int current_But_of_choice = 0;
int prev_exitBut = 0, current_exitBut = 0;

int amount_of_left = 0, amount_of_right = 0;
bool exit_flag = false;
bool water_mode_lcd_flag = true; //флаг для того чтобы печатать выбор этого режима 1 раз в loop
bool auto_poliv_flag = true; //аналогично чтоб на дисплее 1 раз печаталось

int prev_corner = 0, current_corner = 0; //отсчитывать углы поворота для шагового двигателя
const int fl4_corner = 0;
const int fl3_corner = 90;
const int fl2_corner = 180;
const int fl1_corner = 270;

const int in1 =12;
const int in2 = 11;
const int in3 = 10;
const int in4 = 9;
const bool stepLookup[8][4] = {
  {1, 0, 0, 0},
  {1, 1, 0, 0},
  {0, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 0},
  {0, 0, 1, 1},
  {0, 0, 0, 1},
  {1, 0, 0, 1}
};

int currentStep = 0; 
const double STEPS_PER_DEGREE = 11.321591; 
double stepAccumulator = 0.0;

COMMANDS_T command = NOTHING, prev_command = NOTHING; 

SoftwareSerial linkToESP(10, 11); 
// Создаем экземпляр класса для работы с модулем DS1302
iarduino_RTC rtc(RTC_DS1302, RST_PIN, CLK_PIN, DAT_PIN);

OneWire ds(14); // Объект OneWire

int temperature = 0; // Глобальная переменная для хранения значения температуры

unsigned long lastUpdateTime = 0; // Для millis() всегда используйте unsigned long
const int TEMP_UPDATE_TIME = 1000; // Периодичность опроса (1 секунда)

struct flower_t {
  long int last_time;
  bool mode_time;
  int to_write;
  int pause_time;
};

struct flower_t FLOWERS[4] = {};

// -------------------------------------------------------------------------------------------------------------
// 
// void DrawArrows()
// 
// рисует стрелочки типа чтоб пользователь понимал где право, где лево и тд и тп
// -------------------------------------------------------------------------------------------------------------
void DrawArrows()
{
  lcd.setCursor(1, 0);
  lcd.print("_");
  lcd.setCursor(0, 0);
  lcd.print("/");
  lcd.setCursor(0, 1);
  lcd.write(0);

  lcd.setCursor(14, 0);
  lcd.print("_");
  lcd.setCursor(15, 0);
  lcd.write(0);
  lcd.setCursor(15, 1);
  lcd.print("/");
}

// -------------------------------------------------------------------------------------------------------------
// 
// void DrawPlusMinus()
// 
// рисует - в левом краю дисплея, + в правом
// -------------------------------------------------------------------------------------------------------------
void DrawPlusMinus()
{
  lcd.setCursor(0, 0);
  lcd.print("-");

  lcd.setCursor(15, 0);
  lcd.print("+");
}

// -------------------------------------------------------------------------------------------------------------
// 
// void BodyOfWATER(int chosen_flower)
// 
// полив вкл/выкл
// -------------------------------------------------------------------------------------------------------------
void BodyOfWATER(int chosen_flower)
{
  // SetServo(chosen_flower);
  SetStepMotor(chosen_flower);
  int to_write = FLOWERS[chosen_flower - 1].to_write;
  digitalWrite(relayPin, to_write); 
  FLOWERS[chosen_flower - 1].to_write = !to_write;  

  if (FLOWERS[chosen_flower - 1].to_write)
  {
    FLOWERS[chosen_flower - 1].last_time = rtc.seconds + rtc.minutes * 60 + rtc.hours * 3600;

    lcd.clear();
    DrawArrows();
    lcd.setCursor(5, 1);
    lcd.print("ON");
  } 
  else
  {
    lcd.clear();
    DrawArrows();
    lcd.setCursor(5, 1);
    lcd.print("OFF");
  }
}

// -------------------------------------------------------------------------------------------------------------
// 
// void BodyOfTIME(long int pauseTime, int chosen_flower)
// 
// полив по времени
// -------------------------------------------------------------------------------------------------------------

void BodyOfTIME(long int pauseTime, int chosen_flower)
{
  FLOWERS[chosen_flower - 1].pause_time = pauseTime;

  if (pauseTime == 0)
  {
    FLOWERS[chosen_flower - 1].mode_time = false;
    return;
  }

  FLOWERS[chosen_flower - 1].mode_time = true;
  
  int input_time = 10; //время работы насоса позже подберем нужное
  rtc.gettime();

  int to_write = FLOWERS[chosen_flower - 1].to_write;
  long int last_time = FLOWERS[chosen_flower - 1].last_time;

      Serial.println(rtc.seconds + rtc.minutes * 60 + rtc.hours * 3600 - last_time);
      Serial.println(to_write);
      Serial.println(pauseTime);


  if (to_write && (rtc.seconds + rtc.minutes * 60 + rtc.hours * 3600 - last_time) >= input_time)
  {
    digitalWrite(relayPin, !to_write);
    FLOWERS[chosen_flower - 1].to_write = !to_write;
    FLOWERS[chosen_flower - 1].last_time = rtc.seconds + rtc.minutes * 60 + rtc.hours * 3600;
  }

  else if (!to_write && (rtc.seconds + rtc.minutes * 60 + rtc.hours * 3600 - last_time) >= pauseTime)
  {
    // SetServo(chosen_flower);
    SetStepMotor(chosen_flower);
    digitalWrite(relayPin, !to_write);
    FLOWERS[chosen_flower - 1].to_write = !to_write;
    FLOWERS[chosen_flower - 1].last_time = rtc.seconds + rtc.minutes * 60 + rtc.hours * 3600;
  }

}

// -------------------------------------------------------------------------------------------------------------
// 
// void GetButtons()
// 
// меняет глобальные переменные состояния кнопок
// -------------------------------------------------------------------------------------------------------------


void GetButtons()
{
  prev_But = current_But;
  current_But = digitalRead(ButtonPin);//оставить

  prev_exitBut = current_exitBut;
  current_exitBut = digitalRead(ExitButtonPin); //оставить

  if (current_exitBut && prev_exitBut != current_exitBut)
    exit_flag = true;
  
  int analogVal = analogRead(ButtonsPin);
  int detectedButton = 0;

  if      (analogVal >= 320 && analogVal <= 350)  detectedButton = 1; // Вправо
  else if (analogVal >= 640 && analogVal <= 670)  detectedButton = 2; // Влево

  prev_analogBut = current_analogBut;
  current_analogBut = detectedButton;

  if (current_analogBut == 0 && prev_analogBut != 0) 
  {
    switch (prev_analogBut) 
    {
      case 1: // Была нажата кнопка 5 (Вправо)
        amount_of_right++;
        break;

      case 2: // Была нажата кнопка 3 (Влево)
        amount_of_left++;
        break;

    }
  }
  
  // Небольшая задержка против дребезга контактов
  delay(20);
}


// void GetButtons()
// {
//   // 1) 640-670
//   // 2) 590-620+
//   // 3) 440-470+
//   // 4) 320-350+
//   // 5) 210-230+

//   // prev_But = current_But;
//   int current_But0 = analogRead(ButtonsPin);

//   Serial.println(current_But0);
//   if (current_But != prev_But)
//   {
//     if ((current_But0 <= 230) && (current_But0 >= 210))
//     {
//       amount_of_right++;
//       prev_But = current_But;
//       current_But = 5;
//     }

//     else if ((current_But0 <= 470) && (current_But0 >= 440))
//     {
//       amount_of_left++;  
//       prev_But = current_But;
//       current_But = 3;
//     }

//     else if ((current_But0 <= 620) && (current_But0 >= 590))
//     {
//       current_But_of_choice = 1;
//       prev_But = current_But;
//       current_But = 2;
//     }

//     else if ((current_But0 <= 350) && (current_But0 >= 320))
//     {
//       exit_flag = true;
//       prev_But = current_But;
//       current_But = 4;
//     }
//   }
// }


// -------------------------------------------------------------------------------------------------------------
// 
// int ChooseFlower()
// 
// пользователь может пролистать все цветы и выбрать номер, которому хочет выставить режим
// функция возвращает номер выбранного цветка
// -------------------------------------------------------------------------------------------------------------

int ChooseFlower()
{
  lcd.clear();
  int chosen_flower = 0;
  while (!chosen_flower)
  {
    WaterIfItsTimeForWatering();

    lcd.setCursor(2, 0);
    lcd.print("choose plant");
    DrawArrows();

    GetButtons();
    if (Abs(amount_of_right - amount_of_left) % 4 == 0)
    {
      lcd.setCursor(8, 1);
      lcd.print("4");
      if (current_But && prev_But != current_But)
        chosen_flower = 4;
    }

    else if (Abs(amount_of_right - amount_of_left) % 4 == 1)
    {
      lcd.setCursor(8, 1);
      lcd.print("1");
      if (current_But && prev_But != current_But)
        chosen_flower = 1;
    }

    else if (Abs(amount_of_right - amount_of_left) % 4 == 2)
    {
      lcd.setCursor(8, 1);
      lcd.print("2");
      if (current_But && prev_But != current_But)
        chosen_flower = 2;
    }

    else if (Abs(amount_of_right - amount_of_left) % 4 == 3)
    {
      lcd.setCursor(8, 1);
      lcd.print("3");
      if (current_But && prev_But != current_But)
        chosen_flower = 3;
    }
  }
  return chosen_flower;
}


// -------------------------------------------------------------------------------------------------------------
// 
// void ChooseMode(int chosen_flower)
// 
// на дисплее появляется надпись "выберите режим", нажимая на боковые кнопки можно ознакомиться со всеми режимами
// для выбора режима надо тыкнуть на кнопку посередине
// для выхода к выбору другого цветка надо нажать на кнопку exit
// -------------------------------------------------------------------------------------------------------------

void ChooseMode(int chosen_flower)
{

  if (Abs(amount_of_right - amount_of_left) % amount_of_mods == 0) //вместо 0 надо название режима приделать
  {  
    if (water_mode_lcd_flag)
    {
      lcd.clear();
      DrawArrows();
      lcd.setCursor(2, 1);
      lcd.print("ON/OFF mode");
      water_mode_lcd_flag = false;
    }

    if (current_But && prev_But != current_But)         
      BodyOfWATER(chosen_flower); 
                 
    auto_poliv_flag = true;
  }

  else if (Abs(amount_of_right - amount_of_left) % amount_of_mods == 1)
  {
    if (auto_poliv_flag) //печать режима на выбор
    {
      lcd.clear();
      DrawArrows();
      lcd.setCursor(2, 1);
      lcd.print("auto mode");
      auto_poliv_flag = false;
    }

    if (current_But && prev_But != current_But) //если режим выбран
    {
      lcd.clear();
      DrawPlusMinus();
      lcd.setCursor(2, 1);
      lcd.print("times/week");
      lcd.setCursor(7, 0);
      lcd.print(0);
      int userNumber = 0;
      int amount_of_pluses = 0, amount_of_minuses = 0;
      int init_amount_of_right = amount_of_right; // amount_of_right - init_amount_of_right = amount_of_pluses
      int init_amount_of_left = amount_of_left; // amount_of_left - init_amount_of_left = amount_of_minuses
      GetButtons();

      while (!(current_But && prev_But != current_But)) //пока не нажато ок - выбор числа (число на дисплее увеличивается в реальном времени от нажатия боковых кнопок)
      {
        GetButtons();
        if (current_exitBut && prev_exitBut != current_exitBut)
        {
          water_mode_lcd_flag = true;
          exit_flag = true;
          return;
        }

        userNumber = (amount_of_left - init_amount_of_left) - (amount_of_right - init_amount_of_right);
        
        if (userNumber >= 0)
        {
          // печатаем число посередине 1й строки
          lcd.setCursor(4, 0);
          lcd.print("   ");
          lcd.setCursor(7, 0);
          lcd.print(userNumber);
        }
        else
        {
          lcd.setCursor(4, 0);
          lcd.print("       ");
          lcd.setCursor(4, 0);
          lcd.print("ban");
          userNumber = -1;
          continue;
        }
      }

      amount_of_left = init_amount_of_left;
      amount_of_right = init_amount_of_right;

      long int pauseTime = 7 * 24 * 3600 / userNumber;
      FLOWERS[chosen_flower - 1].to_write = 0;

      while (Abs(amount_of_right - amount_of_left) % amount_of_mods == 1)   
      {
        BodyOfTIME(pauseTime, chosen_flower);          
        GetButtons();

        if (current_exitBut && prev_exitBut != current_exitBut)
        {
          water_mode_lcd_flag = true;
          exit_flag = true;
          return;
        }
      }  
    }
    water_mode_lcd_flag = true; 
  }
}

// -------------------------------------------------------------------------------------------------------------
// 
// void WaterIfItsTimeForWatering()
// 
// функция проверяет, не надо ли полить какой-то цветок, который выставлен в авто-режим, и если надо - поливает
// -------------------------------------------------------------------------------------------------------------
void WaterIfItsTimeForWatering()
{
  for (int i = 0; i < 4; i++)
  {
    BodyOfTIME(FLOWERS[i].pause_time, i + 1);
  }
}

// -------------------------------------------------------------------------------------------------------------
// 
// int Abs(int num)
// 
// считает модуль, моя любимая функция в этом проекте
// -------------------------------------------------------------------------------------------------------------
int Abs(int num)
{
  return (num > 0) ? num : num * (-1);
}

// void SetServo(int chosen_flower)
// {
//   switch (chosen_flower)
//   {
//     case 1: servo1.write(0); break;
//     case 2: servo1.write(90); break;
//     case 3: servo1.write(180); break;
//     case 4: servo1.write(270); break;
//   }
// }

void SetStepMotor(int chosen_flower)
{
  if (chosen_flower == 4)
  {
    prev_corner = current_corner;
    current_corner = fl4_corner - prev_corner;
    rotateDegrees(current_corner);
    if (current_corner < 0)
      current_corner = 360 + current_corner;
  }
  else if (chosen_flower == 3)
  {
    prev_corner = current_corner;
    current_corner = fl3_corner - prev_corner;
    rotateDegrees(current_corner);
    if (current_corner < 0)
      current_corner = 360 + current_corner;
  }
  else if (chosen_flower == 2)
  {
    prev_corner = current_corner;
    current_corner = fl2_corner - prev_corner;
    rotateDegrees(current_corner);
    if (current_corner < 0)
      current_corner = 360 + current_corner;
  }
  else if (chosen_flower == 1)
  {
    prev_corner = current_corner;
    current_corner = fl1_corner - prev_corner;
    rotateDegrees(current_corner);
    if (current_corner < 0)
      current_corner = 360 + current_corner;
  }
}






void setup() {
  Serial.begin(9600);
  rtc.begin(&Wire);

  pinMode(relayPin, OUTPUT);
  
  pinMode(ButtonsPin, INPUT);
  pinMode(ButtonPin, INPUT);
  pinMode(ExitButtonPin, INPUT);

  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);

  ds.reset();
  ds.write(0xCC);
  ds.write(0x44); 

  // servo1.attach(9);

  for (int i = 0; i < 4; i++)
  {
    FLOWERS[i].last_time = rtc.seconds + rtc.minutes * 60 + rtc.hours * 3600;
  }

  linkToESP.begin(9600);  // Для связи с ESP

  lcd.init();
  lcd.backlight();// Включаем подсветку дисплея
  lcd.createChar(0, backslash);
  // lcd.setCursor(3, 1);
  // lcd.print("bybildo4ka");
}
 





void loop() {

  detectTemperature();
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("choose mode");
  detectTemperature();

  GetButtons();           
  int chosen_flower = ChooseFlower();
  WaterIfItsTimeForWatering();

  amount_of_left = 0; amount_of_right = 0; //после выбора цветка обнуляем счетчики

  while (!exit_flag)
  {
    detectTemperature();
    GetButtons();           
    ChooseMode(chosen_flower);
    WaterIfItsTimeForWatering();
  }

  exit_flag = false;
  
  if( millis()%1000==0 ){                                // Если прошла 1 секунда.
         Serial.println(rtc.gettime("d-m-Y, H:i:s, D"));  // Выводим время.
         delay(1);                                          // Приостанавливаем скетч на 1 мс, чтоб не выводить время несколько раз за 1мс.
     } 

                      
}



// я верю гугл ии, что это работает
// Функция точного поворота на заданный угол (можно передавать и отрицательные углы)
void rotateDegrees(double degrees) {
  // Вычисляем точное плавающее количество шагов для этого поворота
  double targetSteps = degrees * STEPS_PER_DEGREE;
  
  // Добавляем остаток от прошлых поворотов
  targetSteps += stepAccumulator;
  
  // Округляем до ближайшего целого числа шагов, которое мы физически можем сделать
  long stepsToRun = round(targetSteps);
  
  // Сохраняем новую дробную погрешность на будущее
  stepAccumulator = targetSteps - stepsToRun;

  // Направление вращения
  int direction = (stepsToRun > 0) ? 1 : -1;
  stepsToRun = abs(stepsToRun);

  // Выполняем шаги
  for (long i = 0; i < stepsToRun; i++) {
    currentStep += direction;
    
    // Зацикливаем фазы от 0 до 7
    if (currentStep > 7) currentStep = 0;
    if (currentStep < 0) currentStep = 7;
    
    setOutput(currentStep);
    delayMicroseconds(1200); // Скорость вращения (меньше — быстрее, но не меньше 1000)
  }
  
  // Отключаем пины после движения, чтобы мотор не грелся
  stopMotor();
}

// Функция подачи сигналов на пины
void setOutput(int out) {
  digitalWrite(in1, stepLookup[out][0]);
  digitalWrite(in2, stepLookup[out][1]);
  digitalWrite(in3, stepLookup[out][2]);
  digitalWrite(in4, stepLookup[out][3]);
}

// Функция снятия напряжения с обмоток
void stopMotor() {
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, LOW);
}

void detectTemperature(){
  // Проверяем, прошло ли 1000 мс с момента последнего считывания
  if (millis() - lastUpdateTime >= TEMP_UPDATE_TIME)
  {
    lastUpdateTime = millis();
    
    byte data[2];
    
    // 1. Читаем данные предыдущего измерения
    ds.reset();
    ds.write(0xCC);
    ds.write(0xBE); // Команда чтения памяти датчика
    data[0] = ds.read(); // Младший байт (LSB)
    data[1] = ds.read(); // Старший байт (MSB)

    // 2. Формируем целочисленное значение температуры
    int raw = (data[1] << 8) | data[0];
    
    // Обработка отрицательных температур для 12-битного режима
    if (raw & 0x8000) {
      raw = raw | 0xFFFF0000;
    }
    
    temperature = raw >> 4; // Сдвиг на 4 бита убирает дробную часть

    // Выводим значение в порт сразу после успешного обновления
    Serial.print("Temperature: ");
    Serial.println(temperature);

    // 3. Сразу запускаем СЛЕДУЮЩЕЕ измерение. 
    // Датчику DS18B20 нужно до 750 мс, чтобы подготовить новые данные.
    ds.reset();
    ds.write(0xCC);
    ds.write(0x44); 
  }
}