#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#include "driver/timer.h"

/**
 * GPIO status:
 * GPIO_INPUT_IO_0:  input, pulled up, interrupt from rising edge.
 */

#define GPIO_INPUT_IO_0     TRIGGER_BUTTON_GPIO
#define GPIO_INPUT_PIN_SEL  (1ULL<<GPIO_INPUT_IO_0)
#define ESP_INTR_FLAG_DEFAULT 0

static xQueueHandle gpio_evt_queue = NULL;
double pivot_time, time;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    timer_get_counter_time_sec(TIMER_GROUP_0, TIMER_0, &time);
    if((time - pivot_time) > 0.1){
        xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
        pivot_time = time;
    }
}

void trigger_button_task()
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_PIN_INTR_POSEDGE;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void*) GPIO_INPUT_IO_0);

    timer_config_t config={
        .counter_dir = TIMER_COUNT_UP,
        .divider = 80
    };
    timer_init(TIMER_GROUP_0, TIMER_0, &config);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
    timer_start(TIMER_GROUP_0, TIMER_0);

    timer_get_counter_time_sec(TIMER_GROUP_0, TIMER_0, &pivot_time);
    printf("start time: %lf\n",pivot_time);

    uint32_t io_num;
    while(1) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
}

