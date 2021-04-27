#include "common.h"
#include "wf_debug.h"

static int prv_hardware_init(nic_info_st *nic_info)
{
    int ret=0;
    wf_u8 mac_temp[WF_ETH_ALEN];

    LOG_D("[NIC] prv_hardware_init - entry");

    nic_info->is_driver_stopped = wf_false;
    nic_info->is_surprise_removed = wf_false;

    /* tx  init */
    if (wf_tx_info_init(nic_info) < 0)
    {
        LOG_E("===>wf_tx_info_init error");
        return WF_RETURN_FAIL;
    }

    /* rx  init */
    if (wf_rx_init(nic_info))
    {
        LOG_E("===>wf_rx_init error");
        return WF_RETURN_FAIL;
    }

    if (nic_info->virNic==wf_false)
    {
        /* fw download */
        if (wf_fw_download(nic_info) < 0)
        {
            LOG_E("===>wf_fw_download error, exit!!");
            return WF_RETURN_FAIL;
        }
    }

    /* hw info init */
    if (wf_hw_info_init(nic_info) < 0)
    {
        LOG_E("===>wf_hw_info_init error");
        return WF_RETURN_FAIL;
    }

	/* get hw default cfg */
    if (wf_hw_info_get_default_cfg(nic_info) < 0)
    {
        LOG_E("===>wf_hw_info_get_default_cfg error");
        return WF_RETURN_FAIL;
    }

    /* local info init */
    if (wf_local_cfg_init(nic_info) < 0)
    {
        LOG_E("===>wf_local_cfg_init error");
        return WF_RETURN_FAIL;
    }

    /* get local default cfg */
    if (wf_local_cfg_get_default(nic_info) < 0)
    {
        LOG_E("===>wf_local_cfg_get_default error");
        return WF_RETURN_FAIL;
    }

	/* init hardware by default cfg */
	if (wf_hw_info_set_default_cfg(nic_info) < 0)
    {
        LOG_E("===>wf_hw_info_set_default_cfg error");
        return WF_RETURN_FAIL;
    }

    nic_info->ndev_num++;

	LOG_D("[NIC] prv_hardware_init - exit");

    return ret;
}


static int prv_hardware_term(nic_info_st *nic_info)
{
    LOG_D("[NIC] prv_hardware_term - entry");

    if (nic_info->ndev_id == 0)
    {
        /* rx term */
        if (wf_rx_term(nic_info) < 0)
        {
            LOG_E("===>wf_rx_term error");
            return WF_RETURN_FAIL;
        }


        /* tx term */
        if (wf_tx_info_term(nic_info) < 0)
        {
            LOG_E("===>wf_tx_info_term error");
            return WF_RETURN_FAIL;
        }


        /* hw info term */
        if (wf_hw_info_term(nic_info) < 0)
            LOG_E("===>wf_hw_info_term error");
    }

    nic_info->ndev_num--;

    LOG_D("[NIC] prv_hardware_term - exit");

    return WF_RETURN_OK;
}

int nic_init(nic_info_st *nic_info)
{
    LOG_D("[NIC] nic_init - start");

    wf_os_api_sema_init(&nic_info->cmd_sema,1);
    nic_info->cmd_lock_use = wf_false;
    /* hardware init by chip */
    if (prv_hardware_init(nic_info) < 0)
    {
        LOG_E("===>prv_hardware_init error");
        return WF_RETURN_FAIL;
    }

#ifdef CONFIG_ARS_SUPPORT
    if(ars_init(nic_info) < 0)
    {
        LOG_E("===>ars_init error");
        return WF_RETURN_FAIL;
    }
#endif
    /*p2p*/
#ifdef CONFIG_P2P
    if (p2p_info_init(nic_info) < 0)
    {
        LOG_E("===>p2p_info_init error");
        return WF_RETURN_FAIL;
    }
#endif
    /*wdn init*/
    if (wf_wdn_init(nic_info) < 0)
    {
        LOG_E("===>wf_wdn_init error");
        return WF_RETURN_FAIL;
    }

    /* wlan init */
    if (wf_wlan_init(nic_info) < 0)
    {
        LOG_E("===>wf_wlan_init error");
        return WF_RETURN_FAIL;
    }

    /* mlme init */
    if (wf_mlme_init(nic_info) < 0)
    {
        LOG_E("===>wf_mlme_init error");
        return WF_RETURN_FAIL;
    }

    /* scan init */
    if (wf_scan_init(nic_info) < 0)
    {
        LOG_E("===>wf_scan_info_init error");
        return WF_RETURN_FAIL;
    }

    /* auth init */
    if (wf_auth_init(nic_info) < 0)
    {
        LOG_E("===>wf_auth_init error");
        return WF_RETURN_FAIL;
    }

    /* assoc init */
    if (wf_assoc_init(nic_info) < 0)
    {
        LOG_E("===>wf_assoc_init error");
        return WF_RETURN_FAIL;
    }

    /* sec init */
    if (wf_sec_info_init(nic_info) < 0)
    {
        LOG_E("===>wf_sec_info_init error");
        return WF_RETURN_FAIL;
    }

    /*odm mangment init*/
    if (wf_odm_mgnt_init(nic_info) < 0)
    {
        LOG_E("===>wf_odm_mgnt_init error");
        return WF_RETURN_FAIL;
    }

#ifdef CONFIG_LPS
    /* pwr_info init  */
    if (wf_lps_init(nic_info) < 0)
    {
        LOG_E("===>wf_lps_init error");
        return WF_RETURN_FAIL;
    }
#endif
#ifdef CFG_ENABLE_AP_MODE
    /* ap init */
    if (wf_ap_init(nic_info) < 0)
    {
        LOG_E("===>wf_ap_init error");
        return WF_RETURN_FAIL;
    }
#endif


    /* configure */
    if (wf_local_cfg_set_default(nic_info) < 0)
    {
        LOG_E("===>wf_local_cfg_set_default error");
        return WF_RETURN_FAIL;
    }


    LOG_D("[NIC] nic_init - end");
    nic_info->cmd_lock_use = wf_true;
    return WF_RETURN_OK;

}



int nic_term(nic_info_st *nic_info)
{
    wf_wlan_info_t *pwlan_info = nic_info->wlan_info;
    wf_wlan_network_t *cur_network = &pwlan_info->cur_network;

    nic_info->cmd_lock_use = wf_false;
    LOG_D("[NIC] nic_term - start");

#ifdef CFG_ENABLE_AP_MODE
    if (wf_ap_work_stop(nic_info) < 0)
    {
        LOG_E("===>wf_ap_work_stop error");
        return WF_RETURN_FAIL;
    }
#endif

    /* mlme term */
    if (wf_mlme_term(nic_info) < 0)
    {
        LOG_E("===>wf_mlme_term error");
        return WF_RETURN_FAIL;
    }

    /* scan term */
    if (wf_scan_term(nic_info) < 0)
    {
        LOG_E("===>wf_scan_term error");
        return WF_RETURN_FAIL;
    }

    /* auth term */
    if (wf_auth_term(nic_info) < 0)
    {
        LOG_E("===>wf_auth_term error");
        return WF_RETURN_FAIL;
    }

    /* assoc term */
    if (wf_assoc_term(nic_info) < 0)
    {
        LOG_E("===>wf_assoc_term error");
        return WF_RETURN_FAIL;
    }
    /* sec term */
    if (wf_sec_info_term(nic_info) < 0)
    {
        LOG_E("===>wf_sec_info_term error");
        return WF_RETURN_FAIL;
    }

    /* odm term */
    if (wf_odm_mgnt_term(nic_info) < 0)
    {
        LOG_E("===>wf_odm_mgnt_term error");
        return WF_RETURN_FAIL;
    }

    /* local configure term */
    if (wf_local_cfg_term(nic_info) < 0)
    {
        LOG_E("===>wf_local_cfg_term error");
        return WF_RETURN_FAIL;
    }

	/* wlan term */
    if (wf_wlan_term(nic_info) < 0)
    {
        LOG_E("===>wf_wlan_term error");
        return WF_RETURN_FAIL;
    }

    /*wdn term*/
    if (wf_wdn_term(nic_info) < 0)
    {
        LOG_E("===>wf_wdn_term error");
        return WF_RETURN_FAIL;
    }


    #ifdef CONFIG_RICHV200_FPGA
        wf_mcu_reset_chip(nic_info);
    #endif
#ifdef CONFIG_P2P
    if (p2p_info_term(nic_info) < 0)
    {
        LOG_E("===>p2p_info_term error");
        return WF_RETURN_FAIL;
    }
#endif
#ifdef CONFIG_ARS_SUPPORT
    if (ars_term(nic_info) < 0)
    {
        LOG_E("===>ars_term error");
        return WF_RETURN_FAIL;
    }
#endif

    /* hardware term */
    if (prv_hardware_term(nic_info) < 0)
    {
        LOG_E("prv_hardware_term, fail!");
        return WF_RETURN_FAIL;
    }
    LOG_D("[NIC] nic_term - end");
    nic_info->cmd_lock_use = wf_true;
    return WF_RETURN_OK;
}


int nic_enable(nic_info_st *nic_info)
{
    int ret = 0;
    odm_mgnt_st *odm = nic_info->odm;
    LOG_I("[NIC] nic_enable\n");
    nic_info->cmd_lock_use = wf_false;

    if( 0 == nic_info->is_up)
    {
        wf_mcu_enable_xmit(nic_info);
        nic_info->is_up = 1;
    }
    nic_info->cmd_lock_use = wf_true;
#ifdef CONFIG_ARS_SUPPORT
    //ars to do
#else
    wf_os_api_timer_set(&odm->odm_wdg_timer, 5000);
#endif
    return WF_RETURN_OK;
}


int nic_disable(nic_info_st *nic_info)
{
    int ret = WF_RETURN_FAIL;

    LOG_D("[NIC] nic_disable");
    nic_info->cmd_lock_use = wf_false;

    if(nic_info->is_up)
    {
        if(wf_local_cfg_get_work_mode(nic_info) == WF_MASTER_MODE)
        {
#ifdef CFG_ENABLE_AP_MODE
            wf_ap_work_stop(nic_info);
#endif
        }
        else
        {
            wf_mlme_deauth(nic_info);
        }
        nic_info->is_up = 0;

        ret = WF_RETURN_OK;
    }
    nic_info->cmd_lock_use = wf_true;

    return ret;
}


int nic_suspend(nic_info_st *nic_info)
{
    return WF_RETURN_OK;
}

int nic_resume(nic_info_st *nic_info)
{
    return WF_RETURN_OK;
}


int nic_shutdown(nic_info_st *nic_info)
{
    nic_info->is_driver_stopped = wf_true;
    nic_info->cmd_lock_use = wf_false;

    wf_scan_stop(nic_info);

    nic_info->cmd_lock_use = wf_true;

    return WF_RETURN_OK;
}

wf_u8 *nic_to_local_addr(nic_info_st *nic_info)
{
    hw_info_st *hw_info = nic_info->hw_info;

    if (hw_info == NULL)
    {
        return NULL;
    }

	return hw_info->macAddr;
}

