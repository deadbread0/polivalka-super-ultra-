// 1. добавить режимы (когда экран норм будет)
// 2. подключить вифи модуль
// 3. подумать как с одним насосом для нескольких растений сделать
// 4. подставка изи на сервопривод, если окажется тяжеловато можно на шаговый двигатель

//ДАШЕ!!! короче у меня была идея, что отправляются сигналы в виде чисел, их обозначения лежат в enum в polivalka_include.h если есть идея покруче - предлагай  

#include <SoftwareSerial.h>
#include <Wire.h>
#include <iarduino_RTC.h> 
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

#include "polivalka_include.h"

Servo servo1;
LiquidCrystal_I2C lcd(0x27,16,2);  // Устанавливаем дисплей

const int amount_of_mods = 2;

const int relayPin = 13;

const int leftButtonPin = 4;
const int rightButtonPin = 2;
const int ButtonPin = 3;
const int ExitButtonPin = 5;

const int inputWiFiPin = 10;  //передача на ардуино от esp
const int outputWiFiPin = 11; //передача от ардуино на esp

// Определение пинов для подключения DS1302
const int RST_PIN = 8;  // Пин для управления
const int DAT_PIN = 7;  // Пин для передачи данных
const int CLK_PIN = 6;  // Пин для синхронизации тактов

int prev_leftBut = 0, current_leftBut = 0;
int prev_rightBut = 0, current_rightBut = 0;
int prev_But = 0, current_But = 0;
int prev_exitBut = 0, current_exitBut = 0;

int amount_of_left = 0, amount_of_right = 0;
bool exit_flag = false;
bool water_mode_lcd_flag = true; //флаг для того чтобы печатать выбор этого режима 1 раз в loop
bool auto_poliv_flag = true; //аналогично чтоб на дисплее 1 раз печаталось

COMMANDS_T command = NOTHING, prev_command = NOTHING; 

SoftwareSerial linkToESP(10, 11); 
// Создаем экземпляр класса для работы с модулем DS1302
iarduino_RTC rtc(RTC_DS1302, RST_PIN, CLK_PIN, DAT_PIN);

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
  SetServo(chosen_flower);
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
    SetServo(chosen_flower);
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
  current_But = digitalRead(ButtonPin);

  prev_leftBut = current_leftBut;
  current_leftBut = digitalRead(leftButtonPin);

  prev_rightBut = current_rightBut;
  current_rightBut = digitalRead(rightButtonPin);

  prev_exitBut = current_exitBut;
  current_exitBut = digitalRead(ExitButtonPin);

  if (current_leftBut && prev_leftBut != current_leftBut)
    amount_of_left++;  

  if (current_rightBut && prev_rightBut != current_rightBut)
    amount_of_right++;

  if (current_exitBut && prev_exitBut != current_exitBut)
    exit_flag = true;
}


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
    BodyOfTIME(FLOWERS[i].pause_time, 1 + 1);
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

void SetServo(int chosen_flower)
{
  switch (chosen_flower)
  {
    case 1: servo1.write(0); break;
    case 2: servo1.write(90); break;
    case 3: servo1.write(180); break;
    case 4: servo1.write(-90); break;
  }
}






void setup() {
  Serial.begin(9600);
  rtc.begin(&Wire);

  pinMode(relayPin, OUTPUT);
  
  pinMode(ButtonPin, INPUT);
  pinMode(leftButtonPin, INPUT);
  pinMode(rightButtonPin, INPUT);

  servo1.attach(9);

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

  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("choose mode");

  GetButtons();           
  int chosen_flower = ChooseFlower();
  WaterIfItsTimeForWatering();

  amount_of_left = 0; amount_of_right = 0; //после выбора цветка обнуляем счетчики

  while (!exit_flag)
  {
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