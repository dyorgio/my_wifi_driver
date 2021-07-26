#include "common.h"
#include "wf_debug.h"

#define WF_PUBLIC_ACTION_IE_OFFSET (8)

#if 1
#define P2P_FRAME_DBG(fmt, ...)      LOG_D("P2P_FRAME[%s:%d][%d]"fmt, __func__,__LINE__,pnic_info->ndev_id, ##__VA_ARGS__)
#define P2P_FRAME_ARRAY(data, len)   log_array(data, len)
#else
#define P2P_FRAME_DBG(fmt, ...)
#define P2P_FRAME_ARRAY(data, len)
#endif
#define P2P_FRAME_INFO(fmt, ...)     LOG_I("P2P_FRAME[%s:%d][%d]"fmt, __func__,__LINE__,pnic_info->ndev_id, ##__VA_ARGS__)
#define P2P_FRAME_WARN(fmt, ...)     LOG_E("P2P_FRAME[%s:%d][%d]"fmt, __func__,__LINE__,pnic_info->ndev_id, ##__VA_ARGS__)

#define wf_memmove memmove

wf_u8 *wf_p2p_get_ie(wf_u8 * in_ie, wf_s32 in_len, wf_u8 * p2p_ie, wf_u32 * p2p_ielen)
{
    wf_u32 cnt;
    wf_u8 *p2p_ie_ptr = NULL;
    wf_u8 eid;

    if (p2p_ielen)
    {
        *p2p_ielen = 0;
    }

    if (!in_ie || in_len < 0)
    {
        LOG_W("[%s,%d] in_len is not right",__func__,__LINE__);
        return p2p_ie_ptr;
    }

    if (in_len <= 0)
    {
        return p2p_ie_ptr;
    }

    cnt = 0;

    while (cnt + 1 + 4 < in_len)
    {
        eid = in_ie[cnt];

        if (cnt + 1 + 4 >= WF_80211_IES_SIZE_MAX)
        {
            LOG_W("[%s,%d] cnt is not right",__func__,__LINE__);
            return NULL;
        }

        if (eid == WF_80211_MGMT_EID_VENDOR_SPECIFIC && wf_memcmp(&in_ie[cnt + 2], P2P_OUI, 4) == 0)
        {
            p2p_ie_ptr = in_ie + cnt;

            if (p2p_ie)
            {
                wf_memcpy(p2p_ie, &in_ie[cnt], in_ie[cnt + 1] + 2);
            }

            if (p2p_ielen)
            {
                *p2p_ielen = in_ie[cnt + 1] + 2;
            }

            break;
        }
        else
        {
            cnt += in_ie[cnt + 1] + 2;
        }

    }

    return p2p_ie_ptr;
}

static wf_u8 *p2p_get_attr(wf_u8 * p2p_ie, wf_u32 p2p_ielen, wf_u8 target_attr_id,wf_u8 * buf_attr, wf_u32 * len_attr)
{
    wf_u8 *attr_ptr = NULL;
    wf_u8 *target_attr_ptr = NULL;
    if (len_attr)
        *len_attr = 0;

    if (!p2p_ie || p2p_ielen <= 6 || (p2p_ie[0] != WF_80211_MGMT_EID_VENDOR_SPECIFIC) || (wf_memcmp(p2p_ie + 2, P2P_OUI, 4) != 0))
    {
        return attr_ptr;
    }

    attr_ptr = p2p_ie + 6;

    while ((attr_ptr - p2p_ie + 3) <= p2p_ielen)
    {
        wf_u8 attr_id = *attr_ptr;
        wf_u16 attr_data_len = WF_GET_LE16(attr_ptr + 1);
        wf_u16 attr_len = attr_data_len + 3;

        if (0)
        {
            LOG_I("%s attr_ptr:%p, id:%u, length:%u\n", __func__, attr_ptr, attr_id, attr_data_len);
        }

        if ((attr_ptr - p2p_ie + attr_len) > p2p_ielen)
        {
            break;
        }

        if (attr_id == target_attr_id)
        {
            target_attr_ptr = attr_ptr;

            if (buf_attr)
            {
                wf_memcpy(buf_attr, attr_ptr, attr_len);
            }

            if (len_attr)
            {
                *len_attr = attr_len;
            }

            break;
        } else {
            attr_ptr += attr_len;
        }
    }

    return target_attr_ptr;
}

wf_u8 *wf_p2p_get_attr_content(wf_u8 * p2p_ie, wf_u32 p2p_ielen, wf_u8 target_attr_id, wf_u8 * buf_content, wf_u32 * len_content)
{
    wf_u8 *attr_ptr = NULL;
    wf_u32 attr_len = 0;

    if (len_content)
    {
        *len_content = 0;
    }

    attr_ptr = p2p_get_attr(p2p_ie, p2p_ielen, target_attr_id, NULL, &attr_len);

    if (attr_ptr && attr_len)
    {
        if (buf_content)
        {
            wf_memcpy(buf_content, attr_ptr + 3, attr_len - 3);
        }

        if (len_content)
        {
            *len_content = attr_len - 3;
        }

        return attr_ptr + 3;
    }

    return NULL;
}

wf_u32 p2p_set_ie(wf_u8 * pbuf, wf_u8 index, wf_u16 attr_len, wf_u8 * pdata_attr)
{
    wf_u32 a_len = 0;
    *pbuf = index;

    WF_PUT_LE16(pbuf + 1, attr_len);

    if (pdata_attr)
    {
        wf_memcpy(pbuf + 3, pdata_attr, attr_len);
    }

    a_len = attr_len + 3;

    return a_len;
}

wf_s32 wf_p2p_fill_assoc_rsp(nic_info_st *pnic_info, wf_u8 *pframe, wf_u16 *pkt_len,WF_P2P_IE_E pie_type)
{
    p2p_info_st *p2p_info = pnic_info->p2p;
    p2p_wd_info_st *pwdinfo = &p2p_info->wdinfo;

    if(WF_P2P_IE_MAX <= pie_type)
    {
        P2P_FRAME_WARN("unknown ie_type:%d",pie_type);
        return -1;
    }
    if (pwdinfo->driver_interface == DRIVER_CFG80211 && pwdinfo->role == P2P_ROLE_GO) 
    {
        if (p2p_info->p2p_ie[WF_P2P_IE_ASSOC_RSP] && p2p_info->p2p_ie_len[WF_P2P_IE_ASSOC_RSP] > 0)
        {
            P2P_FRAME_DBG(" %s:%d",wf_p2p_ie_to_str(WF_P2P_IE_ASSOC_RSP),p2p_info->p2p_ie_len[WF_P2P_IE_ASSOC_RSP]);
            wf_memcpy((void *)pframe, (void *)p2p_info->p2p_ie[WF_P2P_IE_ASSOC_RSP],p2p_info->p2p_ie_len[WF_P2P_IE_ASSOC_RSP]);
            pframe += p2p_info->p2p_ie_len[WF_P2P_IE_ASSOC_RSP];
            *pkt_len += p2p_info->p2p_ie_len[WF_P2P_IE_ASSOC_RSP];
            
        }
        else if (p2p_info->p2p_ie[pie_type] && p2p_info->p2p_ie_len[pie_type] > 0) 
        {
            P2P_FRAME_DBG("%s:%d",wf_p2p_ie_to_str(pie_type),p2p_info->p2p_ie_len[pie_type]);
            wf_memcpy((void *)pframe, (void *)p2p_info->p2p_ie[pie_type],p2p_info->p2p_ie_len[pie_type]);
            pframe += p2p_info->p2p_ie_len[pie_type];
            *pkt_len += p2p_info->p2p_ie_len[pie_type];
        }
    } 
 
    if(wf_p2p_wfd_is_valid(pnic_info))
    {
        wf_u32 wfdielen = 0;
        if(pwdinfo->role == P2P_ROLE_GO)
        {
            wfdielen = assoc_resp_wfd_ie_to_append_func(pnic_info, pframe, 1);
            pframe += wfdielen;
            *pkt_len += wfdielen;
        }
    }

    return 0;
}

wf_u8 * wf_p2p_fill_assoc_req(nic_info_st *pnic_info, wf_u8 *pframe, wf_u32 *pkt_len,WF_P2P_IE_E pie_type)
{

    p2p_info_st *p2p_info           = pnic_info->p2p;
    p2p_wd_info_st *pwdinfo = &p2p_info->wdinfo;

    P2P_FRAME_DBG("tart");
    if(WF_P2P_IE_MAX <= pie_type)
    {
        P2P_FRAME_WARN("unknown ie_type:%d",pie_type);
        return NULL;
    }

    if (p2p_info->p2p_enabled && pwdinfo->driver_interface == DRIVER_CFG80211) 
    {
        if (p2p_info->p2p_ie[pie_type] && p2p_info->p2p_ie_len[pie_type] > 0) 
        {
            P2P_FRAME_DBG("%s:%d",wf_p2p_ie_to_str(pie_type),p2p_info->p2p_ie_len[pie_type]);
            wf_memcpy((void *)pframe, (void *)p2p_info->p2p_ie[pie_type],p2p_info->p2p_ie_len[pie_type]);
            pframe += p2p_info->p2p_ie_len[pie_type];
            *pkt_len += p2p_info->p2p_ie_len[pie_type];
        }
    } 
    else
    {
        if (pwdinfo->p2p_state != P2P_STATE_NONE && pwdinfo->p2p_state != P2P_STATE_IDLE) 
        {
            wf_u8 p2pie[255] = { 0x00 };
            wf_u16 p2pielen = 0;

            p2pie[p2pielen++] = 0x50;
            p2pie[p2pielen++] = 0x6F;
            p2pie[p2pielen++] = 0x9A;
            p2pie[p2pielen++] = 0x09;

            p2pie[p2pielen++] = P2P_ATTR_CAPABILITY;

            *(wf_u16 *) (p2pie + p2pielen) = wf_cpu_to_le16(0x0002);
            p2pielen += 2;

            p2pie[p2pielen++] = DMP_P2P_DEVCAP_SUPPORT;

            if (pwdinfo->persistent_supported)
                p2pie[p2pielen++] =
                    P2P_GRPCAP_PERSISTENT_GROUP | DMP_P2P_GRPCAP_SUPPORT;
            else
                p2pie[p2pielen++] = DMP_P2P_GRPCAP_SUPPORT;

            p2pie[p2pielen++] = P2P_ATTR_EX_LISTEN_TIMING;

            *(wf_u16 *) (p2pie + p2pielen) = wf_cpu_to_le16(0x0004);
            p2pielen += 2;

            *(wf_u16 *) (p2pie + p2pielen) = wf_cpu_to_le16(0xFFFF);
            p2pielen += 2;

            *(wf_u16 *) (p2pie + p2pielen) = wf_cpu_to_le16(0xFFFF);
            p2pielen += 2;

            p2pie[p2pielen++] = P2P_ATTR_DEVICE_INFO;

            *(wf_u16 *) (p2pie + p2pielen) =
                wf_cpu_to_le16(21 + pwdinfo->device_name_len);
            p2pielen += 2;

            wf_memcpy(p2pie + p2pielen, nic_to_local_addr(pnic_info), WF_ETH_ALEN);
            p2pielen += WF_ETH_ALEN;

            if ((pwdinfo->ui_got_wps_info == P2P_GOT_WPSINFO_PEER_DISPLAY_PIN)
                || (pwdinfo->ui_got_wps_info ==
                    P2P_GOT_WPSINFO_SELF_DISPLAY_PIN)) 
            {
                *(wf_u16 *) (p2pie + p2pielen) =
                    wf_cpu_to_be16(WPS_CONFIG_METHOD_DISPLAY);
            } else {
                *(wf_u16 *) (p2pie + p2pielen) =
                    wf_cpu_to_be16(WPS_CONFIG_METHOD_PBC);
            }

            p2pielen += 2;

            *(wf_u16 *) (p2pie + p2pielen) = wf_cpu_to_be16(WPS_PDT_CID_MULIT_MEDIA);
            p2pielen += 2;

            *(wf_u32 *) (p2pie + p2pielen) = wf_cpu_to_be32(WPSOUI);
            p2pielen += 4;

            *(wf_u16 *) (p2pie + p2pielen) =
                wf_cpu_to_be16(WPS_PDT_SCID_MEDIA_SERVER);
            p2pielen += 2;

            p2pie[p2pielen++] = 0x00;

            *(wf_u16 *) (p2pie + p2pielen) = wf_cpu_to_be16(WPS_ATTR_DEVICE_NAME);
            p2pielen += 2;

            *(wf_u16 *) (p2pie + p2pielen) = wf_cpu_to_be16(pwdinfo->device_name_len);
            p2pielen += 2;

            wf_memcpy(p2pie + p2pielen, pwdinfo->device_name,pwdinfo->device_name_len);
            p2pielen += pwdinfo->device_name_len;

            p2pie[p2pielen++] = P2P_ATTR_INTERFACE;

            *(wf_u16 *) (p2pie + p2pielen) = wf_cpu_to_le16(0x000D);
            p2pielen += 2;

            wf_memcpy(p2pie + p2pielen, pwdinfo->device_addr, WF_ETH_ALEN);
            p2pielen += WF_ETH_ALEN;

            p2pie[p2pielen++] = 1;

            wf_memcpy(p2pie + p2pielen, pwdinfo->device_addr, WF_ETH_ALEN);
            p2pielen += WF_ETH_ALEN;

            pframe =set_ie(pframe, WF_80211_MGMT_EID_VENDOR_SPECIFIC, p2pielen,(unsigned char *)p2pie, pkt_len);
        }
    }


    if(wf_p2p_wfd_is_valid(pnic_info))
    {
        wf_u32 wfdielen = 0;
        
        wfdielen  = assoc_req_wfd_ie_to_append_func(pnic_info, pframe, 1);
        pframe   += wfdielen;
        *pkt_len += wfdielen;
    }

    return pframe;
}

wf_u32 p2p_set_attr_content(wf_u8 * pbuf, wf_u8 attr_id, wf_u16 attr_len,wf_u8 * pdata_attr, wf_u8 flag)
{
    wf_u32 a_len;

    *pbuf = attr_id;

    WF_PUT_LE16(pbuf + 1, attr_len);

    if (flag && pdata_attr) 
    {
        wf_memcpy(pbuf + 3, pdata_attr, attr_len);
    }
    a_len = attr_len + 3;

    return a_len;
}

static wf_s32 p2p_add_group_info_attr(nic_info_st *pnic_info, wf_u8 * pbuf, wf_u8 flag)
{
    wf_u32 len = 0;
    wf_u16 attr_len = 0;
    wf_u8 tmplen, *pdata_attr, *pstart, *pcur;
    wdn_net_info_st *pwdn = NULL;
    wf_s32 i = 0;
    p2p_info_st *p2p_info  = pnic_info->p2p;
    P2P_FRAME_DBG("start\n");

    pdata_attr = wf_kzalloc(WF_MAX_P2P_IE_LEN);

    if (NULL == pdata_attr) 
    {
        P2P_FRAME_DBG("pdata_attr malloc failed \n");
        return -1;
    }

    pstart = pdata_attr;
    pcur = pdata_attr;

    if(wf_true == p2p_info->p2p_enabled)
    for (i = 0; i < WDN_NUM_MAX; i++)
    {
        pwdn = wf_wdn_find_info_by_id(pnic_info, i);
        if(pwdn && 0 != wf_memcmp(nic_to_local_addr(pnic_info),pwdn->mac,WF_ETH_ALEN))
        {
            wf_memcpy(pcur, nic_to_local_addr(pnic_info), WF_ETH_ALEN);
            pcur += WF_ETH_ALEN;
            
            wf_memcpy(pcur, pwdn->mac, WF_ETH_ALEN);
            pcur += WF_ETH_ALEN;

            *pcur = pwdn->dev_cap;
            pcur++;

            WF_PUT_BE16(pcur, pwdn->config_methods);
            pcur += 2;

            wf_memcpy(pcur, pwdn->primary_dev_type, 8);
            pcur += 8;

            *pcur = pwdn->num_of_secdev_type;
            pcur++;

            wf_memcpy(pcur, pwdn->secdev_types_list,
                        pwdn->num_of_secdev_type * 8);
            pcur += pwdn->num_of_secdev_type * 8;

            if (pwdn->dev_name_len > 0) 
            {
                WF_PUT_BE16(pcur, WPS_ATTR_DEVICE_NAME);
                pcur += 2;

                WF_PUT_BE16(pcur, pwdn->dev_name_len);
                pcur += 2;

                wf_memcpy(pcur, pwdn->dev_name, pwdn->dev_name_len);
                pcur += pwdn->dev_name_len;
            }

            tmplen = (wf_u8) (pcur - pstart);

            *pstart = (tmplen - 1);

            attr_len += tmplen;

            pstart = pcur;
        }
    }

    if (attr_len > 0) 
    {
        len = p2p_set_attr_content(pbuf, P2P_ATTR_GROUP_INFO, attr_len,
                                     pdata_attr, 1);
    }

    if (flag) 
    {
        wf_kfree(pdata_attr);
    }

    return len;

}

wf_u32 p2p_build_probe_resp(nic_info_st *pnic_info, wf_u8 * pbuf,wf_u8 flag)
{
    wf_u8 p2pie[WF_MAX_P2P_IE_LEN]  = { 0x00 };
    wf_u32 len                      = 0;
    wf_u32 p2pielen                 = 0;
    p2p_info_st *p2p_info           = pnic_info->p2p;
    p2p_wd_info_st * pwdinfo = &p2p_info->wdinfo;

    P2P_FRAME_INFO("start");
    p2pielen = 0;
    p2pie[p2pielen++] = 0x50;
    p2pie[p2pielen++] = 0x6F;
    p2pie[p2pielen++] = 0x9A;
    p2pie[p2pielen++] = 0x09;

    p2pie[p2pielen++] = P2P_ATTR_CAPABILITY;

    WF_PUT_LE16(p2pie + p2pielen, 0x0002);
    p2pielen += 2;

    p2pie[p2pielen++] = DMP_P2P_DEVCAP_SUPPORT;

    if (pwdinfo->role == P2P_ROLE_GO) 
    {
        p2pie[p2pielen] = (P2P_GRPCAP_GO | P2P_GRPCAP_INTRABSS);
        if (pwdinfo->p2p_state == P2P_STATE_PROVISIONING_ING)
        {
            p2pie[p2pielen] |= P2P_GRPCAP_GROUP_FORMATION;
        }

        p2pielen++;
    } 
    else if (pwdinfo->role ==P2P_ROLE_DEVICE) 
    {
        if (pwdinfo->persistent_supported)
        {
            p2pie[p2pielen++] = P2P_GRPCAP_PERSISTENT_GROUP | DMP_P2P_GRPCAP_SUPPORT;
        }
        else
        {
            p2pie[p2pielen++] = DMP_P2P_GRPCAP_SUPPORT;
        }
    }

    p2pie[p2pielen++] = P2P_ATTR_EX_LISTEN_TIMING;

    WF_PUT_LE16(p2pie + p2pielen, 0x0004);
    p2pielen += 2;

    WF_PUT_LE16(p2pie + p2pielen, 0xFFFF);
    p2pielen += 2;

    WF_PUT_LE16(p2pie + p2pielen, 0xFFFF);
    p2pielen += 2;

    p2pie[p2pielen++] = P2P_ATTR_DEVICE_INFO;

    WF_PUT_LE16(p2pie + p2pielen, 21 + pwdinfo->device_name_len);
    p2pielen += 2;

    wf_memcpy(p2pie + p2pielen, pwdinfo->device_addr, WF_ETH_ALEN);
    p2pielen += WF_ETH_ALEN;

    WF_PUT_BE16(p2pie + p2pielen, pwdinfo->supported_wps_cm);
    p2pielen += 2;

    WF_PUT_BE16(p2pie + p2pielen, WPS_PDT_CID_MULIT_MEDIA);
    p2pielen += 2;

    WF_PUT_BE32(p2pie + p2pielen, WPSOUI);
    p2pielen += 4;

    WF_PUT_BE16(p2pie + p2pielen, WPS_PDT_SCID_MEDIA_SERVER);
    p2pielen += 2;

    p2pie[p2pielen++] = 0x00;

    WF_PUT_BE16(p2pie + p2pielen, WPS_ATTR_DEVICE_NAME);
    p2pielen += 2;

    WF_PUT_BE16(p2pie + p2pielen, pwdinfo->device_name_len);
    p2pielen += 2;

    if (flag)
    {
        wf_memcpy(p2pie + p2pielen, pwdinfo->device_name,pwdinfo->device_name_len);
        p2pielen += pwdinfo->device_name_len;
    }
    if (pwdinfo->role == P2P_ROLE_GO)
    {
        p2pielen += p2p_add_group_info_attr(pnic_info, p2pie + p2pielen, 1);
    }

    pbuf = set_ie(pbuf, WF_80211_MGMT_EID_VENDOR_SPECIFIC, p2pielen, (unsigned char *)p2pie, &len);

    return len;

}


wf_s32 p2p_provision_request_to_issue_func(nic_info_st *pnic_info, wf_u8 * pssid, wf_u8 ussidlen,wf_u8 * pdev_raddr, wf_u8 flag)
{

    P2P_FRAME_WARN("need to do");
    return 0;
}

wf_s32 p2p_GO_request_to_issue_func(nic_info_st *pnic_info, wf_u8 *raddr, wf_u8 flag)
{

    P2P_FRAME_WARN("need to do");
    return 0;
}

static void p2p_GO_response_to_issue_func(nic_info_st * pnic_info, wf_u8 * raddr, wf_u8 * frame_body,
                           wf_u32 len, wf_u8 result, wf_u8 flag)
{
    P2P_FRAME_WARN("need to do");
}

static void p2p_GO_confirm_to_issue_func(nic_info_st * pnic_info, wf_u8 * raddr, wf_u8 result, wf_u8 flag)
{
    
    P2P_FRAME_WARN("need to do");
}

wf_s32 p2p_invitation_request_to_issue_func(nic_info_st *pnic_info, wf_u8 * raddr, wf_u8 flag)
{

    P2P_FRAME_WARN("need to do");
    return 0;
}

static void p2p_invitation_response_to_issue_func(nic_info_st * pnic_info, wf_u8 * raddr, wf_u8 dialogToken, wf_u8 status_code, wf_u8 flag)
{

    P2P_FRAME_WARN("need to do");
}

static void p2p_presence_resp_issue_func(nic_info_st *pnic_info, wf_u8 * da, wf_u8 status, wf_u8 dialogToken, wf_u8 flag)
{

    P2P_FRAME_WARN("need to do");

}

wf_s32 wf_p2p_proc_presence_req(nic_info_st *pnic_info, wf_u8 * pframe,wf_s32 len, wf_u8 flag)
{
    wf_u8 *frame_body;
    wf_u8 dialogToken = 0;
    wf_u8 status = P2P_STATUS_SUCCESS;
    p2p_wd_info_st * pwdinfo = NULL;
    p2p_info_st *p2p_info = NULL;

    if(NULL == pnic_info)
    {
        P2P_FRAME_WARN("input param is null");
        return WF_RETURN_FAIL;
    }
    p2p_info = pnic_info->p2p;
    pwdinfo = &p2p_info->wdinfo;
    frame_body =(wf_u8 *)(pframe + sizeof(struct wl_ieee80211_hdr_3addr));

    dialogToken = frame_body[6];

    if (flag) 
    {
        p2p_presence_resp_issue_func(pnic_info, GetAddr2Ptr(pframe), status,dialogToken, 1);
    }
    return WF_RETURN_OK;
}

static void p2p_provision_resp_issue_func(nic_info_st *pnic_info,wf_u8 *raddr, wf_u8 * frame_body, wf_u16 config_method, wf_u8 flag)
{
    P2P_FRAME_DBG("need to do");
}

wf_u32 p2p_devdisc_req_process_func(nic_info_st *pnic_info, wf_u8 * pframe,wf_u32 len, wf_u8 flag)
{
    P2P_FRAME_DBG("need to do");
    return wf_true;
}

wf_bool p2p_devdisc_resp_process_func(nic_info_st *pnic_info, wf_u8 * pframe,wf_u32 len, wf_u8 flag)
{
    return wf_true;
}

wf_u8 p2p_provdisc_req_process_func(nic_info_st *pnic_info, wf_u8 * pframe,wf_u32 len, wf_u8 flag)
{
    wf_u8 *frame_body       = NULL;
    wf_u8 *wpsie            = NULL;
    wf_u32 wps_ielen        = 0;
    wf_u32 attr_contentlen  = 0;
    wf_u16 uconfig_method   = 0;
    p2p_info_st *p2p_info   = pnic_info->p2p;
    p2p_wd_info_st * pwdinfo = &p2p_info->wdinfo;
    
    frame_body = (pframe + sizeof(struct wl_ieee80211_hdr_3addr));

    wpsie = wf_wlan_get_wps_ie(frame_body + WF_PUBLIC_ACTION_IE_OFFSET ,
                                len - WF_PUBLIC_ACTION_IE_OFFSET, NULL, &wps_ielen);
    if (wpsie) 
    {
        if (wf_wlan_get_wps_attr_content(1, wpsie, wps_ielen, WPS_ATTR_CONF_METHOD, (wf_u8 *) & uconfig_method,
             &attr_contentlen)) 
        {
            uconfig_method = wf_be16_to_cpu(uconfig_method);
            switch (uconfig_method) 
            {
                case WPS_CM_DISPLYA:
                    {
                        wf_memcpy(pwdinfo->rx_prov_disc_info.strconfig_method_desc_of_prov_disc_req, "dis",3);
                        break;
                    }
                case WPS_CM_LABEL:
                    {
                        wf_memcpy(pwdinfo->rx_prov_disc_info.strconfig_method_desc_of_prov_disc_req, "lab",3);
                        break;
                    }
                case WPS_CM_PUSH_BUTTON:
                    {
                        wf_memcpy(pwdinfo->rx_prov_disc_info.strconfig_method_desc_of_prov_disc_req, "pbc",3);
                        break;
                    }
                case WPS_CM_KEYPAD:
                    {
                        wf_memcpy(pwdinfo->rx_prov_disc_info.strconfig_method_desc_of_prov_disc_req, "pad",3);
                        break;
                    }
            }
            if (flag) 
            {
                p2p_provision_resp_issue_func(pnic_info, GetAddr2Ptr(pframe),frame_body, uconfig_method, 1);
            }
        }
    }
    
    P2P_FRAME_INFO("config method = %s\n", pwdinfo->rx_prov_disc_info.strconfig_method_desc_of_prov_disc_req);
    
    return wf_true;
}

wf_bool p2p_provdisc_resp_process_func(nic_info_st *pnic_info, wf_u8 * pframe, wf_u8 flag)
{

    return wf_true;
}

wf_s32 p2p_issue_probereq(nic_info_st *pnic_info, wf_u8 * da, wf_s32 wait_ack, wf_u8 flag)
{
    tx_info_st *ptx_info        = (tx_info_st *)pnic_info->tx_info;
    struct xmit_buf *pxmit_buf  = NULL;
    wf_80211_mgmt_t *pframe     = NULL;
    wf_u32 var_len              = 0;
    wf_u8 *pvar                 = NULL;
    p2p_info_st *p2p_info       = pnic_info->p2p;
    p2p_wd_info_st *pwd = &p2p_info->wdinfo;
    wf_u32 wfdielen = 0;

    /* alloc xmit_buf */
    pxmit_buf = wf_xmit_extbuf_new(ptx_info);
    if (pxmit_buf == NULL)
    {
        LOG_W("pxmit_buf is NULL");
        return -1;
    }

    /* set frame head */
    wf_memset(pxmit_buf->pbuf, 0,TXDESC_OFFSET + WF_OFFSETOF(wf_80211_mgmt_t, probe_req));
    pframe = (void *)&pxmit_buf->pbuf[TXDESC_OFFSET];

    /* set control field */
    wf_80211_set_frame_type(&pframe->frame_control, WF_80211_STYPE_PROBE_REQ);

    /* set address field */
    if(da)
    {
        wf_memcpy((void*)pframe->da, (void*)da, sizeof(pframe->da));
        wf_memcpy(pframe->sa, nic_to_local_addr(pnic_info), sizeof(pframe->sa));
        wf_memcpy((void*)pframe->bssid, (void*)da, sizeof(pframe->bssid));
    }
    else
    {
        wf_memset(pframe->da, 0xff, sizeof(pframe->da));
        wf_memcpy(pframe->sa, nic_to_local_addr(pnic_info), sizeof(pframe->sa));
        wf_memset(pframe->bssid, 0xff, sizeof(pframe->bssid));
    }
//    SCAN_DBG("SA="WF_MAC_FMT, WF_MAC_ARG(pframe->da));
//    SCAN_DBG("DA="WF_MAC_FMT, WF_MAC_ARG(pframe->sa));
//    SCAN_DBG("BSSID="WF_MAC_FMT, WF_MAC_ARG(pframe->bssid));

    /* set variable field */
    var_len = 0;
    pvar = &pframe->probe_req.variable[0];
    /*1.SSID*/
    if(pwd->p2p_state == P2P_STATE_TX_PROVISION_DIS_REQ)
    {
        pvar = set_ie(pvar, PROBE_REQUEST_IE_SSID,
                      pwd->tx_prov_disc_info.ssidlen, pwd->tx_prov_disc_info.ssid,
                      &var_len);
    }
    else
    {
        //P2P_FRAME_DBG("p2p_wildcard_ssid:%s",pwd->p2p_wildcard_ssid);
        pvar = set_ie(pvar, PROBE_REQUEST_IE_SSID, P2P_WILDCARD_SSID_LEN, pwd->p2p_wildcard_ssid,&var_len);
    }
    /*2.Supported Rates and BSS Membership Selectors*/
    pvar = set_ie(pvar, PROBE_REQUEST_IE_RATE, 8, &pwd->p2p_support_rate[0], &var_len);

    /*4. wps ie and p2p ie*/
    {
        if(pwd->driver_interface == DRIVER_CFG80211)
        {
            mlme_info_t *mlme_info = pnic_info->mlme_info;
            if(mlme_info && mlme_info->probereq_wps_ie && mlme_info->wps_ie_len)
            {
                //P2P_FRAME_DBG("wps_ie_len:%d",mlme_info->wps_ie_len);
                wf_memcpy(pvar,mlme_info->probereq_wps_ie,mlme_info->wps_ie_len);
                var_len += mlme_info->wps_ie_len;
                pvar+=mlme_info->wps_ie_len;
            }

            if(p2p_info->p2p_ie[WF_P2P_IE_PROBE_REQ] && p2p_info->p2p_ie_len[WF_P2P_IE_PROBE_REQ])
            {
                //P2P_FRAME_DBG("p2p_probe_req_ie_len:%d",p2p_info->p2p_ie_len[WF_P2P_IE_PROBE_REQ]);
                wf_memcpy(pvar,p2p_info->p2p_ie[WF_P2P_IE_PROBE_REQ],p2p_info->p2p_ie_len[WF_P2P_IE_PROBE_REQ]);
                var_len += p2p_info->p2p_ie_len[WF_P2P_IE_PROBE_REQ];
                pvar+=p2p_info->p2p_ie_len[WF_P2P_IE_PROBE_REQ];
            }
        }
        else
        {

        }
    }
    
    if(wf_p2p_wfd_is_valid(pnic_info))
    {
        wfdielen = probe_req_wfd_ie_to_append_func(pnic_info, pvar, 1);
        pvar += wfdielen;
        var_len += wfdielen;
    }

    /* frame send */
    pxmit_buf->pkt_len = WF_OFFSETOF(wf_80211_mgmt_t, probe_req.variable) + var_len;
    if (wf_nic_mgmt_frame_xmit(pnic_info, NULL, pxmit_buf, pxmit_buf->pkt_len))
    {
        LOG_W("probe frame send fail");
        return -1;
    }
    return 0;
}

wf_s32 wf_p2p_issue_probereq(nic_info_st *nic_info, wf_u8 * da)
{
    return p2p_issue_probereq(nic_info, da, wf_false, 1);
}
/*
*p2p rx frame handle of probereq
*
*/
wf_s32 p2p_check_probereq(p2p_wd_info_st * pwdinfo, wf_u8 * probereq_ie, wf_u16 len, wf_u8 flag)
{
    wf_u8 *p;
    wf_s32 ret = -1;
    wf_u8 *p2pie;
    wf_u32 p2pielen = 0;
    wf_s32 ssid_len = 0, rate_cnt = 0;

    p = wf_wlan_get_ie(probereq_ie, WF_80211_MGMT_EID_SUPP_RATES, (wf_s32 *)&rate_cnt, len - WLAN_HDR_A3_LEN);

    if (rate_cnt <= 4) 
    {
        wf_s32 i, g_rate = 0;

        for (i = 0; i < rate_cnt; i++) 
        {
            if (((*(p + 2 + i) & 0xff) != 0x02) &&((*(p + 2 + i) & 0xff) != 0x04) &&
                ((*(p + 2 + i) & 0xff) != 0x0B) &&((*(p + 2 + i) & 0xff) != 0x16)) 
            {
                g_rate = 1;
            }
        }

        if (g_rate == 0) 
        {
            return ret;
        }
    } 

    p = wf_wlan_get_ie(probereq_ie , WF_80211_MGMT_EID_SSID,(wf_s32 *)&ssid_len, len - WLAN_HDR_A3_LEN);

    if (flag) 
    {
        ssid_len &= 0xff;
    }
    if ( (pwdinfo->role == P2P_ROLE_DEVICE)||(pwdinfo->role == P2P_ROLE_GO))
    {
        if ((p2pie = wf_p2p_get_ie(probereq_ie,len - WLAN_HDR_A3_LEN , NULL,&p2pielen)))
        {
            if ((p != NULL)&& (wf_memcmp((void *)(p + 2),(void *)pwdinfo->p2p_wildcard_ssid, 7) == 0))
            {
                ret = 0;
            } 
            else if ((p != NULL) && (ssid_len == 0)) 
            {
                ret = 0;
            }
        } 
        else 
        {
        }

    }

    return ret;

}



wf_s32 p2p_issue_probersp(nic_info_st *pnic_info, unsigned char *da, wf_u8 flag)
{
    tx_info_st *ptx_info        = (tx_info_st *)pnic_info->tx_info;
    struct xmit_buf *pxmit_buf  = NULL;
    wf_80211_mgmt_t *pframe     = NULL;
    wf_u32 var_len              = 0;
    wf_u8 *pvar                 = NULL;
    p2p_info_st *p2p_info       = pnic_info->p2p;
    p2p_wd_info_st *pwdinfo     = &p2p_info->wdinfo;
    mlme_info_t *pmlme_info     = (mlme_info_t *)pnic_info->mlme_info;
    wf_u16 beacon_interval      = 100;
    wf_u16 capInfo = 0;
    wf_u8 wpsie[255] = { 0x00 };
    wf_u32 wpsielen = 0, p2pielen = 0;
    wf_u32 wfdielen = 0;

    P2P_FRAME_DBG("start");
    
    /* aclloc xmit buf*/
    pxmit_buf = wf_xmit_extbuf_new(ptx_info);
    if(pxmit_buf == NULL)
    {
        P2P_FRAME_WARN("pxmit_buf aclloc failed ");
        return -1;
    }

    /*set frame head */
    wf_memset(pxmit_buf->pbuf, 0, TXDESC_OFFSET + WF_OFFSETOF(wf_80211_mgmt_t, probe_resp));
    pframe = (void *)&pxmit_buf->pbuf[TXDESC_OFFSET];

    /*set control field*/
    wf_80211_set_frame_type(&pframe->frame_control, WF_80211_STYPE_PROBE_RESP);
    pframe->frame_control = wf_cpu_to_le16(pframe->frame_control);

    /*set address*/
    wf_memcpy((void*)pframe->da, (void*)da, sizeof(pframe->da));
    wf_memcpy((void*)pframe->sa, nic_to_local_addr(pnic_info), sizeof(pframe->sa));
    wf_memcpy((void*)pframe->bssid, nic_to_local_addr(pnic_info), sizeof(pframe->bssid));

    /*set pies fiexd field */
    pframe->probe_resp.intv = beacon_interval;

    capInfo |= BIT(5);  //cap_ShortPremble
    capInfo |= BIT(10); //cap_ShortSlot
    pframe->probe_resp.capab = capInfo;

    /*set variable filed*/
    pvar = &pframe->probe_resp.variable[0];

    pvar = set_ie(pvar, WF_80211_MGMT_EID_SSID, 7, pwdinfo->p2p_wildcard_ssid,&var_len);

    pvar =set_ie(pvar, WF_80211_MGMT_EID_SUPP_RATES, 8,&pwdinfo->p2p_support_rate[0], &var_len);

    pvar =set_ie(pvar, WF_80211_MGMT_EID_DS_PARAMS, 1,(unsigned char *)&pwdinfo->listen_channel, &var_len);

    if (p2p_info->p2p_enabled && pwdinfo->driver_interface == DRIVER_CFG80211) 
    {
        if (pmlme_info->wps_probe_resp_ie != NULL && p2p_info->p2p_ie[WF_P2P_IE_PROBE_RSP] != NULL) 
        {
            P2P_FRAME_INFO("p2p probersp CONFIG_IOCTL_CFG80211 send ");
            wf_memcpy(pvar, pmlme_info->wps_probe_resp_ie,pmlme_info->wps_probe_resp_ie_len);
            var_len += pmlme_info->wps_probe_resp_ie_len;
            pvar += pmlme_info->wps_probe_resp_ie_len;

            wf_memcpy(pvar, p2p_info->p2p_ie[WF_P2P_IE_PROBE_RSP],p2p_info->p2p_ie_len[WF_P2P_IE_PROBE_RSP]);
            var_len += p2p_info->p2p_ie_len[WF_P2P_IE_PROBE_RSP];
            pvar += p2p_info->p2p_ie_len[WF_P2P_IE_PROBE_RSP];
        }
    } 
    else
    {

        P2P_FRAME_INFO("start");
        wpsielen = 0;
        *(wf_u32 *) (wpsie) = wf_cpu_to_be32(WPSOUI);
        wpsielen += 4;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(WPS_ATTR_VER1);
        wpsielen += 2;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(0x0001);
        wpsielen += 2;

        wpsie[wpsielen++] = WPS_VERSION_1;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(WPS_ATTR_SIMPLE_CONF_STATE);
        wpsielen += 2;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(0x0001);
        wpsielen += 2;

        wpsie[wpsielen++] = WPS_WSC_STATE_NOT_CONFIG;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(WPS_ATTR_RESP_TYPE);
        wpsielen += 2;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(0x0001);
        wpsielen += 2;

        wpsie[wpsielen++] = WPS_RESPONSE_TYPE_8021X;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(WPS_ATTR_UUID_E);
        wpsielen += 2;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(0x0010);
        wpsielen += 2;

        if (pwdinfo->external_uuid == 0) {
            wf_memset(wpsie + wpsielen, 0x0, 16);
            wf_memcpy(wpsie + wpsielen, nic_to_local_addr(pnic_info), WF_ETH_ALEN);
        } else {
            wf_memcpy(wpsie + wpsielen, pwdinfo->uuid, 0x10);
        }
        wpsielen += 0x10;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(WPS_ATTR_MANUFACTURER);
        wpsielen += 2;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(0x0007);
        wpsielen += 2;

        wf_memcpy(wpsie + wpsielen, "Wf_wlan", 7);
        wpsielen += 7;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(WPS_ATTR_MODEL_NAME);
        wpsielen += 2;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(0x0006);
        wpsielen += 2;

        wf_memcpy(wpsie + wpsielen, "9192CU", 6);
        wpsielen += 6;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(WPS_ATTR_MODEL_NUMBER);
        wpsielen += 2;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(0x0001);
        wpsielen += 2;

        wpsie[wpsielen++] = 0x31;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(WPS_ATTR_SERIAL_NUMBER);
        wpsielen += 2;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(WF_ETH_ALEN);
        wpsielen += 2;

        wf_memcpy(wpsie + wpsielen, "123456", WF_ETH_ALEN);
        wpsielen += WF_ETH_ALEN;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(WPS_ATTR_PRIMARY_DEV_TYPE);
        wpsielen += 2;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(0x0008);
        wpsielen += 2;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(WPS_PDT_CID_MULIT_MEDIA);
        wpsielen += 2;

        *(wf_u32 *) (wpsie + wpsielen) = wf_cpu_to_be32(WPSOUI);
        wpsielen += 4;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(WPS_PDT_SCID_MEDIA_SERVER);
        wpsielen += 2;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(WPS_ATTR_DEVICE_NAME);
        wpsielen += 2;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(pwdinfo->device_name_len);
        wpsielen += 2;

        wf_memcpy(wpsie + wpsielen, pwdinfo->device_name, pwdinfo->device_name_len);
        wpsielen += pwdinfo->device_name_len;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(WPS_ATTR_CONF_METHOD);
        wpsielen += 2;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(0x0002);
        wpsielen += 2;

        *(wf_u16 *) (wpsie + wpsielen) = wf_cpu_to_be16(pwdinfo->supported_wps_cm);
        wpsielen += 2;

        pvar = set_ie(pvar, WF_80211_MGMT_EID_VENDOR_SPECIFIC, wpsielen,(unsigned char *)wpsie, &var_len);

        p2pielen = p2p_build_probe_resp(pnic_info, pvar, 1);
        pvar += p2pielen;
        var_len += p2pielen;
    }


    if(wf_p2p_wfd_is_valid(pnic_info))
    {
        wfdielen = probe_resp_wfd_ie_to_append_func(pnic_info, pvar, 1);
        pvar += wfdielen;
        var_len += wfdielen;
    }

    /*frame send*/
    pxmit_buf->pkt_len = WF_OFFSETOF(wf_80211_mgmt_t, probe_resp.variable) + var_len;
    //P2P_FRAME_ARRAY(pframe,pxmit_buf->pkt_len);
//    P2P_FRAME_DBG("p2p probersp frame send  pkt_len=%d",pxmit_buf->pkt_len);

    if (wf_nic_mgmt_frame_xmit(pnic_info, NULL, pxmit_buf, pxmit_buf->pkt_len))
    {
        LOG_W("p2p probersp frame send fail");
        return -1;
    }

    return 0;

}


static wf_u32 p2p_proc_listen_state(nic_info_st *pnic_info, unsigned char *da, wf_u8 flag)
{

    p2p_info_st *p2p_info = pnic_info->p2p;
    p2p_wd_info_st *pwdinfo = &(p2p_info->wdinfo);
    
    wf_bool response = wf_true;

    if (pwdinfo->driver_interface == DRIVER_CFG80211) 
    {
       if (pwdinfo->is_ro_ch == wf_false || p2p_info->p2p_enabled == wf_false) 
       {
            response = wf_false;
       }
    } 
    else if (pwdinfo->driver_interface == DRIVER_WEXT) 
    {
        if (!pwdinfo->device_name_len) 
        {
            response = wf_false;
        }
    }
    
    P2P_FRAME_INFO("response=%d",response);

    if (flag && response == wf_true) 
    {
        p2p_issue_probersp(pnic_info, da, 1);
    }
    return 0;
}


static wf_u8 p2p_proc_probqeq(nic_info_st *pnic_info,wf_80211_mgmt_t *pframe, wf_u16 frame_len, wf_u8 flag)
{
    p2p_info_st *p2p_info = pnic_info->p2p;
    p2p_wd_info_st *pwdinfo = &(p2p_info->wdinfo);
    
    P2P_FRAME_DBG("p2p_role=%s",wf_p2p_role_to_str(pwdinfo->role ));
    if(P2P_ROLE_DEVICE == pwdinfo->role)
    {
        //if(pwdinfo->driver_interface == DRIVER_WEXT)
        //realize wext later
        //survey_event_to_report_func(nic_info, precv_frame, 1);

        if(flag)
        {
           p2p_proc_listen_state(pnic_info, pframe->sa, 1);
        }

        return 0;
    }
    else if (P2P_ROLE_GO == pwdinfo->role)
    {
        return -1;
    }
    
    return -1;
}


wf_s32 wf_p2p_proc_probereq(nic_info_st *pnic_info,wf_80211_mgmt_t *pframe, wf_u16 frame_len)
{
    wf_wlan_mgmt_info_t *pwlan_info = pnic_info->wlan_mgmt_info;
    p2p_info_st *p2p_info = pnic_info->p2p;
    p2p_wd_info_st *pwdinfo = &(p2p_info->wdinfo);
    wf_u8 *probereq_ie = pframe->probe_req.variable;
    wf_s8 is_valid_p2p_probereq = 0;
    
    if (!pnic_info->is_up)
    {
        return -1;
    }

    if (!(mac_addr_equal(pframe->da, pwlan_info->cur_network.mac_addr) || is_bcast_addr(pframe->da)))
    {
        P2P_FRAME_INFO("probe request target address invalid");
        return -3;
    }
    if((pwdinfo->driver_interface == DRIVER_CFG80211) && (pwdinfo->p2p_state != P2P_STATE_NONE)
        && wf_true == pwdinfo->report_mgmt)
    {
        if(wf_true == p2p_info->scb.init_flag &&p2p_info->scb.rx_mgmt)
        {
            P2P_FRAME_INFO(" da addr    : "WF_MAC_FMT,WF_MAC_ARG(pframe->da));
            P2P_FRAME_INFO(" sa addr    : "WF_MAC_FMT,WF_MAC_ARG(pframe->sa));
            P2P_FRAME_INFO(" bssid addr : "WF_MAC_FMT,WF_MAC_ARG(pframe->bssid));
            return p2p_info->scb.rx_mgmt(pnic_info,pframe,frame_len);
        }
    }

    is_valid_p2p_probereq = p2p_check_probereq(pwdinfo, probereq_ie, frame_len, 1);
    if(is_valid_p2p_probereq != 0)
    {
        return -4;
    }
    
    P2P_FRAME_INFO("%s,%s",wf_p2p_state_to_str(pwdinfo->p2p_state),wf_p2p_role_to_str(pwdinfo->role));
    if((pwdinfo->p2p_state != P2P_STATE_NONE) &&
       (pwdinfo->p2p_state != P2P_STATE_IDLE) &&
       (pwdinfo->role      != P2P_ROLE_CLIENT) &&
       (pwdinfo->p2p_state != P2P_STATE_SCAN) &&
       (pwdinfo->p2p_state != P2P_STATE_FIND_PHASE_SEARCH)
        )
    {
        if(p2p_proc_probqeq(pnic_info, pframe, frame_len, 1) < 0)
        {
            P2P_FRAME_INFO("p2p_proc_probqeq failed");
            return -4;
        }
    }

    return 0;
}

static wf_u8 *p2p_dump_attr_ch_list(wf_u8 * p2p_ie, wf_u32 p2p_ielen, wf_u8 * buf, wf_u32 buf_len,wf_u8 flag)
{
    wf_u32 attr_contentlen  = 0;
    wf_u8 *pattr            = NULL;
    wf_u8 ch_cnt            = 0;
    wf_u8 ch_list[40]       = {0};

    pattr =wf_p2p_get_attr_content(p2p_ie, p2p_ielen, P2P_ATTR_CH_LIST, NULL,&attr_contentlen);
    if (pattr) 
    {
        wf_s32 i, j;
        wf_u32 num_of_ch;
        wf_u8 *pattr_temp = pattr + 3;
        wf_u8 index = 0;
        
        attr_contentlen -= 3;
        if (flag) 
        {
            wf_memset(ch_list, 0, 40);
        }
        while (attr_contentlen > 0) 
        {
            num_of_ch = *(pattr_temp + 1);

            for (i = 0; i < num_of_ch; i++) 
            {
                for (j = 0; j < ch_cnt; j++)
                {
                    if (ch_list[j] == *(pattr_temp + 2 + i))
                    {
                        break;
                    }
                }
                if (j >= ch_cnt)
                {
                    ch_list[ch_cnt++] = *(pattr_temp + 2 + i);
                }

            }

            pattr_temp += (2 + num_of_ch);
            attr_contentlen -= (2 + num_of_ch);
        }

        
        for (j = 0; j < ch_cnt; j++) 
        {
            if(0 <= ch_list[j] && 9 >= ch_list[j])
            {
                buf[index++]= ch_list[j]+'0';
            }
            else if(10<=ch_list[j] && 99 >=ch_list[j])
            {
                wf_u8 ch_n1  = ch_list[j]/10;
                wf_u8 ch_n2  = ch_list[j]%10;
                buf[index++] = ch_n1+'0';
                buf[index++] = ch_n2+'0';
            }
            
            if(j!= ch_cnt-1)
            {
                buf[index++]=',';
            }
        }
    }
    return buf;
}

static wf_bool p2p_compare_nego_intent(wf_u8 req, wf_u8 resp, wf_u8 flag)
{
    if (flag) 
    {
        if (req >> 1 == resp >> 1)
        {      
            return req & 0x01 ? wf_true : wf_false;
        }
        else if (req >> 1 > resp >> 1)
        {      
            return wf_true;
        }
        else
        {      
            return wf_false;
        }
    } 
    
    return wf_false;
}

static wf_bool p2p_check_ch_list_with_buddy(nic_info_st *pnic_info,wf_u8 * frame_body, wf_u32 len, wf_u8 flag)
{
    wf_bool fit         = wf_false;
    wf_u8 *ies          = NULL;
    wf_u8 *p2p_ie       = NULL;
    wf_u32 ies_len      = 0;
    wf_u32 p2p_ielen    = 0;
    wf_u8 buddy_ch = wf_p2p_get_buddy_channel(pnic_info);

    ies = (wf_u8 *) (frame_body + WF_PUBLIC_ACTION_IE_OFFSET);
    ies_len = len - WF_PUBLIC_ACTION_IE_OFFSET;

    p2p_ie = wf_p2p_get_ie(ies, ies_len, NULL, &p2p_ielen);
    if(0 == flag)
    {
        return fit;
    }
    
    while (p2p_ie) 
    {
        wf_u32 attr_contentlen = 0;
        wf_u8 *pattr = NULL;
        pattr = wf_p2p_get_attr_content(p2p_ie, p2p_ielen, P2P_ATTR_CH_LIST,NULL, (wf_u32 *) & attr_contentlen);
        if (pattr) 
        {
            wf_s32 i;
            wf_u32 num_of_ch;
            wf_u8 *pattr_temp = pattr + 3;

            attr_contentlen -= 3;

            while (attr_contentlen > 0) 
            {
                num_of_ch = *(pattr_temp + 1);

                for (i = 0; i < num_of_ch; i++) 
                {
                    if (*(pattr_temp + 2 + i) == buddy_ch) 
                    {
                        P2P_FRAME_INFO(" ch_list fit buddy_ch:%u\n", buddy_ch);
                        fit = wf_true;
                        break;
                    }
                }

                pattr_temp += (2 + num_of_ch);
                attr_contentlen -= (2 + num_of_ch);
            }
        }

        p2p_ie = wf_p2p_get_ie(p2p_ie + p2p_ielen,ies_len - (p2p_ie - ies + p2p_ielen), NULL,&p2p_ielen);
    }
    
    return fit;
}

static void p2p_adjust_channel(nic_info_st *pnic_info, wf_u8 * frame_body, wf_u32 len, wf_u8 flag)
{
    wf_u8 *ies = NULL;
    wf_u8 *p2p_ie = NULL;
    wf_u32 ies_len, p2p_ielen;

    ies = (wf_u8 *) (frame_body + WF_PUBLIC_ACTION_IE_OFFSET);
    ies_len = len - WF_PUBLIC_ACTION_IE_OFFSET;

    p2p_ie = wf_p2p_get_ie(ies, ies_len, NULL, &p2p_ielen);

    if (0 == flag) 
    {
        return;
    }
    
    while (p2p_ie) 
    {
        wf_u32 attr_contentlen = 0;
        wf_u8 *pattr = NULL;
        
        pattr = wf_p2p_get_attr_content(p2p_ie, p2p_ielen, P2P_ATTR_CH_LIST,NULL,(wf_u32 *) & attr_contentlen);
        if (pattr) 
        {
            wf_s32 i;
            wf_u32 num_of_ch;
            wf_u8 *pattr_temp = pattr + 3;

            attr_contentlen -= 3;

            while (attr_contentlen > 0) 
           {
                num_of_ch = *(pattr_temp + 1);

                for (i = 0; i < num_of_ch; i++)
                {
                    *(pattr_temp + 2 + i) = wf_p2p_get_buddy_channel(pnic_info);
                }

                pattr_temp      += (2 + num_of_ch);
                attr_contentlen -= (2 + num_of_ch);
            }
        }

        attr_contentlen = 0;
        pattr = NULL;

        pattr = wf_p2p_get_attr_content(p2p_ie, p2p_ielen, P2P_ATTR_OPERATING_CH, NULL,(wf_u32 *) & attr_contentlen);
        if (pattr) 
        {
            *(pattr + 4) = wf_p2p_get_buddy_channel(pnic_info);
        }

        p2p_ie = wf_p2p_get_ie(p2p_ie + p2p_ielen, ies_len - (p2p_ie - ies + p2p_ielen), NULL, &p2p_ielen);

    }
    
}

static void p2p_change_p2pie_ch_list(nic_info_st *pnic_info, wf_u8 * frame_body,wf_u32 len, wf_u8 ch, wf_u8 flag)
{
    wf_u8 *ies, *p2p_ie;
    wf_u32 ies_len, p2p_ielen;
    ies = (wf_u8 *) (frame_body + WF_PUBLIC_ACTION_IE_OFFSET);
    ies_len = len - WF_PUBLIC_ACTION_IE_OFFSET;
    
    p2p_ie = wf_p2p_get_ie(ies, ies_len, NULL, &p2p_ielen);

    if (0 == flag) 
    {
        return;
    }
    
    while (p2p_ie) 
    {
        wf_u32 attr_contentlen = 0;
        wf_u8 *pattr = NULL;
        
        pattr = wf_p2p_get_attr_content(p2p_ie, p2p_ielen, P2P_ATTR_CH_LIST,NULL,(wf_u32 *) & attr_contentlen);
        if (pattr) 
        {
            int i;
            wf_u32 num_of_ch;
            wf_u8 *pattr_temp = pattr + 3;

            attr_contentlen -= 3;

            while (attr_contentlen > 0) 
            {
                num_of_ch = *(pattr_temp + 1);

                for (i = 0; i < num_of_ch; i++)
                    *(pattr_temp + 2 + i) = ch;

                pattr_temp += (2 + num_of_ch);
                attr_contentlen -= (2 + num_of_ch);
            }
        }

        p2p_ie = wf_p2p_get_ie(p2p_ie + p2p_ielen,ies_len - (p2p_ie - ies + p2p_ielen), NULL,&p2p_ielen);
    }
    
}

static wf_bool p2p_check_p2pie_op_ch_with_buddy(nic_info_st *pnic_info, wf_u8 * frame_body, wf_u32 len,wf_u8 flag)
{
    wf_bool fit = wf_false;
    wf_u8 *ies, *p2p_ie;
    wf_u32 ies_len, p2p_ielen;
    wf_u8 buddy_ch = wf_p2p_get_buddy_channel(pnic_info);

    ies = (wf_u8 *) (frame_body + WF_PUBLIC_ACTION_IE_OFFSET);
    ies_len = len - WF_PUBLIC_ACTION_IE_OFFSET;

    p2p_ie = wf_p2p_get_ie(ies, ies_len, NULL, &p2p_ielen);

    if (0 == flag)
    {
        return fit;
    }
    while (p2p_ie) 
    {
        wf_u32 attr_contentlen = 0;
        wf_u8 *pattr = NULL;

        attr_contentlen = 0;
        pattr = wf_p2p_get_attr_content(p2p_ie, p2p_ielen,P2P_ATTR_OPERATING_CH, NULL, (wf_u32 *) & attr_contentlen);
        if (pattr) 
        {
            if (*(pattr + 4) == buddy_ch) 
            {
                P2P_FRAME_INFO(" op_ch fit buddy_ch:%u\n", buddy_ch);
                fit = wf_true;
                break;
            }
        }

        p2p_ie = wf_p2p_get_ie(p2p_ie + p2p_ielen,ies_len - (p2p_ie - ies + p2p_ielen), NULL,&p2p_ielen);
    }
    

    return fit;
}

wf_s32 p2p_check_nego_req(nic_info_st *pnic_info, p2p_frame_check_param_st *check_param)
{
    wf_u8 *cont             = NULL;
    wf_u32 cont_len         = 0;
    wf_u8 ch_list_buf[128]  = { '\0' };
    wf_s32 op_ch            = 0;
    wf_s32 listen_ch        = 0;
    wf_u8 intent            = 0;
    p2p_info_st *p2p_info   = pnic_info->p2p;
    wf_widev_nego_info_t *nego_info = &p2p_info->wdinfo.nego_info;
    
    if (check_param->is_tx) 
    {
#ifdef CONFIG_DRV_ISSUE_PROV_REQ
        if (pwdev_priv->provdisc_req_issued == wf_false)
        {
            Func_Of_Proc_Cfg_Nl_Issue_P2P_Provision_Request(pnic_info, check_param->buf, check_param->len);
        }
#endif

        if (wf_p2p_check_buddy_linkstate(pnic_info) && p2p_info->full_ch_in_p2p_handshake == 0)
        {
            p2p_adjust_channel(pnic_info, check_param->frame_body,check_param->frame_body_len, 1);
        }
    }

    if ((cont =wf_p2p_get_attr_content(check_param->p2p_ie, check_param->p2p_ielen,P2P_ATTR_OPERATING_CH, NULL,&cont_len)))
    {
        op_ch = *(cont + 4);
    }
    if ((cont =wf_p2p_get_attr_content(check_param->p2p_ie, check_param->p2p_ielen, P2P_ATTR_LISTEN_CH, NULL,&cont_len)))
    {
        listen_ch = *(cont + 4);
    }
    if ((cont =wf_p2p_get_attr_content(check_param->p2p_ie, check_param->p2p_ielen,P2P_ATTR_GO_INTENT, NULL,&cont_len)))
    {
        intent = *cont;
    }

    if (nego_info->token != check_param->dialogToken)
    {
        
        wf_widev_nego_info_init(nego_info);
    }

    wf_memcpy(nego_info->peer_mac, check_param->is_tx ? GetAddr1Ptr(check_param->buf) : GetAddr2Ptr(check_param->buf), WF_ETH_ALEN);
    nego_info->active           = check_param->is_tx ? 1 : 0;
    nego_info->token            = check_param->dialogToken;
    nego_info->req_op_ch        = op_ch;
    nego_info->req_listen_ch    = listen_ch;
    nego_info->req_intent       = intent;
    nego_info->state            = 0;

    p2p_dump_attr_ch_list(check_param->p2p_ie, check_param->p2p_ielen, ch_list_buf, 128,1);
    
    P2P_FRAME_INFO("%s:P2P_GO_NEGO_REQ, dialogToken=%d, intent:%u%s, listen_ch:%d, op_ch:%d, ch_list:%s\n",
         (check_param->is_tx == wf_true) ? "Tx" : "Rx", check_param->dialogToken,
         (intent >> 1), intent & 0x1 ? "+" : "-", listen_ch,op_ch, ch_list_buf);

    if (!check_param->is_tx) 
    {
        if (wf_p2p_check_buddy_linkstate(pnic_info)
            && p2p_check_ch_list_with_buddy(pnic_info,
                                                check_param->frame_body,
                                                check_param->frame_body_len,
                                                1) == wf_false
            && p2p_info->full_ch_in_p2p_handshake == 0) 
        {
            P2P_FRAME_INFO(" ch_list has no intersect with buddy\n");
            p2p_change_p2pie_ch_list(pnic_info, check_param->frame_body,
                                     check_param->frame_body_len,
                                     0, 1);
        }
    }

    return 0;
}

wf_s32 p2p_check_nego_rsp(nic_info_st *pnic_info, p2p_frame_check_param_st *check_param)
{
    wf_u8 *cont             = NULL;
    wf_u32 cont_len         = 0;
    wf_s32 status           = -1;
    wf_u8 ch_list_buf[128]  = { '\0' };
    wf_s32 op_ch            = -1;
    wf_u8 intent            = 0;
    p2p_info_st *p2p_info   = pnic_info->p2p;
    wf_widev_nego_info_t *nego_info = &p2p_info->wdinfo.nego_info;
    
    if (check_param->is_tx) 
    {
        if (wf_p2p_check_buddy_linkstate(pnic_info)
            && p2p_info->full_ch_in_p2p_handshake == 0)
        {
            p2p_adjust_channel(pnic_info,
                                          check_param->frame_body,
                                          check_param->frame_body_len,
                                          1);
        }
    }

    if ((cont =wf_p2p_get_attr_content(check_param->p2p_ie, check_param->p2p_ielen,P2P_ATTR_OPERATING_CH, NULL,&cont_len)))
        op_ch = *(cont + 4);
    if ((cont =wf_p2p_get_attr_content(check_param->p2p_ie, check_param->p2p_ielen,P2P_ATTR_GO_INTENT, NULL,&cont_len)))
        intent = *cont;
    if ((cont =wf_p2p_get_attr_content(check_param->p2p_ie, check_param->p2p_ielen,P2P_ATTR_STATUS, NULL,&cont_len)))
        status = *cont;

    if (nego_info->token == check_param->dialogToken && nego_info->state == 0
        && wf_memcmp(nego_info->peer_mac,check_param->is_tx ? GetAddr1Ptr(check_param->buf) : GetAddr2Ptr(check_param->buf),WF_ETH_ALEN) == 0) 
    {
        nego_info->status = (status == -1) ? 0xff : status;
        nego_info->rsp_op_ch = op_ch;
        nego_info->rsp_intent = intent;
        nego_info->state = 1;
        if (status != 0)
        {
            nego_info->token = 0;
        }
    }

    p2p_dump_attr_ch_list(check_param->p2p_ie, check_param->p2p_ielen, ch_list_buf, 128,1);
    P2P_FRAME_INFO("WF_%s:P2P_GO_NEGO_RESP, dialogToken=%d, intent:%u%s, status:%d, op_ch:%d, ch_list:%s\n",
         (check_param->is_tx == wf_true) ? "Tx" : "Rx", check_param->dialogToken,
         (intent >> 1), intent & 0x1 ? "+" : "-", status, op_ch,
         ch_list_buf);

    if (!check_param->is_tx) 
    {
        p2p_info->wdinfo.provdisc_req_issued = wf_false;
        if (wf_p2p_check_buddy_linkstate(pnic_info)
            && p2p_check_ch_list_with_buddy(pnic_info,
                                                check_param->frame_body,
                                                check_param->frame_body_len,
                                                1) == wf_false
            && p2p_info->full_ch_in_p2p_handshake == 0) 
       {
            P2P_FRAME_INFO( " ch_list has no intersect with buddy\n");
            p2p_change_p2pie_ch_list(pnic_info, check_param->frame_body,
                                     check_param->frame_body_len,
                                     0, 1);
        }
    }

    return 0;
}


wf_s32 p2p_check_nego_confirm(nic_info_st *pnic_info, p2p_frame_check_param_st *check_param)
{
    wf_u8 *cont             = NULL;
    wf_u32 cont_len         = 0;
    wf_s32 status           = -1;
    wf_u8 ch_list_buf[128]  = { '\0' };
    wf_s32 op_ch            = -1;
    p2p_info_st *p2p_info   = pnic_info->p2p;
    wf_widev_nego_info_t *nego_info =&p2p_info->wdinfo.nego_info;
    wf_bool is_go           = wf_false;
    
    if (check_param->is_tx) 
    {
        if (wf_p2p_check_buddy_linkstate(pnic_info) && p2p_info->full_ch_in_p2p_handshake == 0)
            p2p_adjust_channel(pnic_info, check_param->frame_body,check_param->frame_body_len,1);

    }

    if ((cont = wf_p2p_get_attr_content(check_param->p2p_ie, check_param->p2p_ielen,P2P_ATTR_OPERATING_CH, NULL, &cont_len)))
    {
        op_ch = *(cont + 4);
    }
    if ((cont = wf_p2p_get_attr_content(check_param->p2p_ie, check_param->p2p_ielen,P2P_ATTR_STATUS, NULL,&cont_len)))
    {
        status = *cont;
    }

    if (nego_info->token == check_param->dialogToken && nego_info->state == 1 && 
        wf_memcmp(nego_info->peer_mac,check_param->is_tx ? GetAddr1Ptr(check_param->buf) : GetAddr2Ptr(check_param->buf),WF_ETH_ALEN) == 0) 
    {
        nego_info->status = (status == -1) ? 0xff : status;
        nego_info->conf_op_ch = (op_ch == -1) ? 0 : op_ch;
        nego_info->state = 2;

        if (status == 0) 
        {
            if (p2p_compare_nego_intent(nego_info->req_intent, nego_info->rsp_intent,1) ^ !check_param->is_tx)
            {
                is_go = wf_true;
            }
        }

        nego_info->token = 0;
    }

    p2p_dump_attr_ch_list(check_param->p2p_ie, check_param->p2p_ielen, ch_list_buf, 128, 1);
    P2P_FRAME_INFO("WF_%s:P2P_GO_NEGO_CONF, dialogToken=%d, status:%d, op_ch:%d, ch_list:%s\n",
         (check_param->is_tx == wf_true) ? "Tx" : "Rx", check_param->dialogToken, status,op_ch, ch_list_buf);

    if (!check_param->is_tx) 
    {
    }

    return 0;
}

wf_s32 p2p_check_invit_req(nic_info_st *pnic_info, p2p_frame_check_param_st *check_param)
{
    wf_u8 *cont             = NULL;
    wf_u32 cont_len         = 0;
    wf_u8 *frame_body       = NULL;
    wf_u8 ch_list_buf[128]  = { '\0' };
    wf_s32 op_ch            = -1;
    p2p_info_st *p2p_info   = pnic_info->p2p;
    struct wf_widev_invit_info *invit_info = &p2p_info->wdinfo.invit_info;
    wf_s32 flags = -1;
    
    if (check_param->is_tx) 
    {
        if (wf_p2p_check_buddy_linkstate(pnic_info)  && p2p_info->full_ch_in_p2p_handshake == 0)
            p2p_adjust_channel(pnic_info,check_param->frame_body,check_param->frame_body_len,1);
    }

    if ((cont = wf_p2p_get_attr_content(check_param->p2p_ie, check_param->p2p_ielen,P2P_ATTR_INVITATION_FLAGS,
                                  NULL, &cont_len)))
        flags = *cont;
    if ((cont = wf_p2p_get_attr_content(check_param->p2p_ie, check_param->p2p_ielen,
                                  P2P_ATTR_OPERATING_CH, NULL,&cont_len)))
        op_ch = *(cont + 4);

    if (invit_info->token != check_param->dialogToken)
    {
        wf_widev_invit_info_init(invit_info);
    }

    wf_memcpy(invit_info->peer_mac,check_param->is_tx ? GetAddr1Ptr(check_param->buf) : GetAddr2Ptr(check_param->buf),WF_ETH_ALEN);
    invit_info->active = check_param->is_tx ? 1 : 0;
    invit_info->token = check_param->dialogToken;
    invit_info->flags = (flags == -1) ? 0x0 : flags;
    invit_info->req_op_ch = op_ch;
    invit_info->state = 0;

    p2p_dump_attr_ch_list(check_param->p2p_ie, check_param->p2p_ielen, ch_list_buf, 128,
                          1);
    P2P_FRAME_INFO
        ("WF_%s:P2P_INVIT_REQ, dialogToken=%d, flags:0x%02x, op_ch:%d, ch_list:%s\n",
         (check_param->is_tx == wf_true) ? "Tx" : "Rx", check_param->dialogToken, flags, op_ch,
         ch_list_buf);

    if (!check_param->is_tx) {
        if (wf_p2p_check_buddy_linkstate(pnic_info) && p2p_info->full_ch_in_p2p_handshake == 0) 
        {
            if (op_ch != -1 && p2p_check_p2pie_op_ch_with_buddy(pnic_info,
                                                  check_param->frame_body,check_param->frame_body_len,
                                                  1) == wf_false) 
           {
                P2P_FRAME_INFO( " op_ch:%u has no intersect with buddy\n", op_ch);
                p2p_change_p2pie_ch_list(pnic_info, check_param->frame_body,
                                          check_param->frame_body_len,
                                         0, 1);
            } else if (p2p_check_ch_list_with_buddy
                    (pnic_info, check_param->frame_body,check_param->frame_body_len,1) == wf_false) 
           {
                P2P_FRAME_INFO(" ch_list has no intersect with buddy\n");
                p2p_change_p2pie_ch_list(pnic_info, frame_body,
                                         check_param->frame_body_len,
                                         0, 1);
            }
        }
    }

    return 0;
}

wf_s32 p2p_check_invit_rsp(nic_info_st *pnic_info, p2p_frame_check_param_st *check_param)
{
    wf_u8 *cont             = NULL;
    wf_u32 cont_len         = 0;
    wf_s32 status           = -1;
    wf_u8 ch_list_buf[128]  = { '\0' };
    wf_s32 op_ch            = -1;
    p2p_info_st *p2p_info   = pnic_info->p2p;
    wf_widev_invit_info_t *invit_info =&p2p_info->wdinfo.invit_info;
    
    if (check_param->is_tx) 
    {
        if (wf_p2p_check_buddy_linkstate(pnic_info)
            && p2p_info->full_ch_in_p2p_handshake == 0)
            p2p_adjust_channel(pnic_info,
                                              check_param->frame_body,
                                              check_param->frame_body_len,
                                              1);
    }

    if ((cont = wf_p2p_get_attr_content(check_param->p2p_ie, check_param->p2p_ielen,P2P_ATTR_STATUS, NULL,&cont_len))) 
    {
#ifdef CONFIG_P2P_INVITE_IOT
        if (check_param->is_tx && *cont == 7) 
       {
            P2P_FRAME_INFO("TX_P2P_INVITE_RESP, status is no common channel, change to unknown group\n");
            *cont = 8;
        }
#endif
        status = *cont;
    }
    if ((cont = wf_p2p_get_attr_content(check_param->p2p_ie, check_param->p2p_ielen,P2P_ATTR_OPERATING_CH, NULL,&cont_len)))
    {
        op_ch = *(cont + 4);
    }

    if (invit_info->token == check_param->dialogToken && invit_info->state == 0
        && wf_memcmp(invit_info->peer_mac,check_param->is_tx ? GetAddr1Ptr(check_param->buf) : GetAddr2Ptr(check_param->buf), WF_ETH_ALEN) == 0) 
    {
        invit_info->status = (status == -1) ? 0xff : status;
        invit_info->rsp_op_ch = op_ch;
        invit_info->state = 1;
        invit_info->token = 0;
    }

    p2p_dump_attr_ch_list(check_param->p2p_ie, check_param->p2p_ielen, ch_list_buf, 128,1);
    P2P_FRAME_INFO("WF_%s:P2P_INVIT_RESP, dialogToken=%d, status:%d, op_ch:%d, ch_list:%s\n",
         (check_param->is_tx == wf_true) ? "Tx" : "Rx", check_param->dialogToken, status,op_ch, ch_list_buf);

    if (!check_param->is_tx) 
    {
    }

    return 0;
}

wf_s32 wf_p2p_check_frames(nic_info_st *pnic_info, const wf_u8 * buf, wf_u32 len, wf_bool is_tx,wf_u8 flag)
{
    wf_s32 is_p2p_frame = (-1);
    wf_u8 *frame_body   = NULL;
    wf_u8 category;
    wf_u8 action;
    wf_u8 OUI_Subtype;
    wf_u8 dialogToken = 0;
    wf_u8 *p2p_ie = NULL;
    wf_u32 p2p_ielen = 0;
    p2p_frame_check_param_st check_param;
    p2p_info_st *p2p_info = NULL;

    if(NULL == pnic_info || NULL== buf)
    {
        LOG_E("[%s,%d] input param is null",__func__,__LINE__);
        return WF_RETURN_FAIL;
    }
    
    p2p_info = pnic_info->p2p;
    
    frame_body =(wf_u8 *)(buf + sizeof(struct wl_ieee80211_hdr_3addr));
    category = frame_body[0];
    if (category == WF_WLAN_CATEGORY_PUBLIC) 
    {
        P2P_FRAME_DBG("WF_WLAN_CATEGORY_PUBLIC");
        action = frame_body[1];
        if (action == WF_WLAN_ACTION_PUBLIC_VENDOR && wf_memcmp(frame_body + 2, P2P_OUI, 4) == 0) 
        {
            OUI_Subtype = frame_body[6];
            dialogToken = frame_body[7];
            is_p2p_frame = OUI_Subtype;
            P2P_FRAME_DBG("ACTION_CATEGORY_PUBLIC: ACT_PUBLIC_VENDOR, OUI=0x%x, OUI_Subtype=%d, dialogToken=%d\n",
                 wf_cpu_to_be32(*((wf_u32 *) (frame_body + 2))), OUI_Subtype, dialogToken);

            p2p_ie = wf_p2p_get_ie((wf_u8 *) buf +sizeof(struct wl_ieee80211_hdr_3addr) + WF_PUBLIC_ACTION_IE_OFFSET,
                                    len - sizeof(struct wl_ieee80211_hdr_3addr) - WF_PUBLIC_ACTION_IE_OFFSET, 
                                    NULL,&p2p_ielen);

            
            check_param.buf             = (wf_u8*)buf;
            check_param.len             = len;
            check_param.frame_body      = frame_body;
            check_param.frame_body_len  = len - sizeof(struct wl_ieee80211_hdr_3addr);
            check_param.p2p_ie          = p2p_ie;
            check_param.p2p_ielen       = p2p_ielen;
            check_param.is_tx           = is_tx;
            check_param.dialogToken     = dialogToken;
            if(p2p_ie)
            {
                wf_p2p_dump_attrs(p2p_ie,p2p_ielen);
            }
            
            switch (OUI_Subtype) 
            {
                
                case P2P_GO_NEGO_REQ:
                {
                    P2P_FRAME_DBG("P2P_GO_NEGO_REQ");
                    
                    p2p_check_nego_req(pnic_info,&check_param);
                    break;
                }
            case P2P_GO_NEGO_RESP:
                {
                    P2P_FRAME_DBG("P2P_GO_NEGO_RESP");
                    p2p_check_nego_rsp(pnic_info,&check_param);

                    break;
                }
            case P2P_GO_NEGO_CONF:
                {
                    P2P_FRAME_DBG("P2P_GO_NEGO_CONF");
                    p2p_check_nego_confirm(pnic_info,&check_param);

                    break;
                }
            case P2P_INVIT_REQ:
                {
                    P2P_FRAME_DBG("P2P_INVIT_REQ");
                    p2p_check_invit_req(pnic_info,&check_param);

                    break;
                }
            case P2P_INVIT_RESP:
                {
                    P2P_FRAME_DBG("P2P_INVIT_RESP");
                    p2p_check_invit_rsp(pnic_info,&check_param);
                    break;
                }
            case P2P_DEVDISC_REQ:
                P2P_FRAME_INFO("WF_%s:P2P_DEVDISC_REQ, dialogToken=%d\n",(is_tx == wf_true) ? "Tx" : "Rx", dialogToken);
                break;
            case P2P_DEVDISC_RESP:
            {
                wf_u8 *cont = NULL;
                wf_s32 cont_len = 0;
                cont = wf_p2p_get_attr_content(p2p_ie, p2p_ielen, P2P_ATTR_STATUS,NULL, &cont_len);
                P2P_FRAME_INFO("WF_%s:P2P_DEVDISC_RESP, dialogToken=%d, status:%d\n",(is_tx == wf_true) ? "Tx" : "Rx", dialogToken,cont ? *cont : -1);
                break;
            }
            case P2P_PROVISION_DISC_REQ:
                {
                    wf_u32 frame_body_len =len - sizeof(struct wl_ieee80211_hdr_3addr);
                    wf_u8 *p2p_ie;
                    wf_u32 p2p_ielen = 0;
                    wf_u32 contentlen = 0;

                    P2P_FRAME_INFO("WF_%s:P2P_PROVISION_DISC_REQ, dialogToken=%d\n",(is_tx == wf_true) ? "Tx" : "Rx", dialogToken);

                    p2p_info->wdinfo.provdisc_req_issued = wf_false;
                    if ((p2p_ie =wf_p2p_get_ie(frame_body + WF_PUBLIC_ACTION_IE_OFFSET,frame_body_len - WF_PUBLIC_ACTION_IE_OFFSET, NULL, &p2p_ielen))) 
                    {

                        if (wf_p2p_get_attr_content (p2p_ie, p2p_ielen, P2P_ATTR_GROUP_ID, NULL, &contentlen)) 
                        {
                            p2p_info->wdinfo.provdisc_req_issued = wf_false;
                        } 
                        else 
                        {
                            P2P_FRAME_INFO("provdisc_req_issued is wf_true\n");
                            p2p_info->wdinfo.provdisc_req_issued = wf_true;
                        }

                    }

                }
                break;
            case P2P_PROVISION_DISC_RESP:
                P2P_FRAME_INFO("WF_%s:P2P_PROVISION_DISC_RESP, dialogToken=%d\n",(is_tx == wf_true) ? "Tx" : "Rx", dialogToken);
                break;
            default:
                P2P_FRAME_INFO("WF_%s:OUI_Subtype=%d, dialogToken=%d\n",(is_tx == wf_true) ? "Tx" : "Rx", OUI_Subtype, dialogToken);
                break;
            }

        }

    } 
    else if (category == WF_WLAN_CATEGORY_P2P) 
    {
        OUI_Subtype = frame_body[5];
        dialogToken = frame_body[6];

        P2P_FRAME_DBG("ACTION_CATEGORY_P2P: OUI=0x%x, OUI_Subtype=%d, dialogToken=%d\n",
             wf_cpu_to_be32(*((wf_u32 *) (frame_body + 1))), OUI_Subtype, dialogToken);

        is_p2p_frame = OUI_Subtype;

        if (flag) 
        {
            switch (OUI_Subtype) 
            {
            case P2P_NOTICE_OF_ABSENCE:
                P2P_FRAME_INFO("WF_%s:P2P_NOTICE_OF_ABSENCE, dialogToken=%d\n",(is_tx == wf_true) ? "TX" : "RX", dialogToken);
                break;
            case P2P_PRESENCE_REQUEST:
                P2P_FRAME_INFO("WF_%s:P2P_PRESENCE_REQUEST, dialogToken=%d\n",(is_tx == wf_true) ? "TX" : "RX", dialogToken);
                break;
            case P2P_PRESENCE_RESPONSE:
                P2P_FRAME_INFO("WF_%s:P2P_PRESENCE_RESPONSE, dialogToken=%d\n",(is_tx == wf_true) ? "TX" : "RX", dialogToken);
                break;
            case P2P_GO_DISC_REQUEST:
                P2P_FRAME_INFO("WF_%s:P2P_GO_DISC_REQUEST, dialogToken=%d\n", (is_tx == wf_true) ? "TX" : "RX", dialogToken);
                break;
            default:
                P2P_FRAME_INFO("WF_%s:OUI_Subtype=%d, dialogToken=%d\n", (is_tx == wf_true) ? "TX" : "RX", OUI_Subtype, dialogToken);
                break;
            }
        }

    } 
    else 
    {
        is_p2p_frame = 0;
        P2P_FRAME_INFO("WF_%s:action frame category=%d\n",(is_tx == wf_true) ? "TX" : "RX", category);
    }

    return is_p2p_frame;
}

static wf_u8 p2p_get_peer_ch_list(p2p_wd_info_st * pwdinfo, wf_u8 * ch_content,wf_u8 ch_cnt, wf_u8 * peer_ch_list, wf_u8 flag)
{
    wf_u8 i = 0, j = 0;
    wf_u8 temp = 0;
    wf_u8 ch_no = 0;
    
    ch_content += 3;
    ch_cnt -= 3;

    if (0 == flag) 
    {
        return 0;
    }
    
    while (ch_cnt > 0) 
    {
        ch_content += 1;
        ch_cnt -= 1;
        temp = *ch_content;
        for (i = 0; i < temp; i++, j++) 
        {
            peer_ch_list[j] = *(ch_content + 1 + i);
        }
        ch_content += (temp + 1);
        ch_cnt -= (temp + 1);
        ch_no += temp;
    }
    
    return ch_no;
}

static wf_u8 inclusion_of_p2p_ch_func(hw_info_st *hw_info, wf_u8 * peer_ch_list,wf_u8 peer_ch_num, wf_u8 * ch_list_inclusioned, wf_u8 flag)
{
    wf_s32 i = 0, j = 0, temp = 0;
    wf_u8 ch_no = 0;

    if (flag) 
    {
        for (i = 0; i < peer_ch_num; i++) 
        {
            for (j = temp; j < hw_info->max_chan_nums; j++) 
            {
                if (*(peer_ch_list + i) == hw_info->channel_set[j].channel_num) 
                {
                    ch_list_inclusioned[ch_no++] = *(peer_ch_list + i);
                    temp = j;
                    break;
                }
            }
        }
    }

    return ch_no;
}


static int p2p_is_channel_list_ok(wf_u8 desired_ch, wf_u8 * ch_list, wf_u8 ch_cnt, wf_u8 flag)
{
    int found = 0, i = 0;

    if (flag) 
    {
        for (i = 0; i < ch_cnt; i++) 
        {
            if (ch_list[i] == desired_ch) 
            {
                found = 1;
                break;
            }
        }
    }
    return (found);
}

wf_u8 p2p_proc_group_negotation_req(nic_info_st *pnic_info,wf_u8 * pframe, wf_u32 len, wf_u8 flag)
{
    wf_u8 result = P2P_STATUS_SUCCESS;
    wf_u32 p2p_ielen = 0, wps_ielen = 0;
    wf_u8 *ies;
    wf_u32 ies_len;
    wf_u8 *p2p_ie;
    wf_u8 *wpsie;
    wf_u16 wps_devicepassword_id = 0x0000;
    wf_u32 wps_devicepassword_id_len = 0;
    p2p_info_st *p2p_info = pnic_info->p2p;
    p2p_wd_info_st * pwdinfo = &p2p_info->wdinfo;

    if ((wpsie = wf_wlan_get_wps_ie(pframe + WF_PUBLIC_ACTION_IE_OFFSET,len - WF_PUBLIC_ACTION_IE_OFFSET, NULL, &wps_ielen))) 
    {
        if (0 == wf_memcmp(pwdinfo->rx_prov_disc_info.strconfig_method_desc_of_prov_disc_req, "000", 3)) 
        {
            wf_wlan_get_wps_attr_content(1, wpsie, wps_ielen, WPS_ATTR_DEVICE_PWID,(wf_u8 *) & wps_devicepassword_id,&wps_devicepassword_id_len);
            wps_devicepassword_id = wf_be16_to_cpu(wps_devicepassword_id);

            if (wps_devicepassword_id == WPS_DPID_USER_SPEC) 
            {
                wf_memcpy(pwdinfo->rx_prov_disc_info.strconfig_method_desc_of_prov_disc_req, "dis", 3);
            } 
            else if (wps_devicepassword_id == WPS_DPID_REGISTRAR_SPEC) 
            {
                wf_memcpy(pwdinfo->rx_prov_disc_info.strconfig_method_desc_of_prov_disc_req, "pad", 3);
            } 
            else 
            {
                wf_memcpy(pwdinfo->rx_prov_disc_info.strconfig_method_desc_of_prov_disc_req, "pbc", 3);
            }
        }
    }
   else 
   {
        P2P_FRAME_INFO("WPS IE not Found!!\n");
        result = P2P_STATUS_FAIL_INCOMPATIBLE_PARAM;
        wf_p2p_set_state(pwdinfo, P2P_STATE_GONEGO_FAIL);
        return (result);
    }

    ies = pframe + WF_PUBLIC_ACTION_IE_OFFSET;
    ies_len = len - WF_PUBLIC_ACTION_IE_OFFSET;

    p2p_ie = wf_p2p_get_ie(ies, ies_len, NULL, &p2p_ielen);

    if (!p2p_ie) 
    {
        P2P_FRAME_INFO("P2P IE not Found!!\n");
        result = P2P_STATUS_FAIL_INCOMPATIBLE_PARAM;
        if (flag) 
        {
            wf_p2p_set_state(pwdinfo, P2P_STATE_GONEGO_FAIL);
        }
    }

    while (p2p_ie) 
    {
        wf_u8 attr_content = 0x00;
        wf_u32 attr_contentlen = 0;
        wf_u8 ch_content[100] = { 0x00 };
        wf_u32 ch_cnt = 0;
        wf_u8 peer_ch_list[100] = { 0x00 };
        wf_u8 peer_ch_num = 0;
        wf_u8 ch_list_inclusioned[100] = { 0x00 };
        wf_u8 ch_num_inclusioned = 0;
        wf_u16 cap_attr;
        wf_u8 listen_ch_attr[5] = { 0x00 };

        wf_p2p_set_state(pwdinfo, P2P_STATE_GONEGO_ING);

        if (wf_p2p_get_attr_content
            (p2p_ie, p2p_ielen, P2P_ATTR_CAPABILITY, (wf_u8 *) & cap_attr,
             (wf_u32 *) & attr_contentlen)) 
        {
            cap_attr = wf_le16_to_cpu(cap_attr);
        }

        if (wf_p2p_get_attr_content
            (p2p_ie, p2p_ielen, P2P_ATTR_GO_INTENT, &attr_content,
             &attr_contentlen)) {
            P2P_FRAME_INFO("GO Intent = %d, tie = %d\n", attr_content >> 1, attr_content & 0x01);
            pwdinfo->peer_intent = attr_content;

            if (pwdinfo->intent == (pwdinfo->peer_intent >> 1)) {
                if (pwdinfo->intent == P2P_MAX_INTENT) {
                    wf_p2p_set_role(pwdinfo, P2P_ROLE_DEVICE);
                    result = P2P_STATUS_FAIL_BOTH_GOINTENT_15;
                } else {
                    if (attr_content & 0x01) {
                        wf_p2p_set_role(pwdinfo, P2P_ROLE_CLIENT);
                    } else {
                        wf_p2p_set_role(pwdinfo, P2P_ROLE_GO);
                    }
                }
            } else if (pwdinfo->intent > (pwdinfo->peer_intent >> 1)) 
            {
                wf_p2p_set_role(pwdinfo, P2P_ROLE_GO);
            } 
            else 
            {
                wf_p2p_set_role(pwdinfo, P2P_ROLE_CLIENT);
            }

            if (pwdinfo->role == P2P_ROLE_GO) 
            {
                wf_memcpy(pwdinfo->groupid_info.go_device_addr,
                            pwdinfo->device_addr, WF_ETH_ALEN);
                wf_memcpy(pwdinfo->groupid_info.ssid, pwdinfo->nego_ssid,
                            pwdinfo->nego_ssidlen);
            }
        }

        if (wf_p2p_get_attr_content
            (p2p_ie, p2p_ielen, P2P_ATTR_LISTEN_CH, (wf_u8 *) listen_ch_attr,
             (wf_u32 *) & attr_contentlen) && attr_contentlen == 5)
            pwdinfo->nego_req_info.peer_ch = listen_ch_attr[4];

        P2P_FRAME_INFO(" listen channel :%u\n" ,pwdinfo->nego_req_info.peer_ch);

        attr_contentlen = 0;
        if (wf_p2p_get_attr_content
            (p2p_ie, p2p_ielen, P2P_ATTR_INTENTED_IF_ADDR,
             pwdinfo->p2p_peer_interface_addr, &attr_contentlen)) 
        {
            if (attr_contentlen != WF_ETH_ALEN) {
                wf_memset(pwdinfo->p2p_peer_interface_addr, 0x00, WF_ETH_ALEN);
            }
        }

        if (wf_p2p_get_attr_content
            (p2p_ie, p2p_ielen, P2P_ATTR_CH_LIST, ch_content, &ch_cnt)) 
        {
            peer_ch_num = p2p_get_peer_ch_list(pwdinfo, ch_content, ch_cnt,peer_ch_list, 1);
            ch_num_inclusioned =inclusion_of_p2p_ch_func(pnic_info->hw_info, peer_ch_list,peer_ch_num, ch_list_inclusioned, 1);

            if (ch_num_inclusioned == 0) 
            {
                P2P_FRAME_INFO("No common channel in channel list!\n");
                result = P2P_STATUS_FAIL_NO_COMMON_CH;
                wf_p2p_set_state(pwdinfo, P2P_STATE_GONEGO_FAIL);
                break;
            }

            if (pwdinfo->role == P2P_ROLE_GO) 
            {
                if (!p2p_is_channel_list_ok(pwdinfo->operating_channel,ch_list_inclusioned, ch_num_inclusioned, 1)) 
                {
                    if (wf_p2p_check_buddy_linkstate(pnic_info) && p2p_info->full_ch_in_p2p_handshake == 0) 
                    {
                        P2P_FRAME_INFO("desired channel NOT Found!\n");
                        result = P2P_STATUS_FAIL_NO_COMMON_CH;
                        wf_p2p_set_state(pwdinfo, P2P_STATE_GONEGO_FAIL);
                        break;
                    } 
                    else
                    {
                        wf_u8 operatingch_info[5] = { 0x00 };
                        wf_u8 peer_operating_ch = 0;
                        
                        attr_contentlen = 0;

                        if (wf_p2p_get_attr_content
                            (p2p_ie, p2p_ielen, P2P_ATTR_OPERATING_CH,
                             operatingch_info, &attr_contentlen)) {
                            peer_operating_ch = operatingch_info[4];
                        }

                        if (p2p_is_channel_list_ok(peer_operating_ch, ch_list_inclusioned, ch_num_inclusioned, 1)) 
                        {
                            pwdinfo->operating_channel = peer_operating_ch;
                            P2P_FRAME_INFO("Change op ch to %02x as peer's\n", pwdinfo->operating_channel);
                        } 
                        else 
                        {
                            pwdinfo->operating_channel = ch_list_inclusioned[0];
                            P2P_FRAME_INFO("Change op ch to %02x\n", pwdinfo->operating_channel);
                        }
                    }

                }
            }
        }

        p2p_ie = wf_p2p_get_ie(p2p_ie + p2p_ielen,
                           ies_len - (p2p_ie - ies + p2p_ielen), NULL,
                           &p2p_ielen);
    }

    if (pwdinfo->ui_got_wps_info == P2P_NO_WPSINFO) 
    {
        result = P2P_STATUS_FAIL_INFO_UNAVAILABLE;
        wf_p2p_set_state(pwdinfo, P2P_STATE_TX_INFOR_NOREADY);
        return result;
    }
    if(wf_p2p_wfd_is_valid(pnic_info))
    {
        //do_work_wfd_ies(pnic_info, __func__, pframe + WF_PUBLIC_ACTION_IE_OFFSET,len - WF_PUBLIC_ACTION_IE_OFFSET);
    }

    return (result);
}



wf_u8 p2p_proc_group_negotation_resp(nic_info_st *pnic_info,wf_u8 * pframe, wf_s32 len, wf_u8 flag)
{
    wf_u8 result = P2P_STATUS_SUCCESS;
    wf_u32 p2p_ielen, wps_ielen;
    wf_u8 *ies;
    wf_u32 ies_len;
    wf_u8 *p2p_ie;
    p2p_info_st *p2p_info = pnic_info->p2p;
    p2p_wd_info_st * pwdinfo = &p2p_info->wdinfo;

    ies = pframe + WF_PUBLIC_ACTION_IE_OFFSET;
    ies_len = len - WF_PUBLIC_ACTION_IE_OFFSET;

    if (wf_wlan_get_wps_ie(ies, ies_len, NULL, &wps_ielen)) 
    {

    }
    else 
    {
        P2P_FRAME_INFO("WPS IE not Found!!\n");
        result = P2P_STATUS_FAIL_INCOMPATIBLE_PARAM;
        if (flag) 
        {
            wf_p2p_set_state(pwdinfo, P2P_STATE_GONEGO_FAIL);
        }
    }

    p2p_ie = wf_p2p_get_ie(ies, ies_len, NULL, &p2p_ielen);
    if (!p2p_ie) {
        wf_p2p_set_role(pwdinfo, P2P_ROLE_DEVICE);
        wf_p2p_set_state(pwdinfo, P2P_STATE_GONEGO_FAIL);
        result = P2P_STATUS_FAIL_INCOMPATIBLE_PARAM;
    } else {

        wf_u8 attr_content = 0x00;
        wf_u32 attr_contentlen = 0;
        wf_u8 operatingch_info[5] = { 0x00 };
        //wf_u32 ch_cnt = 0;
        //wf_u8 ch_content[100] = { 0x00 };
        wf_u8 groupid[38];
        wf_u16 cap_attr;
        wf_u8 peer_ch_list[100] = { 0x00 };
        wf_u8 peer_ch_num = 0;
        wf_u8 ch_list_inclusioned[100] = { 0x00 };
        wf_u8 ch_num_inclusioned = 0;

        while (p2p_ie) {

            if (wf_p2p_get_attr_content
                (p2p_ie, p2p_ielen, P2P_ATTR_CAPABILITY, (wf_u8 *) & cap_attr,
                 (wf_u32 *) & attr_contentlen)) {
                cap_attr = wf_le16_to_cpu(cap_attr);
            }

            wf_p2p_get_attr_content(p2p_ie, p2p_ielen, P2P_ATTR_STATUS,
                                     &attr_content, &attr_contentlen);
            if (attr_contentlen == 1) 
            {
                P2P_FRAME_INFO("Status = %d\n", attr_content);
                if (attr_content == P2P_STATUS_SUCCESS) 
                {
                } 
                else 
                {
                    if (P2P_STATUS_FAIL_INFO_UNAVAILABLE == attr_content) 
                    {
                        wf_p2p_set_state(pwdinfo, P2P_STATE_RX_INFOR_NOREADY);
                    } 
                    else 
                    {
                        wf_p2p_set_state(pwdinfo, P2P_STATE_GONEGO_FAIL);
                    }
                    wf_p2p_set_role(pwdinfo, P2P_ROLE_DEVICE);
                    result = attr_content;
                    break;
                }
            }

            attr_contentlen = 0;
            if (wf_p2p_get_attr_content
                (p2p_ie, p2p_ielen, P2P_ATTR_INTENTED_IF_ADDR,
                 pwdinfo->p2p_peer_interface_addr, &attr_contentlen)) {
                if (attr_contentlen != WF_ETH_ALEN) {
                    wf_memset(pwdinfo->p2p_peer_interface_addr, 0x00, WF_ETH_ALEN);
                }
            }

            attr_content = 0x00;
            attr_contentlen = 0;
            if (wf_p2p_get_attr_content
                (p2p_ie, p2p_ielen, P2P_ATTR_GO_INTENT, &attr_content,
                 &attr_contentlen)) 
            {
                P2P_FRAME_INFO("GO Intent = %d, tie = %d\n", attr_content >> 1, attr_content & 0x01);
                pwdinfo->peer_intent = attr_content;

                if (pwdinfo->intent == (pwdinfo->peer_intent >> 1)) {
                    if (pwdinfo->intent == P2P_MAX_INTENT) {
                        wf_p2p_set_role(pwdinfo, P2P_ROLE_DEVICE);
                        result = P2P_STATUS_FAIL_BOTH_GOINTENT_15;
                        wf_p2p_set_state(pwdinfo, P2P_STATE_GONEGO_FAIL);
                    } else {
                        wf_p2p_set_state(pwdinfo, P2P_STATE_GONEGO_OK);
                        wf_p2p_set_pre_state(pwdinfo, P2P_STATE_GONEGO_OK);
                        if (attr_content & 0x01) {
                            wf_p2p_set_role(pwdinfo, P2P_ROLE_CLIENT);
                        } else {
                            wf_p2p_set_role(pwdinfo, P2P_ROLE_GO);
                        }
                    }
                } else if (pwdinfo->intent > (pwdinfo->peer_intent >> 1)) {
                    wf_p2p_set_state(pwdinfo, P2P_STATE_GONEGO_OK);
                    wf_p2p_set_pre_state(pwdinfo, P2P_STATE_GONEGO_OK);
                    wf_p2p_set_role(pwdinfo, P2P_ROLE_GO);
                } else {
                    wf_p2p_set_state(pwdinfo, P2P_STATE_GONEGO_OK);
                    wf_p2p_set_pre_state(pwdinfo, P2P_STATE_GONEGO_OK);
                    wf_p2p_set_role(pwdinfo, P2P_ROLE_CLIENT);
                }

                if (pwdinfo->role == P2P_ROLE_GO) 
                {
                    wf_memcpy(pwdinfo->groupid_info.go_device_addr,
                                pwdinfo->device_addr, WF_ETH_ALEN);
                    wf_memcpy(pwdinfo->groupid_info.ssid, pwdinfo->nego_ssid,
                                pwdinfo->nego_ssidlen);

                }
            }

            attr_contentlen = 0;
            if (wf_p2p_get_attr_content
                (p2p_ie, p2p_ielen, P2P_ATTR_OPERATING_CH, operatingch_info,
                 &attr_contentlen)) 
            {
                P2P_FRAME_INFO(" Peer's operating channel = %d\n",operatingch_info[4]);
                            pwdinfo->peer_operating_ch = operatingch_info[4];
            }

            if (wf_p2p_get_attr_content(p2p_ie, p2p_ielen, P2P_ATTR_CH_LIST,
                  pwdinfo->channel_list_attr, &pwdinfo->channel_list_attr_len)) 
            {
                P2P_FRAME_INFO("channel list attribute found, len = %d\n",pwdinfo->channel_list_attr_len);

                peer_ch_num = p2p_get_peer_ch_list(pwdinfo,
                                             pwdinfo->channel_list_attr,
                                             pwdinfo->channel_list_attr_len,
                                             peer_ch_list, 1);
                ch_num_inclusioned = inclusion_of_p2p_ch_func(pnic_info->hw_info, peer_ch_list,
                                         peer_ch_num, ch_list_inclusioned, 1);

                if (ch_num_inclusioned == 0) 
                {
                    P2P_FRAME_INFO("No common channel in channel list!\n");
                    result = P2P_STATUS_FAIL_NO_COMMON_CH;
                    wf_p2p_set_state(pwdinfo, P2P_STATE_GONEGO_FAIL);
                    break;
                }

                if (pwdinfo->role == P2P_ROLE_GO) 
                {
                    if (!p2p_is_channel_list_ok(pwdinfo->operating_channel,
                                                    ch_list_inclusioned,ch_num_inclusioned, 1)) 
                    {
                        if (wf_p2p_check_buddy_linkstate(pnic_info) && p2p_info->full_ch_in_p2p_handshake == 0) 
                        {
                            P2P_FRAME_INFO("desired channel NOT Found!\n");
                            result = P2P_STATUS_FAIL_NO_COMMON_CH;
                            wf_p2p_set_state(pwdinfo, P2P_STATE_GONEGO_FAIL);
                            break;
                        } 
                        else
                        {
                            wf_u8 operatingch_info[5] = { 0x00 };
                            wf_u8 peer_operating_ch = 0;
                            attr_contentlen = 0;

                            if (wf_p2p_get_attr_content
                                (p2p_ie, p2p_ielen, P2P_ATTR_OPERATING_CH,
                                 operatingch_info, &attr_contentlen)) {
                                peer_operating_ch = operatingch_info[4];
                            }

                            if (p2p_is_channel_list_ok(peer_operating_ch,
                                                           ch_list_inclusioned,
                                                           ch_num_inclusioned,
                                                           1)) 
                           {
                                pwdinfo->operating_channel = peer_operating_ch;
                                P2P_FRAME_INFO("Change op ch to %02x as peer's\n",pwdinfo->operating_channel);
                            }
                            else 
                            {
                                pwdinfo->operating_channel = ch_list_inclusioned[0];
                                P2P_FRAME_INFO("Change op ch to %02x\n",pwdinfo->operating_channel);
                            }
                        }

                    }
                }

            } 
            else 
            {
                P2P_FRAME_INFO("channel list attribute not found!\n");
            }

            attr_contentlen = 0;
            wf_memset(groupid, 0x00, 38);
            if (wf_p2p_get_attr_content
                (p2p_ie, p2p_ielen, P2P_ATTR_GROUP_ID, groupid,
                 &attr_contentlen)) {
                wf_memcpy(pwdinfo->groupid_info.go_device_addr, &groupid[0],
                            WF_ETH_ALEN);
                wf_memcpy(pwdinfo->groupid_info.ssid, &groupid[6],
                            attr_contentlen - WF_ETH_ALEN);
            }

            p2p_ie =
                wf_p2p_get_ie(p2p_ie + p2p_ielen,
                               ies_len - (p2p_ie - ies + p2p_ielen), NULL,
                               &p2p_ielen);
        }

    }

    if(wf_p2p_wfd_is_valid(pnic_info))
    {
        //do_work_wfd_ies(pnic_info, __func__, pframe + WF_PUBLIC_ACTION_IE_OFFSET,len - WF_PUBLIC_ACTION_IE_OFFSET);
    }

    return (result);

}

 
 wf_u8 p2p_proc_group_negotation_confirm(nic_info_st *pnic_info,wf_u8 * pframe, wf_u32 len, wf_u8 flag)
 {
     wf_u8 *ies;
     wf_u32 ies_len;
     wf_u8 *p2p_ie;
     wf_u32 p2p_ielen = 0;
     wf_u8 result = P2P_STATUS_SUCCESS;
     p2p_info_st *p2p_info = pnic_info->p2p;
     p2p_wd_info_st * pwdinfo = &p2p_info->wdinfo;
     
     ies = pframe + WF_PUBLIC_ACTION_IE_OFFSET;
     ies_len = len - WF_PUBLIC_ACTION_IE_OFFSET;
 
     p2p_ie = wf_p2p_get_ie(ies, ies_len, NULL, &p2p_ielen);
     if (flag) {
         while (p2p_ie) {
             wf_u8 attr_content = 0x00, operatingch_info[5] = { 0x00 };
             wf_u8 groupid[38] = { 0x00 };
             wf_u32 attr_contentlen = 0;
 
             pwdinfo->negotiation_dialog_token = 1;
             wf_p2p_get_attr_content(p2p_ie, p2p_ielen, P2P_ATTR_STATUS,
                                      &attr_content, &attr_contentlen);
             if (attr_contentlen == 1) 
             {
                 P2P_FRAME_INFO(" Status = %d\n", attr_content);
                 result = attr_content;
 
                 if (attr_content == P2P_STATUS_SUCCESS) 
                 {
                     wf_os_api_timer_unreg(&p2p_info->restore_p2p_state_timer);
 
                     wf_p2p_set_state(pwdinfo, P2P_STATE_GONEGO_OK);
                     wf_p2p_set_pre_state(pwdinfo, P2P_STATE_GONEGO_OK);
                     if ((pwdinfo->intent) > (pwdinfo->peer_intent >> 1)) {
                         wf_p2p_set_role(pwdinfo, P2P_ROLE_GO);
                     } else if ((pwdinfo->intent) < (pwdinfo->peer_intent >> 1)) {
                         wf_p2p_set_role(pwdinfo, P2P_ROLE_CLIENT);
                     } else {
                         if (pwdinfo->peer_intent & 0x01) {
                             wf_p2p_set_role(pwdinfo, P2P_ROLE_CLIENT);
                         } else {
                             wf_p2p_set_role(pwdinfo, P2P_ROLE_GO);
                         }
                     }
 
                     if (wf_p2p_check_buddy_linkstate(pnic_info) && p2p_info->full_ch_in_p2p_handshake == 0) 
                     {
                         wf_os_api_timer_set(&p2p_info->ap_p2p_switch_timer, 100);
                     }
                 } 
                 else 
                 {
                     wf_p2p_set_role(pwdinfo, P2P_ROLE_DEVICE);
                     wf_p2p_set_state(pwdinfo, P2P_STATE_GONEGO_FAIL);
                     break;
                 }
             }
 
             attr_contentlen = 0;
             wf_memset(groupid, 0x00, 38);
             if (wf_p2p_get_attr_content
                 (p2p_ie, p2p_ielen, P2P_ATTR_GROUP_ID, groupid,&attr_contentlen)) 
             {
                 P2P_FRAME_INFO("Ssid = %s, ssidlen = %zu\n", &groupid[WF_ETH_ALEN], strlen(&groupid[WF_ETH_ALEN]));
                 wf_memcpy(pwdinfo->groupid_info.go_device_addr, &groupid[0],WF_ETH_ALEN);
                 wf_memcpy(pwdinfo->groupid_info.ssid, &groupid[6],attr_contentlen - WF_ETH_ALEN);
             }
 
             attr_contentlen = 0;
             if (wf_p2p_get_attr_content(p2p_ie, p2p_ielen, P2P_ATTR_OPERATING_CH, operatingch_info,
                  &attr_contentlen)) 
             {
                 P2P_FRAME_INFO("Peer's operating channel = %d\n",operatingch_info[4]);
                 pwdinfo->peer_operating_ch = operatingch_info[4];
             }
 
             p2p_ie =wf_p2p_get_ie(p2p_ie + p2p_ielen,ies_len - (p2p_ie - ies + p2p_ielen), NULL,&p2p_ielen);
 
         }
     }
 
     return (result);
 }

 wf_u8 p2p_is_matched_in_profilelist(wf_u8 * peermacaddr,
                              struct profile_info * profileinfo, wf_u8 flag)
 {
     wf_u8 i, match_result = 0;
 
     LOG_I("[%s] peermac = %.2X %.2X %.2X %.2X %.2X %.2X\n", __FUNCTION__,
             peermacaddr[0], peermacaddr[1], peermacaddr[2], peermacaddr[3],
             peermacaddr[4], peermacaddr[5]);
 
     for (i = 0; i < P2P_MAX_PERSISTENT_GROUP_NUM; i++, profileinfo++) 
     {
         LOG_I("[%s] profileinfo_mac = %.2X %.2X %.2X %.2X %.2X %.2X\n",
                 __FUNCTION__, profileinfo->peermac[0], profileinfo->peermac[1],
                 profileinfo->peermac[2], profileinfo->peermac[3],
                 profileinfo->peermac[4], profileinfo->peermac[5]);
         if (0 == wf_memcmp(peermacaddr, profileinfo->peermac, WF_ETH_ALEN)) 
         {
             if (flag) 
             {
                 match_result = 1;
             }
             LOG_I("[%s] Match!\n", __FUNCTION__);
             break;
         }
     }
 
     return (match_result);
 }

wf_s32 wf_p2p_check_channel(wf_channel_info_t * ch_set, const wf_u32 ch)
{
    wf_s32 i=0;
    for (i = 0; ch_set[i].channel_num != 0; i++) 
    {
        if (ch == ch_set[i].channel_num)
        {
            return i;
        }
    }
    
    return -1;
}

 wf_s32 wf_p2p_proc_action_public(nic_info_st *pnic_info, wf_u8 *pframe, wf_u16 frame_len)
 {
     wf_u32 len = frame_len;
     wf_u8 *frame_body;
     wf_u8 *p2p_ie;
     wf_u32 p2p_ielen;
     //wf_u32 wps_ielen;
     p2p_info_st *p2p_info = pnic_info->p2p;
     p2p_wd_info_st *pwdinfo = &p2p_info->wdinfo;
     wf_u8 result = P2P_STATUS_SUCCESS;
     wf_u8 empty_addr[WF_ETH_ALEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
     wf_u8 *merged_p2pie = NULL;
     wf_u32 merged_p2p_ielen = 0;
 
     frame_body = (unsigned char *)(pframe + sizeof(struct wl_ieee80211_hdr_3addr));
 
     if(wf_p2p_is_valid(pnic_info))
     {
         wf_os_api_timer_unreg(&p2p_info->reset_ch_sitesurvey);
         if (p2p_info->p2p_enabled && pwdinfo->driver_interface == DRIVER_CFG80211) 
         {
            if(wf_true == p2p_info->scb.init_flag && p2p_info->scb.rx_mgmt)
            {
                P2P_FRAME_INFO("report p2p rx action frame");
                p2p_info->scb.rx_mgmt(pnic_info, pframe, len);
            }
         } 
         else
         {
             if (pwdinfo->p2p_state == P2P_STATE_NONE || pwdinfo->p2p_state == P2P_STATE_IDLE)
             {
                 return WF_RETURN_OK;
             }

             len -= sizeof(struct wl_ieee80211_hdr_3addr);
             
             switch (frame_body[6]) 
             {
             case P2P_GO_NEGO_REQ:
                 {
                     P2P_FRAME_INFO("Got GO Nego Req Frame\n");
                     wf_memset(&pwdinfo->groupid_info, 0x00,sizeof(struct group_id_info));
     
                     if (pwdinfo->p2p_state == P2P_STATE_RX_PROVISION_DIS_REQ) 
                     {
                         wf_p2p_set_state(pwdinfo, pwdinfo->pre_p2p_state);
                     }
     
                     if (pwdinfo->p2p_state == P2P_STATE_GONEGO_FAIL) 
                     {
                         wf_os_api_timer_unreg(&p2p_info->restore_p2p_state_timer);
                         wf_p2p_set_state(pwdinfo, pwdinfo->pre_p2p_state);
                         P2P_FRAME_INFO("Restore the previous p2p state to %d\n",pwdinfo->p2p_state);
                     }
                     if (wf_p2p_check_buddy_linkstate(pnic_info)) 
                     {
                         wf_os_api_timer_unreg(&p2p_info->ap_p2p_switch_timer);
                     }
     
                     if (pwdinfo->p2p_state !=P2P_STATE_GONEGO_ING)
                     {
                         wf_p2p_set_pre_state(pwdinfo, pwdinfo->p2p_state);
                     }
     
                     if (0 == wf_memcmp(pwdinfo->rx_prov_disc_info.peerDevAddr, empty_addr,WF_ETH_ALEN))
                     {
                        wf_memcpy(pwdinfo->rx_prov_disc_info.peerDevAddr,GetAddr2Ptr(pframe), WF_ETH_ALEN);
                     }
     
                     result = p2p_proc_group_negotation_req(pnic_info, frame_body, len,1);
                     p2p_GO_response_to_issue_func(pnic_info, GetAddr2Ptr(pframe), frame_body,len, result, 1);
     
                     if(pnic_info->buddy_nic)
                     {
                        wf_os_api_timer_set(&p2p_info->restore_p2p_state_timer, 3000);
                     }
                     else
                     {
                        wf_os_api_timer_set(&p2p_info->restore_p2p_state_timer, 5000);
                     }

                     break;
                 }
             case P2P_GO_NEGO_RESP:
                 {
                     P2P_FRAME_INFO("Got GO Nego Resp Frame\n");
     
                     if (pwdinfo->p2p_state == P2P_STATE_GONEGO_ING) 
                     {
                         wf_os_api_timer_unreg(&p2p_info->restore_p2p_state_timer);
                         pwdinfo->nego_req_info.benable = wf_false;
                         result =p2p_proc_group_negotation_resp(pnic_info, frame_body,len, 1);
                         p2p_GO_confirm_to_issue_func(pnic_info, GetAddr2Ptr(pframe),result, 1);
                         if (P2P_STATUS_SUCCESS == result) {
                             if (pwdinfo->role == P2P_ROLE_CLIENT) {
                                 pwdinfo->p2p_info.operation_ch[0] =
                                     pwdinfo->peer_operating_ch;
#ifdef CONFIG_P2P_OP_CHK_SOCIAL_CH
                                 pwdinfo->p2p_info.operation_ch[1] = 1;
                                 pwdinfo->p2p_info.operation_ch[2] = 6;
                                 pwdinfo->p2p_info.operation_ch[3] = 11;
#endif
                                 pwdinfo->p2p_info.scan_op_ch_only = 1;
                                 wf_os_api_timer_set(&p2p_info->reset_ch_sitesurvey2, P2P_RESET_SCAN_CH);
                             }
                         }
     
                         pwdinfo->negotiation_dialog_token = 1;
     
                         if (pwdinfo->p2p_state == P2P_STATE_GONEGO_FAIL) {
                             wf_os_api_timer_set(&p2p_info->restore_p2p_state_timer, 5000);
                         }
                     } 
                     else 
                     {
                         P2P_FRAME_INFO("Skipped GO Nego Resp Frame (p2p_state != P2P_STATE_GONEGO_ING)\n");
                     }
     
                     break;
                 }
             case P2P_GO_NEGO_CONF:
                 {
                     P2P_FRAME_INFO("Got GO Nego Confirm Frame\n");
                     result =
                         p2p_proc_group_negotation_confirm(pnic_info, frame_body,
                                                              len, 1);
                     if (P2P_STATUS_SUCCESS == result) {
                         if (pwdinfo->role == P2P_ROLE_CLIENT) {
                             pwdinfo->p2p_info.operation_ch[0] =
                                 pwdinfo->peer_operating_ch;
#ifdef CONFIG_P2P_OP_CHK_SOCIAL_CH
                             pwdinfo->p2p_info.operation_ch[1] = 1;
                             pwdinfo->p2p_info.operation_ch[2] = 6;
                             pwdinfo->p2p_info.operation_ch[3] = 11;
#endif
                             pwdinfo->p2p_info.scan_op_ch_only = 1;
                             wf_os_api_timer_set(&p2p_info->reset_ch_sitesurvey2, P2P_RESET_SCAN_CH);
                         }
                     }
                     break;
                 }
             case P2P_INVIT_REQ:
                 {
     
                     P2P_FRAME_INFO("Got invite request frame!\n");
                     if ((p2p_ie =wf_p2p_get_ie(frame_body + WF_PUBLIC_ACTION_IE_OFFSET,
                                         len - WF_PUBLIC_ACTION_IE_OFFSET, NULL,&p2p_ielen))) 
                     {
                         wf_u32 attr_contentlen = 0;
                         wf_u8 status_code = P2P_STATUS_FAIL_INFO_UNAVAILABLE;
                         struct group_id_info group_id;
                         wf_u8 invitation_flag = 0;
                         //wf_s32 j = 0;
     
                         merged_p2p_ielen = p2p_merged_ies_len_tp_get_func(frame_body +
                                                        WF_PUBLIC_ACTION_IE_OFFSET,
                                                        len -
                                                        WF_PUBLIC_ACTION_IE_OFFSET,
                                                        1);
     
                         merged_p2pie = wf_kzalloc(merged_p2p_ielen + 2);
                         if (merged_p2pie == NULL) 
                         {
                             P2P_FRAME_INFO("wf_kzalloc p2p ie fail\n");
                             goto exit;
                         }
                         wf_memset(merged_p2pie, 0x00, merged_p2p_ielen);
     
                         merged_p2p_ielen =proc_p2p_merge_ies_func(frame_body +
                                               WF_PUBLIC_ACTION_IE_OFFSET,
                                               len - WF_PUBLIC_ACTION_IE_OFFSET,
                                               merged_p2pie, 1);
     
                         wf_p2p_get_attr_content(merged_p2pie, merged_p2p_ielen,
                                                  P2P_ATTR_INVITATION_FLAGS,
                                                  &invitation_flag,
                                                  &attr_contentlen);
                         if (attr_contentlen) 
                         {
                             wf_p2p_get_attr_content(merged_p2pie, merged_p2p_ielen,
                                                      P2P_ATTR_GROUP_BSSID,
                                                      pwdinfo->p2p_peer_interface_addr,
                                                      &attr_contentlen);
     
                             if (attr_contentlen) 
                             {
                                 P2P_FRAME_INFO("GO's BSSID = %.2X %.2X %.2X %.2X %.2X %.2X\n",
                                      pwdinfo->p2p_peer_interface_addr[0],
                                      pwdinfo->p2p_peer_interface_addr[1],
                                      pwdinfo->p2p_peer_interface_addr[2],
                                      pwdinfo->p2p_peer_interface_addr[3],
                                      pwdinfo->p2p_peer_interface_addr[4],
                                      pwdinfo->p2p_peer_interface_addr[5]);
                             }
     
                             if (invitation_flag & P2P_INVITATION_FLAGS_PERSISTENT) {
     
                                 wf_memset(&group_id, 0x00,sizeof(struct group_id_info));
                                 wf_p2p_get_attr_content(merged_p2pie,
                                                          merged_p2p_ielen,
                                                          P2P_ATTR_GROUP_ID,
                                                          (wf_u8 *) & group_id,
                                                          &attr_contentlen);
                                 if (attr_contentlen) {
                                     if (0 == wf_memcmp(group_id.go_device_addr,nic_to_local_addr(pnic_info), WF_ETH_ALEN)) 
                                     {
                                         wf_p2p_set_state(pwdinfo,P2P_STATE_RECV_INVITE_REQ_GO);
                                         wf_p2p_set_role(pwdinfo, P2P_ROLE_GO);
                                         status_code = P2P_STATUS_SUCCESS;
                                     } 
                                     else 
                                     {
                                         if (p2p_is_matched_in_profilelist(pwdinfo->p2p_peer_interface_addr,&pwdinfo->profileinfo[0], 1))
                                            {
                                             wf_u8 operatingch_info[5] = { 0x00 };
                                             if (wf_p2p_get_attr_content(merged_p2pie, merged_p2p_ielen,P2P_ATTR_OPERATING_CH,operatingch_info,&attr_contentlen)) 
                                             {
                                                hw_info_st *hw_info = pnic_info->hw_info;
                                                 if (wf_p2p_check_channel(hw_info->channel_set,(wf_u32) operatingch_info[4]) >= 0)
                                                 {
                                                     pwdinfo->rx_invitereq_info.operation_ch[0] = operatingch_info[4];
#ifdef CONFIG_P2P_OP_CHK_SOCIAL_CH
                                                     pwdinfo->rx_invitereq_info.operation_ch[1] = 1;
                                                     pwdinfo->rx_invitereq_info.operation_ch[2] = 6;
                                                     pwdinfo->rx_invitereq_info.operation_ch[3] = 11;
#endif
                                                     pwdinfo->rx_invitereq_info.scan_op_ch_only = 1;
                                                     wf_os_api_timer_set(&p2p_info->reset_ch_sitesurvey,P2P_RESET_SCAN_CH);
                                                     wf_p2p_set_state(pwdinfo,P2P_STATE_RECV_INVITE_REQ_MATCH);
                                                     wf_p2p_set_role(pwdinfo,P2P_ROLE_CLIENT);
                                                     status_code =P2P_STATUS_SUCCESS;
                                                 } 
                                                 else 
                                                 {
                                                     wf_p2p_set_state(pwdinfo,P2P_STATE_RECV_INVITE_REQ_DISMATCH);
                                                     wf_p2p_set_role(pwdinfo,P2P_ROLE_DEVICE);
                                                     status_code =P2P_STATUS_FAIL_NO_COMMON_CH;
                                                     wf_os_api_timer_set(&p2p_info->restore_p2p_state_timer,3000);
                                                 }
                                             } 
                                             else 
                                             {
                                                 wf_p2p_set_state(pwdinfo,P2P_STATE_RECV_INVITE_REQ_MATCH);
                                                 wf_p2p_set_role(pwdinfo,P2P_ROLE_CLIENT);
                                                 status_code = P2P_STATUS_SUCCESS;
                                             }
                                         } 
                                         else 
                                         {
                                             wf_p2p_set_state(pwdinfo,P2P_STATE_RECV_INVITE_REQ_DISMATCH);
     
                                             status_code = P2P_STATUS_FAIL_UNKNOWN_P2PGROUP;
                                         }
                                     }
                                 } 
                                 else 
                                 {
                                     P2P_FRAME_INFO("P2P Group ID Attribute NOT FOUND!\n");
                                     status_code = P2P_STATUS_FAIL_INFO_UNAVAILABLE;
                                 }
                             } 
                             else 
                             {
                                 wf_memset(&group_id, 0x00, sizeof(struct group_id_info));
                                 wf_p2p_get_attr_content(merged_p2pie,
                                                          merged_p2p_ielen,
                                                          P2P_ATTR_GROUP_ID,
                                                          (wf_u8 *) & group_id,
                                                          &attr_contentlen);
                                 if (attr_contentlen) 
                                 {
                                     if (0 == wf_memcmp(group_id.go_device_addr,nic_to_local_addr(pnic_info), WF_ETH_ALEN)) 
                                     {
                                         wf_p2p_set_state(pwdinfo,
                                                           P2P_STATE_RECV_INVITE_REQ_DISMATCH);
                                         status_code =
                                             P2P_STATUS_FAIL_INFO_UNAVAILABLE;
                                     } 
                                     else 
                                     {
                                         wf_memcpy(pwdinfo->p2p_peer_device_addr,
                                                     group_id.go_device_addr,
                                                     WF_ETH_ALEN);
                                         wf_p2p_set_role(pwdinfo, P2P_ROLE_CLIENT);
                                         wf_p2p_set_state(pwdinfo,
                                                           P2P_STATE_RECV_INVITE_REQ_JOIN);
                                         status_code = P2P_STATUS_SUCCESS;
                                     }
                                 } 
                                 else 
                                 {
                                     P2P_FRAME_INFO("P2P Group ID Attribute NOT FOUND!\n");
                                     status_code = P2P_STATUS_FAIL_INFO_UNAVAILABLE;
                                 }
                             }
                         } 
                         else 
                         {
                             P2P_FRAME_INFO("P2P Invitation Flags Attribute NOT FOUND!\n");
                             status_code = P2P_STATUS_FAIL_INFO_UNAVAILABLE;
                         }
     
                         P2P_FRAME_INFO("status_code = %d\n",status_code);
     
                         pwdinfo->inviteresp_info.token = frame_body[7];
                         p2p_invitation_response_to_issue_func(pnic_info, GetAddr2Ptr(pframe),pwdinfo->inviteresp_info.token, status_code, 1);
                         wf_os_api_timer_set(&p2p_info->restore_p2p_state_timer, 3000);
                     }
                     break;
                 }
             case P2P_INVIT_RESP:
                 {
                     wf_u8 attr_content = 0x00;
                     wf_u32 attr_contentlen = 0;
     
                     P2P_FRAME_INFO(" Got invite response frame!\n");
                     wf_os_api_timer_unreg(&p2p_info->restore_p2p_state_timer);
                     if ((p2p_ie = wf_p2p_get_ie(frame_body + WF_PUBLIC_ACTION_IE_OFFSET,len - WF_PUBLIC_ACTION_IE_OFFSET, NULL, &p2p_ielen))) 
                     {
                         wf_p2p_get_attr_content(p2p_ie, p2p_ielen, P2P_ATTR_STATUS,&attr_content, &attr_contentlen);
                         if (attr_contentlen == 1) 
                         {
                             P2P_FRAME_INFO("Status = %d\n",attr_content);
                             pwdinfo->invitereq_info.benable = wf_false;
     
                             if (attr_content == P2P_STATUS_SUCCESS) 
                             {
                                 if (0 == wf_memcmp(pwdinfo->invitereq_info.go_bssid, nic_to_local_addr(pnic_info), WF_ETH_ALEN))
                                     wf_p2p_set_role(pwdinfo, P2P_ROLE_GO);
                                 else
                                     wf_p2p_set_role(pwdinfo, P2P_ROLE_CLIENT);
     
                                 wf_p2p_set_state(pwdinfo,
                                                   P2P_STATE_RX_INVITE_RESP_OK);
                             } 
                             else 
                             {
                                 wf_p2p_set_role(pwdinfo, P2P_ROLE_DEVICE);
                                 wf_p2p_set_state(pwdinfo,
                                                   P2P_STATE_RX_INVITE_RESP_FAIL);
                             }
                         } 
                         else 
                         {
                             wf_p2p_set_role(pwdinfo, P2P_ROLE_DEVICE);
                             wf_p2p_set_state(pwdinfo,
                                               P2P_STATE_RX_INVITE_RESP_FAIL);
                         }
                     } 
                     else 
                     {
                         wf_p2p_set_role(pwdinfo, P2P_ROLE_DEVICE);
                         wf_p2p_set_state(pwdinfo, P2P_STATE_RX_INVITE_RESP_FAIL);
                     }
     
                     if (pwdinfo->p2p_state == P2P_STATE_RX_INVITE_RESP_FAIL) 
                     {
                         wf_os_api_timer_set(&p2p_info->restore_p2p_state_timer, 5000);
                     }
                     break;
                 }
             case P2P_DEVDISC_REQ:
     
                 p2p_devdisc_req_process_func(pnic_info, pframe, len, 1);
     
                 break;
     
             case P2P_DEVDISC_RESP:
     
                 p2p_devdisc_resp_process_func(pnic_info, pframe, len, 1);
     
                 break;
     
             case P2P_PROVISION_DISC_REQ:
                 P2P_FRAME_INFO("Got Provisioning Discovery Request Frame\n");
                 p2p_provdisc_req_process_func(pnic_info, pframe, len, 1);
                 wf_memcpy(pwdinfo->rx_prov_disc_info.peerDevAddr,GetAddr2Ptr(pframe), WF_ETH_ALEN);
     
                 if (pwdinfo->p2p_state != P2P_STATE_RX_PROVISION_DIS_REQ)
                 {
                     wf_p2p_set_pre_state(pwdinfo, pwdinfo->p2p_state);
                 }
     
                 wf_p2p_set_state(pwdinfo, P2P_STATE_RX_PROVISION_DIS_REQ);
                 wf_os_api_timer_set(&p2p_info->restore_p2p_state_timer,P2P_PROVISION_TIMEOUT);
                 break;
     
             case P2P_PROVISION_DISC_RESP:
                 P2P_FRAME_INFO("Got Provisioning Discovery Response Frame\n");
                 wf_os_api_timer_unreg(&p2p_info->restore_p2p_state_timer);
                 wf_p2p_set_state(pwdinfo, P2P_STATE_RX_PROVISION_DIS_RSP);
                 p2p_provdisc_resp_process_func(pnic_info, pframe, 1);
                 wf_os_api_timer_set(&p2p_info->restore_p2p_state_timer, P2P_PROVISION_TIMEOUT);
                 break;
     
             }
         }
     
     exit:
     
         if (merged_p2pie) 
         {
             wf_kfree(merged_p2pie);
             merged_p2pie = NULL;
         }
     }
     return WF_RETURN_OK;
 }

wf_s32 wf_p2p_ie_valid(void *p2p, wf_u16 len)
{
    wf_80211_p2p_param_ie_t *pie = NULL;
    wf_u32 oui_value=0;
    
    if (NULL == p2p  || 0 ==len )
    {
        LOG_E("[%s] NUll point",__func__);
        return -1;
    }
    
    pie = p2p;

    if (!(pie->element_id == WF_80211_MGMT_EID_VENDOR_SPECIFIC &&
        len >= WF_OFFSETOF(wf_80211_p2p_param_ie_t, oui) + pie->len))
    {
        LOG_E("[%s] data corrupt",__func__);
        return -2;
    }

    oui_value = (pie->oui[0] << 16) | (pie->oui[1] << 8) | (pie->oui[2] << 0);
    LOG_I("oui_value:0x%x,oui_type:0x%x",oui_value,pie->oui_type);
    if (!(oui_value == WF_80211_OUI_WFA && pie->oui_type == WF_80211_OUI_TYPE_WFA_P2P))
    {
        LOG_I("no p2p element");
        return -3;
    }

    return 0;
}

wf_u8 *wf_p2p_ie_to_str(WF_P2P_IE_E ie_type)
{
    switch (ie_type)
    {
        case WF_P2P_IE_BEACON       : return to_str(WF_P2P_IE_BEACON);
        case WF_P2P_IE_PROBE_REQ    : return to_str(WF_P2P_IE_PROBE_REQ);
        case WF_P2P_IE_PROBE_RSP    : return to_str(WF_P2P_IE_PROBE_RSP);
        case WF_P2P_IE_ASSOC_REQ    : return to_str(WF_P2P_IE_ASSOC_REQ);
        case WF_P2P_IE_ASSOC_RSP    : return to_str(WF_P2P_IE_ASSOC_RSP);
        default:return to_str(WF_P2P_IE_MAX);
    }
    return "unknown ietype";
}
wf_s32 wf_p2p_parse_p2pie(nic_info_st *pnic_info, void *p2p, wf_u16 len, WF_P2P_IE_E ie_type)
{
    wf_80211_p2p_param_ie_t *pie    = NULL;
    wf_s32 ret                      = 0;
    p2p_info_st *p2p_info           = NULL;
    p2p_wd_info_st *pwdinfo         = NULL;
    
    if (NULL == p2p  || 0 ==len || NULL == pnic_info)
    {
        P2P_FRAME_WARN("NUll point");
        return -1;
    }
    if(WF_P2P_IE_MAX <= ie_type)
    {
        P2P_FRAME_WARN("unknown ie type:%d",ie_type);
        return -2;
    }

    ret = wf_p2p_ie_valid(p2p,len);
    if(ret)
    {
        P2P_FRAME_INFO("no p2p ie");
        return ret;
    }

    P2P_FRAME_DBG("parsing ie:%s",wf_p2p_ie_to_str(ie_type));
    p2p_info = pnic_info->p2p;
    pwdinfo = &p2p_info->wdinfo; 
    pie  = p2p;
    
    if(NULL != p2p_info->p2p_ie[ie_type])
    {
        wf_kfree(p2p_info->p2p_ie[ie_type]);
        p2p_info->p2p_ie[ie_type] = NULL;
        p2p_info->p2p_ie_len[ie_type] = 0;
    }

    //P2P_FRAME_ARRAY(pie,2 + pie->len);
    
    p2p_info->p2p_ie_len[ie_type] = 2 + pie->len;
    p2p_info->p2p_ie[ie_type] = wf_kzalloc(p2p_info->p2p_ie_len[ie_type]);
    if(NULL != p2p_info->p2p_ie[ie_type])
    {
        wf_memcpy(p2p_info->p2p_ie[ie_type],pie,p2p_info->p2p_ie_len[ie_type]);
    }
    else
    {
        LOG_E("wf_kzalloc for [%s] failed",wf_p2p_ie_to_str(ie_type));
    }
    
     return ret;
 }

wf_s32 wf_p2p_proc_assoc_req(nic_info_st *pnic_info, wf_u8 * p2p_ie,wf_u32 p2p_ielen, wdn_net_info_st *pwdn_info, wf_u8 flag)
{
    wf_u8 status_code       = P2P_STATUS_SUCCESS;
    wf_u8 *pbuf             = NULL;
    wf_u8 *pattr_content    = NULL;
    wf_u32 attr_contentlen  = 0;
    wf_u16 cap_attr         = 0;
    p2p_info_st *p2p_info   = pnic_info->p2p;
    p2p_wd_info_st *pwdinfo = &p2p_info->wdinfo;

    if (flag && pwdinfo->role != P2P_ROLE_GO) 
    {
        return P2P_STATUS_FAIL_REQUEST_UNABLE;
    }

    if(p2p_ie) 
    {
        if (wf_p2p_get_attr_content
            (p2p_ie, p2p_ielen, P2P_ATTR_CAPABILITY, (wf_u8 *) & cap_attr,(wf_u32 *) & attr_contentlen)) 
        {
            P2P_FRAME_INFO("Got P2P Capability Attr!!\n");
            cap_attr = wf_le16_to_cpu(cap_attr);
            pwdn_info->dev_cap = cap_attr & 0xff;
        }

        if (wf_p2p_get_attr_content(p2p_ie, p2p_ielen, P2P_ATTR_DEVICE_INFO, NULL,
             (wf_u32 *) & attr_contentlen)) 
        {
            P2P_FRAME_INFO("Got P2P DEVICE INFO Attr!!\n");
            pattr_content = pbuf = wf_kzalloc(attr_contentlen);
            if (pattr_content) 
            {
                wf_u8 num_of_secdev_type;
                wf_u16 dev_name_len;

                wf_p2p_get_attr_content(p2p_ie, p2p_ielen,
                                         P2P_ATTR_DEVICE_INFO, pattr_content,
                                         (wf_u32 *) & attr_contentlen);

                wf_memcpy(pwdn_info->dev_addr, pattr_content, WF_ETH_ALEN);

                pattr_content += WF_ETH_ALEN;

                wf_memcpy(&pwdn_info->config_methods, pattr_content, 2);
                pwdn_info->config_methods = wf_be16_to_cpu(pwdn_info->config_methods);

                pattr_content += 2;

                wf_memcpy(pwdn_info->primary_dev_type, pattr_content, 8);

                pattr_content += 8;

                num_of_secdev_type = *pattr_content;
                pattr_content += 1;

                if (num_of_secdev_type == 0) {
                    pwdn_info->num_of_secdev_type = 0;
                } else {
                    wf_u32 len;

                    pwdn_info->num_of_secdev_type = num_of_secdev_type;

                    len =
                        (sizeof(pwdn_info->secdev_types_list) <
                         (num_of_secdev_type *
                          8)) ? (sizeof(pwdn_info->secdev_types_list))
                        : (num_of_secdev_type * 8);

                    wf_memcpy(pwdn_info->secdev_types_list, pattr_content, len);

                    pattr_content += (num_of_secdev_type * 8);
                }

                pwdn_info->dev_name_len = 0;
                if (WPS_ATTR_DEVICE_NAME == wf_be16_to_cpu(*(wf_u16 *) pattr_content)) {
                    dev_name_len = wf_be16_to_cpu(*(wf_u16 *) (pattr_content + 2));

                    pwdn_info->dev_name_len =
                        (sizeof(pwdn_info->dev_name) <
                         dev_name_len) ? sizeof(pwdn_info->dev_name) : dev_name_len;

                    wf_memcpy(pwdn_info->dev_name, pattr_content + 4,
                                pwdn_info->dev_name_len);
                }

                wf_kfree(pbuf);

            }

        }
    }

    return status_code;

}

