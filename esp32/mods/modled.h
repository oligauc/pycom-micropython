/*
 * Copyright (c) 2017, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef MODLED_H
#define	MODLED_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "driver/rmt.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
    
/******************************************************************************
 DEFINE PUBLIC TYPES
 ******************************************************************************/

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} color_t;

typedef struct {
    rmt_channel_t rmt_channel;
    gpio_num_t gpio; 
    color_t color;
    SemaphoreHandle_t access_semaphore;
} led_info_t;

/******************************************************************************
 DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/

bool led_init(led_info_t *led_info);
void led_set_color(led_info_t *led_info);
void led_clear_color(led_info_t *led_info);

#ifdef	__cplusplus
}
#endif

#endif	/* MODLED_H */

