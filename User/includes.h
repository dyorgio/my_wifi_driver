/****************************************Copyright (c)****************************************************
**                             成 都 世 纪 华 宁 科 技 有 限 公 司
**                                http://www.huaning-iot.com
**                                http://hichard.taobao.com
**
**
**--------------File Info---------------------------------------------------------------------------------
** File Name:           includes.h
** Last modified Date:  2018.06.13
** Last Version:        v1.0
** Description:         系统总头文件
**
**--------------------------------------------------------------------------------------------------------
** Created By:          任海波
** Created date:        2018.06.13
** Version:             v1.0
** Descriptions:        The original version 初始版本
**
**--------------------------------------------------------------------------------------------------------
** Modified by:
** Modified date:
** Version:
** Description:
**
*********************************************************************************************************/

#ifndef  __INCLUDES_H__
#define  __INCLUDES_H__

#ifdef __cplusplus
extern "C" {
#endif

/*********************************************************************************************************
  Standard header files 标准头文件
*********************************************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

/*********************************************************************************************************
  RT Thread操作系统头文件
*********************************************************************************************************/
#include <rtdef.h>
#include <rtthread.h>
#include <rtdevice.h>

/*********************************************************************************************************
  网络协议栈头文件
*********************************************************************************************************/
#include <lwip_if.h>
#ifdef RT_USING_POSIX
#include <sys/socket.h>
#include <netdb.h> 
#else
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#endif
  
/*********************************************************************************************************
  文件系统支持的头文件
*********************************************************************************************************/
#include <dfs_posix.h>
//#include <minIni.h>
  
/*********************************************************************************************************
  在线包库支持头文件
*********************************************************************************************************/
  
/*********************************************************************************************************
** 设备驱动头文件
*********************************************************************************************************/
#include "board_init.h"
#include "board_led.h"
#include "board_serial.h"
#include "board_ethernet.h"
#include "board_io_ctrl.h"

/*********************************************************************************************************
  User's header files 用户头文件
*********************************************************************************************************/
#include "Main.h"
#include "DeviceInit.h"



#ifdef __cplusplus
}
#endif

#endif
/*********************************************************************************************************
  END FILE
*********************************************************************************************************/
