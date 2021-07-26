#include "common.h"
#include "wf_debug.h"

#if 1
#define P2P_PROTO_DBG(fmt, ...)      LOG_D("P2P_PROTO[%s:%d]"fmt, __func__,__LINE__, ##__VA_ARGS__)
#define P2P_PROTO_ARRAY(data, len)   log_array(data, len)
#else
#define P2P_PROTO_DBG(fmt, ...)
#define P2P_PROTO_ARRAY(data, len)
#endif

#define P2P_PROTO_INFO(fmt, ...)     LOG_I("P2P_PROTO[%s:%d]"fmt, __func__,__LINE__, ##__VA_ARGS__)
#define P2P_PROTO_WARN(fmt, ...)     LOG_E("P2P_PROTO[%s:%d]"fmt, __func__,__LINE__, ##__VA_ARGS__)


typedef wf_s32 (*proto_handle)(nic_info_st *nic_info);
typedef struct p2p_proto_ops_st_
{
    P2P_PROTO_WK_ID id;
    proto_handle proto_func;
}p2p_proto_ops_st;


static wf_s32 p2p_proto_proc_concurrent(nic_info_st *nic_info)
{
    
    p2p_info_st *p2p_info = nic_info->p2p;
    p2p_wd_info_st *pwdinfo = &p2p_info->wdinfo;
    if(wf_p2p_check_buddy_linkstate(nic_info))
    {
        pwdinfo->operating_channel = wf_wlan_get_cur_channel(nic_info->buddy_nic);
        if (pwdinfo->driver_interface == DRIVER_CFG80211) 
        {
            wf_u8 current_bwmode = wf_wlan_get_cur_bw(nic_info->buddy_nic);

             wf_hw_info_set_channnel_bw(nic_info,pwdinfo->operating_channel,current_bwmode,HAL_PRIME_CHNL_OFFSET_DONT_CARE);
            
            //if (do_chk_partner_fwstate(pwadptdata, WIFI_FW_STATION_STATE))
            //    nulldata_to_pre_issue_func(pbuddy_wadptdata, NULL, 0, 3, 500);
        }
        else
        {
            LOG_I("WEXT for p2p to be doing.");
        }
    }
    else
    {
        if(P2P_STATE_GONEGO_OK != pwdinfo->p2p_state)
        {
            wf_hw_info_set_channnel_bw(nic_info,pwdinfo->listen_channel,CHANNEL_WIDTH_20,HAL_PRIME_CHNL_OFFSET_DONT_CARE);
        }
    }
    
    return 0;
}


static wf_s32 p2p_proto_proc_findphase(nic_info_st * nic_info)
{
    #if 0
    p2p_wd_info_st *pwdinfo = &pwadptdata->wdinfo;
    struct mlme_priv *pmlmepriv = &pwadptdata->mlmepriv;
    NDIS_802_11_SSID ssid;
    _irqL irqL;
    u8 _status = 0;

    _func_enter_;

    if (flag) {
        memset((unsigned char *)&ssid, 0, sizeof(NDIS_802_11_SSID));
        Func_Of_Proc_Pre_Memcpy(ssid.Ssid, pwdinfo->p2p_wildcard_ssid,
                    P2P_WILDCARD_SSID_LEN);
        ssid.SsidLength = P2P_WILDCARD_SSID_LEN;
    }
    wl_p2p_set_state(pwdinfo, P2P_STATE_FIND_PHASE_SEARCH);

    spin_lock_bh(&pmlmepriv->lock);
    _status = proc_sitesurvey_cmd_func(pwadptdata, &ssid, 1, NULL, 0);
    spin_unlock_bh(&pmlmepriv->lock);

    _func_exit_;
    #else
    //p2p_info_st *p2p_info = nic_info->p2p;
    //p2p_wd_info_st *pwdinfo = &p2p_info->wdinfo;
    
    //wf_scan_start(nic_info,SCAN_TYPE_ACTIVE,NULL,pwdinfo->p2p_wildcard_ssid,1,);
    #endif
    return 0;
}

static wf_s32 p2p_proto_restore_state(nic_info_st *nic_info)
{
    p2p_info_st *p2p_info = nic_info->p2p;
    p2p_wd_info_st *pwdinfo = &p2p_info->wdinfo;
    
    if(P2P_STATE_GONEGO_ING == pwdinfo->p2p_state || P2P_STATE_GONEGO_FAIL == pwdinfo->p2p_state)
    {
        wf_p2p_set_role(pwdinfo, P2P_ROLE_DEVICE);
    }
    if(wf_p2p_check_buddy_linkstate(nic_info))
    {
        if(P2P_STATE_TX_PROVISION_DIS_REQ == pwdinfo->p2p_state  || P2P_STATE_RX_PROVISION_DIS_RSP == pwdinfo->p2p_state)
        {
            wf_u8 cur_channel = wf_wlan_get_cur_channel(nic_info->buddy_nic);
            wf_u8 cur_bwmode  = wf_wlan_get_cur_bw(nic_info->buddy_nic);
            wf_hw_info_set_channnel_bw(nic_info->buddy_nic,cur_channel,cur_bwmode,HAL_PRIME_CHNL_OFFSET_DONT_CARE);
            //nulldata_to_pre_issue_func(pbuddy_wadptdata, NULL, 0, 3, 500);
        }
    }
    
    wf_p2p_set_state(pwdinfo, pwdinfo->pre_p2p_state);

    if (P2P_ROLE_DEVICE == pwdinfo->role ) 
    {
        if(nic_info->buddy_nic)
        {
            p2p_proto_proc_concurrent(nic_info);
        }
        else
        {
            wf_hw_info_set_channnel_bw(nic_info,pwdinfo->listen_channel,CHANNEL_WIDTH_20,HAL_PRIME_CHNL_OFFSET_DONT_CARE);
        }
    }
    
    return 0;
}


static wf_s32 p2p_proto_proc_tx_provdisc(nic_info_st *nic_info)
{
    
    p2p_info_st *p2p_info = nic_info->p2p;
    p2p_wd_info_st *pwdinfo = &p2p_info->wdinfo;
    
    if(wf_p2p_check_buddy_linkstate(nic_info))
    {
        p2p_proto_proc_concurrent(nic_info);
    }
    
    wf_hw_info_set_channnel_bw(nic_info,pwdinfo->tx_prov_disc_info.peer_channel_num[0],CHANNEL_WIDTH_20,HAL_PRIME_CHNL_OFFSET_DONT_CARE);
    wf_mcu_set_mlme_scan(nic_info,wf_true);
    wf_os_api_timer_set(&p2p_info->pre_tx_scan_timer, P2P_TX_PRESCAN_TIMEOUT);

    return 0;
}

static wf_s32 p2p_proto_proc_tx_invitereq(nic_info_st *nic_info)
{
    
    p2p_info_st *p2p_info = nic_info->p2p;
    p2p_wd_info_st *pwdinfo = &p2p_info->wdinfo;

    if(wf_p2p_check_buddy_linkstate(nic_info))
    {
        p2p_proto_proc_concurrent(nic_info);
    }

    wf_hw_info_set_channnel_bw(nic_info,pwdinfo->invitereq_info.peer_ch,CHANNEL_WIDTH_20,HAL_PRIME_CHNL_OFFSET_DONT_CARE);
    wf_mcu_set_mlme_scan(nic_info,wf_true);
    wf_os_api_timer_set(&p2p_info->pre_tx_scan_timer, P2P_TX_PRESCAN_TIMEOUT);
    

    return 0;
}

static wf_s32 p2p_proto_proc_tx_negoreq(nic_info_st *nic_info)
{
    
    p2p_info_st *p2p_info = nic_info->p2p;
    p2p_wd_info_st *pwdinfo = &p2p_info->wdinfo;
    
    if(wf_p2p_check_buddy_linkstate(nic_info))
    {
        p2p_proto_proc_concurrent(nic_info);
    }
    
    wf_hw_info_set_channnel_bw(nic_info,pwdinfo->nego_req_info.peer_channel_num[0],CHANNEL_WIDTH_20,HAL_PRIME_CHNL_OFFSET_DONT_CARE);
    wf_mcu_set_mlme_scan(nic_info,wf_true);
    //wf_p2p_issue_probereq(pwadptdata, NULL);
    //wf_p2p_issue_probereq(pwadptdata, pwdinfo->nego_req_info.peerDevAddr);
    wf_os_api_timer_set(&p2p_info->pre_tx_scan_timer, P2P_TX_PRESCAN_TIMEOUT);
    
    return 0;
}

static wf_s32 p2p_proto_proc_remain_channel(nic_info_st *pnic_info)
{
    wf_u8 ch, bw, offset;
    p2p_wd_info_st *pwdinfo = NULL;
    p2p_info_st *p2p_info = pnic_info->p2p;
    wf_wlan_mgmt_info_t *pwlan_mgmt_info = NULL;
    wf_wlan_network_t *pcur_network = NULL;
    pwdinfo = &p2p_info->wdinfo;

    P2P_PROTO_DBG("start");
    
    pwlan_mgmt_info = (wf_wlan_mgmt_info_t *)pnic_info->wlan_mgmt_info;
    if(NULL == pwlan_mgmt_info)
    {
        return -1;
    }
    
    pcur_network = &pwlan_mgmt_info->cur_network;
    
    if (p2p_info->p2p_enabled && pwdinfo->listen_channel) 
    {
        ch = pwdinfo->listen_channel;
        bw = CHANNEL_WIDTH_20;
        offset = HAL_PRIME_CHNL_OFFSET_DONT_CARE;
        P2P_PROTO_INFO(" back to listen ch - ch:%u, bw:%u, offset:%u\n",
                 ch, bw, offset);
    } 
    else 
    {
        ch = pwdinfo->restore_channel;
        bw = CHANNEL_WIDTH_20;
        offset = HAL_PRIME_CHNL_OFFSET_DONT_CARE;
        P2P_PROTO_INFO(" back to restore ch - ch:%u, bw:%u, offset:%u\n",
                     ch, bw, offset);
    }
    
    pcur_network->channel = ch;
    wf_hw_info_set_channnel_bw(pnic_info, ch, offset, bw);

    pwdinfo->is_ro_ch = wf_false;
    pwdinfo->last_ro_ch_time = wf_os_api_timestamp();
#if 1
    if(NULL != p2p_info->scb.remain_on_channel_expired)
    {
        p2p_info->scb.remain_on_channel_expired(pnic_info,NULL,0);
    }
#endif
    return 0;
}

static p2p_proto_ops_st gl_p2p_proto_funs[]=
{
    {P2P_FIND_PHASE_WK,p2p_proto_proc_findphase},
    {P2P_RESTORE_STATE_WK,p2p_proto_restore_state},
    {P2P_PRE_TX_PROVDISC_PROCESS_WK,p2p_proto_proc_tx_provdisc},
    {P2P_PRE_TX_NEGOREQ_PROCESS_WK,p2p_proto_proc_tx_negoreq},
    {P2P_PRE_TX_INVITEREQ_PROCESS_WK,p2p_proto_proc_tx_invitereq},
    {P2P_AP_P2P_CH_SWITCH_PROCESS_WK,p2p_proto_proc_concurrent},
    {P2P_RO_CH_WK,p2p_proto_proc_remain_channel}
};

char *proto_id_to_str(P2P_PROTO_WK_ID proto_id)
{
    switch(proto_id)
    {
        case P2P_FIND_PHASE_WK:
            return to_str(P2P_FIND_PHASE_WK);
        case P2P_RESTORE_STATE_WK:
            return to_str(P2P_RESTORE_STATE_WK);
        case P2P_PRE_TX_PROVDISC_PROCESS_WK:
            return to_str(P2P_PRE_TX_PROVDISC_PROCESS_WK);
        case P2P_PRE_TX_NEGOREQ_PROCESS_WK:
            return to_str(P2P_PRE_TX_NEGOREQ_PROCESS_WK);
        case P2P_PRE_TX_INVITEREQ_PROCESS_WK:
            return to_str(P2P_PRE_TX_INVITEREQ_PROCESS_WK);
            
        case P2P_AP_P2P_CH_SWITCH_PROCESS_WK:
            return to_str(P2P_AP_P2P_CH_SWITCH_PROCESS_WK);
        case P2P_RO_CH_WK:
            return to_str(P2P_RO_CH_WK);
        default:
            return "unknown proto_id";
    }
}
wf_s32 wf_p2p_protocol_dispatch_entry(nic_info_st *pnic_info, int proto_id)
{
    wf_s32 ret = 0;
    if(NULL == pnic_info)
    {
        return -1;
    }
    
    if(P2P_FIND_PHASE_WK > proto_id || proto_id > P2P_RO_CH_WK)
    {
        LOG_W("%s() proto_id:%d",__func__,proto_id);
    }
    
    if(gl_p2p_proto_funs[proto_id].proto_func)
    {
        ret = gl_p2p_proto_funs[proto_id].proto_func(pnic_info);
        if(WF_RETURN_OK != ret)
        {
            P2P_PROTO_WARN("%s failed",proto_id_to_str(proto_id));
        }
    }
    else
    {
        P2P_PROTO_WARN("no proto func");

    }

    return ret;
}

wf_s32 p2p_mgnt_nego_tx(nic_info_st *pnic_info, wf_u8 tx_ch, wf_u8 *buf, wf_s32 len)
{
    struct xmit_buf *pxmit_buf;
    tx_info_st *ptx_info;
    wf_80211_mgmt_t *pmgmt;
    wf_wlan_mgmt_info_t *pwlan_mgmt_info = (wf_wlan_mgmt_info_t *)pnic_info->wlan_mgmt_info;
    wf_wlan_network_t *pcur_network = &pwlan_mgmt_info->cur_network;

    // wf_lps_deny(pnic_info, 1000);
    wf_scan_wait_done(pnic_info, wf_true, 200);

    /* if (tx_ch != pmlmeext->cur_channel) { */
    if (tx_ch != pcur_network->channel)
    {
        pcur_network->channel = tx_ch;
        wf_hw_info_set_channnel_bw(pnic_info, tx_ch, CHANNEL_WIDTH_20, HAL_PRIME_CHNL_OFFSET_DONT_CARE);
    }

    /* alloc xmit_buf */
    ptx_info = (tx_info_st *)pnic_info->tx_info;
    pxmit_buf = wf_xmit_extbuf_new(ptx_info);
    if(pxmit_buf == NULL)
    {
        P2P_PROTO_WARN("pxmit_buf is NULL");
        return -1;
    }

    /* clear frame head(txd + 80211head) */
    wf_memset(pxmit_buf->pbuf, 0,
              TXDESC_OFFSET + WF_OFFSETOF(wf_80211_mgmt_t, beacon));

    /* set frame type */
    pmgmt = (void *)&pxmit_buf->pbuf[TXDESC_OFFSET];
    wf_memcpy(pmgmt, buf,len);

    wf_nic_mgmt_frame_xmit(pnic_info, NULL, pxmit_buf, len);
    return 1;

}


wf_s32 wf_p2p_mgnt_nego(nic_info_st *pnic_info,void *param)
{
    p2p_nego_param_st *pn = param;
    wf_s32 type         = 0;
    wf_timer_t timer;
    wf_u32 dump_limit   = 8;
    wf_u32 dump_cnt     = 0;
    wf_s32 tx_ret       = 0;

    if(NULL == pnic_info ||NULL == param)
    {
        return -1;
    }
    
    pn = param;
    P2P_PROTO_DBG("action:%d,tx_ch:%d,len:0x%x",pn->action,pn->tx_ch,pn->len);
    type = wf_p2p_check_frames(pnic_info,pn->buf,pn->len,wf_true,1);
    if(type < 0)
    {
        P2P_PROTO_WARN("wf_p2p_check_frames return %d",type);
        return -2;
    }

    wf_timer_set(&timer, 0);
    while(1)
    {
        wf_u32 sleep_ms = 0;
        dump_cnt++;
        tx_ret = p2p_mgnt_nego_tx(pnic_info, pn->tx_ch, pn->buf, pn->len);
        if(WF_WLAN_ACTION_PUBLIC_GAS_INITIAL_REQ == pn->action || 
            WF_WLAN_ACTION_PUBLIC_GAS_INITIAL_RSP == pn->action)
        {
                sleep_ms = 50;
                wf_timer_mod(&timer, 500);
        }
        if (tx_ret == wf_true ||
            (dump_cnt >= dump_limit && wf_timer_expired(&timer)))
        {
            break;
        }
        if (sleep_ms > 0)
        {
            wf_msleep(sleep_ms);
        }
    }

    if (P2P_GO_NEGO_CONF == type)
    {

            P2P_PROTO_INFO("P2P_GO_NEGO_CONF");
            //do_clear_scan_deny(pwadptdata);
    }
    else if(P2P_INVIT_RESP == type)
    {
            p2p_info_st *p2p_info = pnic_info->p2p;
            wf_widev_invit_info_t *invit_info = &p2p_info->wdinfo.invit_info;
            P2P_PROTO_INFO("P2P_INVIT_RESP");
            if (invit_info->flags & BIT(0) && invit_info->status == 0)
            {
                P2P_PROTO_INFO(" agree with invitation of persistent group\n");
                //wl_scan_deny_set(pwadptdata, 5000);
                //wl_pwr_wakeup_ex(pwadptdata, 5000);
                //do_clear_scan_deny(pwadptdata);
            }
       
    }
    else
    {
    }
    
    P2P_PROTO_DBG();
    // wf_lps_deny_cancel(pnic_info,PS_DENY_MGNT_TX);

    return 0;
}
wf_s32 wf_p2p_cannel_remain_on_channel(nic_info_st *pnic_info)
{
    p2p_info_st *p2p_info   = pnic_info->p2p;
    p2p_wd_info_st *pwdinfo = &p2p_info->wdinfo;

    if (pwdinfo->is_ro_ch == wf_true)
    {
        P2P_PROTO_INFO("cancel ro ch timer\n");
        wf_p2p_msg_timer_stop(pnic_info,WF_P2P_MSG_TAG_TIMER_RO_CH_STOP);
        wf_p2p_protocol_dispatch_entry(pnic_info, P2P_RO_CH_WK);
    }

    wf_p2p_set_state(pwdinfo, pwdinfo->pre_p2p_state);
    P2P_PROTO_INFO("role:%s, state:%s",wf_p2p_role_to_str(pwdinfo->role),wf_p2p_state_to_str(pwdinfo->p2p_state));
    pwdinfo->is_ro_ch = wf_false;
    pwdinfo->last_ro_ch_time = wf_os_api_timestamp();
    
    return 0;
}

wf_s32 wf_p2p_remain_on_channel(nic_info_st *pnic_info)
{
    p2p_info_st *p2p_info                   = NULL;
    p2p_wd_info_st *pwdinfo                 = NULL;
    wf_wlan_mgmt_info_t *pwlan_mgmt_info    = NULL;
    wf_wlan_network_t *pcur_network         = NULL;

    if(NULL == pnic_info)
    {
        return -1;
    }
    
    p2p_info                   = pnic_info->p2p;
    pwdinfo                 = &p2p_info->wdinfo;
    pwlan_mgmt_info = (wf_wlan_mgmt_info_t *)pnic_info->wlan_mgmt_info;
    pcur_network    = &pwlan_mgmt_info->cur_network;
    P2P_PROTO_DBG(" ch:%u duration:%d\n",pwdinfo->remain_ch, pwdinfo->ro_ch_duration);
    if (pwdinfo->is_ro_ch == wf_true)
    {
        P2P_PROTO_INFO("cancel ro ch timer\n");
        wf_p2p_msg_timer_stop(pnic_info,WF_P2P_MSG_TAG_TIMER_RO_CH_STOP);
    }

    pwdinfo->is_ro_ch = wf_true;
    pwdinfo->last_ro_ch_time = wf_os_api_timestamp();

    if (pwdinfo->p2p_state == P2P_STATE_NONE)
    {
        wf_p2p_enable(pnic_info, P2P_ROLE_DEVICE);
        p2p_info->p2p_enabled = wf_true;
        pwdinfo->listen_channel = pwdinfo->remain_ch;
    }
    else if (pwdinfo->p2p_state == P2P_STATE_LISTEN)
    {
        P2P_PROTO_INFO("listen_channel:%d, remain_ch:%d",pwdinfo->listen_channel,pwdinfo->remain_ch);
        if(pwdinfo->listen_channel != pwdinfo->remain_ch)
        {
            pwdinfo->listen_channel = pwdinfo->remain_ch;
        }

    }
    else
    {
        wf_p2p_set_pre_state(pwdinfo, pwdinfo->p2p_state);
        P2P_PROTO_INFO("role=%d, p2p_state=%d\n", pwdinfo->role,pwdinfo->p2p_state);
    }

    wf_p2p_set_state(pwdinfo, P2P_STATE_LISTEN);
    P2P_PROTO_INFO("role=%d, p2p_state=%d  listen_channel=%d\n", pwdinfo->role,pwdinfo->p2p_state,pwdinfo->listen_channel);

    pwdinfo->restore_channel = pcur_network->channel;
    pcur_network->channel = pwdinfo->remain_ch;
    P2P_PROTO_DBG("current channel:%d",pcur_network->channel);
    wf_hw_info_set_channnel_bw(pnic_info, pwdinfo->remain_ch, CHANNEL_WIDTH_20, HAL_PRIME_CHNL_OFFSET_DONT_CARE);

    wf_p2p_msg_timer_start(pnic_info,WF_P2P_MSG_TAG_TIMER_RO_CH_START,pwdinfo->ro_ch_duration);

    if(p2p_info->scb.ready_on_channel)
    {
        p2p_info->scb.ready_on_channel(pnic_info,NULL,0);
    }

    return 0;
}

static wf_pt_rst_t p2p_core_thrd (nic_info_st *pnic_info)
{
    p2p_info_st *p2p_info           = pnic_info->p2p;
    p2p_proto_mgt_st *p2p_proto_mgt = &p2p_info->p2p_proto_mgt;
    p2p_timer_st *p2p_timers        = &p2p_info->p2p_timers;
    wf_pt_t *pt             = &p2p_proto_mgt->pt[0];
//    wf_pt_t *pt_sub         = &p2p_proto_mgt->pt[1];
    wf_msg_que_t *pmsg_que  = &p2p_proto_mgt->msg_que;
    wf_msg_t *pmsg          = NULL;
    
    PT_BEGIN(pt);

    while (wf_true)
    {
        if (p2p_proto_mgt->thrd_abort)
        {
            P2P_PROTO_INFO("thread abort");
            PT_EXIT(pt);
        }
        if (!wf_msg_pop(pmsg_que, &pmsg))
        {
            break;
        }
        PT_YIELD(pt);
    }

    P2P_PROTO_INFO("tag:0x%x",pmsg->tag);
    if (WF_P2P_MSG_TAG_TIMER_RO_CH_START == pmsg->tag )
    {
        p2p_timer_param_st *p2p_tp = (void*)pmsg->value;
        p2p_proto_mgt->pmsg = pmsg;
        P2P_PROTO_INFO("WF_P2P_MSG_TAG_TIMER_RO_CH_START(%d)",p2p_tp->duration);
        /* wait until timeout */
        wf_timer_set(&p2p_timers->remain_on_ch_timer, p2p_tp->duration);
        do
        {
            PT_WAIT_UNTIL(pt, !wf_msg_get(pmsg_que, &pmsg) ||
                          wf_timer_expired(&p2p_timers->remain_on_ch_timer));
            if (pmsg && WF_P2P_MSG_TAG_TIMER_RO_CH_STOP == pmsg->tag)
            {
                P2P_PROTO_INFO("WF_P2P_MSG_TAG_TIMER_RO_CH_STOP...");
            }
            else
            {
                if(pmsg)
                {
                    P2P_PROTO_INFO("tag: 0x%x...",pmsg->tag);
                }
                else
                {
                    P2P_PROTO_INFO("tag: 0x%x...",p2p_proto_mgt->pmsg->tag);
                }
            }
            wf_msg_del(pmsg_que, p2p_proto_mgt->pmsg);
            break;
        }
        while (wf_true);
    }
    else if (WF_P2P_MSG_TAG_TIMER_RO_CH_STOP == pmsg->tag )
    {
        P2P_PROTO_INFO("WF_P2P_MSG_TAG_TIMER_RO_CH_STOP...");
        p2p_proto_mgt->pmsg = pmsg;
        wf_msg_del(pmsg_que, p2p_proto_mgt->pmsg);
    }
    else if (WF_P2P_MSG_TAG_NEGO == pmsg->tag )
    {
        P2P_PROTO_INFO("WF_P2P_MSG_TAG_NEGO...");
        p2p_proto_mgt->pmsg = pmsg;
        wf_p2p_mgnt_nego(pnic_info,pmsg->value);
        wf_p2p_proto_thrd_post(pnic_info);
        wf_msg_del(pmsg_que, p2p_proto_mgt->pmsg);
    }
    else if (WF_P2P_MSG_TAG_RO_CH == pmsg->tag )
    {
        P2P_PROTO_INFO("WF_P2P_MSG_TAG_RO_CH...");
        p2p_proto_mgt->pmsg = pmsg;
        wf_p2p_remain_on_channel(pnic_info);
        wf_p2p_proto_thrd_post(pnic_info);
        wf_msg_del(pmsg_que, p2p_proto_mgt->pmsg);
    }
    else if (WF_P2P_MSG_TAG_RO_CH_CANNEL == pmsg->tag )
    {
        P2P_PROTO_INFO("WF_P2P_MSG_TAG_RO_CH_CANNEL...");
        p2p_proto_mgt->pmsg = pmsg;
        wf_p2p_cannel_remain_on_channel(pnic_info);
        wf_p2p_proto_thrd_post(pnic_info);
        wf_msg_del(pmsg_que, p2p_proto_mgt->pmsg);
    }
    else
    {
        P2P_PROTO_INFO("drop unsuited message(tag: %d)", pmsg->tag);
        wf_msg_del(pmsg_que, pmsg);
    }

    /* restart thread */
    PT_RESTART(pt);

    PT_END(pt);
}

static wf_s32 p2p_core (nic_info_st *pnic_info)
{
    p2p_info_st *p2p_info           = NULL;
    p2p_proto_mgt_st *p2p_proto_mgt = NULL;
    P2P_PROTO_DBG();

    wf_os_api_thread_affinity(DEFAULT_CPU_ID);

    while (1)
    {
        if(WF_CANNOT_RUN(pnic_info))
        {
            P2P_PROTO_WARN("WF_CANNOT_RUN");
            break;
        }
        p2p_info        = pnic_info->p2p;
        if(NULL == p2p_info)
        {
            P2P_PROTO_WARN("p2p_info is NULL");
            break;
        }
        
        p2p_proto_mgt   = &p2p_info->p2p_proto_mgt;
        /* poll mlme core */
        
        PT_INIT(&p2p_proto_mgt->pt[0]);
        while (PT_SCHEDULE(p2p_core_thrd(pnic_info)))
        {
            wf_msleep(1);
        }
        P2P_PROTO_DBG("");
    }

    P2P_PROTO_DBG("wait for thread destory...");
    p2p_proto_mgt->thrd_abort_rsp = wf_true;
    while (!wf_os_api_thread_wait_stop(p2p_proto_mgt->proto_tid))
    {
        wf_msleep(1);
    }

    wf_os_api_thread_exit(p2p_proto_mgt->proto_tid);

    return 0;
}


static wf_s32 p2p_proto_msg_init(wf_msg_que_t *pmsg_que)
{
    wf_msg_init(pmsg_que);
    return (wf_msg_alloc(pmsg_que, WF_P2P_MSG_TAG_TIMER_RO_CH_START, sizeof(p2p_timer_param_st), 2) ||
            wf_msg_alloc(pmsg_que, WF_P2P_MSG_TAG_TIMER_RO_CH_STOP, 0, 2) ||
            wf_msg_alloc(pmsg_que, WF_P2P_MSG_TAG_NEGO, sizeof(p2p_nego_param_st), 2) ||
             wf_msg_alloc(pmsg_que, WF_P2P_MSG_TAG_RO_CH, 0, 2) || 
             wf_msg_alloc(pmsg_que, WF_P2P_MSG_TAG_RO_CH_CANNEL, 0, 2)) ? -1 : 0;

}
wf_inline static void p2p_proto_msg_deinit (wf_msg_que_t *pmsg_que)
{
    wf_msg_deinit(pmsg_que);
}

static int p2p_msg_send (nic_info_st *pnic_info, wf_msg_tag_t tag, void *value, wf_u32 len)
{
    p2p_info_st *p2p_info   = NULL;
    p2p_proto_mgt_st *pp    = NULL;
    wf_msg_que_t *pmsg_que  = NULL;
    wf_msg_t *pmsg          = NULL;
    int rst;

    if (pnic_info == NULL)
    {
        return -1;
    }

    p2p_info = pnic_info->p2p;
    if (p2p_info == NULL)
    {
        return -2;
    }

    P2P_PROTO_INFO("tag:0x%x",tag);
    pp  = &p2p_info->p2p_proto_mgt;
    pmsg_que = &pp->msg_que;
    rst = wf_msg_new(pmsg_que, tag, &pmsg);
    if (rst)
    {
        P2P_PROTO_WARN("wf_msg_new fail error code: %d", rst);
        return -3;
    }
    if (value && len)
    {
        pmsg->len = len;
        wf_memcpy(pmsg->value, value, len);
    }

    rst = wf_msg_push(pmsg_que, pmsg);
    if (rst)
    {
        wf_msg_del(pmsg_que, pmsg);
        P2P_PROTO_WARN("wf_msg_push fail error code: %d", rst);
        return -4;
    }

    return 0;
}

wf_s32 wf_p2p_msg_timer_stop(nic_info_st *pnic_info,wf_msg_tag_t tag)
{
    return p2p_msg_send(pnic_info,tag,NULL,0);
}
wf_s32 wf_p2p_msg_timer_start(nic_info_st *pnic_info,wf_msg_tag_t tag,wf_u32 duration)
{
    p2p_timer_param_st param;
    param.duration = duration;
    return p2p_msg_send(pnic_info,tag,&param,sizeof(p2p_timer_param_st));
}

wf_s32 wf_p2p_msg_send(nic_info_st *pnic_info,wf_msg_tag_t tag,void *value,wf_u32 len)
{
    return p2p_msg_send(pnic_info,tag,value,len);
}

wf_s32 p2p_proto_mgt_init(void *p2p)
{
    p2p_info_st *p2p_info           = NULL;
    p2p_proto_mgt_st *p2p_proto_mgt = NULL;
    nic_info_st *nic_info           = NULL;
    
    if(NULL == p2p)
    {
        LOG_E("[%s,%d] input param is null",__func__,__LINE__);
        return WF_RETURN_FAIL;
    }
    p2p_info = p2p;
    nic_info                        = p2p_info->nic_info;
    p2p_proto_mgt                   = &p2p_info->p2p_proto_mgt;
    
    wf_que_init(&p2p_proto_mgt->proto_queue,WF_LOCK_TYPE_SPIN);
    p2p_proto_mgt->thrd_abort = wf_false;
    p2p_proto_msg_init(&p2p_proto_mgt->msg_que);
    wf_os_api_sema_init(&p2p_proto_mgt->thrd_sync_sema, 0);
    sprintf((char *)p2p_proto_mgt->proto_name,
            nic_info->virNic ? "p2p_proto_mgt:vir%d_%d" : "p2p_proto_mgt:wlan%d_%d",
            nic_info->hif_node_id, nic_info->ndev_id);
    if (NULL ==(p2p_proto_mgt->proto_tid=wf_os_api_thread_create(p2p_proto_mgt->proto_tid, (char *)p2p_proto_mgt->proto_name, (void *)p2p_core, nic_info)))
    {
        LOG_E("[%s] create thread failed",__func__);
        return -1;
    }
    else
    {
        wf_os_api_thread_wakeup(p2p_proto_mgt->proto_tid);
    }

    return 0;
}

wf_s32 p2p_proto_mgt_term(void *p2p)
{
    p2p_proto_mgt_st *p2p_proto_mgt = NULL;
    p2p_info_st *p2p_info           = NULL;
    
    if(NULL == p2p)
    {
        return 0;
    }
    p2p_info = p2p;
    p2p_proto_mgt = &p2p_info->p2p_proto_mgt;
    if (p2p_proto_mgt && p2p_proto_mgt->proto_tid)
    {
        p2p_proto_mgt->thrd_abort = wf_true;
        while(wf_false == p2p_proto_mgt->thrd_abort_rsp)
        {
            wf_msleep(1);
        }
        wf_os_api_thread_destory(p2p_proto_mgt->proto_tid);
        p2p_proto_msg_deinit(&p2p_proto_mgt->msg_que);
        p2p_proto_mgt->proto_tid = NULL;
    }

    return 0;
}

wf_s32 wf_p2p_proto_thrd_wait(nic_info_st *pnic_info)
{
    p2p_proto_mgt_st *p2p_proto_mgt = NULL;
    p2p_info_st *p2p_info           = NULL;

    if(NULL == pnic_info)
    {
        return -1;
    }
    p2p_info = pnic_info->p2p;
    p2p_proto_mgt = &p2p_info->p2p_proto_mgt;
    wf_os_api_sema_wait(&p2p_proto_mgt->thrd_sync_sema);
    return 0;
}
wf_s32 wf_p2p_proto_thrd_post(nic_info_st *pnic_info)
{
    p2p_proto_mgt_st *p2p_proto_mgt = NULL;
    p2p_info_st *p2p_info           = NULL;

    if(NULL == pnic_info)
    {
        return -1;
    }
    p2p_info = pnic_info->p2p;
    p2p_proto_mgt = &p2p_info->p2p_proto_mgt;
    wf_os_api_sema_post(&p2p_proto_mgt->thrd_sync_sema);

    return 0;
}
