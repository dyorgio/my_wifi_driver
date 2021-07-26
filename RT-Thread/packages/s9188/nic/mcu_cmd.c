#include "common.h"
#include "wf_debug.h"

#define MCU_LINKED           (1)
#define MCU_UNLINKED         (0)

#define _HW_STATE_NOLINK_       0x00
#define _HW_STATE_ADHOC_        0x01
#define _HW_STATE_STATION_      0x02
#define _HW_STATE_AP_           0x03
#define _HW_STATE_MONITOR_      0x04
#define _HW_STATE_NO_EXIST_     0xAA

#ifndef BIT
#define BIT(x)  ( 1 << (x))
#endif

#define BIT0    0x00000001
#define BIT1    0x00000002
#define BIT2    0x00000004
#define BIT3    0x00000008
#define BIT4    0x00000010
#define BIT5    0x00000020
#define BIT6    0x00000040
#define BIT7    0x00000080
#define BIT8    0x00000100
#define BIT9    0x00000200
#define BIT10   0x00000400
#define BIT11   0x00000800
#define BIT12   0x00001000
#define BIT13   0x00002000
#define BIT14   0x00004000
#define BIT15   0x00008000
#define BIT16   0x00010000
#define BIT17   0x00020000
#define BIT18   0x00040000
#define BIT19   0x00080000
#define BIT20   0x00100000
#define BIT21   0x00200000
#define BIT22   0x00400000
#define BIT23   0x00800000
#define BIT24   0x01000000
#define BIT25   0x02000000
#define BIT26   0x04000000
#define BIT27   0x08000000
#define BIT28   0x10000000
#define BIT29   0x20000000
#define BIT30   0x40000000
#define BIT31   0x80000000
#define BIT32   0x0100000000
#define BIT33   0x0200000000
#define BIT34   0x0400000000
#define BIT35   0x0800000000
#define BIT36   0x1000000000

#define REG_BSSID0                      0x0618
#define REG_BSSID1                      0x0708

#define RND4(x) (((x >> 2) + (((x & 3) == 0) ?  0: 1)) << 2)

/* Data Rate Value */
#define RATE_1M                             2   /* 1M in unit of 500kb/s */
#define RATE_2M                             4   /* 2M */
#define RATE_5_5M                           11  /* 5.5M */
#define RATE_11M                            22  /* 11M */
#define RATE_22M                            44  /* 22M */
#define RATE_33M                            66  /* 33M */
#define RATE_6M                             12  /* 6M */
#define RATE_9M                             18  /* 9M */
#define RATE_12M                            24  /* 12M */
#define RATE_18M                            36  /* 18M */
#define RATE_24M                            48  /* 24M */
#define RATE_36M                            72  /* 36M */
#define RATE_48M                            96  /* 48M */
#define RATE_54M                            108 /* 54M */


wf_s32 translate_percentage_to_dbm(wf_u32 SignalStrengthIndex)
{
    wf_s32 SignalPower;

#ifdef CONFIG_SIGNAL_SCALE_MAPPING
    SignalPower = (char) ((SignalStrengthIndex + 1) >> 1);
    SignalPower -= 95;
#else
    SignalPower = SignalStrengthIndex - 100;
#endif

    return SignalPower; /* in dBM.raw data */
}


wf_s32 signal_scale_mapping(wf_s32 current_sig)
{
    wf_s32 result_sig = 0;

    if (current_sig >= 51 && current_sig <= 100)
    {
        result_sig = 100;
    }
    else if (current_sig >= 41 && current_sig <= 50)
    {
        result_sig = 80 + ((current_sig - 40) * 2);
    }
    else if (current_sig >= 31 && current_sig <= 40)
    {
        result_sig = 66 + (current_sig - 30);
    }
    else if (current_sig >= 21 && current_sig <= 30)
    {
        result_sig = 54 + (current_sig - 20);
    }
    else if (current_sig >= 10 && current_sig <= 20)
    {
        result_sig = 42 + (((current_sig - 10) * 2) / 3);
    }
    else if (current_sig >= 5 && current_sig <= 9)
    {
        result_sig = 22 + (((current_sig - 5) * 3) / 2);
    }
    else if (current_sig >= 1 && current_sig <= 4)
    {
        result_sig = 6 + (((current_sig - 1) * 3) / 2);
    }
    else
    {
        result_sig = current_sig;
    }

    return result_sig;

}


wf_s32 wf_mcu_cmd_get_status(nic_info_st *nic_info,wf_u32 cmd)
{
    wf_s32 ret;
    /*test base on hisilicon platform, it would need 25000*/
    wf_u32 timeout = 750*WF_HZ; //2000->8000->15000(for GK7202 , get efuse, dw fw, )
    wf_u32 data = 0;
    wf_u32 tryCnt = 0;

    if( UMSG_OPS_HAL_DW_FW == cmd)
    {
        timeout = 1500*WF_HZ;
    }

    // set mailbox int finish
    ret = wf_io_write32(nic_info, WF_MAILBOX_INT_FINISH, 0x12345678);
    if ( WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] WF_MAILBOX_INT_FINISH failed, check!!!", __func__);
        return ret;
    }

    // set mailbox triger int
    ret = wf_io_write8(nic_info, WF_MAILBOX_REG_INT, 1);
    if ( WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] WF_MAILBOX_REG_INT failed, check!!!", __func__);
        return ret;
    }

    while (timeout--)
    {
        wf_s32 err = 0;
        if ( nic_info->is_surprise_removed || nic_info->is_driver_stopped)
            return WF_RETURN_REMOVED_FAIL;

        tryCnt++;
        data = wf_io_read32(nic_info, WF_MAILBOX_INT_FINISH,&err);
        if(err)
        {
            if(WF_RETURN_REMOVED_FAIL == err)
            {
                LOG_W("[%s] deviced removed warning",__func__);
                return WF_RETURN_REMOVED_FAIL;
            }
            LOG_E("[%s] read failed,err:%d",__func__,err);
            break;
        }
        if ( NIC_USB == nic_info->nic_type && 0x55 == data)
        {
            //LOG_D("MCU Feedback [tryCnt:%d]",tryCnt);
            return WF_RETURN_OK;

        }
        else if (NIC_SDIO == nic_info->nic_type && 0x000000aa == data)
        {

            //LOG_D("MCU Feedback [tryCnt:%d]",tryCnt);
            return WF_RETURN_OK;

        }

        //wf_msleep(1);
    }

    LOG_I("timeout !!!  data:0x%x", data);
    return WF_RETURN_FAIL;
}


/*************************************************
 * Function     : wf_mcu_get_chip_version
 * Description  : To get the version of this chip
 * Input        : nic_info
 * Output       : version
 * Return       : 1. WF_RETURN_FAIL
                  2. WF_RETURN_OK
 *************************************************/

wf_s32 wf_mcu_get_chip_version(nic_info_st *nic_info, wf_u32 *version)
{
    wf_s32 ret   = 0;

    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_READ_VERSION, NULL, 0, version, 1);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;
}


/*************************************************
* Function     : wf_mcu_set_concurrent_mode
* Description  :
* Input        : nic_info
* Output       :
* Return       : 1. WF_RETURN_FAIL, function work fail
                 2. WF_RETURN_OK, function work well
*************************************************/
wf_s32 wf_mcu_set_concurrent_mode(nic_info_st *nic_info, wf_bool concur_mode)
{
    wf_s32 ret = 0;
    wf_u32 mode    = 0;

    if (concur_mode == wf_true)
    {
        mode = 1;
    }

    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_CONFIG_CONCURRENT_MODE, &mode, 1, NULL, 0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;
}

/*************************************************
* Function     : wf_mcu_cmd_term
* Description  :
* Input        : nic_info
* Output       :
* Return       : 1. WF_RETURN_FAIL, function work fail
                 2. WF_RETURN_OK, function work well
*************************************************/
wf_s32 wf_mcu_cmd_term(nic_info_st *nic_info)
{
    wf_s32 ret                 = 0;
    LOG_I("[%s] %p", __func__, nic_info);
    if ( NIC_USB == nic_info->nic_type)
    {
        ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_DEINIT, NULL, 0, NULL, 0);
    }
    else
    {
        //ret = mcu_cmd_communicate(nic_info,UMSG_OPS_HAL_DEINIT,NULL,0,NULL,0);
    }

    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;
}


/*************************************************
* Function     : wf_mcu_init_hardware1
* Description  :
* Input        : nic_info
* Output       :
* Return       : 1. WF_RETURN_FAIL, function work fail
                 2. WF_RETURN_OK, function work well
*************************************************/
wf_s32 wf_mcu_init_hardware1(nic_info_st *nic_info)
{
    wf_s32 ret = 0;

    wf_u32 is_dw    = 1;

    if (NIC_USB == nic_info->nic_type)
    {
        ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_INIT_STEP0, &is_dw, 1, NULL, 0);
    }
    else
    {
        wf_u32 u4Tmp[5] = { 0 };
        ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_GET_BCN_PAR, &is_dw, 1, u4Tmp, 5);
        LOG_I("[%s] BcnCtrlVal:%d ", __func__,u4Tmp[0]);
        LOG_I("[%s] TxPause:%d ", __func__,u4Tmp[1]);
        LOG_I("[%s] FwHwTxQCtrl:%d ", __func__,u4Tmp[2]);
        LOG_I("[%s] TbttR:%d ", __func__,u4Tmp[3]);
        LOG_I("[%s] CR_1:%d ", __func__,u4Tmp[4]);
    }

    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;
}


/*************************************************
* Function     : wf_mcu_burst_pktlen_init
* Description  :
* Input        : 1. nic_info
                 2. pktdata,this can be true or false
                 3. len, 'pktdata' length, here is 2
* Output       :
* Return       : 1. WF_RETURN_FAIL, function work fail
                 2. WF_RETURN_OK, function work well
*************************************************/
wf_s32 wf_mcu_burst_pktlen_init(nic_info_st *nic_info)
{
    wf_s32 ret = 0;

    wf_u32 u4Tmp[2] = { 0 };


    if (NIC_USB == nic_info->nic_type)
    {
        //ret = mcu_cmd_communicate(nic_info,UMSG_OPS_HAL_INIT_STEP0,&is_dw,1,NULL,0);
    }
    else
    {
        u4Tmp[0] = 0;
        u4Tmp[1] = 1;//1. normal chip
        ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_INIT_PKT_LEN, u4Tmp, 2, NULL, 0);
    }
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;
}


wf_s32 wf_mcu_ant_sel_init(nic_info_st *nic_info)
{
    wf_s32 ret = 0;

    if (NIC_USB == nic_info->nic_type)
    {
        //ret = mcu_cmd_communicate(nic_info,UMSG_OPS_HAL_INIT_STEP0,&is_dw,1,NULL,0);
    }
    else
    {
        ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_INIT_ANT_SEL, NULL, 0, NULL, 0);
    }

    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;
}


wf_s32 wf_mcu_update_tx_fifo(nic_info_st *nic_info)
{
    wf_s32 ret = 0;

    ret =   mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_UPDATE_TX_FIFO, NULL, 0, NULL, 0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;
}

wf_s32 wf_mcu_update_thermal(nic_info_st *nic_info)
{
    wf_s32 ret = 0;
    if (NIC_USB == nic_info->nic_type)
    {
        ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_UPDATE_THERMAL, NULL, 0, NULL, 0);
    }
    else
    {
        // todo
    }

    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;
}

wf_s32 wf_mcu_get_ap_num(nic_info_st *pnic_info)
{
     nic_info_st *buddy_nic = NULL;
     sys_work_mode_e work_mode;
     wf_s32 ap_num = 0;
     wf_bool bconnect = wf_false;
     
    if(NULL == pnic_info)
    {
        return 0;
    }

    work_mode = wf_local_cfg_get_work_mode(pnic_info);
    if(WF_MASTER_MODE == work_mode)
    {
        wf_mlme_get_connect(pnic_info, &bconnect);
        if(wf_true == bconnect)
        {
            ap_num++;
        }
    }

    buddy_nic = pnic_info->buddy_nic;
    
    if(NULL == buddy_nic)
    {
        return ap_num;
    }
    
    work_mode = wf_local_cfg_get_work_mode(buddy_nic);
    if(WF_MASTER_MODE == work_mode)
    {
        wf_mlme_get_connect(buddy_nic, &bconnect);
        if(wf_true == bconnect)
        {
            ap_num++;
        }
    }

    return ap_num;
}
wf_s32 wf_mcu_get_buddy_mlmestate(nic_info_st *pnic_info)
{
    nic_info_st *buddy_nic = NULL;
    sys_work_mode_e work_mode;

    
    if(NULL == pnic_info)
    {
        return _HW_STATE_NO_EXIST_;
    }

    buddy_nic = pnic_info->buddy_nic;
    if(NULL == buddy_nic)
    {
        return _HW_STATE_NO_EXIST_;
    }

    work_mode = wf_local_cfg_get_work_mode(buddy_nic);
    if (work_mode == WF_ADHOC_MODE)
    {
        return _HW_STATE_ADHOC_;
    }
    else if (work_mode == WF_INFRA_MODE)
    {
        return _HW_STATE_STATION_;
    }
    else if(work_mode == WF_MASTER_MODE)
    {
        return _HW_STATE_AP_;
    }
    
    return _HW_STATE_NO_EXIST_;
}

wf_s32 wf_mcu_get_buddy_fwstate(nic_info_st *pnic_info)
{
    nic_info_st *pvir_nic = pnic_info->buddy_nic;
    if(pnic_info == NULL)
    {
        return WIFI_FW_NO_EXIST;
    }
    if(pvir_nic == NULL)
    {
        return WIFI_FW_NO_EXIST;
    }

    return pvir_nic->nic_state;
}

#ifdef CONFIG_LPS
int wf_mcu_set_lps_opt(nic_info_st *pnic_info, wf_u32 data)
{
    wf_u32 arg[1];
    wf_u32 val;
    int ret = 0;

    if(data == 0)
    {
        arg[0] = data;
        ret = mcu_cmd_communicate(pnic_info, UMSG_OPS_HAL_LPS_OPT, arg, 1, &val, 1);
    }
    else
    {
        arg[0] = data;
        ret = mcu_cmd_communicate(pnic_info, UMSG_OPS_HAL_LPS_OPT, arg, 1, NULL, 0);
    }


    if (WF_RETURN_FAIL == ret )
    {
        LOG_E("[%s] UMSG_OPS_HAL_LPS_OPT failed", __func__);
        return WF_RETURN_FAIL;
    }

    return WF_RETURN_OK;

}
int wf_mcu_set_lps_config(nic_info_st *pnic_info)
{
    wf_u32 arg[2];
    pwr_info_st *pwr_info = pnic_info->pwr_info;
    int ret = 0;

    arg[0] = pwr_info->smart_lps;
    arg[1] = pwr_info->pwr_mgnt;

    ret = mcu_cmd_communicate(pnic_info, UMSG_OPS_HAL_LPS_CONFIG, arg, 2, NULL, 0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return 0;
}

int wf_mcu_set_fw_lps_config(nic_info_st *pnic_info)
{
    wf_u32 aid = 0;
    wf_wlan_mgmt_info_t *wlan_mgmt_info = (wf_wlan_mgmt_info_t *)pnic_info->wlan_mgmt_info;
    wf_wlan_network_t *cur_network = &(wlan_mgmt_info->cur_network);
    int ret = 0;

    aid = (wf_u32)cur_network->aid;

    ret = mcu_cmd_communicate(pnic_info, UMSG_OPS_HAL_LPS_SET, &aid, 1, NULL, 0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return 0;
}

int wf_mcu_set_fw_lps_get(nic_info_st *pnic_info)
{
    wf_s32 ret = WF_RETURN_OK;
    wf_u32 arg[1];

    arg[0] = wf_false;

    ret = mcu_cmd_communicate(pnic_info, UMSG_OPS_HAL_LPS_GET, arg, 1, NULL, 0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] UMSG_OPS_HAL_LPS_GET failed", __func__);
        return WF_RETURN_FAIL;
    }

    return WF_RETURN_OK;
}

#endif

wf_s32 wf_mcu_set_op_mode(nic_info_st *nic_info, wf_u32 mode)
{
    wf_s32 ret = 0;
    wf_u32 tmp[5] = { 0 };
    wf_bool bConnect;
    wf_u32 mlmeState;
    wf_u32 fwState;

    if ((nic_info->is_driver_stopped == wf_true) || (nic_info->is_surprise_removed == wf_true))
    {
        return WF_RETURN_FAIL;
    }

    wf_mlme_get_connect(nic_info, &bConnect);
    if (bConnect == wf_true)
    {
        mlmeState = _HW_STATE_STATION_;
        fwState = WIFI_STATION_STATE | WIFI_ASOC_STATE;
    }
    else
    {
        mlmeState = _HW_STATE_NO_EXIST_;
        fwState =  WIFI_FW_NO_EXIST;
    }

    switch (mode)
    {
        /* STA mode */
        case WF_AUTO_MODE:
        case WF_INFRA_MODE:
            tmp[0] = _HW_STATE_STATION_;
            break;

        /* AdHoc mode */
        case WF_ADHOC_MODE:
            tmp[0] = _HW_STATE_ADHOC_;
            break;

        /* AP mode */
        case WF_MASTER_MODE:
            tmp[0] = _HW_STATE_AP_;
            break;

        /* Sniffer mode */
        case WF_MONITOR_MODE:
            tmp[0] = _HW_STATE_MONITOR_;
            tmp[4] = BIT1|BIT2|BIT3|BIT29|BIT13|BIT14|BIT30|BIT28;


            tmp[4] |= BIT8;

            break;

        case WF_REPEAT_MODE:
        case WF_SECOND_MODES:
        case WF_MESH_MODE:
        default:
        {
            LOG_E("Unsupport Mode!!");
            return WF_RETURN_FAIL;
        }
        break;
    }

    tmp[1] = nic_info->nic_num; //iface: 0 or 1
    tmp[2] = wf_mcu_get_buddy_mlmestate(nic_info); //get mlme state
    tmp[3] = wf_mcu_get_buddy_fwstate(nic_info);
    tmp[4] = WF_BIT(0) | WF_BIT(2) | WF_BIT(31);
    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HW_SET_OP_MODE, tmp, 5, NULL, 0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;

}


wf_s32 wf_mcu_set_ch_bw(nic_info_st *nic_info, wf_u32 *args, wf_u32 arg_len)
{
    wf_s32 ret = 0;

    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_CHNLBW_MODE, args, arg_len, NULL, 0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    /*some kind of register operation*/
    //LOG_I("To do");

    return WF_RETURN_OK;


}


wf_s32  wf_mcu_set_hw_reg(nic_info_st *nic_info, wf_u32 *value, wf_u32 len)
{
    wf_s32  ret = WF_RETURN_OK;
    if (len > MAILBOX_MAX_TXLEN)
    {
        LOG_E("%s len = %d is bigger than MAILBOX_MAX_TXLEN(%d), check!!!! ", __func__,  len, MAILBOX_MAX_TXLEN);
        return WF_RETURN_FAIL;
    }

    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_SET_HWREG, value, len, NULL, 0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return ret;
}


wf_s32 wf_mcu_set_hw_invalid_all(nic_info_st *nic_info)
{
    wf_u32 arg[2] = {0};
    hw_info_st *hw_info = (hw_info_st *)nic_info->hw_info;
    arg[0] = HW_VAR_CAM_INVALID_ALL;
    arg[1] = 0;
    hw_info->hw_reg.cam_invalid = arg[1];

    return wf_mcu_set_hw_reg(nic_info, arg, 2);
}


wf_s32 wf_mcu_set_config_xmit(nic_info_st *nic_info, wf_s32 event, wf_u32 val)
{
    wf_u32 temp;
    wf_s32 ret = 0;
    temp = wf_io_read32(nic_info, WF_XMIT_CTL,NULL);

    if (event & WF_XMIT_AGG_MAXNUMS)
    {
        temp = temp & 0x07FFFFFF;
        val = val & 0x1F;
        temp = temp | (val << 27);
    }

    if (event & WF_XMIT_AMPDU_DENSITY)
    {
        temp = temp & 0xFFFF1FFF;
        val = val & 0x07;
        temp = temp | (val << 13);

    }

    if (event & WF_XMIT_OFFSET)
    {
        temp = temp & 0xFFFFFF00;
        val = val & 0xFF;
        temp = temp | (val << 0);
    }

    if (event & WF_XMIT_PKT_OFFSET)
    {
        temp = temp & 0xFFFFE0FF;
        val = val & 0x1F;
        temp = temp | (val << 8);
    }

    ret  = wf_io_write32(nic_info, WF_XMIT_CTL, temp);

    return WF_RETURN_OK;
}

// wf_s32  wf_mcu_set_no_filter(nic_info_st *nic_info)
// {
//     wf_s32 ret = 0;
//     wf_s32 var;

//     if (NIC_USB == nic_info->nic_type)
//     {
//         var = 1;
//         ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_NOTCH_FILTER, &var, 1, NULL, 0);
//     }
//     else
//     {
//         //todo
//     }

//     if (WF_RETURN_FAIL == ret)
//     {
//         LOG_E("[%s] failed", __func__);
//         return ret;
//     }
//     else if(WF_RETURN_CMD_BUSY == ret)
//     {
//         LOG_W("[%s] cmd busy,try again if need!",__func__);
//         return ret;
//     }

//     return WF_RETURN_OK;
// }

// use for control ODM
wf_s32  wf_mcu_set_user_info(nic_info_st *nic_info, wf_bool state)
{
#ifdef CONFIG_RICHV200

    wf_u32 var;
    wf_s32 ret = 0;
    if(state) {
        var = 0x000000001;
    } else {
        var = 0x00000000;
    }

    if ((wf_mlme_check_mode(nic_info, WF_INFRA_MODE) == wf_true)) {
        var |= 0x000000008;
    }
    else if ((wf_mlme_check_mode(nic_info, WF_MASTER_MODE) == wf_true)) {
        var |= 0x000000010;
    }
    else if (wf_mlme_check_mode(nic_info, WF_ADHOC_MODE) == wf_true)
    {
        var |= 0x000000020;
    } else {
        LOG_E("[%s]:not support work mode", __func__);
        return WF_RETURN_FAIL;
    }

    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_MP_USER_INFO, &var, 1, NULL, 0);

    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

#else
    hw_info_st *phw_info = NULL;

    if (nic_info->hw_info)
    {
        phw_info = nic_info->hw_info;
    }

    if (state == wf_false)
    {
        #if 0
        // ODM disable
        wf_mcu_dig_set(nic_info,wf_true,0x26);
        wf_mcu_msg_body_set_ability(nic_info, ODM_FUNC_BACKUP, 0);
        #else
        phw_info->use_drv_odm = wf_false;
        LOG_D("Diable ODM");
        #endif
    }
    else
    {
        //ODM enable
        #if 0
        wf_mcu_msg_body_set_ability(nic_info, ODM_FUNC_RESTORE, 0);
        wf_mcu_dig_set(nic_info,wf_true,0xff);
        #else
        phw_info->use_drv_odm = wf_true;
        LOG_D("Enable ODM");
        #endif
    }

#endif

    return WF_RETURN_OK;
}

wf_s32 wf_mcu_enable_xmit(nic_info_st *nic_info)
{
    return WF_RETURN_OK;
}


wf_s32 wf_mcu_disable_xmit(nic_info_st *nic_info)
{
    return WF_RETURN_OK;
}


wf_s32 wf_mcu_hw_init(nic_info_st *nic_info, hw_param_st *param)
{
    wf_s32 ret = 0;
  
    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_FW_INIT, (wf_u32*)param, 9, NULL, 0);

    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;
}


#ifdef CONFIG_RICHV100
wf_s32 wf_mcu_init_hardware2(nic_info_st *nic_info, hw_param_st *param)
{
    wf_s32 ret = 0;
    hw_info_st *hwinfo = (hw_info_st *)nic_info->hw_info;
    wf_s32 i = 0;

    LOG_D("arg[0]:0x%x\n", param->send_msg[0]);
    for (i = 0; i < WF_ETH_ALEN; i++)
    {
        param->send_msg[i + 1] = hwinfo->macAddr[i];
        LOG_D("mac[%d]= 0x%x\n", i + 1, param->send_msg[i + 1]);
    }

    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_FW_INIT, param->send_msg, 7, param->recv_msg, 9);

    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;
}

#endif
wf_s32 wf_mcu_set_mlme_scan(nic_info_st *nic_info, wf_bool enable)
{
    wf_s32 ret = 0;
    wf_u32 arg[7] = {0};
    wf_bool bConnect;
    wf_u8 linkState;
    wf_u32 fwState;
    sys_work_mode_e work_mode = get_sys_work_mode(nic_info);

    if (work_mode == WF_ADHOC_MODE)
    {
        fwState = WIFI_ADHOC_STATE | WIFI_ADHOC_MASTER_STATE;
    }
    else if (work_mode == WF_INFRA_MODE)
    {
        fwState = WIFI_STATION_STATE;
    }
    else if (work_mode == WF_MONITOR_MODE)
    {
        fwState = WIFI_SITE_MONITOR;
    }
    else
    {
        LOG_E("unknow fw state!!!");
        return -1;
    }

    wf_mlme_get_connect(nic_info, &bConnect);
    if (bConnect == wf_true)
    {
        nic_info->nic_state = WIFI_ASOC_STATE | fwState;
        linkState = MCU_LINKED;
    }
    else
    {
        nic_info->nic_state = fwState;
        linkState = MCU_UNLINKED;
    }

    arg[0] = enable;
    arg[1] = nic_info->nic_num;
    arg[2] = wf_mcu_get_ap_num(nic_info);
    arg[3] = nic_info->nic_state;
    arg[4] = linkState;
    arg[5] = wf_mcu_get_buddy_fwstate(nic_info);
    arg[6] = 0;
    
    if(WIFI_FW_NO_EXIST != arg[5])
    {
        wf_bool buddy_connect = wf_false;
        wf_mlme_get_connect(nic_info->buddy_nic, &buddy_connect);
        if(wf_true == buddy_connect)
        {
            arg[6] = 1;
        }
    }
    

    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HW_SET_MLME_SITE, arg, 7, NULL, 0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    if (enable == wf_true)
    {
        //LOG_I("disable bssid filter");
    }
    else
    {
        //LOG_I("enable bssid filter");
    }

    return 0;
}




wf_s32 wf_mcu_set_mlme_join(nic_info_st *nic_info, wf_u8 type)
{
    wf_u32 param[5] = { 0 };
    wf_s32 ret = 0;
    wf_u32 mlmeState;
    wf_u32 fwState;
    sys_work_mode_e work_mode = wf_local_cfg_get_work_mode(nic_info);


    if (work_mode == WF_ADHOC_MODE)
    {
        mlmeState = _HW_STATE_ADHOC_;
        fwState = WIFI_ADHOC_STATE | WIFI_ADHOC_MASTER_STATE;
    }
    else if (work_mode == WF_INFRA_MODE)
    {
        mlmeState = _HW_STATE_STATION_;
        fwState = WIFI_STATION_STATE;
    }
    else
    {
        LOG_E("unknow fw state!!!");
        return -1;
    }

    param[0] = type;
    param[1] = nic_info->nic_num;  //iface0 or iface1
    param[2] = fwState;
    param[3] = wf_mcu_get_buddy_mlmestate(nic_info);
    param[4] = wf_mcu_get_buddy_fwstate(nic_info);

    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HW_SET_MLME_JOIN, param, 5, NULL, 0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return 0;
}

wf_s32 wf_mcu_hw_var_set_macaddr(nic_info_st *nic_info, wf_u8 * val)
{
    wf_u8 idx = 0;
    wf_s32 ret = 0;
    wf_u32 var[7] = { 0 };

    var[0] = nic_info->nic_num;
    for (idx = 0; idx < 6; idx++)
        var[idx + 1] = val[idx];

    ret =
        mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_SET_MAC, var, 7,NULL,0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }
    return 0;
}



wf_s32 wf_mcu_set_bssid(nic_info_st *nic_info, wf_u8 *bssid)
{
    wf_u8 idx = 0;
    wf_u32 var[7] = { 0 };
    wf_s32 ret = 0;

    if(nic_info->nic_num == 1)
    {
        var[0] = REG_BSSID1;
    }
    else
    {
        var[0] = REG_BSSID0;  //iface0 or iface1
    }

    if (bssid != NULL)
    {
        for (idx = 0; idx < 6; idx++)
            var[idx + 1] = bssid[idx];
    }
    else
    {
        for (idx = 0; idx < 6; idx++)
            var[idx + 1] = 0;
    }

    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_SET_BSSID, var, 7, NULL, 0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return 0;
}



wf_s32 wf_mcu_set_sifs(nic_info_st *nic_info, wf_u32 value)
{
    wf_s32 i = 0;
    wf_u32 arg[5] = {0};

    arg[0] = HW_VAR_RESP_SIFS;

    for(i=0; i<4; i++)
    {
        arg[i+1] = ((wf_u8*)&value)[i];
    }

    return wf_mcu_set_hw_reg(nic_info, arg, 5);
}

wf_s32 wf_mcu_set_macid_wakeup(nic_info_st *nic_info, wf_u32 wdn_id)
{
    wf_u32 arg[2] = {0};

    arg[0] = HW_VAR_MACID_WAKEUP;
    arg[1] = wdn_id;

    return wf_mcu_set_hw_reg(nic_info, arg, 2);
}

wf_s32 wf_mcu_set_basic_rate (nic_info_st *nic_info, wf_u16 br_cfg)
{
    wf_s32 ret = 0;
    wf_u32 BrateCfg;
    wf_u16 rrsr_2g_force_mask = WF_RATE_1M | WF_RATE_2M | WF_RATE_5_5M | WF_RATE_11M;
    wf_u16 rrsr_2g_allow_mask = WF_RATE_24M | WF_RATE_12M | WF_RATE_6M | rrsr_2g_force_mask;

    BrateCfg = rrsr_2g_force_mask | br_cfg;
    BrateCfg &= rrsr_2g_allow_mask;

    LOG_D("[%s] br_cfg=0x%x", __func__,br_cfg);

    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HW_SET_BASIC_RATE, &BrateCfg, 1, NULL, 0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;
}


wf_s32 wf_mcu_set_preamble(nic_info_st *nic_info, wf_u8 short_val)
{
    wf_u32 arg[2] = {0};

    arg[0] = HW_VAR_ACK_PREAMBLE;
    arg[1] = short_val;

    return wf_mcu_set_hw_reg(nic_info, arg, 2);
}

wf_s32 wf_mcu_set_wmm_para_disable(nic_info_st *nic_info, wdn_net_info_st *wdn_info)
{
    wf_u8 AIFS, ECWMin, ECWMax, aSifsTime;
    wf_u32 acParm;
    wf_u16 TXOP;
    wf_u32 arg[2] = {0};

    if(wdn_info->network_type & WIRELESS_11_24N)
    {
        aSifsTime = 16;
    }
    else
    {
        aSifsTime = 10;
    }
    AIFS = aSifsTime + (2 * SHORT_SLOT_TIME);
    ECWMax = 10;
    ECWMin = 4;
    TXOP = 0;
    acParm = AIFS | (ECWMin << 8) | (ECWMax << 12) | (TXOP << 16);

    arg[0] = HW_VAR_AC_PARAM_BE;
    arg[1] = acParm;
    if (wf_mcu_set_hw_reg(nic_info, arg, 2) != 0)
    {
        return -1;
    }

    arg[0] = HW_VAR_AC_PARAM_BK;
    if (wf_mcu_set_hw_reg(nic_info, arg, 2) != 0)
    {
        return -1;
    }

    arg[0] = HW_VAR_AC_PARAM_VI;
    if (wf_mcu_set_hw_reg(nic_info, arg, 2) != 0)
    {
        return -1;
    }

    TXOP = 0x2f;
    ECWMax = 3;
    ECWMin = 2;
    acParm = AIFS | (ECWMin << 8) | (ECWMax << 12) | (TXOP << 16);
    arg[0] = HW_VAR_AC_PARAM_VO;
    arg[1] = acParm;
    if (wf_mcu_set_hw_reg(nic_info, arg, 2) != 0)
    {
        return -1;
    }
    return 0;

}

wf_s32 wf_mcu_set_wmm_para_enable(nic_info_st *nic_info, wdn_net_info_st *wdn_info)
{
    wf_u8 ACI, ACM, AIFS, ECWMin, ECWMax, aSifsTime;
    wf_u16 TXOP;
    wf_u32 acParm;
    wf_s32 i = 0;
    wf_wmm_para_st *wmm_info = &wdn_info->wmm_info;
    wf_u32 arg[2] = {0};

    wdn_info->acm_mask = 0;
    if(wdn_info->network_type & WIRELESS_11_24N)
    {
        aSifsTime = 16;
    }
    else
    {
        aSifsTime = 10;
    }
    for(i = 0 ; i < 4; i++)
    {
        ACI = (wmm_info->ac[i].ACI >> 5) & 0x03;
        ACM = (wmm_info->ac[i].ACI >> 4) & 0x01;
        AIFS = (wmm_info->ac[i].ACI & 0x0f)* SHORT_SLOT_TIME + aSifsTime;

#if 0
        LOG_D("ACI:0x%x", ACI);
        LOG_D("ACM:0x%x", ACM);
        LOG_D("AIFS:0x%x", AIFS);
#endif

        ECWMin = (wmm_info->ac[i].ECW & 0x0f);
        ECWMax = (wmm_info->ac[i].ECW & 0xf0)>>4;
        TXOP = le16_to_cpu(wmm_info->ac[i].TXOP_limit);

        aSifsTime = 16;
        acParm = AIFS | (ECWMin << 8) | (ECWMax << 12) | (TXOP << 16);
        switch (ACI)
        {
            case 0x00:
                arg[0] = HW_VAR_AC_PARAM_BE;
                wdn_info->acm_mask |= (ACM ? BIT(1) : 0);
                break;
            case 0x01:
                arg[0] = HW_VAR_AC_PARAM_BK;
                break;
            case 0x02:
                arg[0] = HW_VAR_AC_PARAM_VI;
                wdn_info->acm_mask |= (ACM ? BIT(2) : 0);
                break;
            case 0x03:
                arg[0] = HW_VAR_AC_PARAM_VO;
                wdn_info->acm_mask |= (ACM ? BIT(3) : 0);
                break;
        }

        arg[1] = acParm;

        wf_mcu_set_hw_reg(nic_info,arg,2);

        LOG_D("acParm:0x%x   acm_mask:0x%x", acParm, wdn_info->acm_mask);
    }

    return 0;
}


wf_s32 wf_mcu_set_bcn_intv (nic_info_st *nic_info, wf_u16 val)
{
    wf_u32 arg[2] = {0};

    arg[0] = HW_VAR_BEACON_INTERVAL;
    arg[1] = val;
    return wf_mcu_set_hw_reg(nic_info, arg, 2);
}


wf_s32 wf_mcu_set_slot_time(nic_info_st *nic_info, wf_u32 slotTime)
{
    wf_u32 arg[2] = {0};

    arg[0] = HW_VAR_SLOT_TIME;
    arg[1] = slotTime;

    return wf_mcu_set_hw_reg(nic_info, arg, 2);
}


wf_s32 wf_mcu_set_correct_tsf(nic_info_st *nic_info, wf_u64 tsf)
{
    wf_u32 arg[6] = {0};
    wf_s32 ret = WF_RETURN_OK;

    arg[0] = HW_VAR_CORRECT_TSF;
    arg[1] = (wf_u32)tsf;
    arg[2] = tsf >> 32;
    if(nic_info->nic_num == 1)
    {
        arg[3] = 1;
    }
    else
    {
        arg[3] = 0;
    }
    arg[4] = nic_info->nic_state;
    arg[5] = wf_mcu_get_buddy_fwstate(nic_info);

    LOG_D("[wf_mcu_set_correct_tsf]TSF:%x   %x", arg[1], arg[0]);

    if (NIC_USB == nic_info->nic_type)
    {
        ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HW_SET_CORRECT_TSF, arg, 6, NULL, 0);
    }
    else
    {
//        ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_SET_HWREG, value, len, NULL, 0);
    }

    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }
    return 0;
}


wf_s32 wf_mcu_set_media_status(nic_info_st *nic_info, wf_u32 status)
{
    wf_u32 arg[2] = {0};

    if(nic_info->nic_num == 1)
    {
        arg[0] = HW_VAR_MEDIA_STATUS1;
    }
    else
    {
        arg[0] = HW_VAR_MEDIA_STATUS;
    }

    arg[1] = status;

    return wf_mcu_set_hw_reg(nic_info, arg, 2);
}

wf_s32 wf_mcu_ars_init(nic_info_st *nic_info)
{
    wf_s32 ret = 0;
    hw_info_st *hw_info = nic_info->hw_info;
    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_ARS_INIT, 
            (wf_u32 *)&hw_info->Regulation2_4G, 1, NULL, 0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return 0;
}

wf_s32 wf_mcu_set_phy_config(nic_info_st *nic_info, phy_config_t *cfg)
{
    wf_s32 ret = 0;

    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_CONFIG_MSG, (wf_u32 *)cfg, sizeof(phy_config_t) / 4, NULL, 0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return 0;
}

wf_s32 wf_mcu_mp_bb_rf_gain_offset(nic_info_st *nic_info)
{
    wf_s32 ret = 0;

    if (NIC_USB == nic_info->nic_type)
    {
        //do nothing
    }
    else
    {
        ret = mcu_cmd_communicate(nic_info, UMSG_OPS_MP_BB_RF_GAIN_OFFSET, NULL, 0, NULL, 0);
    }

    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;
}

wf_s32 wf_mcu_mbox1_cmd(nic_info_st *nic_info, wf_u8 *cmd, wf_u32 cmd_len, wf_u8 ElementID )
{
    wf_s32 ret     = 0;
    wf_u32 *tmp     = NULL;
    wf_u32 slen  = 0;
    wf_u32 status = 0;
    wf_u32 i = 0;

    if (NIC_USB == nic_info->nic_type)
    {
        //do nothing
    }
    else
    {
        slen = (cmd_len + 2) * 4;
        tmp = wf_kzalloc(slen);
        if (NULL == tmp)
        {
            LOG_E("[%s] wf_kzalloc failed,check!!!", __func__);
            return WF_RETURN_FAIL;
        }
        tmp[0] = ElementID;
        tmp[1] = cmd_len;
        LOG_I("arg[0]:0x%x, arg[1]:0x%x", tmp[0], tmp[1]);
        for (i = 0; i < cmd_len; i++)
        {
            tmp[i + 2] = cmd[i];
            LOG_I("arg[%d]:0x%x", i + 2, tmp[i + 2]);
        }
        ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_H2C_CMD, tmp, cmd_len + 2, &status, 1);
        wf_kfree(tmp);
    }

    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;
}

wf_s32 wf_mcu_cca_config(nic_info_st *nic_info)
{
    wf_s32 ret = 0;

    if (NIC_USB == nic_info->nic_type)
    {
        //do nothing
    }
    else
    {
        ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_CCA_CONFIG, NULL, 0, NULL, 0);
    }

    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;
}


wf_s32 wf_mcu_watchdog(nic_info_st *nic_info)
{
    wf_s32 ret = 0;

    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_MSG_WDG, NULL, 0, NULL, 0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;

}

wf_s32 wf_mcu_set_on_rcr_am (nic_info_st *nic_info, wf_bool on_off)
{
    wf_u32 buf[2];

    buf[0] = (wf_u32)on_off ? HW_VAR_ON_RCR_AM : HW_VAR_OFF_RCR_AM;
    buf[1] = 0;
    wf_mcu_set_hw_reg(nic_info, buf, 2);

    return WF_RETURN_OK;
}

wf_s32 wf_mcu_set_dk_cfg (nic_info_st *nic_info, wf_u32 auth_algrthm, wf_bool dk_en)
{
    wf_u32 buf[2];
    wf_s32 ret = 0;
    buf[0] = dk_en;
    buf[1] = auth_algrthm;
    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HW_SET_DK_CFG, buf, 2, NULL, 0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;
}

wf_s32 wf_mcu_set_sec_cfg (nic_info_st *nic_info, wf_u8 val)
{
    wf_u32 buf[5];

    buf[0] = HW_VAR_SEC_CFG;
    buf[1] = wf_false;
    buf[2] = wf_false;
    buf[3] = val;
    buf[4] = wf_true;
    wf_mcu_set_hw_reg(nic_info, buf, 5);

    return WF_RETURN_OK;
}

wf_s32 wf_mcu_set_sec_cam (nic_info_st *nic_info,
                        wf_u8 cam_id, wf_u16 ctrl, wf_u8 *mac, wf_u8 *key)
{
    wf_s32 ret = 0;
    wf_s32 i   = 0;
    wf_u32 buff[WF_SECURITY_CAM_SIZE] = {0};

    buff[0] = cam_id;
    buff[1] = ctrl;

    for (i = 0; i < WF_ETH_ALEN; i++)
    {
        buff[i + 2] = mac[i];
    }

    for (i = 0; i < WF_SECURITY_KEY_SIZE; i++)
    {
        buff[i + 2 + WF_ETH_ALEN] = key[i];
    }

    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_SEC_WRITE_CAM,
                              buff, WF_SECURITY_CAM_SIZE, NULL, 0);

    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;
}





/**/
wf_s32 wf_mcu_fill_mbox1_fw(nic_info_st *nic_info, wf_u8 element_id, wf_u8 *cmd, wf_u32 cmd_len)
{
    wf_s32  ret = 0;
    wf_u32 *buf = NULL;
    wf_s32 i    = 0;

    buf = (wf_u32 *) wf_kzalloc((cmd_len + 2) * 4);
    if (!buf)
    {
        LOG_E("[%s] failed, check", __func__);
        return WF_RETURN_FAIL;
    }

    buf[0] = element_id;
    buf[1] = cmd_len;
    //LOG_I("[%s] element_id:0x%x, cmd_len:%d", __func__, element_id, cmd_len);
    for (i = 0; i < cmd_len; i++)
    {
        buf[i + 2] = cmd[i];
        //LOG_D("[%s] 0x%x", __func__, buf[i + 2]);
    }

    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_H2C_CMD, buf, cmd_len + 2, NULL, 0);
    wf_kfree(buf);

    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    buf = NULL;
    return WF_RETURN_OK;
}

wf_s32 wf_mcu_set_max_ampdu_len(nic_info_st *pnic_info, wf_u8 max_ampdu_len)
{
    wf_u32 arg[2] = {0};

    arg[0] = HW_VAR_AMPDU_FACTOR;
    arg[1] = max_ampdu_len;

    return wf_mcu_set_hw_reg(pnic_info, arg, 2);
}

wf_s32 wf_mcu_set_min_ampdu_space(nic_info_st *pnic_info, wf_u8 min_space)
{
    wf_u32 arg[2] = {0};

    arg[0] = HW_VAR_AMPDU_MIN_SPACE;
    arg[1] = min_space;

    return wf_mcu_set_hw_reg(pnic_info, arg, 2);
}


wf_s32 wf_mcu_set_agg_param(nic_info_st *nic_info, wf_u32 agg_size, wf_u32 agg_timeout, wf_u32 agg_dma_enable)
{
    wf_s32 ret = WF_RETURN_FAIL;
    wf_u32 mbox[3] = { 0 };

    if(NIC_USB != nic_info->nic_type)
    {
        return ret;
    }

    mbox[0] = agg_size;
    mbox[1] = agg_timeout;
    mbox[2] = agg_dma_enable;
    ret =mcu_cmd_communicate(nic_info,UMSG_OPS_HAL_SET_USB_AGG_CUSTOMER,mbox, 3, NULL, 0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }
    
    return WF_RETURN_OK;
}

wf_s32 wf_mcu_set_usb_agg_normal(nic_info_st *nic_info, wf_u8 cur_wireless_mode)
{
    wf_s32 ret = WF_RETURN_FAIL;
    wf_u32 mbox1[1] = { 0 };

    if(NIC_USB != nic_info->nic_type)
    {
        return ret;
    }
#if 0
    if (cur_wireless_mode < WIRELESS_11_24N && cur_wireless_mode > 0)
    {
        if (0x6 != pHalData->RegAcUsbDmaSize || 0x10 != pHalData->RegAcUsbDmaTime)
        {
            pHalData->RegAcUsbDmaSize = 0x6;
            pHalData->RegAcUsbDmaTime = 0x10;

            mbox1[0] = cur_wireless_mode;
            ret =mcu_cmd_communicate(nic_info,UMSG_OPS_HAL_SET_USB_AGG_NORMAL,mbox1, 1,NULL, 0);
            if (!ret)
            {
                LOG_E("[%s] failed \n", __func__);
                return;
            }
        }

    }
    else if (cur_wireless_mode >= WIRELESS_11_24N && cur_wireless_mode <= WIRELESS_MODE_MAX)
    {
        if (0x5 != pHalData->RegAcUsbDmaSize || 0x20 != pHalData->RegAcUsbDmaTime)
        {
            pHalData->RegAcUsbDmaSize = 0x5;
            pHalData->RegAcUsbDmaTime = 0x20;

            mbox2[0] = cur_wireless_mode;

            ret =mcu_cmd_communicate(nic_info,UMSG_OPS_HAL_SET_USB_AGG_NORMAL,mbox2,1, NULL, 0);
            if (!ret)
            {
                LOG_E("[%s] failed", __func__);
                return ret;
            }

        }

    }
    else
    {
    }

    return ret;
#else
    mbox1[0] = cur_wireless_mode;
    ret =mcu_cmd_communicate(nic_info,UMSG_OPS_HAL_SET_USB_AGG_NORMAL,mbox1, 1,NULL, 0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }
    return WF_RETURN_OK;
#endif
}

wf_s32 wf_mcu_check_tx_buff(nic_info_st *nic_info)
{
    wf_u32 arg[1] = {0};

    arg[0] = WLAN_HAL_VALUE_CHECK_TXBUF;
    return wf_mcu_set_hw_reg(nic_info, arg, 1);
}

wf_s32 wf_mcu_check_rx_fifo(nic_info_st *nic_info)
{
    wf_u32 arg[2] = { 0 };
    wf_s32 ret = 0;
    if(NIC_SDIO == nic_info->nic_type)
    {
        ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_CHECK_RXFIFO_FULL, NULL,0,arg, 2);
    }
    else
    {
        return 0;
    }

    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return 0;
}

wf_s32 wf_mcu_reset_chip(nic_info_st *nic_info)
{
    wf_s32 ret = 0;
    if(NIC_USB == nic_info->nic_type)
    {
        ret =  mcu_cmd_communicate(nic_info, UMSG_OPS_RESET_CHIP, NULL,0,NULL, 0);
    }
    else
    {
        return 0;
    }

    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return 0;
}

wf_s32 wf_mcu_set_ac_vo(nic_info_st *pnic_info)
{
    wf_u32 acparm;
    wf_u32 arg[2] = {0};


    if (pnic_info->buddy_nic)
    {
        acparm = 0x005ea42b;
    }
    else
    {
        acparm = 0x002F3217;
    }

    arg[0] = HW_VAR_AC_PARAM_VO;
    arg[1] = acparm;

    return wf_mcu_set_hw_reg(pnic_info, arg, 2);
}

wf_s32 wf_mcu_set_ac_vi(nic_info_st *pnic_info)
{
    wf_u32 acparm;
    wf_u32 arg[2] = {0};

    acparm = 0x005E4317;
    arg[0] = HW_VAR_AC_PARAM_VI;
    arg[1] = acparm;

    return wf_mcu_set_hw_reg(pnic_info, arg, 2);
}

wf_s32 wf_mcu_set_ac_be(nic_info_st *pnic_info)
{
    wf_u32 acparm;
    wf_u32 arg[2] = {0};

    acparm = 0x005ea42b;
    arg[0] = HW_VAR_AC_PARAM_BE;
    arg[1] = acparm;

    return wf_mcu_set_hw_reg(pnic_info, arg, 2);
}

wf_s32 wf_mcu_set_ac_bk(nic_info_st *pnic_info)
{
    wf_u32 acparm;
    wf_u32 arg[2] = {0};

    acparm = 0x0000A444;
    arg[0] = HW_VAR_AC_PARAM_BK;
    arg[1] = acparm;

    return wf_mcu_set_hw_reg(pnic_info, arg, 2);
}

wf_s32 wf_mcu_set_bcn_valid(nic_info_st *pnic_info)
{
    wf_u32 arg[2] = {0};

    if(pnic_info->nic_num == 1)
    {
        arg[0] = HW_VAR_BCN_VALID1;
    }
    else
    {
        arg[0] = HW_VAR_BCN_VALID;
    }

    arg[1] = 0;

    return  wf_mcu_set_hw_reg(pnic_info,arg,2);
}

wf_s32 wf_mcu_get_bcn_valid(nic_info_st *pnic_info,wf_u32 *val32)
{
    wf_s32 ret = 0;

    wf_u32 arg[1] = {0};

    if(pnic_info->nic_num == 1)
    {
        arg[0] = HW_VAR_BCN_VALID1;
    }
    else
    {
        arg[0] = HW_VAR_BCN_VALID;
    }

    ret = mcu_cmd_communicate(pnic_info, UMSG_OPS_HAL_GET_HWREG, arg, 1, val32, 1);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return 0;
}


wf_s32 wf_mcu_set_bcn_sel(nic_info_st *pnic_info)
{
    wf_u32 arg[2] = {0};

    arg[0] = HW_VAR_DL_BCN_SEL;
    arg[1] = 0;

    return wf_mcu_set_hw_reg(pnic_info,arg,2);
}

#ifdef CFG_ENABLE_AP_MODE

wf_s32 wf_mcu_set_ap_mode(nic_info_st *pnic_info)
{
    wf_u32 arg[2] = {0};

    LOG_D("[set AP role] %s", __func__);

    if(pnic_info->nic_num == 1)
    {
        arg[0] = HW_VAR_MEDIA_STATUS1;
    }
    else
    {
        arg[0] = HW_VAR_MEDIA_STATUS;
    }

    arg[1] = WIFI_FW_AP_STATE;

    return wf_mcu_set_hw_reg(pnic_info, arg, 2);
}

wf_s32 wf_mcu_set_sec(nic_info_st *pnic_info)
{
    sec_info_st *psec_info = pnic_info->sec_info;
    wf_u8 val8;
    wf_u32 arg[2] = {0};
    val8 = (psec_info->dot11AuthAlgrthm == dot11AuthAlgrthm_8021X) ? 0xcc : 0xcf;

    arg[0] = HW_VAR_SEC_CFG;
    arg[1] = val8;

    return wf_mcu_set_hw_reg(pnic_info, arg, 2);
}

wf_s32 wf_mcu_disable_rx_agg(nic_info_st * nic_info)
{
    wf_u32 data;
    LOG_I("0x10c:0x%02x",wf_io_read32(nic_info,0x10c,NULL));
    data = wf_io_read32(nic_info,0x10c,NULL);
    data = data & (~BIT(2));
    wf_io_write32(nic_info,0x10c,data);
    LOG_I("0x10c:0x%02x",wf_io_read32(nic_info,0x10c,NULL));
    return 0;

}

#endif

#ifdef CFG_ENABLE_ADHOC_MODE
wf_s32 wf_mcu_set_bcn_reg(nic_info_st *pnic_info)
{
    wf_u32 par[6] = {0};
    wf_s32 ret = 0;

    par[0] = 0x40;
    par[1] = 0x64;
    par[2] = wf_true;
    par[3] = wf_false;
    par[4] = wf_false;
    par[5] = 0x0550;

    ret = mcu_cmd_communicate(pnic_info, UMSG_OPS_HAL_SET_BCN_REG, par, 6, NULL, 0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return 0;
}

#endif

wf_s32 wf_close_fw_dbginfo(nic_info_st *pnic_info)
{
    wf_s32 ret = 0;
    wf_u32 inbuff[2] = {0xffffffff,0xffffffff};

    ret = mcu_cmd_communicate(pnic_info, UMSG_OPS_HAL_DBGLOG_CONFIG, inbuff, 2, NULL, 0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return 0;

}

wf_s32 wf_mcu_handle_rf_lck_calibrate(nic_info_st *nic_info)
{
    wf_s32 ret          = 0;
    wf_u32 outbuf;

    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_CALI_LLC, NULL, 0, &outbuf, 1);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    #ifdef CONFIG_RICHV200
    if(outbuf == 0)
    {
        LOG_E("LOCK FAIL");
    }
    else if(outbuf ==1)
    {
        LOG_D("LOCK success");
    }
    #endif

    return WF_RETURN_OK;
}

wf_s32  wf_mcu_handle_rf_iq_calibrate(nic_info_st *nic_info, wf_u8 channel)
{
    wf_s32 ret = 0;
    wf_u32 buff[2] = { 0 };
    wf_s32 len = 2;

    buff[0] = 0;
    buff[1] = channel;

    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_PHY_IQ_CALIBRATE, buff, len, NULL,  0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;
}

wf_s32 wf_mcu_msg_init_default(nic_info_st * nic_info)
{
    wf_s32 ret = 0;

    ret = mcu_cmd_communicate(nic_info,UMSG_OPS_HAL_MSG_INIT_DEFAULT_VALUE,NULL,0,NULL,0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;
}

wf_s32 wf_mcu_msg_init(nic_info_st * nic_info)
{
    wf_s32 ret = 0;

    ret =   mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_INIT_MSG, NULL, 0, NULL, 0);

    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;
}

wf_s32 wf_mcu_msg_body_init(nic_info_st *nic_info, mcu_msg_body_st *msg)
{
    wf_s32 ret = 0;

    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_INIT_MSG_VAR, (wf_u32 *) msg, sizeof(mcu_msg_body_st) / 4, NULL, 0);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return 0;

}

wf_s32 wf_mcu_msg_body_get(nic_info_st *nic_info, mcu_msg_body_st *mcu_msg)
{
    int ret = 0;
    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_READVAR_MSG, NULL, 0,(wf_u32 *)mcu_msg, sizeof(mcu_msg_body_st) / 4);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    //wf_display_odm_msg(&odm->odm_msg);

    return WF_RETURN_OK;
}

wf_s32 wf_mcu_msg_body_set(nic_info_st *nic_info,mcu_msg_body_st *mcu_msg)
{
    wf_s32 ret = 0;
    //wf_display_odm_msg(odm->odm_msg);

    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_WRITEVAR_MSG, (wf_u32 *) mcu_msg,sizeof(mcu_msg_body_st) / 4,NULL,0 );
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;
}

wf_s32 wf_mcu_msg_body_sync(nic_info_st *nic_info,MSG_BODY_VARIABLE ops,  wf_u32 val)
{
    mcu_msg_body_st mcu_msg;

    wf_mcu_msg_body_get(nic_info,&mcu_msg);
    switch (ops)
    {
        case HAL_MSG_STA_INFO:
        {
            break;
        }
        case HAL_MSG_P2P_STATE:
        {
            mcu_msg.wifi_direct = val;
            break;
        }
        case HAL_MSG_WIFI_DISPLAY_STATE:
        {
            mcu_msg.wifi_display = val;
            break;
        }
        default:
        {
            break;
        }
    }

    wf_mcu_msg_body_set(nic_info,&mcu_msg);

    return WF_RETURN_OK;
}
wf_s32 wf_mcu_msg_body_set_ability(nic_info_st *nic_info,MCU_MSG_BODY_ABILITY_OPS ops,  wf_u32 ability)
{
    mcu_msg_body_st mcu_msg;

    wf_mcu_msg_body_get(nic_info,&mcu_msg);

    //lock

    switch (ops)
    {
        case ODM_DIS_ALL_FUNC:
            mcu_msg.ability = ability;
            break;
        case ODM_FUNC_SET:
            mcu_msg.ability |= ability;
            break;
        case ODM_FUNC_CLR:
            mcu_msg.ability &= ~(ability);
            break;
        case ODM_FUNC_BACKUP:
            //LOG_I("ability backup %x\r\n", odm->backup_ability);
            //odm->backup_ability = mcu_msg.ability;
            break;
        case ODM_FUNC_RESTORE:
            //LOG_I("ability restory %X\r\n", odm->backup_ability);
            //mcu_msg.ability = odm->backup_ability;
            break;
    }

    //unlock

    wf_mcu_msg_body_set(nic_info,&mcu_msg);

    return WF_RETURN_OK;
}


wf_s32 wf_mcu_dig_set(nic_info_st *nic_info, wf_bool init_gain, wf_u32 rx_gain)
{
    wf_s32 ret;
    wf_u32 buf[3];

    if (init_gain == wf_true)
    {
        buf[1] = 0;
        buf[2] = rx_gain;

        if (rx_gain == 0xff)
        {
            buf[0] = WF_BIT(1);
        }
        else
        {
            buf[0] = WF_BIT(0);
        }

        //LOG_I("[%s] %d,%d,%d",__func__,buf[0],buf[1],buf[2]);
        ret = mcu_cmd_communicate(nic_info, UMSG_OPS_MSG_PAUSEIG, buf, 3, NULL, 0);

        if (WF_RETURN_FAIL == ret)
        {
            LOG_E("[%s] failed", __func__);
            return ret;
        }
        else if(WF_RETURN_CMD_BUSY == ret)
        {
            LOG_W("[%s] cmd busy,try again if need!",__func__);
            return ret;
        }
    }
    else
    {
        //msg_var_req_t msg_var_req;

        //msg_var_req.var = eVariable;
        //msg_var_req.msg = (SIZE_T) pValue1;
        //msg_var_req.set = (SIZE_T) bSet;
        //Func_Set_Msg_Var_Req(Adapter, &msg_var_req);
    }

    return WF_RETURN_OK;
}

wf_s32 wf_mcu_rfconfig_set(nic_info_st *nic_info, wf_u8 mac_id, wf_u8 raid, wf_u8 bw, wf_u8 sgi, wf_u32 mask)
{
    wf_u8 u1wMBOX1MacIdConfigParm[wMBOX1_MACID_CFG_LEN] = { 0 };
    mcu_msg_body_st mcu_msg;
    int ret = 0;

    // LOG_I("[%s] mac_id:%d, raid:%d, bw:%d, sgi:%d,mask:0x%x",__func__, mac_id, raid, bw, sgi,mask);
    SET_9086X_wMBOX1CMD_MACID_CFG_MACID(u1wMBOX1MacIdConfigParm, mac_id);
    SET_9086X_wMBOX1CMD_MACID_CFG_RAID(u1wMBOX1MacIdConfigParm, raid);
    SET_9086X_wMBOX1CMD_MACID_CFG_SGI_EN(u1wMBOX1MacIdConfigParm, (sgi) ? 1 : 0);
    SET_9086X_wMBOX1CMD_MACID_CFG_BW(u1wMBOX1MacIdConfigParm, bw);

    ret = wf_mcu_msg_body_get(nic_info,&mcu_msg);
    if (mcu_msg.bDisablePowerTraining)
    {
        SET_9086X_wMBOX1CMD_MACID_CFG_DISPT(u1wMBOX1MacIdConfigParm, 1);
    }

    SET_9086X_wMBOX1CMD_MACID_CFG_RATE_MASK0(u1wMBOX1MacIdConfigParm,
                                          (wf_u8) (mask & 0x000000ff));
    SET_9086X_wMBOX1CMD_MACID_CFG_RATE_MASK1(u1wMBOX1MacIdConfigParm,
                                          (wf_u8) ((mask & 0x0000ff00) >> 8));
    SET_9086X_wMBOX1CMD_MACID_CFG_RATE_MASK2(u1wMBOX1MacIdConfigParm,
                                          (wf_u8) ((mask & 0x00ff0000) >> 16));
    SET_9086X_wMBOX1CMD_MACID_CFG_RATE_MASK3(u1wMBOX1MacIdConfigParm,
                                          (wf_u8) ((mask & 0xff000000) >> 24));

    ret = wf_mcu_fill_mbox1_fw(nic_info, wMBOX1_9086X_MACID_CFG, u1wMBOX1MacIdConfigParm, wMBOX1_MACID_CFG_LEN);

    return ret;
}

static wf_s32 wf_mcu_media_status_set(nic_info_st *nic_info, wf_bool opmode,
                                     wf_bool miracast, wf_bool miracast_sink, wf_u8 role,
                                     wf_u8 macid,wf_bool macid_ind,wf_u8 macid_end)
{
    wf_u8 parm[wMBOX1_MEDIA_STATUS_RPT_LEN] = { 0 };
    int ret = 0;

    SET_wMBOX1CMD_MSRRPT_PARM_OPMODE(parm, opmode);
    SET_wMBOX1CMD_MSRRPT_PARM_MACID_IND(parm, macid_ind);
    SET_wMBOX1CMD_MSRRPT_PARM_MIRACAST(parm, miracast);
    SET_wMBOX1CMD_MSRRPT_PARM_MIRACAST_SINK(parm, miracast_sink);
    SET_wMBOX1CMD_MSRRPT_PARM_ROLE(parm, role);
    SET_wMBOX1CMD_MSRRPT_PARM_MACID(parm, macid);
    SET_wMBOX1CMD_MSRRPT_PARM_MACID_END(parm, macid_end);

    ret = wf_mcu_fill_mbox1_fw(nic_info, wMBOX1_9086X_MEDIA_STATUS_RPT, parm ,wMBOX1_MEDIA_STATUS_RPT_LEN);

    return ret;
}

wf_s32 wf_odm_disconnect_media_status(nic_info_st *nic_info,wdn_net_info_st *wdn_net_info)
{
    wf_s32 ret = 0;

    ret = wf_mcu_media_status_set(nic_info,wf_false,wf_false,wf_false,wMBOX1_MSR_ROLE_RSVD,wdn_net_info->wdn_id,wf_false,0);
    return ret;
}

#ifdef CFG_ENABLE_ADHOC_MODE
wf_s32 wf_adhoc_odm_connect_media_status(nic_info_st *pnic_info, wdn_net_info_st *pwdn_info)
{
    wf_s32 ret = 0;

    ret = wf_mcu_media_status_set(pnic_info,wf_true, wf_false, wf_false, wMBOX1_MSR_ROLE_ADHOC, pwdn_info->wdn_id, 0, 0);
    return ret;
}

wf_s32 wf_adhoc_odm_disconnect_media_status(nic_info_st *pnic_info,wdn_net_info_st *pwdn_info)
{
    wf_s32 ret = 0;

    ret = wf_mcu_media_status_set(pnic_info,wf_false, wf_false, wf_false, wMBOX1_MSR_ROLE_ADHOC, pwdn_info->wdn_id, 0, 0);
    return ret;
}
#endif

#ifdef CFG_ENABLE_AP_MODE
wf_s32 wf_ap_odm_connect_media_status(nic_info_st *pnic_info, wdn_net_info_st *pwdn_info)
{
    wf_s32 ret = 0;

    ret = wf_mcu_media_status_set(pnic_info,wf_true, wf_false, wf_false, wMBOX1_MSR_ROLE_STA, pwdn_info->wdn_id, 0, 0);
    return ret;
}

wf_s32 wf_ap_odm_disconnect_media_status(nic_info_st *pnic_info,wdn_net_info_st *pwdn_info)
{
    wf_s32 ret = 0;

    ret = wf_mcu_media_status_set(pnic_info,wf_false, wf_false, wf_false, wMBOX1_MSR_ROLE_STA, pwdn_info->wdn_id, 0, 0);
    return ret;
}
#endif

static wf_s32 wf_mcu_media_connect_set(nic_info_st *nic_info,wdn_net_info_st *wdn_net_info)
{
    wf_s32 ret = 0;

    ret = wf_mcu_media_status_set(nic_info,wf_true,wf_false,wf_false,wMBOX1_MSR_ROLE_AP,wdn_net_info->wdn_id,wf_false,0);
    return ret;
}

wf_s32 wf_mcu_msg_sta_info_get(nic_info_st *nic_info, wf_u32 wdn_id,mcu_msg_sta_info_st *msg_sta)
{
    wf_s32 ret = 0;
    int len = WF_RND4(sizeof(mcu_msg_sta_info_st));

    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_GET_MSG_STA_INFO, &wdn_id, 1, (wf_u32 *)&msg_sta, len / 4);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return ret;
}


static wf_s32 wf_mcu_msg_sta_info_set(nic_info_st *nic_info, mcu_msg_sta_info_st *msg_sta)
{
    wf_s32 ret = 0;
    wf_u32 *pbuf  = NULL;
    wf_s32 len = 0;

    len = WF_RND4(sizeof(mcu_msg_sta_info_st));

    pbuf = (wf_u32 *) wf_kzalloc(len);
    if (!pbuf)
    {
        LOG_E("[%s] failed", __func__);
        return WF_RETURN_FAIL;
    }

    wf_memcpy(pbuf, msg_sta, len);


    ret = mcu_cmd_communicate(nic_info, UMSG_OPS_HAL_SYNC_MSG_STA_INFO, pbuf, len / 4, NULL, 0);
    wf_kfree(pbuf);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return WF_RETURN_OK;
}

static wf_s32 bit_value_from_ieee_value_to_get_func(wf_u8 val, wf_u8 flag)
{
    wf_u8 dot11_rate_table[] ={ RATE_1M, RATE_2M, RATE_5_5M, RATE_11M, RATE_6M, RATE_9M, RATE_12M, RATE_18M, RATE_24M, RATE_36M, RATE_48M, RATE_54M, 0 };

    wf_s32 i = 0;
    if (flag)
    {
        while (dot11_rate_table[i] != 0)
        {
            if (dot11_rate_table[i] == val)
                return BIT(i);
            i++;
        }
    }
    return 0;
}

wf_s32 wf_mcu_get_rate_bitmap(nic_info_st *nic_info, wdn_net_info_st *wdn_net_info,mcu_msg_sta_info_st *msg_sta, wf_u32 *rate_bitmap)
{
    wf_s32 ret = 0;
    wf_u32 buf[3] = {0};
    wf_u32 ra_mask = 0;
    wf_s32 i = 0;

    if (NULL == wdn_net_info)
    {
        LOG_E("[%s] param is null, check!!!", __func__);
        return WF_RETURN_OK;
    }

    /*calc ra_mask*/
    for (i = 0; i < wdn_net_info->datarate_len; i++)
    {
        if (wdn_net_info->datarate[i])
            ra_mask |= bit_value_from_ieee_value_to_get_func(wdn_net_info->datarate[i] & 0x7f, 1);
    }

    for (i = 0; i < wdn_net_info->ext_datarate_len; i++)
    {
        if (wdn_net_info->ext_datarate[i])
            ra_mask |= bit_value_from_ieee_value_to_get_func(wdn_net_info->ext_datarate[i] & 0x7f, 1);
    }

    for (i = 0; i < 8; i++)
    {
        if (msg_sta->htpriv.ht_cap.supp_mcs_set[i / 8] & WF_BIT(i % 8))
        {
            ra_mask |= WF_BIT(i + 12);
        }
    }

    LOG_D("[%s] ra_mask: 0x%x", __func__, ra_mask);

    buf[0] = wdn_net_info->wdn_id;
    buf[1] = ra_mask;
    buf[2] = msg_sta->rssi_level;
    LOG_I("[%s] 0x%x,0x%x,0x%x", __func__, buf[0], buf[1], buf[2]);
    ret = mcu_cmd_communicate(nic_info, UMSG_0PS_MSG_GET_RATE_BITMAP, buf, 3, rate_bitmap, 1);
    if (WF_RETURN_FAIL == ret)
    {
        LOG_E("[%s] failed", __func__);
        return ret;
    }
    else if(WF_RETURN_CMD_BUSY == ret)
    {
        LOG_W("[%s] cmd busy,try again if need!",__func__);
        return ret;
    }

    return ra_mask;
}

wf_s32 mcu_msg_sta_info_pars(wdn_net_info_st *wdn_net_info, mcu_msg_sta_info_st *msg_sta)
{
    msg_sta->bUsed = wf_true;//???
    msg_sta->mac_id = wdn_net_info->wdn_id;

    wf_memcpy(msg_sta->hwaddr,wdn_net_info->mac,WF_ETH_ALEN);
    msg_sta->ra_rpt_linked   = wf_false;
    msg_sta->wireless_mode   = wdn_net_info->network_type;
    msg_sta->rssi_level      = 0;
    msg_sta->ra_change       = wf_false;
    msg_sta->htpriv.ht_option           = wdn_net_info->htpriv.ht_option;
    msg_sta->htpriv.ampdu_enable        = wdn_net_info->htpriv.ampdu_enable;
    msg_sta->htpriv.tx_amsdu_enable     = wdn_net_info->htpriv.tx_amsdu_enable;
    msg_sta->htpriv.bss_coexist         = wdn_net_info->htpriv.bss_coexist;
    msg_sta->htpriv.tx_amsdu_maxlen     = wdn_net_info->htpriv.tx_amsdu_maxlen;
    msg_sta->htpriv.rx_ampdu_maxlen     = wdn_net_info->htpriv.rx_ampdu_maxlen;
    msg_sta->htpriv.rx_ampdu_min_spacing= wdn_net_info->htpriv.rx_ampdu_min_spacing;
    msg_sta->htpriv.ch_offset           = wdn_net_info->htpriv.ch_offset;
    msg_sta->htpriv.sgi_20m             = wdn_net_info->htpriv.sgi_20m;
    msg_sta->htpriv.sgi_40m             = wdn_net_info->htpriv.sgi_40m;
    msg_sta->htpriv.agg_enable_bitmap   = wdn_net_info->htpriv.agg_enable_bitmap;
    msg_sta->htpriv.candidate_tid_bitmap= wdn_net_info->htpriv.candidate_tid_bitmap;
    msg_sta->htpriv.ldpc_cap            = wdn_net_info->htpriv.ldpc;
    msg_sta->htpriv.stbc_cap            = wdn_net_info->htpriv.tx_stbc;//??rx_stbc
    msg_sta->htpriv.smps_cap            = wdn_net_info->htpriv.smps;
    wf_memcpy(&msg_sta->htpriv.ht_cap,&wdn_net_info->ht_cap,sizeof(wdn_net_info->ht_cap));

    return 0;
}
wf_s32 wf_mcu_wdn_update(nic_info_st *nic_info, wdn_net_info_st *wdn_net_info)
{
    wf_u32 rate_bitmap = 0;
    wf_u32 macid = wdn_net_info->wdn_id;
    wf_u8 sgi = 0;
    wf_s32 ret = 0;
    mcu_msg_sta_info_st msg_sta;

    ret = mcu_msg_sta_info_pars(wdn_net_info,&msg_sta);

    ret = wf_mcu_msg_sta_info_set(nic_info, &msg_sta);
    if (ret == WF_RETURN_FAIL)
    {
        return ret;
    }

    ret = wf_mcu_get_rate_bitmap(nic_info, wdn_net_info,&msg_sta, &rate_bitmap);
    if (ret == WF_RETURN_FAIL)
        return ret;

    sgi = wf_ra_sGI_get(wdn_net_info, 1);
    LOG_D("macid=%d raid=%d bw=%d sgi=%d rate_bitmap=0x%08x", macid, wdn_net_info->raid, wdn_net_info->bw_mode, sgi, rate_bitmap);

    ret = wf_mcu_rfconfig_set(nic_info, macid, wdn_net_info->raid, wdn_net_info->bw_mode, sgi, rate_bitmap);
    if (ret == WF_RETURN_FAIL)
        return ret;

    ret = wf_mcu_media_connect_set(nic_info, wdn_net_info);
    if (ret == WF_RETURN_FAIL)
        return ret;

    return WF_RETURN_OK;
}
