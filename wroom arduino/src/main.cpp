#include <Arduino.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/apps/sntp.h"

#include "driver/rtc_io.h"

/* The examples use simple WiFi configuration that you can set via
   'make menuconfig'.
   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_WIFI_SSID "yyy"
#define EXAMPLE_WIFI_PASS "xxx"
#define WAKEUP_GPIO GPIO_NUM_36
//enable GPIO36 and GPIO33
#define BUTTON_PIN_BITMASK 0x1200000000
/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;
int i=0;
esp_sleep_wakeup_cause_t wakeup_probe;
uint64_t time_in_us = 10000000;
time_t now;
tm timeinfo;
static const char *TAG = "example";

/* Variable holding number of times ESP32 restarted since first boot.
 * It is placed into RTC memory using RTC_DATA_ATTR and
 * maintains its value when ESP32 wakes from deep sleep.
 */
RTC_DATA_ATTR static int boot_count = 0;
RTC_DATA_ATTR static int bootCount = 0;
static void obtain_time(void);
static void initialize_sntp(void);
static void initialise_wifi(void);
static esp_err_t event_handler(void *ctx, system_event_t *event);
void print_wakeup_reason();

void setup() {
  
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(1000); //Take some time to open up the Serial Monitor

  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("BootCount number: " + String(++bootCount));

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
  rtc_gpio_pulldown_dis(GPIO_NUM_33);
  rtc_gpio_pullup_en(GPIO_NUM_33);
  //UNMARK 
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_33,0); //1 = High, 0 = Low

  //If you were to use ext1, you would use it like
  //esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK,ESP_EXT1_WAKEUP_ANY_HIGH);

  //Go to sleep now
  Serial.println("Going to sleep now");
  //UNMARK 
  //esp_deep_sleep_start();
}
 



void loop() {
  // put your main code here, to run repeatedly:
    delay(2000);
    print_wakeup_reason();

    Serial.printf("Boot count: %d\n", ++boot_count);


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
    
        char strftime_buf[64];

    // Set timezone to Eastern Standard Time and print local time
    
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    Serial.printf("The current date/time in New York is: %s\n", strftime_buf);

    

    //rtc_gpio_deinit(WAKEUP_GPIO);
    
    gpio_pulldown_dis(WAKEUP_GPIO);
    gpio_pullup_en(WAKEUP_GPIO);
    for (i=0;i<3;i++){
      Serial.printf("Waked up on %d times!!!\n", boot_count);
      delay(1000);
      Serial.printf("GPIO%d level=%d\n",WAKEUP_GPIO,gpio_get_level(GPIO_NUM_36));
    }

    Serial.println("\nNow entering in sleep mode ***\n");
    //rtc_gpio_init(WAKEUP_GPIO);
    //rtc_gpio_pulldown_dis(WAKEUP_GPIO);
    //rtc_gpio_pullup_en(WAKEUP_GPIO);
    //esp_sleep_enable_ext0_wakeup(WAKEUP_GPIO, 0);
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
    //wifi_config_t wifi_config ;
    wifi_config_t wifi_config = {
      .sta = {
        {.ssid = EXAMPLE_WIFI_SSID},
        {.password = EXAMPLE_WIFI_PASS}
      },
    };
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