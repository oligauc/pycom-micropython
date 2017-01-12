/*
 * Copyright (c) 2017, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdbool.h>
#include "modled.h"

/******************************************************************************
 DEFINE PRIVATE CONSTANTS
 ******************************************************************************/
#define LED_RMT_CLK_DIV      (8) // The RMT clock is 80 MHz
#define LED_RMT_DUTY_CYLE    (50)
#define LED_RMT_CARRIER_FREQ (100)
#define LED_RMT_MEM_BLCK     (1)


#define LED_BIT_1_HIGH_PERIOD (9) // 900ns 
#define LED_BIT_1_LOW_PERIOD  (3) // 300ns 
#define LED_BIT_0_HIGH_PERIOD (3) // 300ns 
#define LED_BIT_0_LOW_PERIOD  (9) // 900ns 
#define BITS_PER_COLOR        (8)

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/

static bool led_init_rmt(led_info_t *led_info);
static void led_encode_color(color_t *color, rmt_item32_t *buf);
static void set_high_bit(rmt_item32_t *item);
static void set_low_bit(rmt_item32_t *item);

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
bool led_init(led_info_t *led_info)
{
    if ((led_info == NULL) ||
        (led_info->rmt_channel == RMT_CHANNEL_MAX) ||
        (led_info->gpio > GPIO_NUM_33) || 
        (led_info->access_semaphore == NULL)) {
        return false;
    }
    
    led_info->color = (color_t){0x0, 0x0, 0x0};
    
    if (!led_init_rmt(led_info)) {
        return false;
    }

    xSemaphoreGive(led_info->access_semaphore);
 
    return true;
}

void led_set_color(led_info_t *led_info)
{
    rmt_item32_t rmt_buf[BITS_PER_COLOR  * 3];
    
    led_encode_color(&led_info->color, rmt_buf);
    
    rmt_wait_tx_done(led_info->rmt_channel);
    xSemaphoreTake(led_info->access_semaphore, portMAX_DELAY);
    rmt_write_items(led_info->rmt_channel, rmt_buf, sizeof(rmt_item32_t) * BITS_PER_COLOR * 3, false);
    xSemaphoreGive(led_info->access_semaphore);
}

void led_clear_color(led_info_t *led_info)
{
    rmt_item32_t rmt_buf[BITS_PER_COLOR  * 3];
    
    color_t color = (color_t){0x0, 0x0, 0x0};
    led_encode_color(&color, rmt_buf);
    
    rmt_wait_tx_done(led_info->rmt_channel);
    xSemaphoreTake(led_info->access_semaphore, portMAX_DELAY);
    rmt_write_items(led_info->rmt_channel, rmt_buf, sizeof(rmt_item32_t) * BITS_PER_COLOR * 3, false);
    xSemaphoreGive(led_info->access_semaphore);
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

static void set_high_bit(rmt_item32_t *item){
    item->duration0 = LED_BIT_1_HIGH_PERIOD;
    item->level0    = 1;
    item->duration1 = LED_BIT_1_LOW_PERIOD;
    item->level1    = 0;
}

static void set_low_bit(rmt_item32_t *item){
    item->duration0 = LED_BIT_0_HIGH_PERIOD;
    item->level0    = 1;
    item->duration1 = LED_BIT_0_LOW_PERIOD;
    item->level1    = 0;
}

static void led_encode_color(color_t *color, rmt_item32_t *buf){
   
    uint32_t rmt_idx = 0;
   
    /* Green */
    for (uint8_t bit = BITS_PER_COLOR; bit != 0; --bit){    
        bool is_set = (color->green >> (bit - 1)) & 0x1;
        if (is_set){
            set_high_bit(&(buf[rmt_idx]));
        } else {
            set_low_bit(&(buf[rmt_idx]));
        }
        
        rmt_idx++;
    }
    
    /* Red */
    for (uint8_t bit = BITS_PER_COLOR; bit != 0; --bit){
        
        bool is_set = (color->red >> (bit - 1)) & 0x1;
        if (is_set){
            set_high_bit(&(buf[rmt_idx]));
        } else {
            set_low_bit(&(buf[rmt_idx]));
        }
        
        rmt_idx++;
    }
    
    /* Blue */
    for (uint8_t bit = BITS_PER_COLOR; bit != 0; --bit){    
        bool is_set = (color->blue >> (bit - 1)) & 0x1;
        if (is_set){
            set_high_bit(&(buf[rmt_idx]));
        } else {
            set_low_bit(&(buf[rmt_idx]));
        }
     
        rmt_idx++;
    }
}

static bool led_init_rmt(led_info_t *led_info)
{
    rmt_config_t rmt_info = {
        .rmt_mode = RMT_MODE_TX,
        .channel = led_info->rmt_channel,
        .clk_div = LED_RMT_CLK_DIV,
        .gpio_num = led_info->gpio,
        .mem_block_num = LED_RMT_MEM_BLCK,
        .tx_config = {
            .loop_en = false,
            .carrier_freq_hz = LED_RMT_CARRIER_FREQ, 
            .carrier_duty_percent = LED_RMT_DUTY_CYLE,
            .carrier_level = RMT_CARRIER_LEVEL_LOW,
            .carrier_en = false,
            .idle_level = RMT_IDLE_LEVEL_LOW,
            .idle_output_en = true,
        }
    };

    if (rmt_config(&rmt_info) != ESP_OK) {
        return false;
    }
    
    if (rmt_driver_install(rmt_info.channel, 0, 0) != ESP_OK) {
        return false;
    }

    return true;
}
