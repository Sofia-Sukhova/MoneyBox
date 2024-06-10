#include <GyverStepper.h>
#include <EEPROM.h>
#include <PinChangeInterrupt.h>
#include <string.h>
#include <LiquidCrystal_I2C.h>
GStepper<STEPPER4WIRE> stepper(2048, 5, 3, 4, 2);
LiquidCrystal_I2C lcd(0x27,20,4);

#define inSEN A0
#define outSEN A3
#define led 7
#define button A2
#define pin_to_reset_sum A1

#define TIME_TO_FALL 50
#define TIME_TO_CHECK 0
#define COUNT_TO_REFRESH 100

const int border = 10;
const int max_step = 20;

int data;

volatile int flag = 0;

int money[9] = {0}; // 8 монет, 9 - для отработки ошибокы

#define VALUE_FOR_1_K   -100 
#define VALUE_FOR_10_K  -850
#define VALUE_FOR_5_K   -60
#define VALUE_FOR_50_K  -50
#define VALUE_FOR_1_R   -65
#define VALUE_FOR_10_R  -40
#define VALUE_FOR_2_R   -80
#define VALUE_FOR_5_R   -200

#define RETURN VALUE_FOR_1_K + VALUE_FOR_10_K + VALUE_FOR_5_K + VALUE_FOR_50_K + VALUE_FOR_1_R + VALUE_FOR_2_R + VALUE_FOR_5_R + VALUE_FOR_10_R - 100

#define TARGER_MESSAGE "Budget"

int current_screen = 0;
int change_screen = 0;
int refresh = 0;

uint8_t attachPCINT(uint8_t pin) {
  if (pin < 8) {            // D0-D7 (PCINT2)
    PCICR |= (1 << PCIE2);
    PCMSK2 |= (1 << pin); return 2; } else if (pin > 13) {    //A0-A5 (PCINT1)
    PCICR |= (1 << PCIE1);
    PCMSK1 |= (1 << pin - 14);
    return 1;
  } else {                  // D8-D13 (PCINT0)
    PCICR |= (1 << PCIE0);
    PCMSK0 |= (1 << pin - 8);
    return 0;
  }
}

ISR(PCINT1_vect) {  // пины A0-A5
  flag = 1;
}

void my_rotate(int R, int W) {
  stepper.setTarget(R , RELATIVE);

  for (int i = 0; i < abs(R) * 2; i++){
    stepper.tick();
    delay(1);
  }
  delay(W);
}

void comeback() {
  stepper.setTarget(abs(RETURN));

  for (int i = 0; i < 2* abs(RETURN); i++){
    stepper.tick();
    delay(1);
  }
}

bool check(int * mass, int R, int W, int Y){
  int data = analogRead(inSEN);
  if (data > 100) {
      my_rotate(R, W);
    } else {
      comeback();
      return 1;
    }
    delay(Y);
    return 0;
}

float cal_sum (){
  return money[0]*0.01 + money[1]*0.05 + money[2]*0.1 + money[3]*0.5 + money[4] + money[5]*2 + money[6]*5 + money[7]*10;
}

void sc_greeter(){
  lcd.clear();
  lcd.setCursor(10-strlen(TARGER_MESSAGE)/2,0);
  lcd.print(TARGER_MESSAGE);
  
  String sum(cal_sum(), 2);

  lcd.setCursor(10-sum.length()/2,1);
  lcd.print(sum);
}

void sc_counter(){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("0.01-");
  lcd.print(money[0]);
  lcd.setCursor(0,1);
  lcd.print("0.05-");
  lcd.print(money[1]);
  lcd.setCursor(0,2);
  lcd.print(" 0.1-");
  lcd.print(money[2]);
  lcd.setCursor(0,3);
  lcd.print(" 0.5-");
  lcd.print(money[3]);
  lcd.setCursor(10,0);
  lcd.print(" 1-");
  lcd.print(money[4]);
  lcd.setCursor(10,1);
  lcd.print(" 2-");
  lcd.print(money[5]);
  lcd.setCursor(10,2);
  lcd.print(" 5-");
  lcd.print(money[6]);
  lcd.setCursor(10,3);
  lcd.print("10-");
  lcd.print(money[7]);
}

void setup() {    
  EEPROM.get(0, money);

  lcd.init();                       //  Инициируем работу с LCD дисплеем
  lcd.backlight();                  //  Включаем подсветку LCD дисплея
  lcd.setCursor(0,0);
  lcd.print("Loading...");
  
  stepper.setRunMode(FOLLOW_POS);
  stepper.reverse(true);
  stepper.setMaxSpeed(500);
  stepper.setAcceleration(0);
  stepper.setTarget(1500);

  pinMode(inSEN, INPUT);
  pinMode(button, INPUT);
  pinMode(pin_to_reset_sum, INPUT);
  pinMode(outSEN, OUTPUT);
  pinMode(led, OUTPUT);

  digitalWrite(outSEN, 1);
  digitalWrite(led, 1);

  attachPCINT(button);

  for (int i = 0; i < 10000; i++){
    stepper.tick();
    delay(1);
  }

  sc_greeter();
  Serial.begin(9600);
}

int counter = 0;
int new_money = 0;

void loop(){
  delay(10);
  data = analogRead(pin_to_reset_sum);

  if(data > 500) {
    memset(money,0,sizeof(money));
    refresh = 1;
  }

  data = analogRead(inSEN);

  counter++;
  if ((counter > COUNT_TO_REFRESH) && (current_screen == 0)) {
    lcd.setCursor(0,3);
    lcd.print("                    ");
  }
  if ((counter > 2 * COUNT_TO_REFRESH) && (current_screen == 0)) {
    lcd.setCursor(0,2);
    lcd.print("                    ");
  }

  if (data > 100) {
    counter = 0;

    int ret = 0;

    if      (check(money, VALUE_FOR_1_K,  TIME_TO_FALL, TIME_TO_CHECK)){refresh = 1;new_money = 0; }
    else if (check(money, VALUE_FOR_10_K, TIME_TO_FALL, TIME_TO_CHECK)){refresh = 1;new_money = 1;money[0]++;}
    else if (check(money, VALUE_FOR_5_K,  TIME_TO_FALL, TIME_TO_CHECK)){refresh = 1;new_money = 3;money[2]++;}
    else if (check(money, VALUE_FOR_50_K, TIME_TO_FALL, TIME_TO_CHECK)){refresh = 1;new_money = 2;money[1]++;}
    else if (check(money, VALUE_FOR_1_R,  TIME_TO_FALL, TIME_TO_CHECK)){refresh = 1;new_money = 4;money[3]++;}
    else if (check(money, VALUE_FOR_10_R, TIME_TO_FALL, TIME_TO_CHECK)){refresh = 1;new_money = 5;money[4]++;}
    else if (check(money, VALUE_FOR_2_R,  TIME_TO_FALL, TIME_TO_CHECK)){refresh = 1;new_money = 8;money[7]++;}
    else if (check(money, VALUE_FOR_5_R,  TIME_TO_FALL + 10, TIME_TO_CHECK)){refresh = 1;new_money = 6;money[5]++;}
    else    {refresh = 1;new_money = 7;comeback();money[6]++;}

    EEPROM.put(0, money);
  }

  if(flag == 1){
    flag = 0;
    delay(500);
    if (flag == 1){
      flag = 0;
      change_screen = 1;
    }
  }

  if ((current_screen == 0 && change_screen == 1) || (current_screen == 1 && refresh == 1)) {
    current_screen = 1;
    change_screen = 0;
    sc_counter(); // это первый режим работы 
  }
  if ((current_screen == 1 && change_screen == 1) || (current_screen == 0 && refresh == 1)) {
    current_screen = 0;
    change_screen = 0;
    sc_greeter(); // это вторый режим работы 
    switch (new_money){
      case 0:lcd.setCursor(5,2);lcd.print("");break;
      case 1:lcd.setCursor(8,2);lcd.print("+0.01");lcd.setCursor(9,3);lcd.print("WOW");break;
      case 2:lcd.setCursor(8,2);lcd.print("+0.05");lcd.setCursor(8,3);lcd.print("good");break;
      case 3:lcd.setCursor(8,2);lcd.print("+0.1"); lcd.setCursor(8,3);lcd.print("cool");break;
      case 4:lcd.setCursor(8,2);lcd.print("+0.5"); lcd.setCursor(8,3);lcd.print("Nice");break;
      case 5:lcd.setCursor(9,2);lcd.print("+1");   lcd.setCursor(7,3);lcd.print("Great!");break;
      case 6:lcd.setCursor(9,2);lcd.print("+2");   lcd.setCursor(6,3);lcd.print("Amazing!");break;
      case 7:lcd.setCursor(9,2);lcd.print("+5");   lcd.setCursor(5,3);lcd.print("Wonderful!");break;
      case 8:lcd.setCursor(9,2);lcd.print("+10");  lcd.setCursor(6,3);lcd.print("PERFECT!");break;
      default:lcd.setCursor(4,2);lcd.print("ERROR DEFAULT");break;
    }
  }
  refresh = 0;
}