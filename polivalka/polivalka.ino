// 1. добавить режимы (когда экран норм будет)
// 2. подключить вифи модуль
// 3. подумать как с одним насосом для нескольких растений сделать
// 4. подставка изи на сервопривод, если окажется тяжеловато можно на шаговый двигатель

//ДАШЕ!!! короче у меня была идея, что отправляются сигналы в виде чисел, их обозначения лежат в enum в polivalka_include.h если есть идея покруче - предлагай  

#include <SoftwareSerial.h>
#include <Wire.h>
#include <iarduino_RTC.h> 

#include "polivalka_include.h"

const int amount_of_mods = 3;

const int relayPin = 13;

const int leftButtonPin = 4;
const int rightButtonPin = 2;
const int ButtonPin = 3;

const int ledPin1 = A4;
const int ledPin2 = A5;

const int inputWiFiPin = 10;  //передача на ардуино от esp
const int outputWiFiPin = 11; //передача от ардуино на esp

// Определение пинов для подключения DS1302
const int RST_PIN = 8;  // Пин для управления
const int DAT_PIN = 7;  // Пин для передачи данных
const int CLK_PIN = 6;  // Пин для синхронизации тактов

int prev_leftBut = 0, current_leftBut = 0;
int prev_rightBut = 0, current_rightBut = 0;
int prev_But = 0, current_But = 0;

int to_write = 1;
int to_write_led1 = 1;
int to_write_led2 = 1;

int amount_of_left = 0, amount_of_right = 0;

long int last_time = 0;

COMMANDS_T command = NOTHING, prev_command = NOTHING; 

SoftwareSerial linkToESP(10, 11); 
// Создаем экземпляр класса для работы с модулем DS1302
iarduino_RTC rtc(RTC_DS1302, RST_PIN, CLK_PIN, DAT_PIN);

void BodyOfWATER()
{
  digitalWrite(relayPin, to_write); 
  to_write = !to_write;   
  last_time = rtc.seconds + rtc.minutes * 60 + rtc.hours * 3600;
  analogWrite(ledPin1, 0);                    
  analogWrite(ledPin2, 0);
}

void BodyOfTIME(int pauseTime)
{
  analogWrite(ledPin1, 255);                    
  analogWrite(ledPin2, 0);  
  
  int input_time = 10; //время работы насоса
  rtc.gettime();

      // Serial.println(rtc.seconds + rtc.minutes * 60 + rtc.hours * 3600 - last_time);


  if (to_write && (rtc.seconds + rtc.minutes * 60 + rtc.hours * 3600 - last_time) >= input_time)
  {
    digitalWrite(relayPin, to_write);
    to_write = !to_write;
    last_time = rtc.seconds + rtc.minutes * 60 + rtc.hours * 3600;
  }

  else if (!to_write && (rtc.seconds + rtc.minutes * 60 + rtc.hours * 3600 - last_time) >= 1069) // 5 это время паузы
  {
    digitalWrite(relayPin, to_write);
    to_write = !to_write;
    last_time = rtc.seconds + rtc.minutes * 60 + rtc.hours * 3600;
  }
}

void GetButtons()
{
  prev_But = current_But;
  current_But = digitalRead(ButtonPin);

  prev_leftBut = current_leftBut;
  current_leftBut = digitalRead(leftButtonPin);

  prev_rightBut = current_rightBut;
  current_rightBut = digitalRead(rightButtonPin);

  if (current_leftBut && prev_leftBut != current_leftBut)
    amount_of_left++;  

  if (current_rightBut && prev_rightBut != current_rightBut)
    amount_of_right++;
}

void setup() {
  Serial.begin(9600);
  rtc.begin(&Wire);

  pinMode(relayPin, OUTPUT);
  pinMode(ledPin1, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  
  pinMode(ButtonPin, INPUT);
  pinMode(leftButtonPin, INPUT);
  pinMode(rightButtonPin, INPUT);

  last_time = rtc.seconds + rtc.minutes * 60 + rtc.hours * 3600;

  linkToESP.begin(9600);  // Для связи с ESP
}
 
void loop() {

  GetButtons();               

  if (current_But && prev_But != current_But && (amount_of_right - amount_of_left) % amount_of_mods == 0) //вместо 0 надо название режима приделать
  {
    BodyOfWATER();               
  }

  else if (current_But && prev_But != current_But && (amount_of_right - amount_of_left) % amount_of_mods == 1)
  {
    Serial.print("Сколько раз в неделю нужно поливать цветы?\nВведите число: "); //пока ввод с ноута, но потом можно будет на дисплее
    while (Serial.available() == 0) {}
    int userNumber = Serial.parseInt();
    int pauseTime = 7 * 24 * 3600 / userNumber;

    while ((amount_of_right - amount_of_left) % amount_of_mods == 1)   
    {
      BodyOfTIME(pauseTime);          
      GetButtons();
      // Serial.print("in while after getbuttons "); отладочная фигня
      // Serial.println(amount_of_right);
      // Serial.println(pauseTime);
    }   
  }

  else if (current_But && prev_But != current_But && (amount_of_right - amount_of_left) % amount_of_mods == 2)
  {
    analogWrite(ledPin1, 255);                    
    analogWrite(ledPin2, 255);                    
  }

  if( millis()%1000==0 ){                                // Если прошла 1 секунда.
         Serial.println(rtc.gettime("d-m-Y, H:i:s, D"));  // Выводим время.
         delay(1);                                          // Приостанавливаем скетч на 1 мс, чтоб не выводить время несколько раз за 1мс.
     } 

  if (linkToESP.available() > 0) //тут блок работы с вай фай модулем, но мы его сносим соуу
  {
    String input = linkToESP.readStringUntil('\n');
    input.trim();

    prev_command = command;
    command = (COMMANDS_T)input.toInt();
      
    Serial.print("Принята команда: ");
    Serial.println(command);
  }

  switch (command) //comes from esp
  {
    case(NOTHING): break;
    case(WATER): if (prev_command != command) 
                  {digitalWrite(relayPin, to_write); //new func
                  to_write = !to_write; }
                  command = NOTHING;
                  break;
    default: command = NOTHING; break;
  }

  
                      
}