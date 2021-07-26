#include "common.h"
#include "wf_debug.h"

#if 1
#define P2P_TIMER_DBG(fmt, ...)      LOG_D("P2P_TIMER[%s:%d][%d]"fmt, __func__,__LINE__,pnic_info->ndev_id, ##__VA_ARGS__)
#define P2P_TIMER_ARRAY(data, len)   log_array(data, len)
#else
#define P2P_TIMER_DBG(fmt, ...)
#define P2P_TIMER_ARRAY(data, len)
#endif
#define P2P_TIMER_INFO(fmt, ...)     LOG_I("P2P_TIMER[%s:%d][%d]"fmt, __func__,__LINE__,pnic_info->ndev_id, ##__VA_ARGS__)
#define P2P_TIMER_WARN(fmt, ...)     LOG_E("P2P_TIMER[%s:%d][%d]"fmt, __func__,__LINE__,pnic_info->ndev_id, ##__VA_ARGS__)


void process_p2p_switch_timer_of_ap_func(wf_os_api_timer_t *ptimer)
{
//    p2p_info_st *p2p_info = WF_CONTAINER_OF((wf_os_api_timer_t *)ptimer,p2p_info_st,ap_p2p_switch_timer);
//    p2p_wd_info_st *pwdinfo = &p2p_info->wdinfo;
//#ifdef CONFIG_IOCTL_CFG80211	
//    struct wf_widev_priv *pwdev_priv = NULL;
//    nic_info_st *nic_info = p2p_info->nic_info;
//#endif
//
//    P2P_TIMER_DBG("start");
//
//    if (pwdinfo->p2p_state == P2P_STATE_NONE)
//    {
//        return;
//    }
//#ifdef CONFIG_IOCTL_CFG80211
//    pwdev_priv = nic_info->widev_priv;
//    atomic_set(&pwdev_priv->switch_ch_to, 1);
//#endif

    //protocol_p2p_wk_cmd_func(wadptdata, P2P_AP_P2P_CH_SWITCH_PROCESS_WK);
}

void p2p_reset_operation_ch(void *pwifidirect_info)
{
    p2p_wd_info_st *pwdinfo = pwifidirect_info;
    pwdinfo->p2p_info.operation_ch[0] = 0;
#ifdef CONFIG_P2P_OP_CHK_SOCIAL_CH
    pwdinfo->p2p_info.operation_ch[1] = 0;
    pwdinfo->p2p_info.operation_ch[2] = 0;
    pwdinfo->p2p_info.operation_ch[3] = 0;
#endif
    pwdinfo->p2p_info.scan_op_ch_only = 0;
}
void process_reset_ch_sitesurvey_timer_plus_func(wf_os_api_timer_t *ptimer)
{
    p2p_info_st *p2p_info = WF_CONTAINER_OF((wf_os_api_timer_t *)ptimer,p2p_info_st,reset_ch_sitesurvey2);
    p2p_wd_info_st *pwdinfo = &p2p_info->wdinfo;
    nic_info_st *pnic_info  = p2p_info->nic_info;
    
    P2P_TIMER_DBG("start");
    if (pwdinfo->p2p_state == P2P_STATE_NONE)
    {
        return;
    }
    
   p2p_reset_operation_ch(pwdinfo);
}

void p2p_reset_invitereq_operation_ch(void *pwifidirect_info)
{
    p2p_wd_info_st *pwdinfo = pwifidirect_info;
    pwdinfo->rx_invitereq_info.operation_ch[0] = 0;
#ifdef CONFIG_P2P_OP_CHK_SOCIAL_CH
    pwdinfo->rx_invitereq_info.operation_ch[1] = 0;
    pwdinfo->rx_invitereq_info.operation_ch[2] = 0;
    pwdinfo->rx_invitereq_info.operation_ch[3] = 0;
#endif
    pwdinfo->rx_invitereq_info.scan_op_ch_only = 0;
}
void process_reset_ch_sitesurvey_timer_func(wf_os_api_timer_t *ptimer)
{
    p2p_info_st *p2p_info = WF_CONTAINER_OF((wf_os_api_timer_t *)ptimer,p2p_info_st,reset_ch_sitesurvey);
    p2p_wd_info_st *pwdinfo = &p2p_info->wdinfo;
    nic_info_st *pnic_info  = p2p_info->nic_info;
    
    P2P_TIMER_DBG("start");
    if (pwdinfo->p2p_state ==  P2P_STATE_NONE)
    {
        return;
    }

    p2p_reset_invitereq_operation_ch(pwdinfo);
}


static void process_pre_tx_scan_timer_func(wf_os_api_timer_t *ptimer)
{
    p2p_info_st *p2p_info = WF_CONTAINER_OF((wf_os_api_timer_t *)ptimer,p2p_info_st,pre_tx_scan_timer);
    p2p_wd_info_st *pwdinfo = &p2p_info->wdinfo;
    nic_info_st *pnic_info  = p2p_info->nic_info;
    P2P_TIMER_DBG("start");
    if (pwdinfo->p2p_state ==  P2P_STATE_NONE)
    {
        return;
    }

    //spin_lock_bh(&pmlmepriv->lock);
    if (pwdinfo->p2p_state ==  P2P_STATE_TX_PROVISION_DIS_REQ) 
    {
        if (wf_true == pwdinfo->tx_prov_disc_info.benable)
        {
//            p2p_proto_queue_insert(&p2p_info->p2p_proto_mgt,P2P_PRE_TX_PROVDISC_PROCESS_WK);
        }
    }
    else if (pwdinfo->p2p_state ==  P2P_STATE_GONEGO_ING)
    {
        if (wf_true == pwdinfo->nego_req_info.benable) 
        {
//            p2p_proto_queue_insert(&p2p_info->p2p_proto_mgt,P2P_PRE_TX_NEGOREQ_PROCESS_WK);
        }
    } 
    else if (pwdinfo->p2p_state ==   P2P_STATE_TX_INVITE_REQ) 
    {
        if (wf_true == pwdinfo->invitereq_info.benable) 
        {
//            p2p_proto_queue_insert(&p2p_info->p2p_proto_mgt,P2P_PRE_TX_INVITEREQ_PROCESS_WK);
        }
    }
    else 
    {
        LOG_I("[%s] p2p_state is %d, ignore!!\n", __FUNCTION__,pwdinfo->p2p_state);
    }

    //spin_unlock_bh(&pmlmepriv->lock);
}

static void p2p_state_timer_process_restore_func(wf_os_api_timer_t *ptimer)
{
    p2p_info_st *p2p_info = WF_CONTAINER_OF((wf_os_api_timer_t *)ptimer,p2p_info_st,restore_p2p_state_timer);
    p2p_wd_info_st *pwdinfo = &p2p_info->wdinfo;
    nic_info_st *pnic_info  = p2p_info->nic_info;
    P2P_TIMER_DBG("start");
    if (pwdinfo->p2p_state ==  P2P_STATE_NONE)
    {
        return;
    }

//    p2p_proto_queue_insert(&p2p_info->p2p_proto_mgt,P2P_RESTORE_STATE_WK);
}

static void process_find_phase_timer_func(wf_os_api_timer_t *ptimer)
{
    p2p_info_st *p2p_info = WF_CONTAINER_OF((wf_os_api_timer_t *)ptimer,p2p_info_st,find_phase_timer);
    p2p_wd_info_st *pwdinfo = &p2p_info->wdinfo;
    nic_info_st *pnic_info  = p2p_info->nic_info;
    
    P2P_TIMER_DBG("start");
    if (pwdinfo->p2p_state ==  P2P_STATE_NONE)
    {
        return;
    }
    
    pwdinfo->find_phase_state_exchange_cnt++;

//    p2p_proto_queue_insert(&p2p_info->p2p_proto_mgt,P2P_FIND_PHASE_WK);
}

void p2p_timers_init(nic_info_st * pnic_info)
{
    p2p_info_st *p2p_info = pnic_info->p2p;
    
    P2P_TIMER_DBG("start");
    wf_os_api_timer_reg(&p2p_info->find_phase_timer,process_find_phase_timer_func, &p2p_info->find_phase_timer);
    wf_os_api_timer_reg(&p2p_info->restore_p2p_state_timer, p2p_state_timer_process_restore_func, &p2p_info->restore_p2p_state_timer);
    wf_os_api_timer_reg(&p2p_info->pre_tx_scan_timer, process_pre_tx_scan_timer_func, &p2p_info->pre_tx_scan_timer);
    wf_os_api_timer_reg(&p2p_info->reset_ch_sitesurvey,process_reset_ch_sitesurvey_timer_func, &p2p_info->reset_ch_sitesurvey);
    wf_os_api_timer_reg(&p2p_info->reset_ch_sitesurvey2, process_reset_ch_sitesurvey_timer_plus_func, &p2p_info->reset_ch_sitesurvey2);
    wf_os_api_timer_reg(&p2p_info->ap_p2p_switch_timer, process_p2p_switch_timer_of_ap_func, &p2p_info->ap_p2p_switch_timer);

}
