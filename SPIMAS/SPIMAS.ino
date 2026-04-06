
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
uint PrevCnt = 0;
uint CurrCnt = 0;

hw_timer_t *timer = NULL;

void onReceiveSerial1() {
  if (Serial1.available()) {
    char data = Serial1.read();
    lcd.print(data);
    if (Serial) Serial.write(data);
    Serial0.write(data);
    //if (Butt8 && Serial1.availableForWrite()) Serial1.write(data);
  }
}

void setup(void) {

  pinMode(8, INPUT_PULLUP);
  pinMode(9, INPUT_PULLUP);

  lcd.begin();    
  //lcd.setupScroll(16, ysize - 16 - 16);
  lcd.setupScroll(12, ysize-12-12);
  lcd.fillRect(0,0, xsize, ysize, 0x00ff);
  lcd.fillRect(0 ,14, xsize, 1, 0xff00);
  lcd.fillRect(0 ,15, xsize, 1, 0x0000);
  
  Serial.begin(115200);

  Serial0.begin(115200);


//  pinMode(0, INPUT);
  Serial1.setTxBufferSize(1024);
  Serial1.setRxBufferSize(1024);
  Serial1.begin(9600, SERIAL_8N1, 1, 0); // Укажите ваши пины RX TX
  //pinMode(1, INPUT_PULLUP); //похоже так нельзя
  gpio_pullup_en(gpio_num_t(1));
  //Serial1.onReceive(onReceiveSerial1);

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

  if ( Serial && Serial.available()) {
    char data = Serial.read();
    lcd.print(data);
    if (Serial1.availableForWrite()) {}
    Serial1.write(data);
    Serial0.write(data);
  }

  if ( Serial1 && Serial1.available()) {
      char data = Serial1.read();
      lcd.print(data);
      if (Serial) Serial.write(data);
      Serial0.write(data);
    //if (Butt8 && Serial1.availableForWrite()) Serial1.write(data);
  }

  
  if (Butt8) { 
    if (CurrCnt - PrevCnt >= 1000) {
       Serial1.printf("%i\n",CurrCnt);
       PrevCnt = CurrCnt; 
    } else CurrCnt++;   
    lcd.drawStr(128,0,"1"); 
  } else lcd.drawStr(128,0,"0");


  delay(1);

}
