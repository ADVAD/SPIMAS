#pragma once
#ifndef _ST7789_
#define _ST7789_

#include <Print.h>
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"


 uint16_t const xsize = 240, ysize = 320, xoff = 0, yoff = 0, invert = 1, rotate = 0, bgr = 0;

#include "vga866.h"

#define LCD_HOST    SPI2_HOST

#define PIN_NUM_MISO -1
#define PIN_NUM_MOSI 3
#define PIN_NUM_CLK  2
#define PIN_NUM_CS   7

#define PIN_NUM_DC   (gpio_num_t)6
#define PIN_NUM_RST  (gpio_num_t)10
#define PIN_NUM_BCKL (gpio_num_t)11

//To speed up transfers, every SPI transfer sends a bunch of lines. This define specifies how many. More means more memory use,
//but less overhead for setting up / finishing transfers. Make sure 240 is dividable by this.
#define PARALLEL_LINES 16
/*
 The LCD needs a bunch of command/argument values to be initialized. They are stored in this struct.
*/
typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;

//Place data into DRAM. Constant data gets placed into DROM by default, which is not accessible by DMA.
DRAM_ATTR static const lcd_init_cmd_t st_init_cmds[]={
    /* Memory Data Access Control, MX=MV=1, MY=ML=MH=0, RGB=0 */
    {0x36, {(0<<5)|(0<<6)}, 1}, //{0x36, {(1<<5)|(1<<6)}, 1},
    /* Interface Pixel Format, 16bits/pixel for RGB/MCU interface */
    {0x3A, {0x55}, 1},
    {0x21, {0}, 0},    
    /* Porch Setting */
    {0xB2, {0x0c, 0x0c, 0x00, 0x33, 0x33}, 5},
    /* Gate Control, Vgh=13.65V, Vgl=-10.43V */
    {0xB7, {0x45}, 1},
    /* VCOM Setting, VCOM=1.175V */
    {0xBB, {0x2B}, 1},
    /* LCM Control, XOR: BGR, MX, MH */
    {0xC0, {0x2C}, 1},
    /* VDV and VRH Command Enable, enable=1 */
    {0xC2, {0x01, 0xff}, 2},
    /* VRH Set, Vap=4.4+... */
    {0xC3, {0x11}, 1},
    /* VDV Set, VDV=0 */
    {0xC4, {0x20}, 1},
    /* Frame Rate Control, 60Hz, inversion=0 */
    {0xC6, {0x0f}, 1},
    /* Power Control 1, AVDD=6.8V, AVCL=-4.8V, VDDS=2.3V */
    {0xD0, {0xA4, 0xA1}, 1},
    /* Positive Voltage Gamma Control */
    {0xE0, {0xD0, 0x00, 0x05, 0x0E, 0x15, 0x0D, 0x37, 0x43, 0x47, 0x09, 0x15, 0x12, 0x16, 0x19}, 14},
    /* Negative Voltage Gamma Control */
    {0xE1, {0xD0, 0x00, 0x05, 0x0D, 0x0C, 0x06, 0x2D, 0x44, 0x40, 0x0E, 0x1C, 0x18, 0x16, 0x19}, 14},
    /* Sleep Out */
    {0x11, {0}, 0x80},
    /* Display On */
    {0x29, {0}, 0x80},
    {0, {0}, 0xff}
};

//This function is called (in irq context!) just before a transmission starts. It will
//set the D/C line to the value indicated in the user field.
void lcd_spi_pre_transfer_callback(spi_transaction_t *t) {
    int dc=(int)t->user;
    gpio_set_level(PIN_NUM_DC, dc);
}

class ST7789 : public Print {
public:
  static const uint16_t BLACK = 0x0000;
  static const uint16_t WHITE = 0xFFFF;
  static const uint16_t GRAY = 0x38E7;
  static const uint16_t RED = 0xF800;
  static const uint16_t GREEN = 0x07E0;
  static const uint16_t BLUE = 0x001F;
  static const uint16_t YELLOW = 0xFFE0;

void send_cmd(const uint8_t cmd) {
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=8;                     //Command is 8 bits
    t.tx_buffer=&cmd;               //The data is the cmd itself
    t.user=(void*)0;                //D/C needs to be set to 0
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}
  
void send_data(const uint8_t *data, int len) {
    esp_err_t ret;
    spi_transaction_t t;
    if (len==0) return;             //no need to send anything
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=len*8;                 //Len is in bytes, transaction length is in bits.
    t.tx_buffer=data;               //Data
    t.user=(void*)1;                //D/C needs to be set to 1
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}

//Initialize the display
void lcd_init() {
    int cmd=0;
    const lcd_init_cmd_t* lcd_init_cmds;

    //Initialize non-SPI GPIOs
    gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_BCKL, GPIO_MODE_OUTPUT);

    //Reset the display
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(100 / portTICK_RATE_MS);
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(100 / portTICK_RATE_MS);

    lcd_init_cmds = st_init_cmds;

    //Send all the commands
    while (lcd_init_cmds[cmd].databytes!=0xff) {
        send_cmd(lcd_init_cmds[cmd].cmd);
        send_data(lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes&0x1F);
        if (lcd_init_cmds[cmd].databytes&0x80) {
            vTaskDelay(100 / portTICK_RATE_MS);
        }
        cmd++;
    }

    ///Enable backlight
    gpio_set_level(PIN_NUM_BCKL, 1);
}

uint16_t xp = 0;
uint16_t yp = 0;
uint16_t bg = BLACK;
uint16_t fg = WHITE;
int wrap = 0;
int bold = 0;
int sx = 1;
int sy = 1;
int horizontal = -1;
int scrollMode = 1;

spi_device_handle_t spi;

ST7789() {    
  }

uint8_t spiBusyCheck = 0;      // Number of ESP32 transfer buffers to check

void dmaWait(void) {
  if (!spiBusyCheck) return;
  spi_transaction_t *rtrans;
  esp_err_t ret;
  for (; spiBusyCheck; --spiBusyCheck) {
    ret = spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
    assert(ret == ESP_OK);
  }
  spiBusyCheck = 0;
}
  
uint16_t TFA, VSA, BFA, VSP;

uint16_t calcY(uint16_t y) {
  if ((y<TFA) || (y>=TFA+VSA)) return y;
  if (y<TFA+VSA+TFA-VSP) return y+VSP-TFA;
  return y+VSP-VSA-TFA; //y-VSP+TFA;
}

void scroll() {
  xp=0;
  if (yp+FONT_HEIGHT*sy<TFA+VSA) {
    yp+=FONT_HEIGHT*sy;
  } else {
    if (VSP+FONT_HEIGHT*sy<TFA+VSA) {
 
    scrollFrame(VSP+FONT_HEIGHT*sy);
    } else {
      scrollFrame(TFA);
    }
    fillRect(xp, calcY(yp), xsize-xp, FONT_HEIGHT*sy, bg);
  }
}

void setupScroll() {
  setupScroll(FONT_HEIGHT, ysize - FONT_HEIGHT - FONT_HEIGHT);
  yp = FONT_HEIGHT;
}

void setupScroll(uint16_t tfa, uint16_t vsa) { // ILI9341_VSCRDEF  0x33
  yp = TFA = tfa; VSA = vsa; BFA = 320 - tfa - vsa - yoff; //ysize - TFA - BFA; VSP = TFA;
  static uint8_t *lines;
  //Allocate memory for the pixel buffers
  if (lines==NULL)
    lines=(uint8_t*)heap_caps_malloc(6*sizeof(uint8_t), MALLOC_CAP_DMA);

  static spi_transaction_t trans[3];
  memset(&trans, 0, sizeof(trans));

  trans[0] = {.flags=SPI_TRANS_USE_TXDATA, .length=8, .user=(void*)0, .tx_data={0x33}}; 
  trans[1] = {.flags=0, .length=8*6, .user=(void*)1, .tx_buffer=lines}; 
    //lines* = {(uint8_t)TFA>>8, (uint8_t)TFA&0xff, (uint8_t)VSA>>8, (uint8_t)VSA&0xff, (uint8_t)BFA>>8, (uint8_t)BFA&0xff};
  lines[0]=TFA>>8;              //Start Col High
  lines[1]=TFA&0xff;              //Start Col Low
  lines[2]=VSA>>8;       //End Col High
  lines[3]=VSA&0xff;     //End Col Low
  lines[4]=BFA>>8;              //Start Col High
  lines[5]=BFA&0xff;              //Start Col Low

  trans[2].flags=SPI_TRANS_USE_TXDATA;
  trans[2].length=8*2;
  trans[2].user=(void*)1;
  trans[2].tx_data[0]=BFA>>8;              //Start Col High
  trans[2].tx_data[1]=BFA&0xff;              //Start Col Low

  esp_err_t ret;
  for (int i=0; i<2; i++) {
    ret=spi_device_queue_trans(spi, &trans[i], portMAX_DELAY);
    assert(ret==ESP_OK);
    spiBusyCheck++;
  }
  dmaWait();

  scrollFrame(TFA);
}

void scrollFrame(uint16_t vsp) { //ILI9341_VSCRSADD 0x37
  VSP = vsp;

  static spi_transaction_t trans[2];
  memset(&trans, 0, sizeof(trans));

  trans[0] = {.flags=SPI_TRANS_USE_TXDATA, .length=8, .user=(void*)0, .tx_data={0x37}}; 
  trans[1] = {.flags=SPI_TRANS_USE_TXDATA, .length=8*2, .user=(void*)1, .tx_data={VSP>>8, VSP&0xff}}; 

  esp_err_t ret;
  for (int i=0; i<2; i++) {
    ret=spi_device_queue_trans(spi, &trans[i], portMAX_DELAY);
    assert(ret==ESP_OK);
    spiBusyCheck++;
  }
  dmaWait();
}

/* To send a set of lines we have to send a command, 2 data bytes, another command, 2 more data bytes and another command
 * before sending the line data itself; a total of 6 transactions. (We can't put all of this in just one transaction
 * because the D/C line needs to be toggled in the middle.)
 * This routine queues these commands up as interrupt transactions so they get
 * sent faster (compared to calling spi_device_transmit several times), and at
 * the mean while the lines for next transactions can get calculated.
 */
void send_lines(int x, int y, int w, int h, int n, int l, uint16_t *linedata) {
    esp_err_t ret;
    //Transaction descriptors. Declared static so they're not allocated on the stack; we need this memory even when this
    //function is finished because the SPI driver needs access to it even while we're already calculating the next line.
    static spi_transaction_t trans[7];
    memset(&trans, 0, sizeof(trans));

    //In theory, it's better to initialize trans and data only once and hang on to the initialized
    //variables. We allocate them on the stack, so we need to re-init them each call.

//    memset(&trans[0], 0, sizeof(spi_transaction_t));

    trans[0] = {.flags=SPI_TRANS_USE_TXDATA, .length=8, .user=(void*)0, .tx_data={0x2A}}; //CAS
    trans[1] = {.flags=SPI_TRANS_USE_TXDATA, .length=8*4, .user=(void*)1, .tx_data={x>>8,x&0xff,(x+w-1)>>8,(x+w-1)&0xff}}; 
    trans[2] = {.flags=SPI_TRANS_USE_TXDATA, .length=8, .user=(void*)0, .tx_data={0x2B}}; //RAS
    trans[3] = {.flags=SPI_TRANS_USE_TXDATA, .length=8*4, .user=(void*)1, .tx_data={y>>8,y&0xff,(y+h-1)>>8,(y+h-1)&0xff}}; 
    trans[4] = {.flags=SPI_TRANS_USE_TXDATA, .length=8, .user=(void*)0, .tx_data={0x2C}}; //memory write
    trans[5] = {.flags=0, .length=l*2*8, .user=(void*)1, .tx_buffer=linedata};
    trans[6] = {.flags=0, .length=n*2*8, .user=(void*)1, .tx_buffer=linedata};

    for (int i=0; i<5; i++) {
      ret=spi_device_queue_trans(spi, &trans[i], portMAX_DELAY);
      assert(ret==ESP_OK);
      spiBusyCheck++;
    }
    
    while (n && l && (n>l)) {
      n-=l;
      ret=spi_device_queue_trans(spi, &trans[5], portMAX_DELAY);
      assert(ret==ESP_OK);
      spiBusyCheck++;
      dmaWait(); //помогло заработать с буфером 5000
    }

    if (n) {
      trans[6].length=n*2*8;
      ret=spi_device_queue_trans(spi, &trans[6], portMAX_DELAY);
      assert(ret==ESP_OK);
      spiBusyCheck++;
      dmaWait();
    }
      
}

void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t c) {
    if ((x < xsize) && (y < ysize)) {
      if (x + w > xsize) w = xsize - x;
      if (y + h > ysize) h = ysize - y;
    static uint16_t *lines;
    //Allocate memory for the pixel buffers
    if (lines==NULL) //7000 - перезагруз  5000 - виснет
      //lines=(uint16_t*)heap_caps_malloc(4096*sizeof(uint16_t), MALLOC_CAP_DMA);
      lines=(uint16_t*)spi_bus_dma_memory_alloc(LCD_HOST ,4096*sizeof(uint16_t), MALLOC_CAP_DMA);
      int n = w * h;
    for (int i = 0; i < (n > 4096 ? 4096 : n); i++) lines[i]=millis(); c;
    send_lines(x, y, w, h, n, 4096, lines);
    dmaWait();
    }
  }

void begin() {
    esp_err_t ret;
    //spi_device_handle_t spi;
    spi_bus_config_t buscfg={
        .mosi_io_num=PIN_NUM_MOSI,
        .miso_io_num=PIN_NUM_MISO,
        .sclk_io_num=PIN_NUM_CLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz=4096*2 //PARALLEL_LINES*320*2+8 //Максимальный размер передачи в байтах (или 0 для значения по умолчанию)
    };

    
    spi_device_interface_config_t devcfg={
        .mode=0,                                //SPI mode 0
        .clock_speed_hz=26*1000*1000,           //Clock out at 26 MHz
//        .clock_speed_hz=10*1000*1000,           //Clock out at 10 MHz
        .spics_io_num=PIN_NUM_CS,               //CS pin
        .queue_size=7,                          //We want to be able to queue 7 transactions at a time
        .pre_cb=lcd_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
    };

    
    //Initialize the SPI bus
    ret=spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    //Attach the LCD to the SPI bus
    ret=spi_bus_add_device(LCD_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);
    //Initialize the LCD
    lcd_init();
    //Initialize the effect displayed
    //ret=pretty_effect_init();
    ESP_ERROR_CHECK(ret);
}

static const uint8_t FONT_WIDTH = 8;
static const uint8_t FONT_HEIGHT = 12;
static const uint8_t FONT_GAP = 0;

void drawChar(uint16_t x, uint16_t y, unsigned char c, uint16_t cf = 0xffff, uint16_t cb = 0) {
    static uint16_t *lines;
    //Allocate memory for the pixel buffers
    if (lines==NULL)
      //lines=(uint16_t*)heap_caps_malloc(8*12*sizeof(uint16_t), MALLOC_CAP_DMA);
      lines=(uint16_t*)spi_bus_dma_memory_alloc(LCD_HOST ,8*12*sizeof(uint16_t), MALLOC_CAP_DMA);

    int i = 0;
    for (uint8_t dy = 0; dy < 12; dy++) {
      uint8_t bits = font_vga866[c * 12 + dy]; 
      for (int8_t dx = 7; dx >= 0; dx--) lines[i++] = ((bits & (1 << dx)) ? cf : 0);
    }
    send_lines(x, y, 8, 12, 8*12, 8*12, lines);
    dmaWait();

  } 

void drawStr(uint16_t x, uint16_t y, const char *str, uint16_t cf = WHITE, uint16_t cb = BLACK) {
  while (*str && (x < xsize)) {
    drawChar(x, y, *str++, cf, cb);
    x += FONT_WIDTH + FONT_GAP;
  }
}

size_t write(uint8_t c) {
  if(c==10) { scroll(); return 1; }
  if(c==13) { xp=0; return 1; }
  if(c==8) { 
    if(xp>0) xp-=FONT_WIDTH*sx; 
    fillRect(xp, yp, FONT_WIDTH*sx, FONT_HEIGHT*sy, bg);
    return 1; 
  }

  if(xp<xsize)
    drawChar(xp, calcY(yp), c, fg, bg); // sx, sy);
    //drawChar(xp, yp, c, fg, bg); // sx, sy);
    xp+=FONT_WIDTH*sx;  
    return 1; 
}


};

#endif 
