/****************************************Copyright (c)****************************************************
**                             成 都 世 纪 华 宁 科 技 有 限 公 司
**                                http://www.huaning-iot.com
**                                http://hichard.taobao.com
**
**
**--------------File Info---------------------------------------------------------------------------------
** File Name:           arch_io_fmc.h
** Last modified Date:  2019-03-15
** Last Version:        v1.0
** Description:         fsmc模块寄存器封装函数实现声明
**
**--------------------------------------------------------------------------------------------------------
** Created By:          Renhaibo任海波
** Created date:        2019-03-15
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
#ifndef __ARCH_IO_FMC_H__
#define __ARCH_IO_FMC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <rthw.h>

/*********************************************************************************************************
** 外部函数的声明
*********************************************************************************************************/
extern void FmcNandInit(void);
extern void FmcNandEnable(void);
extern void FmcNandEccEnable(void);
extern rt_uint32_t FmcNandEccRead(void);


#ifdef __cplusplus
    }
#endif      // __cplusplus

#endif // endif of __ARCH_IO_FMC_H__
/*********************************************************************************************************
  END FILE
*********************************************************************************************************/
