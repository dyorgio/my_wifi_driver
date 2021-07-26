/****************************************Copyright (c)****************************************************
**                             成 都 世 纪 华 宁 科 技 有 限 公 司
**                                http://www.huaning-iot.com
**                                http://hichard.taobao.com
**
**
**--------------File Info---------------------------------------------------------------------------------
** File Name:           board_led.h
** Last modified Date:  2014-12-13
** Last Version:        v1.00
** Description:         LED驱动头文件声明
**--------------------------------------------------------------------------------------------------------
** Created By:          Renhaibo
** Created date:        2014-12-13
** Version:             v1.00
** Descriptions:        
**--------------------------------------------------------------------------------------------------------
** Modified by:         
** Modified date:       
** Version:             
** Description:         
*********************************************************************************************************/
#ifndef __BOARD_LED_H__
#define __BOARD_LED_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <rtthread.h>

/*********************************************************************************************************
**  实现的外部函数声明
*********************************************************************************************************/
void rt_hw_led_on(rt_uint8_t led);
void rt_hw_led_off(rt_uint8_t led);
void rt_hw_led_toggle(rt_uint8_t led);


#ifdef __cplusplus
    }
#endif      // __cplusplus
    
#endif      // __BOARD_LED_H__
/*********************************************************************************************************
END FILE
*********************************************************************************************************/
