#include <M5StickCPlus.h>
#include "esp_pm.h"
#include <driver/rmt.h>



struct TimeParser
{
  Stream &input;
  
  TimeParser(Stream &s) : input(s)
  {
  }
    
  void set_time(char *sz)
  {
    if (8 != strlen(sz))
      return;
    if (sz[2] != ':' || sz[5] != ':')
      return;
    sz[2] = 0;
    sz[5] = 0;
    RTC_TimeTypeDef TimeStruct;
    TimeStruct.Hours   = atoi(sz+0);;
    TimeStruct.Minutes = atoi(sz+3);
    TimeStruct.Seconds = atoi(sz+6);
    M5.Rtc.SetTime(&TimeStruct);
  }

  void loop()
  {
    static char serial_buffer[10];
    static size_t serial_len=0;
    static size_t serial_lastchar = '\n';
  
    while (input.available())
    {
      uint8_t ch = input.read();
      if (ch == '\n')
      {
        if (serial_len == 8)
        {
          serial_buffer[serial_len++] = 0;
          set_time(serial_buffer);
        }
      }
      else if (ch == ':')
      {
        if (serial_len==2 || serial_len==5)
        {
          serial_buffer[serial_len++] = ch;
          serial_buffer[serial_len] = 0;
//          _println(serial_buffer);
        }
        else ch = 0;
      }
      else if (ch >= '0' && ch <= '9')
      {
          if ((serial_len==0 && serial_lastchar=='\n') || (0<serial_len && serial_len<8))
          {
            serial_buffer[serial_len++] = ch;
            serial_buffer[serial_len] = 0;
            Serial.println(serial_buffer);
          }
          else ch = 0;
      }
      if (ch == '\n' || ch == 0)
        serial_len = 0;
      serial_lastchar = ch;
    }
  }
};





RTC_TimeTypeDef rtctime;
int prevSeconds = -1;

void setup()
{
    M5.begin();
    M5.Lcd.fillScreen(BLACK);
    M5.Rtc.GetTime(&rtctime);
    prevSeconds = rtctime.Seconds;

    InitIRTx();
}


TimeParser parser(Serial);

void loop()
{
    M5.Rtc.GetTime(&rtctime);
    if (rtctime.Seconds != prevSeconds)
    {
         char buf[50];
         snprintf(buf, 50, "\xaa\x55\xaa\x55\n%02d:%02d:%02d\n", rtctime.Hours, rtctime.Minutes, rtctime.Seconds);
         ir_uart_tx((uint8_t *)buf, strlen(buf), true);
         snprintf(buf, 50, "%02d:%02d:%02d", rtctime.Hours, rtctime.Minutes, rtctime.Seconds);
         M5.Lcd.setCursor(10, 10);
         //M5.Lcd.fillScreen(BLACK);
         M5.Lcd.print(buf);
    }
    parser.loop();
    prevSeconds = rtctime.Seconds;
    M5.update();
}






// from FactoryTest.ino

static esp_pm_lock_handle_t rmt_freq_lock;
#define RMT_TX_CHANNEL RMT_CHANNEL_0
#define RMT_TX_GPIO_NUM GPIO_NUM_9  // IR
//#define RMT_TX_GPIO_NUM GPIO_NUM_10 // LED
#define RMT_CLK_DIV (1) // 80000000 / 1(HZ)

rmt_item32_t *tx_buffer = NULL;

void ir_tx_callback(rmt_channel_t channel, void *arg)
{
    //static BaseType_t xHigherPriorityTaskWoken = false;
    if (channel == RMT_TX_CHANNEL)
    {
        esp_pm_lock_release(rmt_freq_lock);
        //xHigherPriorityTaskWoken = pdFALSE;
        //xSemaphoreGiveFromISR( irTxSem, &xHigherPriorityTaskWoken );
        free(tx_buffer);
    }
}

bool InitIRTx()
{
    rmt_config_t rmt_tx;
    rmt_tx.rmt_mode = RMT_MODE_TX;
    rmt_tx.channel = RMT_TX_CHANNEL;
    rmt_tx.gpio_num = RMT_TX_GPIO_NUM;

    rmt_tx.mem_block_num = 1;
    rmt_tx.clk_div = RMT_CLK_DIV;

    rmt_tx.tx_config.loop_en = false;
    rmt_tx.tx_config.carrier_duty_percent = 50;
    rmt_tx.tx_config.carrier_freq_hz = 38000;
    rmt_tx.tx_config.carrier_level = RMT_CARRIER_LEVEL_LOW;
    rmt_tx.tx_config.carrier_en = true;
    rmt_tx.tx_config.idle_level = RMT_IDLE_LEVEL_HIGH;
    rmt_tx.tx_config.idle_output_en = true;
    rmt_config(&rmt_tx);
    rmt_driver_install(rmt_tx.channel, 0, 0);

    return true;
}



bool ir_uart_tx(const uint8_t *src, uint8_t len, bool wait_tx_done)
{
    if (src == NULL)
        return false;

    const rmt_item32_t bit0 = {{{16842, 0, 16842, 0}}}; //Logical 0
    const rmt_item32_t bit1 = {{{16842, 1, 16842, 1}}}; //Logical 1

    uint8_t *psrc = (uint8_t *)src;

    tx_buffer = (rmt_item32_t *)malloc(sizeof(rmt_item32_t) * 10 * len);
    if (tx_buffer == NULL)
        return false;

    rmt_item32_t *pdest = tx_buffer;
    for (uint8_t ptr = 0; ptr < len; ptr++)
    {
        *pdest++ = bit0;
        *pdest++ = (*psrc & (0x1 << 0)) ? bit1 : bit0;
        *pdest++ = (*psrc & (0x1 << 1)) ? bit1 : bit0;
        *pdest++ = (*psrc & (0x1 << 2)) ? bit1 : bit0;
        *pdest++ = (*psrc & (0x1 << 3)) ? bit1 : bit0;
        *pdest++ = (*psrc & (0x1 << 4)) ? bit1 : bit0;
        *pdest++ = (*psrc & (0x1 << 5)) ? bit1 : bit0;
        *pdest++ = (*psrc & (0x1 << 6)) ? bit1 : bit0;
        *pdest++ = (*psrc & (0x1 << 7)) ? bit1 : bit0;
        *pdest++ = bit1;
        psrc++;
    }

    esp_pm_lock_acquire(rmt_freq_lock);
    rmt_write_items(RMT_TX_CHANNEL, tx_buffer, 10 * len, true);
    free(tx_buffer);
    return true;
}
