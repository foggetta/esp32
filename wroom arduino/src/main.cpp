#include <Arduino.h>
#include <string.h>
#include <time.h>
//#include "sys/time.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/apps/sntp.h"

#include "driver/gpio.h"
#include "driver/rtc_io.h"


#include "gpio_esp32.h"
#include "adc_esp32.h"
/* The examples use simple WiFi configuration that you can set via
   'make menuconfig'.
   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_WIFI_SSID "MiA2_luke"
#define EXAMPLE_WIFI_PASS "foggettA"
#define ANA_CH ADC1_CHANNEL_6
#define ANA_CH_GPIO_NUM ADC1_CHANNEL_6_GPIO_NUM


//if you want to have more RTC GPIO waking up from sleep enable esp_sleep_enable_ext1_wakeup 
//the bit mask of GPIO numbers which will cause wakeup. 
//Only GPIOs which are have RTC functionality can be used in this bit map: 0,2,4,12-15,25-27,32-39
#define BUTTON_PIN_BITMASK  ((1ULL<<GPIO_NUM_26) | (1ULL<<GPIO_NUM_25))

uint32_t volt;
int i=0;
esp_sleep_wakeup_cause_t wakeup_probe;
uint64_t time_in_us = 10000000;
/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;
/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;
//static const char *TAG = "example";

/* Variable holding number of times ESP32 restarted since first boot.
 * It is placed into RTC memory using RTC_DATA_ATTR and
 * maintains its value when ESP32 wakes from deep sleep.
 */
static void try_time(void);
static void obtain_time(void);
static void initialize_sntp(void);
static void initialise_wifi(void);
static esp_err_t event_handler(void *ctx, system_event_t *event);
void setup_adc1(adc1_channel_t);
void print_wakeup_reason(void);



//ulp memory data in sleep mode
RTC_DATA_ATTR static int bootCount_loop = 0;
RTC_DATA_ATTR static int bootCount_setup = 0;
RTC_DATA_ATTR static int cnt = 0;

void setup() {
  
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(1000); //Take some time to open up the Serial Monitor

  //Increment boot number and print it every reboot
  ++bootCount_setup;
  Serial.println("BootCount_setup number: " + String(++bootCount_setup));

  //Print the wakeup reason for ESP32
  print_wakeup_reason();

  /*
  First we configure the wake up source
  We set our ESP32 to wake up for an external trigger.
  There are two types for ESP32, ext0 and ext1 .
  ext0 uses RTC_IO to wakeup thus requires RTC peripherals
  to be on while ext1 uses RTC Controller so doesnt need
  peripherals to be powered on.
  Note that using internal pullups/pulldowns also requires
  RTC peripherals to be turned on.
  */
  
  rtc_gpio_pulldown_dis(GPIO_NUM_26);
  rtc_gpio_pullup_en(GPIO_NUM_26);
  ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(GPIO_NUM_26,0)); //1 = High, 0 = Low
  //If you were to use ext1, you would use it like
  //esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK,ESP_EXT1_WAKEUP_ALL_LOW);

  //Go to sleep now
  //Serial.println("Going to sleep now");
  //UNMARK 
  //esp_deep_sleep_start();
}
 



void loop() {
  // put your main code here, to run repeatedly:
    delay(2000);
    print_wakeup_reason();

    Serial.printf("Boot count: %d\n", ++bootCount_loop);


    try_time();
    setup_gpio(NULL);
    setup_adc1(ANA_CH);

    //rtc_gpio_deinit(DIG_GPIO);
    
    int loopn=30;
    for (i=1;i<=loopn;i++){
        Serial.printf("\n\n\nLOOP %d\nWaked up on %d times!!!\n",i, bootCount_loop);
        gpio_set_level((gpio_num_t)DIG_GPIO, 1);
        delay(500);
        gpio_set_level((gpio_num_t)DIG_GPIO, 0);

      //Note that even the hall sensor is internal to ESP32,
      // reading from it uses channels 0 and 3 of ADC1 (GPIO 36 and 39). Do not connect anything else to these pins
        Serial.printf("Hall sensor level=%d\n",hall_sensor_read());

        printf("OUTPUT level %d on cnt: %d\n",cnt % 2, cnt++);
        gpio_set_level((gpio_num_t)GPIO_OUTPUT_IO_0, cnt % 2);
        gpio_set_level((gpio_num_t)GPIO_OUTPUT_IO_1, i % loopn);
        Serial.printf("ADC%d on GPIO%d level=%f\n", ANA_CH, ANA_CH_GPIO_NUM, (read_voltage(ADC_CHANNEL_6,&volt))/1000.);
        
     
    }

    Serial.println("\nNow entering in sleep mode ***\n");
    //rtc_gpio_init(DIG_GPIO);
    //rtc_gpio_pulldown_dis(DIG_GPIO);
    //rtc_gpio_pullup_en(DIG_GPIO);
    //esp_sleep_enable_ext0_wakeup(DIG_GPIO, 0);
    esp_sleep_enable_timer_wakeup(time_in_us);
    esp_deep_sleep_start();

}



/*
Method to print the reason by which ESP32
has been awaken from sleep
*/
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}


static void try_time(){

    char strftime_buf[64];
    time_t now;
    tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        Serial.printf("Time is not set yet. Connecting to WiFi and getting time over NTP.\n");
        obtain_time();
        // update 'now' variable with current time
        time(&now);
        Serial.printf("Wakeup time is %s \n", asctime(&timeinfo) );
   }    
    
    

    // Set timezone to Eastern Standard Time and print local time
    
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    Serial.printf("The current date/time in New York is: %s\n", strftime_buf);


  return;
}

static void obtain_time(void)
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    initialise_wifi();
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    initialize_sntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        Serial.printf("Waiting for system time to be set... (%d/%d)\n", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    ESP_ERROR_CHECK( esp_wifi_stop() );
}

static void initialize_sntp(void)
{
    Serial.printf("Initializing SNTP\n");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    wifi_config_t wifi_config = {
        .sta = {
            {.ssid = EXAMPLE_WIFI_SSID},
            {.password = EXAMPLE_WIFI_PASS},
        }
    };

/* wifi_config_t wifi_config = { };
strcpy((char*)wifi_config.sta.ssid, "ssid");
strcpy((char*)wifi_config.sta.password, "password"); */

    Serial.printf("Setting WiFi configuration SSID %s...\n", wifi_config.sta.ssid);
    Serial.printf("Wifi mode err %d,",ESP_OK || esp_wifi_set_mode(WIFI_MODE_STA) );
    Serial.printf("Wifi set err %d,",ESP_OK || esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    Serial.printf("Wifi set err %d\n", esp_wifi_start() );
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}