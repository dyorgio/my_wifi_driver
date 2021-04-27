#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/moduleparam.h>

#include "wf_os_api.h"
#include "wf_list.h"
#include "hif.h"
#include "sdio.h"
#include "hif_queue.h"
#include "wf_debug.h"
#define SDIO_HW_QUEUE_HIGH  (1)
#define SDIO_HW_QUEUE_MID   (2)
#define SDIO_HW_QUEUE_LOW   (3)

#define SDIO_RETRUN_OK      (0)
#define SDIO_RETRUN_FAIL    (1)

#define SDIO_VENDOR_ID_WL   0x02e7
#define WL_DEVICE_ID_SDIO   0x9086
#define SDIO_BLK_SIZE       (512)

#define ALIGN_SZ_RXBUFF     8

#define RND4(x) (((x >> 2) + (((x & 3) == 0) ?  0: 1)) << 2)

#define WORD_LEN (4)

#ifndef BIT
#define BIT(x)  (1 << (x))
#endif

#define ADDR_CHANGE(addr) ((8 << 13) | ((addr) & 0xFFFF))

static int sdhz = 0;
module_param(sdhz,  int, 0);

static void sdio_interrupt_deregister(struct sdio_func *func);



static int sdio_func_print(struct sdio_func *func)
{
    LOG_I("func_num:%d, vender:0x%x, device:0x%x, max_blksize:%d, cur_blksize:%d, state:%d",
          (int)func->num,
          (int)func->vendor,
          (int)func->device,
          (int)func->max_blksize,
          (int)func->cur_blksize,
          func->state
         );

    return 0;
}

/* to set func->max_blksize, func->cur_blksize*/
static wf_u32 sdio_func_set_blk(struct sdio_func *func, unsigned blksize)
{
    int ret = 0;

    sdio_claim_host(func);
    ret = sdio_set_block_size(func, blksize);
    if( ret )
    {
        LOG_E("[%s] sdio_set_block_size failed",__func__);
        sdio_release_host(func);
        return SDIO_RETRUN_FAIL;
    }

    ret = sdio_enable_func(func);
    if( 0 != ret )
    {
        LOG_E("[%s] sdio_enable_func failed",__func__);
        sdio_release_host(func);
        return SDIO_RETRUN_FAIL;
    }
    sdio_release_host(func);

    return ret;
}



static wf_u32 sdio_get_current_time(void)
{
    return jiffies;
}


static wf_u8 sdio_get_devid(wf_u32 addr)
{
    wf_u8 dev_id;
    wf_u16 pdev_id;

    pdev_id = (wf_u16)(addr >> 16);
    switch (pdev_id)
    {
        case 0x1025:
            dev_id = 0;
            break;

        case 0x1026:
            dev_id = 8;
            break;
        case 0x1031:
        case 0x1032:
        case 0x1033:
            dev_id = 4 + (pdev_id - 0x1031);
            break;
        default:
            dev_id = 8;
            break;
    }

    return dev_id;
}



static wf_u32 sdio_get_destaddr(const wf_u32 src_addr, wf_u8 *p_id, wf_u16 *p_set)
{
    wf_u8 dev_id;
    wf_u16 val_set;
    wf_u32 des_addr;
    int is_sdio_id = 0;

    dev_id = sdio_get_devid(src_addr);
    val_set = 0;

    switch (dev_id)
    {
        case 0:
            is_sdio_id = 1;
            dev_id   = 8;
            val_set = src_addr & 0xFFFF;
            break;
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            val_set = src_addr & 0x1FFF;
            break;
        case 7:
            val_set = src_addr & 0x0003;
            break;

        case 8:
        default:
            dev_id = 8;
#ifdef CONFIG_RICHV200_FPGA
            val_set = src_addr & 0xFFFF;
#else
            val_set = src_addr & 0x7FFF;
#endif
            break;
    }
    des_addr = (dev_id << 13) | val_set;


    if (p_id)
    {
        if(!is_sdio_id)
        {
            *p_id = dev_id;
        }
        else
        {
            *p_id = 0;
        }
    }
    if (p_set) *p_set = val_set;

    return des_addr;
}

static wf_u32 sdio_data_disable(struct sdio_func *func)
{
    int err     = 0;
    wf_u8 recv      = 0;
    wf_u8 send      = 0;

    sdio_claim_host(func);
    recv = sdio_readb(func, ADDR_CHANGE(WL_REG_HCTL), &err);
    if (err)
    {
        LOG_E("%s: ERR (%d) addr=0x%x\n", __func__, err, ADDR_CHANGE(WL_REG_HCTL));
        sdio_release_host(func);
        return SDIO_RETRUN_FAIL;
    }

    if (!(recv & BIT(1)))
    {
        LOG_E("recv bit(1) failed");
        sdio_release_host(func);
        return SDIO_RETRUN_FAIL;
    }

    send = recv | BIT(0);
    sdio_writeb(func, send, ADDR_CHANGE(WL_REG_HCTL), &err);
    if (err)
    {
        LOG_E("%s: ERR (%d) addr=0x%x\n", __func__, err, ADDR_CHANGE(WL_REG_HCTL));
        sdio_release_host(func);
        return SDIO_RETRUN_FAIL;
    }

    sdio_release_host(func);
    return SDIO_RETRUN_OK;

}


static wf_u8 sdio_data_enable(struct sdio_func *func)
{
    int err     = 0;
    wf_u8 recv      = 0;
    wf_u8 send      = 0;
    wf_timer_t timer;

    sdio_claim_host(func);
    recv = sdio_readb(func, ADDR_CHANGE(WL_REG_HCTL), &err);
    if (err)
    {
        LOG_E("%s: ERR (%d) addr=0x%x\n", __func__, err, ADDR_CHANGE(WL_REG_HCTL));
        sdio_release_host(func);
        return 0;
    }
    if(recv & BIT(1) ) //resume
    {
        sdio_release_host(func);
        return 0;
    }

    send = recv & ( ~ BIT(0));
    sdio_writeb(func, send, ADDR_CHANGE(WL_REG_HCTL), &err);
    if (err)
    {
        LOG_E("%s: ERR (%d) addr=0x%x\n", __func__, err, ADDR_CHANGE(WL_REG_HCTL));
        sdio_release_host(func);
        return 0;
    }

    /* polling for BIT1 */
    wf_timer_set(&timer, 200);
    while (1)
    {
        recv = sdio_readb(func, ADDR_CHANGE(WL_REG_HCTL), &err);
        if (err)
        {
            LOG_E("%s: ERR (%d) addr=0x%x\n", __func__, err, ADDR_CHANGE(WL_REG_HCTL));
            sdio_release_host(func);
            return 0;
        }

        if (!err && (recv & BIT(1)))
            break;

        if (wf_timer_expired(&timer))
        {
            LOG_E("timeout(err:%d) sdh_val:0x%02x\n",err, recv);
            sdio_release_host(func);
            return 0;
        }
    }

    sdio_release_host(func);

    return 1;

}

static wf_u32 sdio_get_ac_index_by_qsel(wf_u8 qsel, wf_u8 sec)
{
    wf_u32 addr = 0;
    if(sec)
    {
        LOG_I("sec:%d",sec);
        switch (qsel)
        {
            case 0:
            case 1:
            case 2:
            case 3:
                addr = AC5_IDX;
                break;
            case 4:
            case 5:
                addr = AC4_IDX;
                break;
            case 6:
            case 7:
                addr = AC3_IDX;
                break;
            case QSLT_BEACON:
                addr = AC3_IDX;
                break;
            case QSLT_HIGH:
                addr = AC3_IDX;
                break;
            case QSLT_MGNT:
            default:
                addr = AC3_IDX;
                break;
        }
    }
    else if(0 == sec)
    {
        switch (qsel)
        {
            case 0:
            case 1:
            case 2:
            case 3:
                addr = AC3_IDX;
                break;
            case 4:
            case 5:
                addr = AC4_IDX;
                break;
            case 6:
            case 7:
                addr = AC5_IDX;
                break;
            case QSLT_BEACON:
                addr = AC5_IDX;
                break;
            case QSLT_HIGH:
                addr = AC5_IDX;
                break;
            case QSLT_MGNT:
            default:
                addr = AC5_IDX;
                break;
        }

    }

    return addr;

}


wf_u8 sdio_get_hwQueue_by_fifoID(wf_u8 fifo_id)
{
    wf_u8 ret;

    switch(fifo_id)
    {
        case 1:
        case 2:
        case 4:
            ret = SDIO_HW_QUEUE_HIGH;
            break;

        case 5:
            ret = SDIO_HW_QUEUE_MID;
            break;

        case 6:
        default:
            ret = SDIO_HW_QUEUE_LOW;
            break;
    }

    return ret;
}


wf_u8 sdio_get_fifoaddr_by_que_Index(wf_u8 queIndex, wf_u32 len, wf_u32 *fifo_addr, wf_u8 sec)
{
    wf_u8 fifo_id;

    switch(queIndex)
    {
#ifdef CONFIG_RICHV200_FPGA
        case CMD_QUEUE_INX:
            fifo_id = 3;
            break;
#endif
        case BE_QUEUE_INX:
        case BK_QUEUE_INX:
            if (sec == 1)
            {
                fifo_id = 6;
            }
            else
            {
                fifo_id = 6;
            }
            break;

        case VI_QUEUE_INX:
            fifo_id = 5;
            break;

        case MGT_QUEUE_INX:
        case BCN_QUEUE_INX:
        case VO_QUEUE_INX:
        case HIGH_QUEUE_INX:
            if (sec == 1)
            {
                fifo_id = 1;
            }
            else
            {
                fifo_id = 1;
            }
            break;

        case READ_QUEUE_INX:
            fifo_id = 7;
            break;
        default:
            break;
    }

    if (fifo_id == 7)
    {
        *fifo_addr = (fifo_id << 13) | ((len/4) & 0x0003);
    }
    else
    {
        *fifo_addr = (fifo_id << 13) | ((len/4) & 0x1FFF);
    }

    //LOG_I("sdio_get_fifoaddr_by_que_Index -> fifo_id[%d] fifo_addr[0x%x]",fifo_id, *fifo_addr);
    return fifo_id;

}


static int sdio_write_data(struct sdio_func *func, wf_u32 addr, wf_u8 * data, wf_u32 len)
{

    wf_u32 des_addr    = 0;
    wf_u8 device_id    = 0;
    wf_u16 val_set     = 0;
    wf_u8 sus_leave    = 0;
    wf_u32 *err        = NULL;
    int err_ret     = 0;
    int i           = 0;
	hif_node_st *node;
    wf_u32 print_val;
    wf_u32 len4rnd;
	
	node		= sdio_get_drvdata(func);

    len4rnd = WF_RND4(len);

    if(len <= 4)
    {
        des_addr = sdio_get_destaddr(addr, &device_id, &val_set);
        if (device_id == 8 && val_set < 0x100)
        {
            sus_leave = sdio_data_enable(func);
        }
    }
    else
    {
        //LOG_D("%s,addr=0x%04x,len=%d",__FUNCTION__,addr,len);

        sdio_get_fifoaddr_by_que_Index(addr,len4rnd,&des_addr, 0);
    }

    // print addr and value
#if 0
    if(len == 1)
    {
        LOG_I("write 0x%02x -> addr[0x%08x],des_addr=[0x%08x]",data[0],addr,des_addr);
    }
    else if (len == 2)
    {
        LOG_I("write 0x%04x -> addr[0x%08x],des_addr=[0x%08x]",*((wf_u16 *)data),addr,des_addr);
    }
    else if (len == 4)
    {
        LOG_I("write 0x%08x -> addr[0x%08x],des_addr=[0x%08x]",*((wf_u32 *)data),addr,des_addr);
    }
    else
    {
        LOG_I("write %d len bytes-> addr[0x%08x],des_addr=[0x%08x]",len,addr,des_addr);
    }
#endif

	
    // end of print
    sdio_claim_host(func);
    if(WORD_LEN == len)
    {
        wf_u32 data_value = * (wf_u32*)data;
		if(hm_get_mod_removed() == wf_false && node->dev_removed == wf_true)
    	{
        	return -1;
    	}
        sdio_writel(func, data_value, des_addr, &err_ret);
        if (err_ret < 0)
        {
            LOG_E("sderr: Failed to write word, Err: 0x%08x,%s, ret:%d\n", addr, __func__,err_ret);
        }
    }
    else if(WORD_LEN < len )
    {
#if 0
        printk("int sdio_write_data,printk data:len=%d\n",len);
        for(i=0; i<len; i++)
        {
            printk("0x%02x,",data[i]);
            if((i+1)%16 == 0)
                printk("\n");
        }
        printk("\nend\n");
#endif

        err_ret=sdio_memcpy_toio(func,des_addr,data,len);
        if (err_ret < 0)
        {
            LOG_E("sderr: sdio_memcpy_toio, Err: 0x%08x,%s, ret:%d\n", addr, __func__,err_ret);
        }
    }
    else
    {
        for (i = 0; i < len; i++)
        {
            sdio_writeb(func, *(data + i), des_addr + i, &err_ret);
            if (err_ret)
            {
                LOG_E("sderr: Failed to write word, Err: 0x%08x,%s\n", addr, __func__);
                break;
            }
        }
    }
    sdio_release_host(func);

    if(1 == sus_leave )
    {
        sdio_data_disable(func);
        //LOG_I("[%s,%d]",__func__,__LINE__);
    }
    return err_ret;


}

int sdio_read_data(struct sdio_func *func, wf_u32 addr, wf_u8 * data, wf_u32 len)
{
    wf_u32 des_addr = 0;
    wf_u8 device_id = 0;
    wf_u16 val_set      = 0;
    wf_u8 sus_leave     = 0;
    wf_u32 *err     = NULL;
    wf_u32 value32      = 0;
    int err_ret     = 0;
    int i           = 0;
    wf_u32 len4rnd;

    if(len <= 4)
    {
        des_addr = sdio_get_destaddr(addr, &device_id, &val_set);
        if (device_id == 8 && val_set < 0x100)
        {
            sus_leave =sdio_data_enable(func);
        }
    }
    else
    {
        len4rnd = WF_RND4(len);
        sdio_get_fifoaddr_by_que_Index(READ_QUEUE_INX,len4rnd,&des_addr, 0);
    }

    sdio_claim_host(func);
    if(WORD_LEN == len)
    {
        value32 = sdio_readl(func,des_addr,&err_ret);
        if (err_ret)
        {
            LOG_E("sderr: Failed to read word, Err: 0x%08x,%s, ret=%d\n", addr, __func__, err_ret);
        }
        else
        {
            for (i = 0; i < len; i++)
            {
                data[i] = ((wf_u8*)&value32)[i];
            }
        }
    }
    else if(WORD_LEN < len )
    {
        //LOG_D("in sdio_read_data,dest_addr=0x%04x,len=%d",des_addr,len);
        err_ret = sdio_memcpy_fromio(func,data,des_addr,len);
        if (err_ret < 0)
        {
            LOG_E("sderr: sdio_memcpy_fromio, Err: 0x%08x,%s, ret:%d\n", addr, __func__,err_ret);
        }
    }
    else
    {
        for (i = 0; i < len; i++)
        {
            data[i] = sdio_readb(func, des_addr + i, &err_ret);
            //LOG_I("read addr[0x%08x]=0x%02x,des_addr=0x%08x",addr+i,data[i],des_addr+i);
            if (err_ret)
            {
                LOG_E("sderr: Failed to write word, Err: 0x%08x,%s\n", addr, __func__);
                break;
            }

        }
    }
    sdio_release_host(func);
    if(1 == sus_leave )
    {
        sdio_data_disable(func);
    }

    return err_ret;

}

static int wf_sdio_wait_enough_txoqt_space(hif_node_st *hif_info, wf_u32 PageIdx, wf_u8 pg_num)
{
    hif_sdio_st *sd         = &hif_info->u.sdio;
    nic_info_st *nic_info   = NULL;
    wf_u8 DedicatedPgNum = 0;
    wf_u8 RequiredPublicFreePgNum = 0;
    int i=0;
    int ret = 0;

    nic_info = hif_info->nic_info[0];
    //LOG_I("PageIdx:%d",PageIdx);
    while (sd->SdioTxFIFOFreePage[PageIdx] +  sd->SdioTxFIFOFreePage[ACX_IDX] <= TX_RESERVED_PG_NUM + pg_num)
    {
        if (nic_info->is_driver_stopped || nic_info->is_surprise_removed)
        {
            return WF_RETURN_FAIL;
        }

        for (i = 0; i < 6; i++)
        {
            ret = sdio_read_data(sd->func, SDIO_BASE | (WL_REG_AC0_FREEPG + i * 4),&sd->SdioTxFIFOFreePage[i],1);
            if(0 != ret)
            {
                if(-ENOMEDIUM == ret)
                {
                    nic_info->is_surprise_removed = wf_true;
                }
            }
        }
    }

    DedicatedPgNum = sd->SdioTxFIFOFreePage[PageIdx];
    if (pg_num <= DedicatedPgNum)
    {
        sd->SdioTxFIFOFreePage[PageIdx] -= pg_num;
    }
    else
    {
        sd->SdioTxFIFOFreePage[PageIdx] = 0;
        RequiredPublicFreePgNum = pg_num - DedicatedPgNum;
        sd->SdioTxFIFOFreePage[ACX_IDX] -= RequiredPublicFreePgNum;
    }

    return WF_RETURN_OK;
}

static int wf_sdio_tx_wait_freeAGG(hif_node_st *hif_info,wf_u8 need_agg_num)
{
    hif_sdio_st *sd         = &hif_info->u.sdio;
    nic_info_st *nic_info   = NULL;
    int n = 0;
    int ret = 0;

    nic_info = hif_info->nic_info[0];
    while(sd->SdioTxOQTFreeSpace < need_agg_num)
    {
        if (nic_info->is_driver_stopped || nic_info->is_surprise_removed)
        {
            return WF_RETURN_FAIL;
        }

        ret = sdio_read_data(sd->func, SDIO_BASE | WL_REG_AC_OQT_FREEPG,&sd->SdioTxOQTFreeSpace,1);
        if(0 != ret)
        {
            if(-ENOMEDIUM == ret)
            {
                nic_info->is_surprise_removed = wf_true;
            }
        }
        if ((++n % 60) == 0)
        {
            if ((n % 300) == 0)
            {
                LOG_W("%s(%d): QOT free space(%d), agg_num: %d\n", __func__, n, sd->SdioTxOQTFreeSpace, need_agg_num);

            }
        }
    }

    return 0;
}
static int wf_sdio_tx_wait_freePG(hif_node_st *hif_info, wf_u8 hw_queue, wf_u8 need_pg_num)
{
    nic_info_st *nic_info   = NULL;
    wf_u32 value32 = 0;
    int ret    = 0;
    wf_u8 pg_type;
    hif_sdio_st *sd         = &hif_info->u.sdio;
    nic_info = hif_info->nic_info[0];

    while (1)
    {
        if (nic_info->is_driver_stopped || nic_info->is_surprise_removed)
        {
            return WF_RETURN_FAIL;
        }

        ret = sdio_read_data(sd->func, SDIO_BASE | WL_REG_PUB_FREEPG,(wf_u8*)&value32,4);
        if(0 != ret )
        {
            if(-ENOMEDIUM == ret)
            {
                nic_info->is_surprise_removed = wf_true;
            }
        }
        sd->tx_fifo_ppg_num = value32;

        ret = sdio_read_data(sd->func, SDIO_BASE | WL_REG_HIG_FREEPG,(wf_u8*)&value32,4);
        if(0 != ret )
        {
            if(-ENOMEDIUM == ret)
            {
                nic_info->is_surprise_removed = wf_true;
            }
        }
        sd->tx_fifo_hpg_num = value32;

        ret = sdio_read_data(sd->func, SDIO_BASE | WL_REG_MID_FREEPG,(wf_u8*)&value32,4);
        if(0 != ret )
        {
            if(-ENOMEDIUM == ret)
            {
                nic_info->is_surprise_removed = wf_true;
            }
        }
        sd->tx_fifo_mpg_num = value32;

        ret = sdio_read_data(sd->func, SDIO_BASE | WL_REG_LOW_FREEPG,(wf_u8*)&value32,4);
        if(0 != ret )
        {
            if(-ENOMEDIUM == ret)
            {
                nic_info->is_surprise_removed = wf_true;
            }
        }
        sd->tx_fifo_lpg_num = value32;

        if (hw_queue == SDIO_HW_QUEUE_HIGH) // HIGH
        {
            if (sd->tx_fifo_hpg_num + sd->tx_fifo_ppg_num  > TX_RESERVED_PG_NUM + need_pg_num)
            {
                return WF_RETURN_OK;
            }
        }
        else if (hw_queue == SDIO_HW_QUEUE_MID) // MID
        {
            if (sd->tx_fifo_mpg_num + sd->tx_fifo_ppg_num  > TX_RESERVED_PG_NUM + need_pg_num)
            {
                return WF_RETURN_OK;
            }
        }
        else   // LOW
        {
            if (sd->tx_fifo_lpg_num + sd->tx_fifo_ppg_num  > TX_RESERVED_PG_NUM + need_pg_num)
            {
                return WF_RETURN_OK;
            }
        }

    };

    return  WF_RETURN_FAIL;

}



static int wf_sdio_tx_flow_agg_num_check(hif_node_st *hif_info, wf_u8 agg_num,wf_u8 data_type)
{
    hif_sdio_st *sd         = &hif_info->u.sdio;
    int txAggNum_remain = 0;

    if( data_type != WF_PKT_TYPE_FRAME  )
    {
        return WF_RETURN_OK;
    }

    txAggNum_remain = sd->SdioTxOQTFreeSpace - agg_num;
    if (txAggNum_remain < 0)
    {
        return  WF_RETURN_FAIL;
    }

    return WF_RETURN_OK;
}

static int wf_sdio_tx_flow_free_pg_check(hif_node_st *hif_info, wf_u32 hw_queue, wf_u8 pg_num,wf_u8 data_type)
{
    hif_sdio_st *sd         = &hif_info->u.sdio;
    nic_info_st *nic_info   = NULL;
    int lpg_remain_num      = 0;
    int mpg_remain_num      = 0;
    int hpg_remain_num      = 0;
    int ppg_remain_num      = 0;
    wf_u32 n                   = 0;
    wf_u32 value32             = 0;
    nic_info = hif_info->nic_info[0];

    if(data_type != WF_PKT_TYPE_FRAME  )
    {
        return WF_RETURN_OK;
    }

    if(hw_queue == SDIO_HW_QUEUE_LOW)   //LOW
    {
        lpg_remain_num = sd->tx_fifo_lpg_num - pg_num;

        if (lpg_remain_num < 0)
        {
            if ((sd->tx_fifo_ppg_num  + lpg_remain_num) > TX_RESERVED_PG_NUM)
            {
                return WF_RETURN_OK;
            }
            else
            {
                return WF_RETURN_FAIL;
            }
        }
        else
        {
            return WF_RETURN_OK;
        }
    }
    else if (hw_queue == SDIO_HW_QUEUE_MID)    //MID
    {
        mpg_remain_num = sd->tx_fifo_mpg_num - pg_num;

        if (mpg_remain_num < 0)
        {
            if ((sd->tx_fifo_ppg_num  + mpg_remain_num) > TX_RESERVED_PG_NUM)
            {
                return WF_RETURN_OK;
            }
            else
            {
                return WF_RETURN_FAIL;
            }
        }
        else
        {
            return WF_RETURN_OK;
        }
    }
    else                                       // HIGH
    {
        hpg_remain_num = sd->tx_fifo_hpg_num - pg_num;

        if (hpg_remain_num < 0)
        {
            if ((sd->tx_fifo_ppg_num  + hpg_remain_num) > TX_RESERVED_PG_NUM)
            {
                return WF_RETURN_OK;
            }
            else
            {
                return WF_RETURN_FAIL;
            }
        }
        else
        {
            return WF_RETURN_OK;
        }
    }

    return WF_RETURN_FAIL;
}


static int wf_sdio_tx_flow_agg_num_ctl(hif_node_st *hif_info, wf_u8 agg_num)
{
    hif_sdio_st *sd         = &hif_info->u.sdio;

    sd->SdioTxOQTFreeSpace -= agg_num;

    return WF_RETURN_OK;
}

static int wf_sdio_tx_flow_free_pg_ctl(hif_node_st *hif_info, wf_u32 hw_queue, wf_u8 pg_num)
{
    hif_sdio_st *sd         = &hif_info->u.sdio;

    if(hw_queue == SDIO_HW_QUEUE_LOW)   //LOW
    {
        if (sd->tx_fifo_lpg_num > pg_num)
        {
            sd->tx_fifo_lpg_num = sd->tx_fifo_lpg_num - pg_num;
        }
        else
        {
            sd->tx_fifo_ppg_num = sd->tx_fifo_ppg_num - (pg_num - sd->tx_fifo_lpg_num);
            sd->tx_fifo_lpg_num = 0;
        }
    }
    else if (hw_queue == SDIO_HW_QUEUE_MID)    //MID
    {
        if (sd->tx_fifo_mpg_num > pg_num)
        {
            sd->tx_fifo_mpg_num = sd->tx_fifo_mpg_num - pg_num;
        }
        else
        {
            sd->tx_fifo_ppg_num = sd->tx_fifo_ppg_num - (pg_num - sd->tx_fifo_mpg_num);
            sd->tx_fifo_mpg_num = 0;
        }
    }
    else                                       // HIGH
    {
        if (sd->tx_fifo_hpg_num > pg_num)
        {
            sd->tx_fifo_hpg_num = sd->tx_fifo_hpg_num - pg_num;
        }
        else
        {
            sd->tx_fifo_ppg_num = sd->tx_fifo_ppg_num - (pg_num - sd->tx_fifo_hpg_num);
            sd->tx_fifo_hpg_num = 0;
        }
    }

    return WF_RETURN_OK;
}

static int wf_sdio_req_packet(hif_sdio_st * sd, wf_u8 rw, wf_u32 addr, wf_u32 pkt_len, void *pkt)
{
    int err_ret = 0;
    sdio_claim_host(sd->func);
    if (rw)
    {
        err_ret = sdio_memcpy_fromio(sd->func, pkt, addr, pkt_len);
    }
    else
    {
        err_ret = sdio_memcpy_toio(sd->func, addr, pkt, pkt_len);
    }
    sdio_release_host(sd->func);
    return err_ret;
}


static int wf_sdio_write_net_data(hif_node_st *hif_node, wf_u32 addr, wf_u8 * sdata, wf_u32 slen)
{
    hif_sdio_st        *sd      = &hif_node->u.sdio;
    data_queue_node_st * data_queue_node    = (data_queue_node_st *)sdata;
    int ret                                 = 0;
    struct xmit_buf *pxmitbuf   = (struct xmit_buf *)data_queue_node->param;
    int cnt                     = 0;
    int pg_num                  = 0;
    wf_u32 write_size              = 0;
    wf_u32 page_idx                = 0;
    wf_u8 DedicatedPgNum           = 0;
    wf_u8 RequiredPublicFreePgNum  = 0;
    wf_u32 fifo_addr;
    wf_u8 fifo_id;
    wf_u8 data_type = 0;
    wf_u32 len4rnd;
    wf_u8 hw_queue;

    data_queue_node->state = TX_STATE_FLOW_CTL;
    len4rnd = WF_RND4(slen);

    hif_node->trx_pipe.tx_queue_cnt++;

    pg_num = pxmitbuf->pg_num;

    if(2!= addr && 5!=addr && 8!=addr)
    {
        LOG_I("sec:%d,addr:%d",pxmitbuf->encrypt_algo,addr);
    }
    fifo_id=sdio_get_fifoaddr_by_que_Index(addr, len4rnd, &fifo_addr, pxmitbuf->encrypt_algo);
    hw_queue = sdio_get_hwQueue_by_fifoID(fifo_id);

#ifdef CONFIG_RICHV200_FPGA
    data_type = data_queue_node->buff[0]& 0x03;
    ret = wf_sdio_tx_flow_free_pg_check(hif_node,hw_queue,pg_num,data_type);
    if (ret == WF_RETURN_FAIL)
    {
        ret = wf_sdio_tx_wait_freePG(hif_node, hw_queue, pg_num);
        data_queue_node->state = TX_STATE_FLOW_CTL_SECOND;
    }

    ret = wf_sdio_tx_flow_agg_num_check(hif_node,pxmitbuf->agg_num,data_type);
    if (ret == WF_RETURN_FAIL)
    {
        wf_sdio_tx_wait_freeAGG(hif_node,pxmitbuf->agg_num);
        data_queue_node->state = TX_STATE_FLOW_CTL_SECOND;
    }
#else
    //page_idx =sdio_get_ac_page_by_que_index(addr,pxmitbuf->encrypt_algo);
    page_idx = sdio_get_ac_index_by_qsel(pxmitbuf->qsel,pxmitbuf->encrypt_algo);
    ret = wf_sdio_wait_enough_txoqt_space(hif_node, page_idx,pg_num);
#endif
    data_queue_node->state = TX_STATE_SENDING;
    ret = wf_sdio_req_packet(&hif_node->u.sdio, SDIO_WD, fifo_addr, len4rnd, data_queue_node->buff);
    if(ret < 0)
    {
        LOG_E("[%s] wf_sdio_req_packet failed,ret=%d, q_sel:%d, fifo_addr:0x%x, data_addr:%p, data_len:%d",
              __func__,ret, addr, fifo_addr, data_queue_node->buff, len4rnd);

        wf_sdio_tx_wait_freePG(hif_node, hw_queue, pg_num);
        wf_sdio_tx_wait_freeAGG(hif_node,pxmitbuf->agg_num);
    }
    else
    {
        wf_sdio_tx_flow_free_pg_ctl(hif_node, hw_queue, pg_num);
        wf_sdio_tx_flow_agg_num_ctl(hif_node, pxmitbuf->agg_num);
    }

    data_queue_node->state = TX_STATE_COMPETE;

    if(data_queue_node->tx_callback_func)
    {
        ret = data_queue_node->tx_callback_func(data_queue_node->tx_info, data_queue_node->param);
        if(wf_true == ret)
        {
            ret = WF_RETURN_OK;
        }
    }

    wf_data_queue_insert(&hif_node->trx_pipe.free_tx_queue, data_queue_node);
    return WF_RETURN_OK;
}



static int wf_sdio_read_net_data(hif_node_st *hif_node, wf_u32 addr, wf_u8 * rdata, wf_u32 rlen)
{
    hif_sdio_st        *sd      = &hif_node->u.sdio;
    struct sk_buff *pskb        = NULL;
    int rx_queue_len            = 0;
    wf_u32 read_size               = 0;
    int ret                     = -1;
    wf_u32 fifo_addr;
    SIZE_T tmpaddr = 0;
    SIZE_T alignment = 0;

    if((rlen < 16) || rlen > MAX_RXBUF_SZ)
    {
        LOG_E("[%s] rlen error ，rlen:%d",__func__,rlen);
        return -1;
    }
    hif_node->trx_pipe.rx_queue_cnt++;

    if(rlen>512)
    {
        read_size = WF_RND_MAX(rlen, 512);
    }
    else
    {
        read_size = rlen;
    }

#if HIF_QUEUE_PRE_ALLOC_DEBUG
    if(read_size > WF_MAX_RECV_BUFF_LEN_SDIO + HIF_QUEUE_ALLOC_SKB_ALIGN_SZ)
    {
        LOG_E("[%s] read_size(%d) should be less than (%d)",__func__,read_size,WF_MAX_RECV_BUFF_LEN_SDIO + HIF_QUEUE_ALLOC_SKB_ALIGN_SZ);
        while(1);
    }

    pskb = skb_dequeue(&hif_node->trx_pipe.free_rx_queue_skb);
    if(NULL == pskb)
    {
#if 1

        if(hif_node->trx_pipe.alloc_cnt<HIF_MAX_ALLOC_CNT)
        {
            LOG_W("[%s] alloc_skb again",__func__);
            hif_node->trx_pipe.alloc_cnt++;
            wf_hif_queue_alloc_skb(&hif_node->trx_pipe.free_rx_queue_skb,hif_node->hif_type);
        }
        else
        {
            LOG_W("[%s] wf_alloc_skb skip", __func__);
        }
        return -1;
#else
        LOG_E("[%s] skb_dequeue failed",__func__);
        return -1;
#endif
    }
    else
    {
        if (skb_tailroom(pskb) < read_size)
        {
            skb_queue_tail(&hif_node->trx_pipe.free_rx_queue_skb, pskb);
            return -1;
        }
    }
#else  // real-time alloc skb
    pskb = wf_alloc_skb(read_size + ALIGN_SZ_RXBUFF);
    if(NULL == pskb)
    {
        LOG_E("[%s] wf_alloc_skb failed ", __func__);
        return -1;
    }
    else
    {
        if (skb_tailroom(pskb) < read_size)
        {
            LOG_E("[%s] skb tailroom is not enough ", __func__);
            return -1;
        }

        tmpaddr = (SIZE_T) pskb->data;
        alignment = tmpaddr & (ALIGN_SZ_RXBUFF - 1);
        skb_reserve(pskb, (ALIGN_SZ_RXBUFF - alignment));
    }
#endif // HIF_QUEUE_PRE_ALLOC_DEBUG

    sdio_get_fifoaddr_by_que_Index(addr, read_size, &fifo_addr, 0);
    ret = wf_sdio_req_packet(sd, SDIO_RD, fifo_addr, read_size, pskb->data);
    if(ret < 0)
    {
        LOG_E("sdio_req_packet error:0x%x",ret);
        if (pskb)
        {
#if HIF_QUEUE_PRE_ALLOC_DEBUG
            skb_reset_tail_pointer(pskb);
            pskb->len = 0;
            skb_queue_tail(&hif_node->trx_pipe.free_rx_queue_skb, pskb);
#else
            wf_free_skb(pskb);
            pskb = NULL;
#endif
        }
        return -1;
    }

    skb_put(pskb, rlen);

    if (hif_node->nic_info[0] != NULL)
    {
        ret = wf_rx_data_len_check(hif_node->nic_info[0],pskb->data, pskb->len);
    }
    else
    {
        ret = -1;
    }

    if (ret == -1)
    {
        if (pskb)
        {
#if HIF_QUEUE_PRE_ALLOC_DEBUG
            skb_reset_tail_pointer(pskb);
            pskb->len = 0;
            skb_queue_tail(&hif_node->trx_pipe.free_rx_queue_skb, pskb);
#else
            wf_free_skb(pskb);
            pskb = NULL;
#endif
        }
    }
    else
    {

        if (wf_rx_data_type(pskb->data) == WF_PKT_TYPE_FRAME)
        {
            skb_queue_tail(&hif_node->trx_pipe.rx_queue, pskb);

            rx_queue_len = skb_queue_len(&hif_node->trx_pipe.rx_queue);
            if ( rx_queue_len <= 1)
            {
#if HIF_QUEUE_RX_WORKQUEUE_USE
                hif_node->trx_pipe.rx_wq.ops->workqueue_work(&hif_node->trx_pipe.rx_wq);
#else
                wf_tasklet_sched(&hif_node->trx_pipe.recv_task);
#endif
            }

            ret = WF_PKT_TYPE_FRAME;
        }
        else
        {
#ifdef CONFIG_RICHV200_FPGA
            if (wf_rx_cmd_check(pskb->data, pskb->len) == 0)
            {
                switch(wf_rx_data_type(pskb->data))
                {
                    case WF_PKT_TYPE_CMD:
                        wf_hif_bulk_cmd_post(hif_node, pskb->data, pskb->len);
                        ret = WF_PKT_TYPE_CMD;
                        break;

                    case WF_PKT_TYPE_FW:
                        wf_hif_bulk_fw_post(hif_node, pskb->data, pskb->len);
                        ret = WF_PKT_TYPE_FW;
                        break;

                    case WF_PKT_TYPE_REG:
                        wf_hif_bulk_reg_post(hif_node, pskb->data, pskb->len);
                        ret = WF_PKT_TYPE_REG;
                        break;

                    default:
                        LOG_E("recv rxd type error");
                        ret = -1;
                        break;
                }

            }

            if (pskb)
            {
#if HIF_QUEUE_PRE_ALLOC_DEBUG
                skb_reset_tail_pointer(pskb);
                pskb->len = 0;
                skb_queue_tail(&hif_node->trx_pipe.free_rx_queue_skb, pskb);
#else
                wf_free_skb(pskb);
                pskb = NULL;
#endif
            }

#endif
        }
    }

    return ret;
}

// 0:cmd  1:data
static int wf_sdio_recv_data(hif_node_st   *hif_info, wf_u32 len)
{
    hif_sdio_st *sd = &hif_info->u.sdio;
    int ret         = 0;
    int rx_req_len = 0;

    ret = wf_sdio_read_net_data(hif_info, READ_QUEUE_INX, NULL, len);
    if (ret < 0)
    {
        //LOG_E("wf_sdio_recv error,ret:%d",ret);
        return -1;
    }

    return ret;
}


static int wf_sdio_write(struct hif_node_ *node, unsigned char flag, unsigned int addr, char *data, int datalen)
{
    int ret = 0;
    if(NULL ==node|| 0 == datalen)
    {
        LOG_I("node null,datalen:%d",datalen);
        return -1;
    }
    if(hm_get_mod_removed() == wf_false && node->dev_removed == wf_true)
    {
        return -1;
    }
    else
    {
        if(WF_SDIO_TRX_QUEUE_FLAG == flag)
        {
            ret = wf_sdio_write_net_data(node, addr,data,datalen);
        }
        else
        {
            ret = sdio_write_data(node->u.sdio.func,addr,data,datalen);
        }
    }
    return ret;
}

static int wf_sdio_read(struct hif_node_ *node, unsigned char flag, unsigned int addr, char *data, int datalen)
{
    int ret = 0;

    //LOG_I("wf_sdio_read");
    if(hm_get_mod_removed() == wf_false && node->dev_removed == wf_true)
    {
        return -1;
    }
    else
    {
        if(WF_SDIO_TRX_QUEUE_FLAG == flag)
        {
            ret = wf_sdio_read_net_data(node,addr,data,datalen);
        }
        else
        {
            ret = sdio_read_data(node->u.sdio.func,addr,data,datalen);
        }
    }
    return ret;
}


static int wf_sdio_show(struct hif_node_ *node)
{
    return 0;
}

static int wf_sdio_init(struct hif_node_ *node)
{
    wf_u32  ret     = 0;
    wf_u8 value8;

    LOG_I("wf_sdio_init start");
    LOG_I("sdio_id=%d\n",node->u.sdio.sdio_id);

    node->u.sdio.sdio_id = hm_new_sdio_id(NULL);
    node->u.sdio.block_transfer_len = SDIO_BLK_SIZE;
    node->u.sdio.SdioRxFIFOCnt      = 0;

#ifdef CONFIG_RICHV200_FPGA
    node->u.sdio.sdio_himr          = 0x10D;
    node->u.sdio.sdio_hisr          = -1;
    node->u.sdio.SysIntrMask        = -1;
#else
    node->u.sdio.sdio_himr          = 1;
    node->u.sdio.sdio_hisr          = 1;
    node->u.sdio.SysIntrMask        = 1;
#endif
    sdio_func_print(node->u.sdio.func);

    LOG_I("wf_sdio_init end");
    return 0;
}

static int wf_sdio_deinit(struct hif_node_ *node)
{
    int ret = 0;
    LOG_I("wf_sdio_deinit start");
    sdio_func_print(node->u.sdio.func);
    LOG_I("remove sdio_id:%d",node->u.sdio.sdio_id);
    ret = hm_del_sdio_id(node->u.sdio.sdio_id);
    if(ret)
    {
        LOG_E("hm_del_sdio_id(%d) failed",node->u.sdio.sdio_id);
    }
    sdio_claim_host(node->u.sdio.func);
    sdio_disable_func(node->u.sdio.func);
    sdio_release_host(node->u.sdio.func);

    LOG_I("wf_sdio_deinit end");

    return 0;
}


static struct hif_node_ops  sdio_node_ops=
{
    .hif_read           = wf_sdio_read,
    .hif_write          = wf_sdio_write,
    .hif_show           = wf_sdio_show,
    .hif_init           = wf_sdio_init,
    .hif_exit           = wf_sdio_deinit,
    .hif_tx_queue_insert = wf_tx_queue_insert,
    .hif_tx_queue_empty  = wf_tx_queue_empty,
};


#define SDIO_INT_READ_BUG 0

static int sdio_get_rx_len(struct sdio_func *func)
{
    int ret = 0;
    wf_u32 rx_req_len = 0;

    ret = sdio_read_data(func, SDIO_BASE | WL_REG_SZ_RX_REQ, (wf_u8*)&rx_req_len, 4);
    if(ret)
    {
        LOG_E("read rx_req_len error,ret=0x%08x",ret);
        rx_req_len = 0;
    }

    return rx_req_len;
}

static void sdio_irq_handle(struct sdio_func *func)
{
    hif_node_st  *hif_node = NULL;
    wf_u32 isr;
    wf_u32 isr_clean;
    int ret         = 0;
    wf_u32 rx_req_len = 0;
    int ret_type;

    hif_node = sdio_get_drvdata(func);
    hif_node->u.sdio.irq_cnt++;
    hif_node->u.sdio.int_flag++;

    hif_node->u.sdio.sdio_hisr = 0;

    ret = sdio_read_data(func, SDIO_BASE | WL_REG_HISR, (wf_u8 *)&isr, 4);
    if(0 != ret )
    {
        LOG_E("[%s] read hisr error, ret=0x%08x",__func__,ret);
        return;
    }

    if (isr == 0)
    {
        LOG_E("[%s] irq:0x%x error, check irq",__func__,isr);
        hif_node->u.sdio.int_flag--;
        return;
    }

    hif_node->u.sdio.sdio_hisr = isr;

    if (hif_node->u.sdio.sdio_hisr & WF_BIT(2))
    {
        wf_u32 value;
        wf_u32 i;
        nic_info_st *nic_info2  = NULL;
        LOG_E("[%s] tx dma error!!",__func__);

        sdio_read_data(func, 0x288, (wf_u8 *)&value, 4);
        LOG_I("0x288----0x%x",value);
        sdio_read_data(func, 0x288, (wf_u8 *)&value, 4);
        LOG_I("0x288----0x%x",value);
        sdio_read_data(func, 0x210, (wf_u8 *)&value, 4);
        LOG_I("0x210----0x%x",value);
        sdio_read_data(func, 0x438, (wf_u8 *)&value, 4);
        LOG_I("0x438----0x%x",value);

        sdio_read_data(func, 0x210, (wf_u8 *)&value, 4);
        sdio_write_data(func,0x210, (wf_u8*)&value, 4);

        isr_clean = WF_BIT(2);
        sdio_write_data(func,SDIO_BASE | WL_REG_HISR, (wf_u8*)&isr_clean, 4);
        hif_node->u.sdio.sdio_hisr ^= WF_BIT(2);
    }

    if (hif_node->u.sdio.sdio_hisr & WF_BIT(3))
    {
        wf_u32 value;
        LOG_E("[%s] rx dma error!!",__func__);
        sdio_read_data(func, 0x210, (wf_u8 *)&value, 4);
        LOG_I("0x210----0x%x",value);
        sdio_read_data(func, 0x210, (wf_u8 *)&value, 4);
        LOG_I("0x210----0x%x",value);
        sdio_read_data(func, 0x288, (wf_u8 *)&value, 4);
        LOG_I("0x288----0x%x",value);
        isr_clean = WF_BIT(3);
        sdio_write_data(func,SDIO_BASE | WL_REG_HISR, (wf_u8*)&isr_clean, 4);
        hif_node->u.sdio.sdio_hisr ^= WF_BIT(3);
    }

    if (hif_node->u.sdio.sdio_hisr & (WF_BIT(8)|WF_BIT(0)))
    {
        while(1)
        {
            rx_req_len = sdio_get_rx_len(func);

            if (rx_req_len == 0)
            {
                break;
            }

            if ((rx_req_len < MIN_RXD_SIZE) || (rx_req_len > MAX_RXBUF_SZ) )
            {
                LOG_E("wf_sdio_recv error,rx_req_len:0x%x",rx_req_len);
                break;
            }

            ret_type = wf_sdio_read_net_data(hif_node, READ_QUEUE_INX, NULL, rx_req_len);
            if (ret_type < 0)
            {
                //LOG_E("ret_type:%d",ret_type);
                break;
            }

            if (ret_type != TYPE_DATA)
            {
                isr_clean = WF_BIT(8);  /* clean CMD irq bit*/
                //LOG_I("[1]clean CMD irq bit,rx_req_len:%d,ret_type:0x%x",rx_req_len,ret_type);
                sdio_write_data(func, SDIO_BASE | WL_REG_HISR, (wf_u8*)&isr_clean, 4);
                //LOG_I("[2]clean CMD irq bit,rx_req_len:%d,ret_type:0x%x",rx_req_len,ret_type);
                hif_node->u.sdio.sdio_hisr ^= WF_BIT(8);
            }
        }

        hif_node->u.sdio.sdio_hisr ^= WF_BIT(0);

    }
    hif_node->u.sdio.int_flag--;

}


static int sdio_interrupt_register(struct sdio_func *func)
{
    int err;

    sdio_claim_host(func);
    err = sdio_claim_irq(func, &sdio_irq_handle);
    if (err < 0)
    {
        LOG_E("[%s] sdio_interrupt_register error ",__func__);
        sdio_release_host(func);
        return err;
    }
    sdio_release_host(func);
    return 0;
}

static void sdio_interrupt_deregister(struct sdio_func *func)
{
    int err;
	hif_node_st *node;
	
	node		= sdio_get_drvdata(func);

    sdio_claim_host(func);
    err = sdio_release_irq(func);
	if(hm_get_mod_removed() == wf_false && node->dev_removed == wf_true)
	{
		return ;
	}
    if (err < 0)
    {
        LOG_E("[%s] sdio_interrupt_deregister error ",__func__);
    }

    sdio_release_host(func);
}


static int sdio_ctl_init(struct sdio_func *func)
{
    wf_u8  value8;
    wf_u16 value16;
    int count = 0;
    int initSuccess;
    int ret;

    LOG_I("[%s] ",__func__);

    sdio_read_data(func, SDIO_BASE|WL_REG_HCTL, &value8, 1);
    value8 &= 0xFE;
    ret = sdio_write_data(func, SDIO_BASE|WL_REG_HCTL, &value8, 1);
    if( ret < 0)
    {
        LOG_E("[%s] 0x903a failed, check!!!",__func__);
        return ret;
    }

    while(1)
    {
        ret = sdio_read_data(func, SDIO_BASE | WL_REG_HCTL, &value8, 1);
        if(0 != ret )
        {
            break;
        }
        if(value8 & WF_BIT(1))
        {
            initSuccess = wf_true;
            break;
        }

        count++;
        if(count > 1000)
        {
            break;
        }
    }

    if(initSuccess == wf_false)
    {
        LOG_E("[%s] failed!!!",__func__);
        return -1;
    }

#if 0

#if 0

    //set bus mode
    value8 = 0x10;
    ret = sdio_write_data(func, 0x07,&value8, 1);
    ret = sdio_read_data(func,0x07,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x07,value8);

    //enable IOE1
    value8 = 0x02;
    ret = sdio_write_data(func, 0x02,&value8, 1);
    ret = sdio_read_data(func,0x02,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x02,value8);


    //wait io_ready
    while(1)
    {
        ret = sdio_read_data(func,0x03,&value8, 1);
        if(0x02 == value8)
        {
            break;
        }
    }

    //set bus mode
    value16 = 0x2810;
    ret = sdio_write_data(func, SDIO_BASE|0x07,(wf_u8*)&value16, 2);
    ret = sdio_read_data(func,SDIO_BASE|0x07,(wf_u8*)&value16, 2);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x07,value16);

    //enable IEN1,IEN0
    value8 = 0x03;
    ret = sdio_write_data(func,SDIO_BASE| 0x04,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x04,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x04,value8);

    //set card capability
    value8 = 0xff;
    ret = sdio_write_data(func, SDIO_BASE|0x08,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x08,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x08,value8);

    //set bus suspend
    value8 = 0x00;
    ret = sdio_write_data(func, SDIO_BASE|0x0c,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x0c,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x0c,value8);

    //set function select
    value8 = 0x00;
    ret = sdio_write_data(func, SDIO_BASE|0x0d,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x0d,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x0d,value8);


    //set function 0 block size
    value8 = 0x01;
    ret = sdio_write_data(func, SDIO_BASE|0x10,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x10,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x10,value8);
    value8 = 0x00;
    ret = sdio_write_data(func,SDIO_BASE| 0x11,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x11,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x11,value8);

    //set powet control
    value8 = 0x00;
    ret = sdio_write_data(func,SDIO_BASE| 0x12,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x12,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x12,value8);

    //set high speed
    value8 = 0x00;
    ret = sdio_write_data(func, SDIO_BASE|0x13,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x13,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x13,value8);

    //set sdio standard function
    value8 = 0x00;
    ret = sdio_write_data(func, SDIO_BASE|0x100,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x100,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x13,value8);

    //set function 1 block size,[8:0]=9'h100
    value8 = 0x00;
    ret = sdio_write_data(func, SDIO_BASE|0x110,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x110,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x110,value8);

    value8 = 0x01;
    ret = sdio_write_data(func, SDIO_BASE|0x111,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x111,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x111,value8);


#endif
    //set int mask timeout value low
    value8 = 0x04;
    ret = sdio_write_data(func, SDIO_BASE|0x9002,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x9002,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x9002,value8);

    //set int mask timeout value high
    value8 = 0x00;
    ret = sdio_write_data(func, SDIO_BASE|0x9003,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x9003,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x9003,value8);

    //enable host int
    value8 = 0xff;
    ret = sdio_write_data(func, SDIO_BASE|0x9004,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x9004,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x9004,value8);

    value8 = 0xcf;
    ret = sdio_write_data(func, SDIO_BASE|0x9034,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x9034,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x9034,value8);

    value8 = 0x1;
    ret = sdio_write_data(func, SDIO_BASE|0x9058,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x9058,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x9058,value8);

    value8 = 0x3;
    ret = sdio_write_data(func, SDIO_BASE|0x9060,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x9060,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x9060,value8);

    value8 = 0x8;
    ret = sdio_write_data(func, SDIO_BASE|0x9048,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x9048,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x9048,value8);

    value8 = 0x3;
    ret = sdio_write_data(func, SDIO_BASE|0x9060,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x9060,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x9060,value8);


#if 0
    value8 = 0xff;
    ret = sdio_write_data(func, SDIO_BASE|0x904c,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x904c,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x904c,value8);

    value8 = 0xff;
    ret = sdio_write_data(func, SDIO_BASE|0x904d,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x904d,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x904d,value8);

    value8 = 0xff;
    ret = sdio_write_data(func, SDIO_BASE|0x904e,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x904e,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x904e,value8);

    value8 = 0xff;
    ret = sdio_write_data(func, SDIO_BASE|0x904f,&value8, 1);
    ret = sdio_read_data(func,SDIO_BASE|0x904f,&value8, 1);
    LOG_I("[%s] reg[0x%x]---0x%x",__func__,0x904f,value8);
#endif
#endif

    return 0;
}


static int sdio_speed_set(struct sdio_func *func,unsigned int hz)
{
    struct mmc_host *host = func->card->host;
    struct mmc_ios *ios = &host->ios;

    LOG_I("sdio speed max=%d,min=%d,set=%d",host->f_max,host->f_min,hz);
    sdio_claim_host(func);
    host->ios.clock = hz;
    host->ops->set_ios(host, ios);
    sdio_release_host(func);

    return 0;

}
static int sdio_speed_get(struct sdio_func *func)
{
    struct mmc_host *host = func->card->host;

    LOG_I("sdio speed %d",host->ios.clock);

    return 0;

}

static int sdio_func_probe(struct sdio_func *func, const struct sdio_device_id *id)
{
    hif_node_st  *hif_node = NULL;
    int ret;

    LOG_I("Class=%x",func->class);
    LOG_I("Vendor ID:%x",func->vendor);
    LOG_I("Device ID:%x",func->device);
    LOG_I("Function#:%d",func->num);

    if(sdhz)
    {
        sdio_speed_set(func,sdhz);
    }

    /*set sdio blksize*/
    sdio_func_print(func);
    ret = sdio_func_set_blk(func, SDIO_BLK_SIZE);
    if(ret)
    {
        LOG_E("[%s] sdio_func_set_blk failed",__func__);
        return SDIO_RETRUN_FAIL;
    }

    if (sdio_ctl_init(func) < 0)
    {
        LOG_E("sdio_ctl_init error");
        return -ENODEV;
    }

    hif_node_register(&hif_node,HIF_SDIO,&sdio_node_ops);
    if(NULL == hif_node )
    {
        LOG_E("hif_node_register for HIF_SDIO failed");
        return -ENODEV;
    }

    LOG_I("%p",hif_node);
    hif_node->u.sdio.sdio_hisr_en = 0;
    hif_node->u.sdio.func = func;
    hif_node->u.sdio.irq_cnt = 0;
    sdio_set_drvdata(func,hif_node);

    if(NULL != hif_node->ops->hif_init)
    {
        hif_node->ops->hif_init(hif_node);
    }
    ret = sdio_interrupt_register(hif_node->u.sdio.func);

    if(ret < 0)
    {
        LOG_E("interrupt_register failed");
        hif_node_unregister(hif_node);
        return -ENODEV;

    }

    /*insert netdev*/
    if( NULL != hif_node->ops->hif_insert_netdev )
    {
        if(hif_node->ops->hif_insert_netdev(hif_node) < 0)
        {
            LOG_E("hif_insert_netdev error");
            return -ENODEV;
        }
    }
    else
    {
        if (hif_dev_insert(hif_node) < 0)
        {
            LOG_E("hif_dev_insert error");
            hif_node_unregister(hif_node);
            return -ENODEV;
        }
    }
    {
        hif_sdio_st *sd = &hif_node->u.sdio;
        wf_u32 value32;
        wf_u8 value8;

        sdio_read_data(sd->func, SDIO_BASE | WL_REG_PUB_FREEPG,(wf_u8*)&value32,4);
        sd->tx_fifo_ppg_num = value32;
        sdio_read_data(sd->func, SDIO_BASE | WL_REG_HIG_FREEPG,(wf_u8*)&value32,4);
        sd->tx_fifo_hpg_num = value32;
        sdio_read_data(sd->func, SDIO_BASE | WL_REG_MID_FREEPG,(wf_u8*)&value32,4);
        sd->tx_fifo_mpg_num = value32;
        sdio_read_data(sd->func, SDIO_BASE | WL_REG_LOW_FREEPG,(wf_u8*)&value32,4);
        sd->tx_fifo_lpg_num = value32;
        sdio_read_data(sd->func, SDIO_BASE | WL_REG_EXT_FREEPG,(wf_u8*)&value32,4);
        sd->tx_fifo_epg_num = value32;

        LOG_I("ppg_num:%d,hpg_num:%d,mgp_num:%d,lpg_num:%d,epg_num:%d",sd->tx_fifo_ppg_num,sd->tx_fifo_hpg_num,
              sd->tx_fifo_mpg_num,sd->tx_fifo_lpg_num,sd->tx_fifo_epg_num);

        sdio_read_data(sd->func, SDIO_BASE | WL_REG_QUE_PRI_SEL,(wf_u8*)&value8,1);
        if (value8 & BIT(0))
        {
            sd->tx_no_low_queue = wf_false;
            LOG_I("HIGH(fifo_1,fifo_2,fifo_4) MID(fifi_5) LOW(fifo_6)");
        }
        else
        {
            sd->tx_no_low_queue = wf_true;
            LOG_I("HIGH(fifo_1,fifo_2,fifo_4) MID(fifi_5, fifo_6)");
        }

        sdio_read_data(sd->func, SDIO_BASE | WL_REG_AC_OQT_FREEPG, &sd->SdioTxOQTFreeSpace,1);
        LOG_I("SdioTxOQTFreeSpace:%d",sd->SdioTxOQTFreeSpace);

        LOG_I("[%s] end",__func__);
    }

    sdio_speed_get(func);
    return 0;
}

static void sdio_func_remove(struct sdio_func *func)
{
    wf_list_t *tmp   = NULL;
    wf_list_t *next  = NULL;
    hif_node_st *node       = sdio_get_drvdata(func);

    LOG_I("[%s] start",__func__);
    if( NULL != node )
    {
        hif_dev_removed(node);
        sdio_interrupt_deregister(node->u.sdio.func);
        wf_sdio_deinit(node);
        hif_node_unregister(node);
    }
    else
    {
        LOG_E("wf_sdio_func_remove failed");
        return ;
    }


    LOG_I("[%s] end",__func__);
}

struct sdio_device_id sdio_ids[] =
{
    {SDIO_DEVICE(SDIO_VENDOR_ID_WL, WL_DEVICE_ID_SDIO)},
    {},
};

static struct sdio_driver wf_sdio_driver =
{
    .name       = "s9083h",
    .id_table   = sdio_ids,
    .probe      = sdio_func_probe,
    .remove     = sdio_func_remove,
};


int sdio_init(void)
{
    int ret = 0;
    LOG_I("sdio_init !!!");
    ret = sdio_register_driver(&wf_sdio_driver);

    return ret;
}

int sdio_exit(void)
{
    LOG_I("sdio_exit !!!");
    sdio_unregister_driver(&wf_sdio_driver);
    return 0;
}



int wf_sdioh_interrupt_disable(void *hif_info)
{
    wf_u32 himr = 0;
    hif_sdio_st *sd = &((hif_node_st*)hif_info)->u.sdio;
    if(sd->sdio_hisr_en)
    {
        sdio_write_data(sd->func, SDIO_BASE | WL_REG_HIMR,(wf_u8*)&himr, WORD_LEN);
        sd->sdio_hisr = himr;
        sd->sdio_hisr_en = 0;
    }

    return WF_RETURN_OK;
}

int wf_sdioh_interrupt_enable(void *hif_info)
{
    wf_u32 himr;
    hif_sdio_st *sd = &((hif_node_st*)hif_info)->u.sdio;

    himr = cpu_to_le32(sd->sdio_himr);
    sdio_write_data(sd->func, SDIO_BASE | WL_REG_HIMR, (wf_u8*)&himr, WORD_LEN);
    sd->sdio_hisr = himr;
    sd->sdio_hisr_en = 1;
    return WF_RETURN_OK;
}

int wf_sdioh_config(void *hif_info)
{
    hif_sdio_st *sd = &((hif_node_st*)hif_info)->u.sdio;
    unsigned int value32;
    unsigned long value16;
    unsigned char value8;
    /* need open bulk transport */
    //enable host int
    value32 = 0xFFFFFFFF;
    sdio_write_data(sd->func, SDIO_BASE | 0x9048,(wf_u8 *)&value32, 4);

    value32 = 0xFFFFFFFF;
    sdio_write_data(sd->func, SDIO_BASE | 0x904C,(wf_u8 *)&value32, 4);

#if 0
    sdio_read_data(sd->func,  SDIO_BASE | WL_REG_TXCTL,(wf_u8 *)&value32, 4);
    value32 &= 0xFFF8;
    sdio_write_data(sd->func, SDIO_BASE | WL_REG_TXCTL, (wf_u8 *)&value32, 4);
#endif

    value8 = 0xFF;
    sdio_write_data(sd->func, SDIO_BASE | 0x9068, (wf_u8 *)&value8, 1);

    return 0;
}
