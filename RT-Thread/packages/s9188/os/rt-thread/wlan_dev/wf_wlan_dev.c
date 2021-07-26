#include "common.h"
#include "wf_wlan_dev.h"
#include "trx/tx_rtthread.h"
#include "wf_debug.h"
#include "cfg80211.h"
#include "wifi_bsp.h"
#include "wpa.h"

#include "utils/os.h"
#include "defs.h"
#include "ieee802_11_defs.h"
#include "sha1.h"

int wpa_init;
struct wpa_supplicant wpa_s_obj;
struct wpa_config wpaconfig;
const struct wpa_driver_ops wpa_driver_bsd_ops = {
	.name = "bsd",
	.desc = "BSD 802.11 support",
#ifdef HOSTAPD
	.hapd_init = bsd_init,
	.hapd_deinit = bsd_deinit,
	.set_privacy = bsd_set_privacy,
	.get_seqnum = bsd_get_seqnum,
	.flush = bsd_flush,
	.read_sta_data = bsd_read_sta_driver_data,
	.sta_disassoc = bsd_sta_disassoc,
	.sta_deauth = bsd_sta_deauth,
	.sta_set_flags = bsd_set_sta_authorized,
	.commit = bsd_commit,
#else
	.find_bssid = wf_rt_get_bssid,
	.get_ssid = wf_rt_get_ssid,
#endif
	.set_key = wf_rt_add_key,
};

static void Wpa_Supplicant_Config(nic_info_st *pnic_info,const char * hwaddr, int len, char *essid,
                                  char *password, int ap_mode)
{
  struct wpa_supplicant *wpa_s = &wpa_s_obj;
  struct wpa_sm *sm = wpa_s->wpa;
  static int wpa_init = 0;
  struct wpa_ssid *entry;
  int i=0;
  u8 psk[32];
  u8 GNonce[32];
  u8 GTK[40];
  u8 GMK[32];
  
  if (!wpa_init) {
    wpa_init = 1;
    memset(wpa_s, 0, sizeof(struct wpa_supplicant));
    wpa_s->wpa = wpa_sm_init(wpa_s);
    wpa_s->wpa->pnic_info = pnic_info;
    if (wpa_s->wpa == NULL) {
      return;
    }
    wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_PROTO, WPA_RSN_PROTO);
    wpa_s->driver = &wpa_driver_bsd_ops;
    
    wpa_s->confname = "sci-wifi";
    wpa_s->conf = wpa_config_read(wpa_s->confname, NULL);
    if (wpa_s->conf == NULL) {
      return;
    }
    wpa_s->conf->ap_scan = 1;
    wpa_supplicant_ctrl_iface_add_network(wpa_s);
  }
  
  wpa_s->ap_mode_start = ap_mode;
  os_memcpy(wpa_s->wpa->own_addr, hwaddr, len);
  if (!wpa_s->ap_mode_start) {
    
    
    entry = wpa_s->conf->ssid;
    entry->key_mgmt = WPA_PSK_KEY_MGMT;
    entry->ssid = (u8 *) essid;
    entry->ssid_len = strlen(essid);
    entry->passphrase = password;
    entry->disabled = 0;
    os_memset(entry->psk, 0, 32);
    pbkdf2_sha1(password, (u8 const *)essid, entry->ssid_len,4096, entry->psk, PMK_LEN);
    entry->psk_set = 1;
    wpa_s->confanother = NULL;
    sm->pmk_len = 32;
    memcpy(sm->pmk,psk,32);
	entry->pairwise_cipher = 8;
	entry->key_mgmt = 2;
    
    
  } else {
    pbkdf2_sha1(password, (u8 *)essid, strlen(essid), 4096, psk, PMK_LEN);
    os_memcpy(wpa_s->hapd.pmk, psk, PMK_LEN);
    wpa_s->hapd.pmk_len = PMK_LEN;
    os_memcpy(wpa_s->hapd.own_addr, hwaddr, len);
    os_memset(GTK, 0, 40);
    os_get_random(GMK, WPA_LEN_NONCE);
    os_get_random(GNonce, WPA_LEN_NONCE);
    
    WpaCountGTK(GMK, GNonce, wpa_s->hapd.own_addr, GTK, 32);
    
    os_memcpy(wpa_s->hapd.GTK, GTK, 32);
    
    wpa_s->hapd.gtk_installed = 0;
    sm->pmk_len = 32;
    memcpy(sm->pmk,psk,32);
  }
}

static wf_bool wf_is_8021x_auth (wf_80211_mgmt_ie_t *pies, wf_u16 ies_len)
{
    wf_80211_mgmt_ie_t *pie;

    if (!wf_80211_mgmt_ies_search(pies, ies_len,
                                  WF_80211_MGMT_EID_RSN, &pie))
    {
        LOG_D("RSN");
        return wf_true;
    }

    if (!wf_80211_mgmt_ies_search(pies, ies_len,
                                  WF_80211_MGMT_EID_VENDOR_SPECIFIC, &pie))
    {
        wf_u32 pmulticast_cipher, punicast_cipher;
        if (!wf_80211_mgmt_wpa_parse(pie,
                                     WF_OFFSETOF(wf_80211_mgmt_ie_t, data) + pie->len,
                                     &pmulticast_cipher,
                                     &punicast_cipher))
        {
            LOG_D("WPA");
            return wf_true;
        }
    }

    LOG_D("NO 8021X");
    return wf_false;
}


static rt_err_t wf_wlan_init(struct rt_wlan_device *wlan)
{
  struct rt_wlan_priv *wlan_priv = wlan->user_data;
  nic_info_st *pnic_info = wlan_priv->nic;
  hw_info_st *hw_info;
  
  LOG_D("F:%s L:%d", __FUNCTION__, __LINE__);
  
  if (nic_init(pnic_info)<0)
    return -1;
  
  tx_work_init(wlan);
  
  hw_info = (hw_info_st *)pnic_info->hw_info;
  if (hw_info)
  {
    if((0 == hw_info->macAddr[0]) && (0 == hw_info->macAddr[1]) && (0 == hw_info->macAddr[2])
       && (0 == hw_info->macAddr[3]) && (0 == hw_info->macAddr[4]) && (0 == hw_info->macAddr[5]))
    {
      LOG_E("[%s]: no valid mac addr", __func__);
      return -1;
    }
    
    LOG_I("efuse_macaddr:"WF_MAC_FMT,WF_MAC_ARG(hw_info->macAddr));
    if(wlan_priv->nic->nic_num == 0)
    {
      memcpy(wlan_priv->hw_addr, hw_info->macAddr, WF_ETH_ALEN);
      //wf_mcu_hw_var_set_macaddr(ndev_priv->nic,ndev->dev_addr);
    }
    else if(wlan_priv->nic->nic_num == 1)
    {
      hw_info->macAddr[0] = hw_info->macAddr[0] + 0x2;
      memcpy(wlan_priv->hw_addr, hw_info->macAddr, WF_ETH_ALEN);
      //wf_mcu_hw_var_set_macaddr(ndev_priv->nic,ndev->dev_addr);
    }
    wf_mcu_hw_var_set_macaddr(wlan_priv->nic,wlan_priv->hw_addr);
  } 
  
  return RT_EOK;
}

static rt_err_t wf_wlan_mode(struct rt_wlan_device *wlan, rt_wlan_mode_t mode)
{
  struct rt_wlan_priv *wlan_priv = wlan->user_data;
  nic_info_st *pnic_info = wlan_priv->nic;
  
  LOG_D("F:%s L:%d mode:%d", __FUNCTION__, __LINE__, mode);
  
  local_info_st * plocal = (local_info_st *)pnic_info->local_info;
  wf_bool bConnect = wf_false;
  wf_wlan_mgmt_info_t *wlan_mgmt_info = pnic_info->wlan_mgmt_info;
  wf_wlan_network_t *cur_network = &wlan_mgmt_info->cur_network;
  sys_work_mode_e cur_mode;
  switch(mode)
  {
	case RT_WLAN_STATION:
		cur_mode = WF_INFRA_MODE;
		break;
	case RT_WLAN_AP:
		cur_mode = WF_MASTER_MODE;
		break;
	default:
		break;
  }
//  if(plocal->work_mode == cur_mode)
//  {
//    return RT_EOK;
//  }
  
  wf_local_cfg_set_work_mode(pnic_info, cur_mode);
  
  wf_mlme_get_connect(pnic_info, &bConnect);
  if(bConnect)
  {
    wf_mlme_deauth(pnic_info, wf_true);
  }
  
  wf_mcu_set_op_mode(pnic_info, cur_mode);
  
  switch(cur_mode)
  {
  case WF_MASTER_MODE :
    cur_network->join_res = -1;
    wf_mlme_abort(pnic_info);
    break;
    
  case WF_MONITOR_MODE :
  case WF_INFRA_MODE :
  case WF_AUTO_MODE :
  default :
    break;
  }
  
  LOG_I("wlan dev open");
  if (nic_enable(pnic_info) == WF_RETURN_FAIL)
  {
    return -1;
  }
  
  return RT_EOK;
}

static rt_err_t wf_wlan_scan(struct rt_wlan_device *wlan, struct rt_scan_info *scan_info)
{
  struct rt_wlan_priv *wlan_priv = wlan->user_data;
  nic_info_st *pnic_info = wlan_priv->nic;
  
  wf_bool is_connected, is_busy;
  
  wf_mlme_get_connect(pnic_info, &is_connected);
  if (is_connected)
  {
    wf_mlme_get_traffic_busy(pnic_info, &is_busy);
    if (is_busy)
    {
      wf_os_api_ind_scan_done(pnic_info, wf_true, WF_MLME_FRAMEWORK_WEXT);
      
      return RT_EOK;
    }
    wf_mlme_scan_start(pnic_info, SCAN_TYPE_ACTIVE,
                       NULL, 0, NULL, 0,
                       WF_MLME_FRAMEWORK_WEXT);
  }
  else
  {
    wf_wlan_ssid_t ssids[WF_SCAN_REQ_SSID_NUM];
    wf_memset(ssids, 0, sizeof(ssids));
    if(scan_info != NULL)
    {
      if (scan_info->ssid.len != 0)
      {
        int len = sizeof(scan_info->ssid);
        if(len > RT_WLAN_SSID_MAX_LENGTH)
        {
          len = RT_WLAN_SSID_MAX_LENGTH;
        }
        wf_memcpy(ssids[0].data, scan_info->ssid.val, scan_info->ssid.len);
        ssids[0].length = len;
        
        LOG_D("ssid = %s, ssid_len = %d", ssids[0].data, ssids[0].length);
        
        wf_mlme_scan_start(pnic_info, SCAN_TYPE_ACTIVE,
                           ssids, 1, NULL, 0,
                           WF_MLME_FRAMEWORK_WEXT);
      }
    }
    else
    {
      wf_mlme_scan_start(pnic_info, SCAN_TYPE_ACTIVE,
                         NULL, 0, NULL, 0,
                         WF_MLME_FRAMEWORK_WEXT);
    }
  }
  
  return RT_EOK;
}



int wf_wlan_set_wpa_ie (nic_info_st *pnic_info, wf_u8 *pie, size_t ielen)
{
    sec_info_st *sec_info = pnic_info->sec_info;
    wf_u8 *buf = NULL;
    wf_u32 group_cipher = 0, pairwise_cipher = 0;
    wf_u16 cnt = 0;
    wf_u8 eid, wps_oui[4] = { 0x0, 0x50, 0xf2, 0x04 };
    int res = 0;

    if (pie == NULL)
    {
        goto exit;
    }

    if (ielen > WF_MAX_WPA_IE_LEN)
    {
        res = -EINVAL;
        goto exit;
    }

    if (ielen)
    {
        buf = wf_kzalloc(ielen);
        if (buf == NULL)
        {
            res = -ENOMEM;
            goto exit;
        }
        memcpy(buf, pie, ielen);

        if (ielen < RSN_HEADER_LEN)
        {
            res = -EINVAL;
            goto exit;
        }

        if (!wf_80211_mgmt_wpa_parse(buf, ielen, &group_cipher, &pairwise_cipher))
        {
            sec_info->dot11AuthAlgrthm = dot11AuthAlgrthm_8021X;
            sec_info->ndisauthtype = wf_ndis802_11AuthModeWPAPSK;
            sec_info->wpa_enable = wf_true;
            memcpy(sec_info->supplicant_ie, &buf[0], ielen);
        }
        else if (!wf_80211_mgmt_rsn_parse(buf, ielen, &group_cipher, &pairwise_cipher))
        {
            sec_info->dot11AuthAlgrthm = dot11AuthAlgrthm_8021X;
            sec_info->ndisauthtype = wf_ndis802_11AuthModeWPA2PSK;
            sec_info->rsn_enable = wf_true;
            memcpy(sec_info->supplicant_ie, &buf[0], ielen);
        }

        switch (group_cipher)
        {
            case CIPHER_SUITE_TKIP:
                sec_info->dot118021XGrpPrivacy = _TKIP_;
                sec_info->ndisencryptstatus = wf_ndis802_11Encryption2Enabled;
                break;
            case CIPHER_SUITE_CCMP:
                sec_info->dot118021XGrpPrivacy = _AES_;
                sec_info->ndisencryptstatus = wf_ndis802_11Encryption3Enabled;
                break;
        }

        switch (pairwise_cipher)
        {
            case CIPHER_SUITE_NONE:
                //                sec_info->dot11PrivacyAlgrthm = _NO_PRIVACY_;
                //                sec_info->ndisencryptstatus = wf_ndis802_11EncryptionDisabled;
                //                IW_FUNC_DBG("dot11PrivacyAlgrthm=_NO_PRIVACY_");
                break;
            case CIPHER_SUITE_TKIP:
                sec_info->dot11PrivacyAlgrthm = _TKIP_;
                sec_info->ndisencryptstatus = wf_ndis802_11Encryption2Enabled;
                break;
            case CIPHER_SUITE_CCMP:
                sec_info->dot11PrivacyAlgrthm = _AES_;
                sec_info->ndisencryptstatus = wf_ndis802_11Encryption3Enabled;
                break;
        }

        while (cnt < ielen)
        {
            eid = buf[cnt];
            if (eid == WF_80211_MGMT_EID_VENDOR_SPECIFIC &&
                !memcmp(&buf[cnt + 2], wps_oui, 4))
            {
               
                sec_info->wps_ie_len = WF_MIN(buf[cnt + 1] + 2, 512);
                memcpy(sec_info->wps_ie, &buf[cnt], sec_info->wps_ie_len);
                cnt += buf[cnt + 1] + 2;
                break;
            }
            else
            {
                cnt += buf[cnt + 1] + 2;
            }
        }

        wf_mcu_set_on_rcr_am(pnic_info, wf_false);
        wf_mcu_set_hw_invalid_all(pnic_info);
        wf_mcu_set_sec_cfg(pnic_info, sec_info->dot11AuthAlgrthm);
    }

exit :
    if (buf)
    {
        wf_kfree(buf);
    }
    return res;
}


static int wf_wlan_hw_set_group_key(nic_info_st *pnic_info, wdn_net_info_st *pwdn_info)
{
    sec_info_st *psec_info = pnic_info->sec_info;
    wf_u8 cam_id;
    wf_u16 ctrl;
    int ret;

    cam_id = psec_info->dot118021XGrpKeyid & 0x03; /* cam_id0~3 8021x group key */
    ctrl = BIT(15) | BIT(6) |
           (psec_info->dot118021XGrpPrivacy << 2) |
           psec_info->dot118021XGrpKeyid;
    ret = wf_mcu_set_sec_cam(pnic_info, cam_id, ctrl,
                             pwdn_info->bssid,
                             psec_info->dot118021XGrpKey[psec_info->dot118021XGrpKeyid].skey);
    wf_mcu_set_on_rcr_am(pnic_info, wf_true);

    return ret;
}


/* dk:      default key for wep
   grpk:    group key for tkip/ccmp
   unik:    unnicast key for tkip/ccmp

    |-------cam_id------|
    |0~3            4~31|
     ________WEP________
STA  dk              -
AP   dk              -
     _____TKIP/CCMP_____
STA  grpk           unik
AP   grpk           unik
*/

static int wf_wlan_hw_set_unicast_key(nic_info_st *pnic_info, wdn_net_info_st *pwdn_info)
{
    wf_u8 cam_id;
    wf_u16 ctrl;
    int ret;

    cam_id = 4;
    ctrl = BIT(15) | (pwdn_info->dot118021XPrivacy << 2);
    ret = wf_mcu_set_sec_cam(pnic_info,
                             cam_id, ctrl,
                             pwdn_info->mac, pwdn_info->dot118021x_UncstKey.skey);

    return ret;
}

static int wf_wlan_encryption(nic_info_st *pnic_info,
                          ieee_param *param, wf_u32 param_len)
{
    wf_u32 wep_key_idx, wep_key_len;
    sec_info_st *psec_info = pnic_info->sec_info;
    wdn_net_info_st *pwdn_info;
    int res = 0;

    param->u.crypt.err = 0;
    param->u.crypt.alg[IEEE_CRYPT_ALG_NAME_LEN - 1] = '\0';

    if (param_len !=
        WF_OFFSETOF(ieee_param, u.crypt.key) + param->u.crypt.key_len)
    {
        res = -EINVAL;
        goto exit;
    }

    if (is_bcast_addr(param->sta_addr))
    {
        if (param->u.crypt.idx >= WEP_KEYS)
        {
            res = -EINVAL;
            goto exit;
        }
    }
    else
    {
        res = -EINVAL;
        goto exit;
    }

    if (!strcmp((char const *)param->u.crypt.alg, "WEP"))
    {
        wep_key_idx = param->u.crypt.idx;
        wep_key_len = param->u.crypt.key_len;

        if ((wep_key_idx > WEP_KEYS) || (wep_key_len == 0))
        {
            res = -EINVAL;
            goto exit;
        }

        psec_info->ndisencryptstatus = wf_ndis802_11Encryption1Enabled;

        wep_key_len = wep_key_len <= 5 ? 5 : 13; /* 5B for wep40 and 13B for wep104 */
        if (wep_key_len == 13)
        {
            psec_info->dot11PrivacyAlgrthm = _WEP104_;
        }
        else
        {
            psec_info->dot11PrivacyAlgrthm = _WEP40_;
        }

        if (param->u.crypt.set_tx)
        {
            psec_info->dot11PrivacyKeyIndex = wep_key_idx;
        }
        memcpy(psec_info->dot11DefKey[wep_key_idx].skey,
               param->u.crypt.key, wep_key_len);
		LOG_D("%s",psec_info->dot11DefKey[wep_key_idx].skey);
        psec_info->dot11DefKeylen[wep_key_idx] = wep_key_len;
        psec_info->key_mask |= BIT(wep_key_idx);
    }
    if (psec_info->dot11AuthAlgrthm == dot11AuthAlgrthm_8021X) /* 802_1x */
    {
        local_info_st *plocal_info = pnic_info->local_info;
        if (plocal_info->work_mode == WF_INFRA_MODE) /* sta mode */
        {
            pwdn_info = wf_wdn_find_info(pnic_info,
                                         wf_wlan_get_cur_bssid(pnic_info));
            if (pwdn_info == NULL)
            {
                goto exit;
            }

            if (strcmp((char const *)param->u.crypt.alg, "none") != 0)
            {
                pwdn_info->ieee8021x_blocked = wf_false;
            }

            if (psec_info->ndisencryptstatus == wf_ndis802_11Encryption2Enabled ||
                psec_info->ndisencryptstatus == wf_ndis802_11Encryption3Enabled)
            {
                pwdn_info->dot118021XPrivacy = psec_info->dot11PrivacyAlgrthm;
            }
         

            /* PTK: param->u.crypt.key */
            if (param->u.crypt.set_tx == 1) /* pairwise key */
            {
         
                /* KCK PTK0~127 */
                memcpy(pwdn_info->dot118021x_UncstKey.skey, param->u.crypt.key,
                       min(param->u.crypt.key_len, 16));

                if (strcmp((char const *)param->u.crypt.alg, "TKIP") == 0) /* set mic key */
                {
                    /* KEK PTK128~255 */
                    memcpy(pwdn_info->dot11tkiptxmickey.skey,
                           &(param->u.crypt.key[16]), 8); /* PTK128~191 */
                    memcpy(pwdn_info->dot11tkiprxmickey.skey,
                           &(param->u.crypt.key[24]), 8); /* PTK192~255 */
                    psec_info->busetkipkey = wf_true;
                }
                if (pwdn_info->dot118021XPrivacy == _AES_)
                {
                    
                    wf_wlan_hw_set_unicast_key(pnic_info, pwdn_info);
                }
            }
            else /* group key */
            {
                
                memcpy(psec_info->dot118021XGrpKey[param->u.crypt.idx].skey,
                       param->u.crypt.key,
                       min( param->u.crypt.key_len, 16));
                memcpy(psec_info->dot118021XGrptxmickey[param->u.crypt.idx].skey,
                       &param->u.crypt.key[16], 8);
                memcpy(psec_info->dot118021XGrprxmickey[param->u.crypt.idx].skey,
                       &param->u.crypt.key[24], 8);
                psec_info->binstallGrpkey = wf_true;
                psec_info->dot118021XGrpKeyid = param->u.crypt.idx;
                if (psec_info->dot118021XGrpPrivacy == _AES_)
                {
                    
                    wf_wlan_hw_set_group_key(pnic_info, pwdn_info);
                }
            }
        }
    }

exit:
    return res;
}



static rt_err_t wf_wlan_join(struct rt_wlan_device *wlan, struct rt_sta_info *sta_info)
{
  struct rt_wlan_priv *wlan_priv = wlan->user_data;
  nic_info_st *pnic_info = wlan_priv->nic;
  sec_info_st *psec_info = pnic_info->sec_info;
  hw_info_st *phw_info = pnic_info->hw_info;
  wf_u8 len;
  ieee_param *param = NULL;
  wf_u32 param_len;
  if (wf_local_cfg_get_work_mode(pnic_info) == WF_MASTER_MODE)
  {
    LOG_E("ap no support wf_wlan_join");
    return -RT_ERROR;
  }

  param_len = WF_OFFSETOF(ieee_param, u.crypt.key) + sta_info->key.len;
  param = (ieee_param *)wf_vmalloc(param_len);
  if (param == NULL)
  {
	  return -1;
  }

  memset(param->sta_addr, 0xff, WF_ETH_ALEN);
  LOG_D("sta_info->security:%x",sta_info->security);
  switch (sta_info->security & 0xffff)
  {
	  case WEP_ENABLED:
	  	strncpy((char *)param->u.crypt.alg,"WEP",IEEE_CRYPT_ALG_NAME_LEN);
		param->u.crypt.set_tx = 1;
	  	break;	
	  case AES_ENABLED:
	  	wf_memcpy((char *)param->u.crypt.alg,"ccmp",IEEE_CRYPT_ALG_NAME_LEN);
		Wpa_Supplicant_Config(pnic_info,(const char *)phw_info->macAddr,WF_ETH_ALEN,(char *)sta_info->ssid.val,(char *)sta_info->key.val,0);
	  	break;
	  case TKIP_ENABLED:
	  	wf_memcpy((char *)param->u.crypt.alg,"tkip",IEEE_CRYPT_ALG_NAME_LEN);
		LOG_D("KEY:%s",sta_info->key.val);
		Wpa_Supplicant_Config(pnic_info,(const char *)phw_info->macAddr,WF_ETH_ALEN,(char *)sta_info->ssid.val,(char *)sta_info->key.val,0);
		LOG_D("KEY:%s",sta_info->key.val);
		break;
	  default:
        wf_memcpy((char *)param->u.crypt.alg,"none",IEEE_CRYPT_ALG_NAME_LEN);
		break;
  }

  param->u.crypt.key_len = sta_info->key.len;

  memcpy(param->u.crypt.key, sta_info->key.val, sta_info->key.len);
  param->u.crypt.idx = 0;
  wf_wlan_encryption(pnic_info,param,param_len);

  len = sta_info->ssid.len;
  if (len > 32)
  {
    LOG_E("ssid length %d too long", len);
    return -RT_ERROR;
  }
  
  if (len == 32)
  {
    LOG_D("clear essid");
    return RT_EOK;
  }
  
  if (sta_info->ssid.len)
  {
    wf_wlan_mgmt_scan_que_for_rst_e scan_que_for_rst;
    wf_wlan_mgmt_scan_que_node_t *pscan_que_node;
    wf_wlan_ssid_t ssid;
    wf_u8 *pbssid = NULL;
    
    wf_memcpy(ssid.data, sta_info->ssid.val, ssid.length = len);
    ssid.data[ssid.length] = '\0';
    
    wf_wlan_mgmt_scan_que_for_begin(pnic_info, pscan_que_node)
    {
      if (wf_wlan_is_same_ssid(&pscan_que_node->ssid, &ssid))
      {
        pbssid = pscan_que_node->bssid;
		if(pscan_que_node->wpa_ie != NULL)
		{
			wf_wlan_set_wpa_ie(pnic_info,pscan_que_node->wpa_ie,sizeof(pscan_que_node->wpa_ie));
		}
		else if(pscan_que_node->rsn_ie != NULL)
		{	
			wf_wlan_set_wpa_ie(pnic_info,pscan_que_node->rsn_ie,sizeof(pscan_que_node->rsn_ie));
		}
		else
		{
			LOG_E("IE NULL");
			return -1;
		}
        break;
      }
    }
    wf_wlan_mgmt_scan_que_for_end(scan_que_for_rst);
    
    if (scan_que_for_rst == WF_WLAN_MGMT_SCAN_QUE_FOR_RST_FAIL)
    {
      LOG_W("scan queue for each fail");
      return -RT_ERROR;
    }
    else
    {
      wf_bool is_connected;
      wf_mlme_get_connect(pnic_info, &is_connected);
      if (is_connected)
      {
        if (pbssid &&
            wf_80211_is_same_addr(wf_wlan_get_cur_bssid(pnic_info), pbssid))
        {
          LOG_D("the essid as same as the current associate ssid");
          wf_os_api_ind_connect(pnic_info, WF_MLME_FRAMEWORK_WEXT);
          return RT_EOK;
        }
      }
      
      /* start connection */
      wf_mlme_conn_start(pnic_info, pbssid, &ssid,
                         WF_MLME_FRAMEWORK_WEXT, wf_true);
    }
  }
  
  return RT_EOK;
}

static rt_err_t wf_wlan_softap(struct rt_wlan_device *wlan, struct rt_ap_info *ap_info)
{
  return RT_EOK;
}

static rt_err_t wf_wlan_disconnect(struct rt_wlan_device *wlan)
{
  struct rt_wlan_priv *wlan_priv = wlan->user_data;
  nic_info_st *pnic_info = wlan_priv->nic;
  
  wf_mlme_deassoc(pnic_info);
  return RT_EOK;
}

static rt_err_t wf_wlan_ap_stop(struct rt_wlan_device *wlan)
{
  return RT_EOK;
}

static rt_err_t wf_wlan_ap_deauth(struct rt_wlan_device *wlan, rt_uint8_t mac[])
{
  return RT_EOK;
}

static rt_err_t wf_wlan_scan_stop(struct rt_wlan_device *wlan)
{
  struct rt_wlan_priv *wlan_priv = wlan->user_data;
  nic_info_st *pnic_info = wlan_priv->nic;
  
  wf_scan_stop(pnic_info);
  return RT_EOK;
}

static int wf_wlan_get_rssi(struct rt_wlan_device *wlan)
{
  return RT_EOK;
}

static rt_err_t wf_wlan_set_powersave(struct rt_wlan_device *wlan, int level)
{
  return RT_EOK;
}

static int wf_wlan_get_powersave(struct rt_wlan_device *wlan)
{
  return RT_EOK;
}

static rt_err_t wf_wlan_cfg_promisc(struct rt_wlan_device *wlan, rt_bool_t start)
{
  return RT_EOK;
}

static rt_err_t wf_wlan_cfg_filter(struct rt_wlan_device *wlan, struct rt_wlan_filter *filter)
{
  return RT_EOK;
}

static rt_err_t wf_wlan_cfg_mgmt_filter(struct rt_wlan_device *wlan, rt_bool_t start)
{
  return RT_EOK;
}

static rt_err_t wf_wlan_set_channel(struct rt_wlan_device *wlan, int channel)
{
  struct rt_wlan_priv *wlan_priv = wlan->user_data;
  nic_info_st *pnic_info = wlan_priv->nic;
  
  wf_wlan_mgmt_info_t *pwlan_info = pnic_info->wlan_mgmt_info;
  wf_wlan_network_t *pcur_network = &pwlan_info->cur_network;
  
  pcur_network->channel = channel;
  wf_hw_info_set_channnel_bw(pnic_info, pcur_network->channel, pcur_network->bw, HAL_PRIME_CHNL_OFFSET_DONT_CARE);
  return RT_EOK;
}

static int wf_wlan_get_channel(struct rt_wlan_device *wlan)
{
  struct rt_wlan_priv *wlan_priv = wlan->user_data;
  nic_info_st *pnic_info = wlan_priv->nic;
  
  wf_wlan_mgmt_info_t *pwlan_info = pnic_info->wlan_mgmt_info;
  wf_wlan_network_t *pcur_network = &pwlan_info->cur_network;
  
  return pcur_network->channel;
}

static rt_err_t wf_wlan_set_country(struct rt_wlan_device *wlan, rt_country_code_t country_code)
{
  struct rt_wlan_priv *wlan_priv = wlan->user_data;
  nic_info_st *pnic_info = wlan_priv->nic;
  
  hw_info_st *phw_info = pnic_info->hw_info;
  phw_info->channel_plan = country_code;
  
  channel_init(pnic_info);
  return RT_EOK;
}

static rt_country_code_t wf_wlan_get_country(struct rt_wlan_device *wlan)
{
  struct rt_wlan_priv *wlan_priv = wlan->user_data;
  nic_info_st *pnic_info = wlan_priv->nic;

  hw_info_st *phw_info = pnic_info->hw_info;
  return (rt_country_code_t)phw_info->channel_plan;
}

static rt_err_t wf_wlan_set_mac(struct rt_wlan_device *wlan, rt_uint8_t mac[])
{
  struct rt_wlan_priv *wlan_priv = wlan->user_data;
  nic_info_st *pnic_info = wlan_priv->nic;

  hw_info_st *phw_info = pnic_info->hw_info;
  wf_memcpy(phw_info->macAddr,mac,6);
  return RT_EOK;
}

static rt_err_t wf_wlan_get_mac(struct rt_wlan_device *wlan, rt_uint8_t mac[])
{
  struct rt_wlan_priv *wlan_priv = wlan->user_data;
  nic_info_st *pnic_info = wlan_priv->nic;
  
  hw_info_st *phw_info = pnic_info->hw_info;
  wf_memcpy(mac,phw_info->macAddr,6);
  return RT_EOK;
}

static int wf_wlan_recv(struct rt_wlan_device *wlan, void *buff, int len)
{
  return RT_EOK;
}

static int wf_wlan_send(struct rt_wlan_device *wlan, void *buff, int len)
{
  wf_bool bRet = wf_false;
  struct rt_wlan_priv *wlan_priv = wlan->user_data;
  nic_info_st *pnic_info = wlan_priv->nic;
  struct pbuf *p = (struct pbuf *)buff;
  wf_u8 *frame;
  
  if(!p)
  {
    return -RT_ERROR;
  }
  
  if(WF_CANNOT_RUN(wlan_priv->nic))
  {
    return -RT_EIO;
  }
  
  if (wlan_priv->nic->tx_info == NULL)
  {
    return -RT_EIO;
  }
  
  if (wf_mlme_check_mode(wlan_priv->nic, WF_MONITOR_MODE) == wf_true)
  {
    //work_monitor_tx_entry(pnetdev, (struct sk_buff *)pkt);
  }
  else
  {
    if(wf_false == wf_tx_data_check(wlan_priv->nic))
    {
      ;
    }
    else
    {
       /* tx resource check */
      bRet = wf_need_stop_queue(wlan_priv->nic);
      if (bRet == wf_true) {
        LOG_W(">>>>wlan dev tx stop queue");
        tx_work_wake(wlan);
        return -RT_ENOMEM;
      }
      
      /* actually xmit */
      frame = rt_malloc(p->tot_len);
      if (frame == RT_NULL)
      {
        LOG_E("F:%s L:%d malloc out_buf fail\n", __FUNCTION__, __LINE__);
        return -RT_ENOMEM;
      }
      /*copy pbuf -> data dat*/
      pbuf_copy_partial(p, frame, p->tot_len, 0);
      if(0 != wf_tx_msdu(wlan_priv->nic, frame, p->tot_len, frame))
      {
        rt_free(frame);
      }
      else
      {
        tx_work_wake(wlan);
      }
    }
  }
  
  return RT_EOK;
}


const static struct rt_wlan_dev_ops ops =
{
  .wlan_init          = wf_wlan_init,
  .wlan_mode          = wf_wlan_mode,
  .wlan_scan          = wf_wlan_scan,
  .wlan_join          = wf_wlan_join,
  .wlan_softap        = wf_wlan_softap,
  .wlan_disconnect    = wf_wlan_disconnect,
  .wlan_ap_stop       = wf_wlan_ap_stop,
  .wlan_ap_deauth     = wf_wlan_ap_deauth,
  .wlan_scan_stop     = wf_wlan_scan_stop,
  .wlan_get_rssi      = wf_wlan_get_rssi,
  .wlan_set_powersave = wf_wlan_set_powersave,
  .wlan_get_powersave = wf_wlan_get_powersave,
  .wlan_cfg_promisc   = wf_wlan_cfg_promisc,
  .wlan_cfg_filter    = wf_wlan_cfg_filter,
  .wlan_cfg_mgnt_filter = wf_wlan_cfg_mgmt_filter,
  .wlan_set_channel   = wf_wlan_set_channel,
  .wlan_get_channel   = wf_wlan_get_channel,
  .wlan_set_country   = wf_wlan_set_country,
  .wlan_get_country   = wf_wlan_get_country,
  .wlan_set_mac       = wf_wlan_set_mac,
  .wlan_get_mac       = wf_wlan_get_mac,
  .wlan_recv          = wf_wlan_recv,
  .wlan_send          = wf_wlan_send,
};

int rt_wlan_register (nic_info_st *pnic_info, const char *name)
{
  rt_err_t ret;
  struct rt_wlan_device *wlan_dev;
  struct rt_wlan_priv   *wlan_priv;
  
  wlan_dev = wf_kzalloc(sizeof(struct rt_wlan_device));
  if(NULL == wlan_dev) {
    LOG_E("cann't malloc memmory for wlan dev");
    return -1;
  }
  
  wlan_priv = wf_kzalloc(sizeof(struct rt_wlan_priv));
  if(NULL == wlan_priv) {
    LOG_E("cann't malloc memmory for wlan priv");
    return -1;
  }
  
  wlan_priv->nic = pnic_info;
  pnic_info->ndev = wlan_dev;
  
  /* register wlan device */
  ret = rt_wlan_dev_register(wlan_dev, name, &ops, 0, wlan_priv);
  if (ret != RT_EOK)
  {
    LOG_E("register wlan dev error");
    return ret;
  }
  
  return 0;
}

int wf_wlan_dev_report_eapol(struct rt_wlan_device *wlan, void *buffer, wf_u16 len)
{
  struct rt_wlan_priv *wlan_priv = wlan->user_data;
  nic_info_st *pnic_info = wlan_priv->nic;
  wpa_s_obj.pnic_info = (void *)wlan_priv->nic;
  struct wl_ieee80211_hdr_3addr * pwlanhdr = NULL;  
  pwlanhdr = (struct wl_ieee80211_hdr_3addr *)buffer;
  int i=0;
  
  memcpy(wpa_s_obj.wpa->pmk,wpa_s_obj.conf->ssid->psk,32);
  wpa_s_obj.wpa->pmk_len = 32;
  wpa_s_obj.wpa->pairwise_cipher = wpa_s_obj.conf->ssid->pairwise_cipher;
  wpa_s_obj.wpa->key_mgmt = wpa_s_obj.conf->ssid->key_mgmt;
  memcpy(wpa_s_obj.wpa->bssid,wf_wlan_get_cur_bssid(pnic_info),6);
  wpa_supplicant_rx_eapol(&wpa_s_obj,wf_wlan_get_cur_bssid(pnic_info),(wf_u8 *)buffer,len);
  return 0;
}

int rt_wlan_shutdown(nic_info_st *pnic_info)
{
  LOG_I("enter %s", __func__);
  if (pnic_info == NULL)
  {
    return 0;
  }
  
  nic_shutdown(pnic_info);
  
  return 0;
}

int rt_wlan_unregister (nic_info_st *pnic_info)
{
  LOG_E("NO  support for wlan dev unregister");
  return 0;
}

