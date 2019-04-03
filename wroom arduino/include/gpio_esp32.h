
//#include "stdio.h"
//#include "string.h"
//#include "stdlib.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
//#include "driver/gpio.h"
#include <Arduino.h>

/**
 * Brief:
 * This test code shows how to configure gpio and how to use gpio interrupt.
 *
 * GPIO status:
 * GPIO18: output
 * GPIO19: output
 * GPIO4:  input, pulled up, interrupt from rising edge and falling edge
 * GPIO5:  input, pulled up, interrupt from rising edge.
 *
 * Test:
 * Connect GPIO18 with GPIO4
 * Connect GPIO19 with GPIO5
 * Generate pulses on GPIO18/19, that triggers interrupt on GPIO4/5
 *
 */
#define DIG_GPIO    33
#define GPIO_OUTPUT_IO_0    18
#define GPIO_OUTPUT_IO_1    19
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0) | (1ULL<<GPIO_OUTPUT_IO_1) | (1ULL<<DIG_GPIO))
#define GPIO_INPUT_IO_0     4
#define GPIO_INPUT_IO_1     5
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0) | (1ULL<<GPIO_INPUT_IO_1))


#define ESP_INTR_FLAG_DEFAULT 0


static xQueueHandle gpio_evt_queue_1 = NULL;
static xQueueHandle gpio_evt_queue_2 = NULL;




int read_gpio(gpio_num_t);
int setup_gpio(void*);


static void IRAM_ATTR gpio_isr_handler(void* arg)
{
     uint32_t test = (uint32_t) arg;
/*   uint32_t *test=(uint32_t *) arg;
    uint32_t gpio_num_q1 = (uint32_t) test[0];
    uint32_t gpio_num_q2 = (uint32_t) test[1];
    printf("queue 1 i=on ch%d --- queue 2 on ch%d\n",gpio_num_q1,gpio_num_q2);
    xQueueSendFromISR(gpio_evt_queue_2, &gpio_num_q2, NULL);
   xQueueSendFromISR(gpio_evt_queue_1, &gpio_num_q1, NULL);
   */
   xQueueSendFromISR(gpio_evt_queue_1, &test, NULL);

}

static void gpio_task_1(void* arg)
{
    gpio_num_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue_1, &io_num, portMAX_DELAY)) {
            switch (io_num)
            {
                case GPIO_INPUT_IO_0:
                    printf("TASK 1 on GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
                    break;
                case GPIO_INPUT_IO_1:
                    printf("Deep Sleep start in two second due to GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
                    delay(2000);
                    esp_deep_sleep_start();
                default:

                    break;
            }
        }
    }
}
static void gpio_task_2(void* arg)
{
    gpio_num_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue_2, &io_num, portMAX_DELAY)) {
            printf("TASK 2 on GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
}


int setup_gpio(void* arg) {
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = (gpio_int_type_t) GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = (gpio_pulldown_t) 0;
    //disable pull-up mode
    io_conf.pull_up_en = (gpio_pullup_t) 0;
    //configure GPIO with the given settings
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    //interrupt of rising edge
    io_conf.intr_type = (gpio_int_type_t) GPIO_PIN_INTR_POSEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode    
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = (gpio_pullup_t) 1;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    //change gpio intrrupt type for one pin
    ESP_ERROR_CHECK(gpio_set_intr_type((gpio_num_t) GPIO_INPUT_IO_0, (gpio_int_type_t) GPIO_INTR_ANYEDGE));

    //create a queue to handle gpio event from isr
    gpio_evt_queue_1 = xQueueCreate(10, sizeof(uint32_t));
    //gpio_evt_queue_2 = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(gpio_task_1, "gpio_task_1", 2048, NULL, 10, NULL);
    //xTaskCreate(gpio_task_2, "gpio_task_2", 2048, NULL, 10, NULL);
    //install gpio isr service
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT));


    gpio_num_t ch_arr[2]={(gpio_num_t) GPIO_INPUT_IO_0, (gpio_num_t) GPIO_INPUT_IO_0};
 /*    //hook isr handler for specific gpio pin
    ESP_ERROR_CHECK(gpio_isr_handler_add((gpio_num_t) GPIO_INPUT_IO_0, gpio_isr_handler, (void*) &ch_arr));
    //hook isr handler for specific gpio pin
    ESP_ERROR_CHECK(gpio_isr_handler_add((gpio_num_t) GPIO_INPUT_IO_1, gpio_isr_handler, (void*) &ch_arr));
 */
   ESP_ERROR_CHECK(gpio_isr_handler_add((gpio_num_t) GPIO_INPUT_IO_0, gpio_isr_handler,(void *) GPIO_INPUT_IO_0 ));
   ESP_ERROR_CHECK(gpio_isr_handler_add((gpio_num_t) GPIO_INPUT_IO_1, gpio_isr_handler,(void *) GPIO_INPUT_IO_1 ));
 
    //remove isr handler for gpio number.
    //hook isr handler for specific gpio pin again
    //gpio_isr_handler_add((gpio_num_t)GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);

    
    // //disable interrupt
    // io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    // //interrupt of rising edge
    // io_conf.pin_bit_mask = DIG_GPIO_SEL;
    // //set as input mode    
    // io_conf.mode = GPIO_MODE_INPUT;
    // //enable pull-up mode
    // io_conf.pull_up_en = 1;
    // gpio_config(&io_conf);
    // //gpio_pulldown_dis(DIG_GPIO);
    // //gpio_pullup_en(DIG_GPIO);
   
    return 1;
}

int read_gpio(gpio_num_t ch) {
    
    int lvl = gpio_get_level(ch);
    printf("GPIO[%d] intr, val: %d\n", ch, lvl);
    return lvl;
}