#include "common.h"
#include "wf_debug.h"


static local_info_st default_cfg =
{
    .work_mode = WF_INFRA_MODE,
    .channel = 1,
    .bw = CHANNEL_WIDTH_20,
    .ssid = "SCI9083",
    .channel_plan = WF_CHPLAN_CHINA,
    .ba_enable = 1,
};
#ifdef CONFIG_CONCURRENT_MODE
static local_info_st default_cfg1 =
{
    .work_mode = WF_INFRA_MODE,
    .channel = 1,
    .bw = CHANNEL_WIDTH_20,
    .ssid = "SCI9083",
    .channel_plan = WF_CHPLAN_CHINA,
    .ba_enable = 1,
};

#endif


int wf_local_cfg_init(nic_info_st *nic_info)
{

#ifdef CONFIG_CONCURRENT_MODE
	if(nic_info->nic_num == 0)
    {
    	nic_info->local_info = &default_cfg;
	}
    else
    {
		nic_info->local_info = &default_cfg1;
	}
#else
	nic_info->local_info = &default_cfg;
#endif

    return 0;
}



int wf_local_cfg_term(nic_info_st *nic_info)
{
    return 0;
}


int wf_local_cfg_get_default(nic_info_st *nic_info)
{
    local_info_st *local_info = nic_info->local_info;
    hw_info_st *hw_info = nic_info->hw_info;
    
    if(nic_info->nic_cfg_file_read != NULL)
	{
		if (nic_info->nic_cfg_file_read((void *)nic_info) == 0)
        {      
            hw_info->channel_plan = local_info->channel_plan;
            hw_info->ba_enable = local_info->ba_enable;
        }
	}    

    return 0;
}


int wf_local_cfg_set_default(nic_info_st *nic_info)
{
    local_info_st *local_info = nic_info->local_info;
    int ret = 0;  

    LOG_D("[LOCAL_CFG] work_mode: %d",local_info->work_mode);
    LOG_D("[LOCAL_CFG] channel: %d",local_info->channel);
    LOG_D("[LOCAL_CFG] bw: %d",local_info->bw);
    LOG_D("[LOCAL_CFG] adhoc_master: %d",local_info->adhoc_master);
    LOG_D("[LOCAL_CFG] ssid: %s",local_info->ssid);
    

#ifdef CFG_ENABLE_ADHOC_MODE
	set_adhoc_master(nic_info, wf_false);
#endif
    
	ret = wf_hw_info_set_channnel_bw(nic_info,local_info->channel,local_info->bw, HAL_PRIME_CHNL_OFFSET_DONT_CARE);
    if (ret != WF_RETURN_OK)
    {
        return WF_RETURN_FAIL;
    }
	
	/* bb and iq calibrate */
	ret = wf_mcu_handle_bb_lccalibrate(nic_info);
	if (ret != WF_RETURN_OK)
    {
        return WF_RETURN_FAIL;
    }
	
	ret = wf_mcu_handle_bb_iq_calibrate(nic_info, local_info->channel);
	if (ret != WF_RETURN_OK)
	{
		return WF_RETURN_FAIL;
	}

	ret = wf_mcu_update_thermal(nic_info);
    if (ret != WF_RETURN_OK)
    {
        return WF_RETURN_FAIL;
    }

    // cfg sta/ap/adhoc/monitor mode
    ret = wf_mcu_set_op_mode(nic_info,local_info->work_mode);
    if (ret != WF_RETURN_OK)
    {
        return WF_RETURN_FAIL;
    }
	
    return WF_RETURN_OK;
}


sys_work_mode_e wf_local_cfg_get_work_mode (nic_info_st *pnic_info)
{
    local_info_st *plocal = (local_info_st *)pnic_info->local_info;
    return plocal->work_mode;
}

void wf_local_cfg_set_work_mode (nic_info_st *pnic_info, sys_work_mode_e mode)
{
    local_info_st *plocal = (local_info_st *)pnic_info->local_info;
    plocal->work_mode = mode;
}

