
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
//#include "driver/spi_master.h"


#include "ST7789.h"
ST7789 lcd;

int Last8 = 0;
bool DblPress8 = false;
bool Butt8 = false;
bool Butt9 = false;
uint InReg = 0;

hw_timer_t *timer = NULL;

void onReceiveSerial1() {
  if (Serial1.available()) {
    char data = Serial1.read();
    lcd.print(data);
    Serial.write(data);
    Serial0.write(data);
    if (Butt8 && Serial1.availableForWrite()) Serial1.write(data);
  }
}

void setup(void) {

  pinMode(8, INPUT_PULLUP);
  pinMode(9, INPUT_PULLUP);

  lcd.begin();    
  lcd.setupScroll(16, 288);
  lcd.fillRect(0,0, 240, 320, 0x00ff);
  lcd.fillRect(0 ,14, 240, 1, 0xff00);
  lcd.fillRect(0 ,15, 240, 1, 0x0000);
  
  Serial.begin(115200);

  Serial0.begin(115200);


//  pinMode(0, INPUT);
  Serial1.begin(9600, SERIAL_8N1, 1, 0); // Укажите ваши пины RX TX
  //pinMode(1, INPUT_PULLUP); //похоже так нельзя
  //gpio_pullup_en(gpio_num_t(1));
  Serial1.onReceive(onReceiveSerial1);

  timer = timerBegin(10000);
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, 100, true, 0);


}

void ARDUINO_ISR_ATTR onTimer() {
  Last8 = Last8 +1;
  InReg = REG_READ(GPIO_IN_REG);
  if (!(InReg & BIT(8))) Butt8 = true;
  if (!(InReg & BIT(9))) Butt8 = false;
 }


void loop() {

    if (Serial.available()) {
    char data = Serial.read();
    lcd.print(data);
    if (Serial1.availableForWrite()) {}
    Serial1.write(data);
    Serial0.write(data);
  }

  //if (Butt8) lcd.drawStr(128,0,"1"); else lcd.drawStr(128,0,"0");

  delay(1);

}
