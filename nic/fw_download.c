#include "common.h"
#include "wf_debug.h"

#define WF_START_ADDR        0x00e8
#define REG_MCUFWDL          0x0080
#define MCUFWDL_EN			 WF_BIT(0)
#define MCUFWDL_RDY			 WF_BIT(1)
#define FWDL_ChkSum_rpt		 WF_BIT(2)
#define MACINI_RDY			 WF_BIT(3)
#define BBINI_RDY			 WF_BIT(4)
#define RFINI_RDY			 WF_BIT(5)
#define WINTINI_RDY			 WF_BIT(6)
#define RAM_DL_SEL			 WF_BIT(7)
#define CPU_DL_READY		 WF_BIT(15) /* add flag  by gw for fw download ready 20130826 */
#define ROM_DLEN			 WF_BIT(19)
#define CPRST				 WF_BIT(23)

#define DSP_CTRL_READ8       wf_io_read8
#define DSP_CTRL_WRITE8      wf_io_write8
#define DSP_CTRL_READ32      wf_io_read32
#define DSP_CTRL_WRITE32     wf_io_write32


static int fwdl_cmd_get_status(nic_info_st *nic_info)
{
    int ret;
    /*test base on hisilicon platform, it would need 25000*/
    wf_u32 timeout = 25000; //2000->8000->15000(for GK7202 , get efuse, dw fw, )
    wf_u32 data = 0;
    wf_u32 tryCnt = 0;

    while (timeout--)
    {
        int err = 0;
        if (nic_info->is_driver_stopped || nic_info->is_surprise_removed)
            return WF_RETURN_FAIL;

        tryCnt++;
        data = wf_io_read32(nic_info, WF_MAILBOX_INT_FINISH,&err);
        if(err)
        {
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


static int fwdl_wait_fw_startup(nic_info_st *nic_info)
{
    wf_u32 fw_status = 0;
    int ret = 0;
    wf_u8  value8;
    wf_u16 count;
    wf_u32 value32;
    wf_u32 value_to_check = 0;
	wf_u32 value_expected = (MCUFWDL_RDY | FWDL_ChkSum_rpt | WINTINI_RDY | RAM_DL_SEL);

#ifdef CONFIG_RICHV200_FPGA
	/*  polling for FW ready */
    count = 0;
	do {
        count++;
		value32 = DSP_CTRL_READ32(nic_info, REG_MCUFWDL,NULL);
		value_to_check = value32 & value_expected;
		if (value_to_check == value_expected)
			break;
        LOG_I("Polling FW Ready......, True value:0x%08X,  Need value:0x%08X, but value:0x%08X", value32, value_expected, value_to_check);
		wf_mdelay(100);
	} while (count < 20);

	LOG_I("count:%d",count);
	if (value_to_check != value_expected) {
        LOG_I("%s: fail\n", __func__);
		return WF_RETURN_FAIL;
    }

    LOG_I("%s: Finish\n", __func__);
    return WF_RETURN_OK;
#else

   /* get mcu feedback */
    if (fwdl_cmd_get_status(nic_info) < 0)
    {
        LOG_E("===>wf_mcu_cmd_get_status error, exit");
        return WF_RETURN_FAIL;
    }
    else
    {
        #if 0
        if(NIC_SDIO == nic_info->nic_type)
        {
            wf_io_write8(nic_info,0x98, 0xff);
            wf_io_write8(nic_info,0x99, 0xff);
            wf_io_write8(nic_info,0x9a, 0x00);
        }
        #endif

        /*notice mcu setup dsp fw*/
        if( NIC_USB == nic_info->nic_type)
    	{
    		ret = mcu_cmd_communicate(nic_info,UMSG_OPS_HAL_DW_FW,NULL,0,&fw_status,1);
    	}
    	else
    	{
    		ret = mcu_cmd_communicate(nic_info,WLAN_OPS_DXX0_HAL_DW_FW,NULL,0,&fw_status,1);
    	}
    	if(ret == WF_RETURN_FAIL)
    	{
    		LOG_E("dsp startup failed");
    		return ret;
    	}

        if (!fw_status)
        {
            LOG_E("fw go fail!\n");
            return ret;
        }
        else
        {
            LOG_I("fw go sucess!\n");
        }
    }
#endif

    return WF_RETURN_OK;
}


static void fwdl_configure_check(nic_info_st *nic_info)
{
    wf_u32 value32;

    #ifdef CONFIG_RICHV200_FPGA
		#if 0
		LOG_I("0x9004----0x%x",wf_io_read32(nic_info,SDIO_BASE | WL_REG_HIMR,NULL));
		LOG_I("0x904b----0x%x",wf_io_read32(nic_info,SDIO_BASE | 0x904b,NULL));
		LOG_I("0x9008----0x%x",wf_io_read32(nic_info,SDIO_BASE | WL_REG_HISR,NULL));
		LOG_I("0x9010----0x%x",wf_io_read32(nic_info,SDIO_BASE | WL_REG_SZ_RX_REQ,NULL));
		LOG_I("0x9008----0x%x",wf_io_read32(nic_info,SDIO_BASE | WL_REG_HISR,NULL));
		LOG_I("0x904C----0x%x",wf_io_read32(nic_info,SDIO_BASE | 0x904C,NULL));
		LOG_I("0x9039----0x%x",wf_io_read32(nic_info,SDIO_BASE | 0x9039,NULL));
		LOG_I("0x9000----0x%x",wf_io_read32(nic_info,SDIO_BASE | 0x9000,NULL));
		#endif
    #endif
}


int wf_fw_download(nic_info_st *nic_info)
{
    int ret=0;
    wf_timer_t timer;
    fwdl_info_t *fwdl_info = &nic_info->fwdl_info;

    LOG_I("start");
    wf_hw_mcu_disable(nic_info);
    wf_hw_mcu_enable(nic_info);

#ifdef CONFIG_RICHV200_FPGA
    fwdl_configure_check(nic_info);
#endif

    wf_timer_set(&timer, 0);
    LOG_I("fw downloading.....");
#ifndef CONFIG_RICHV200_FPGA

    if (nic_info->nic_type == NIC_USB)
    {
        wf_u32 i = 0;
        int block;
        int remain;


        block = fwdl_info->fw0_usb_size / 64;
        remain = fwdl_info->fw0_usb_size % 64;

        for (i = 0; i < block; i++)
        {
            ret = nic_info->nic_write(nic_info->hif_node, 0, WF_START_ADDR, (wf_u8 *)&fwdl_info->fw0_usb[i * 64], 64);
            if (ret < 0)
            {
                LOG_E("===>nic_write error");
                return -1;
            }
        }

        if (remain)
        {
            nic_info->nic_write(nic_info->hif_node, 0, WF_START_ADDR, (wf_u8 *)&fwdl_info->fw0_usb[i * 64], remain);
            if (ret < 0)
            {
                LOG_E("===>nic_write error");
                return -1;
            }
        }
    }
    else
    {
        wf_u32 i = 0;
        int block;
        int remain;

        block = fwdl_info->fw0_sdio_size / 4;
        remain = fwdl_info->fw0_sdio_size % 4;

        for (i = 0; i < block; i++)
        {
            ret = nic_info->nic_write(nic_info->hif_node, 0, WF_START_ADDR, (wf_u8 *)&fwdl_info->fw0_sdio[i * 4], 4);
            if (ret < 0)
            {
                LOG_E("===>nic_write error");
                return -1;
            }
        }

        if (remain)
        {
            nic_info->nic_write(nic_info->hif_node, 0, WF_START_ADDR, (wf_u8 *)&fwdl_info->fw0_sdio[i * 4], remain);
            if (ret < 0)
            {
                LOG_E("===>nic_write error");
                return -1;
            }
        }

    }

#else // CONFIG_RICHV200_FPGA

    if (nic_info->nic_type == NIC_USB)
    {
        int ret;
        wf_u8  value8;

        /* ROM select */
        LOG_I("usb old:0xf4---0x%x",wf_io_read8(nic_info, 0xf4,NULL));
        if(fwdl_info->fw_usb_rom_type)
        {
            value8 = wf_io_read8(nic_info, 0xf4,NULL);
            value8 &= 0xFE;
            ret = wf_io_write8(nic_info, 0xf4, value8);
            value8 = wf_io_read8(nic_info, 0xf4,NULL);
        }
        else
        {
            value8 = wf_io_read8(nic_info, 0xf4,NULL);
            value8 |= 0x01;
            ret = wf_io_write8(nic_info, 0xf4, value8);
            value8 = wf_io_read8(nic_info, 0xf4,NULL);
        }
        LOG_I("usb new:0xf4---0x%x",wf_io_read8(nic_info, 0xf4,NULL));

        if(0 != wf_io_write_firmware(nic_info, 0, fwdl_info->fw0_usb, fwdl_info->fw0_usb_size)) {
        	return -1;
        }
        if(0 != wf_io_write_firmware(nic_info, 1, fwdl_info->fw1_usb, fwdl_info->fw1_usb_size)) {
        	return -1;
        }
    }
    else  // sdio
    {
        int ret;
        wf_u8  value8;

        /* ROM select */
        LOG_I("sdio old:0xf4---0x%x",wf_io_read8(nic_info, 0xf4,NULL));
        if(nic_info->fwdl_info.fw_sdio_rom_type) 
        {
            value8 = wf_io_read8(nic_info, 0xf4,NULL);
            value8 &= 0xFE;
            ret = wf_io_write8(nic_info, 0xf4, value8);
            value8 = wf_io_read8(nic_info, 0xf4,NULL);
        }
        else
        {
            value8 = wf_io_read8(nic_info, 0xf4,NULL);
            value8 |= 0x01;
            ret = wf_io_write8(nic_info, 0xf4, value8);
            value8 = wf_io_read8(nic_info, 0xf4,NULL);
        }
        LOG_I("sdio new:0xf4---0x%x",wf_io_read8(nic_info, 0xf4,NULL));

		// download dsp fw
		if(0 != wf_io_write_firmware(nic_info, 1, fwdl_info->fw1_sdio, fwdl_info->fw1_sdio_size)) {
			return -1;
		}

	    // download m0 fw
        if(0 != wf_io_write_firmware(nic_info, 0, fwdl_info->fw0_sdio, fwdl_info->fw0_sdio_size)) {
        	return -1;
        }
    }
#endif

    LOG_D("===>fw download elapsed:  %d ms", wf_timer_elapsed(&timer));

    /* fw startup */
    if (wf_hw_mcu_startup(nic_info) < 0)
    {
        LOG_E("===>wf_hw_mcu_startup error, exit!!");
        return WF_RETURN_FAIL;
    }

    /* wait fw status */
    if(fwdl_wait_fw_startup(nic_info) < 0)
    {
        LOG_E("===>dsp_run_startup error, exit!!");
        return WF_RETURN_FAIL;
    }

    LOG_I("end");

    return 0;
}


