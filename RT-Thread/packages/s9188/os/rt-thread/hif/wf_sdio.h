#ifndef __WF_SDIO_H__
#define __WF_SDIO_H__

#include "wf_os_api.h"

#ifdef CONFIG_RICHV200
#define MIN_RXD_SIZE      16
#else
#define MIN_RXD_SIZE      24
#endif

#define SDIO_BASE                   0x10250000

#define WL_REG_HIMR                 0x9004
#define WL_REG_HISR                 0x9008
#define WL_REG_SZ_RX_REQ            0x9010
#define WL_REG_HCTL                 0x903A
#define WL_REG_PUB_FREEPG           0x901C
#define WL_REG_HIG_FREEPG           0x9020
#define WL_REG_MID_FREEPG           0x9024
#define WL_REG_LOW_FREEPG           0x9028
#define WL_REG_EXT_FREEPG           0x902C

#define WL_REG_AC0_FREEPG           0x9014

#define WL_REG_TXCTL				0x9000
#define WL_REG_AC_OQT_FREEPG        0x9030

#define WL_LPS_CTL1					0x9058
#define WL_LPS_CTL2 			    0x905C

#define WL_REG_QUE_PRI_SEL          0x906C

#define TX_SIDEROAD_FLOW_CTRL       0x9096

#define SDIO_RD                     1
#define SDIO_WD                     0

#define SDIO_FIFO_TYP_NUM 6

#define TX_FLOW_WF_DATA            3
#define TX_RESERVED_PG_NUM         65

enum WF_SDIO_OPERATION_FLAG
{
    WF_SDIO_NORMAL_FLAG     = 0,
    WF_SDIO_TRX_QUEUE_FLAG  = 1,
};

struct hif_sdio_management_;
typedef struct hif_sdio_management_
{
    wf_u8 sdio_id;
    struct rt_sdio_function *func;
    wf_u32 sdio_himr;
    wf_u32 sdio_hisr;
    wf_u8 sdio_hisr_en;
    wf_u32 SysIntrMask;
    int sdio_irq_start;
    wf_u32 block_transfer_len;
    wf_u16 SdioRxFIFOSize;
    wf_u8 SdioRxFIFOCnt;
    wf_u8 SdioTxOQTFreeSpace;
    wf_u8 SdioTxFIFOFreePage[SDIO_FIFO_TYP_NUM];
    wf_u8 SdioTxOQTMaxFreeSpace;
    wf_u8 tx_no_low_queue;
    wf_u8 tx_fifo_ppg_num;
    wf_u8 tx_fifo_lpg_num;
    wf_u8 tx_fifo_epg_num;
    wf_u8 tx_fifo_mpg_num;
    wf_u8 tx_fifo_hpg_num;
    wf_u16 tx_fifo_all_use_num;
    wf_u8 last_tx_ppg_num;
    wf_work_struct irq_work;
    wf_workqueue_struct *irq_wq;
    wf_u64 irq_cnt;
    int int_flag;
    wf_bool isr_triggered;
}hif_sdio_st;

int sdio_init(void);
int sdio_exit(void);




int wf_sdioh_interrupt_disable(void *hif_info);
int wf_sdioh_interrupt_enable(void *hif_info);
int wf_sdioh_config(void *hif_info);



#endif