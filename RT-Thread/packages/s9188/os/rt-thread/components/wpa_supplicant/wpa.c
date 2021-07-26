/*
* WPA Supplicant - WPA state machine and EAPOL-Key processing
* Copyright (c) 2003-2015, Jouni Malinen <j@w1.fi>
* Copyright(c) 2015 Intel Deutschland GmbH
*
* This software may be distributed under the terms of the BSD license.
* See README for more details.
*/

#include "includes.h"

#include "utils/common.h"
#include "crypto/aes_wrap.h"
#include "crypto/crypto.h"
#include "crypto/random.h"
#include "common/ieee802_11_defs.h"
//#include "eapol_supp/eapol_supp_sm.h"
#include "wpa.h"
#include "common/wpa_common.h"
#include "sha1.h"
#include "utils/wpabuf.h"
//#include "eloop.h"
//#include "preauth.h"
//#include "pmksa_cache.h"
//#include "wpa_i.h"
//#include "wpa_ie.h"
//#include "peerkey.h"
#include "common/wpa_common.h"

#include "wlan_dev/wf_wlan_dev.h"
#include "wf_debug.h"

static const u8 null_rsc[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

extern struct wpa_supplicant wpa_s_obj;
extern struct wpa_config wpaconfig;

/*frame send interface songqiang add*/
static int wpa_sm_ether_send(struct wpa_sm *sm, const u8 *dest,u16 proto, u8 *buf, size_t len)
{
  wf_u16 pkt_len;
  wf_u8 *buffer;
  nic_info_st *pnic_info = sm->pnic_info;
  struct rt_wlan_device *wlan = pnic_info->ndev;
  struct rt_wlan_priv *wlan_priv = wlan->user_data;
  struct pbuf *p;
  
  pkt_len = sizeof(struct wf_ethhdr) + len;
  p = pbuf_alloc(PBUF_RAW, pkt_len, PBUF_POOL);
  if (NULL == p) {
    LOG_W("no memory for ethernet packet send");
    return -1;
  }
  
  buffer = p->payload;
  wf_memcpy(buffer, dest, MAC_ADDR_LEN);
  buffer += MAC_ADDR_LEN;
  wf_memcpy(buffer, wlan_priv->hw_addr, MAC_ADDR_LEN);
  buffer += MAC_ADDR_LEN;
  *buffer++ = (proto >> 8) & 0xFF;
  *buffer++ = proto & 0xFF;
  wf_memcpy(buffer, buf, len);
  
  rt_wlan_prot_transfer_dev(wlan,p,pkt_len);
  pbuf_free(p);
  return 0;
}
static int wpa_sm_get_bssid(struct wpa_sm *sm, u8 * bssid)
{
  
  return 0;
}

enum wpa_states wpa_sm_get_state()
{
  return wpa_s_obj.wpa_state;
}

void wpa_sm_set_state(struct wpa_sm *sm, enum wpa_states state)
{
  wpa_s_obj.wpa_state = state;   
  if(wpa_s_obj.last_eapol_matches_bssid && WPA_DISCONNECTED == state)
  {
    wpa_s_obj.last_eapol_matches_bssid = 0;
  }
}

int wpa_sm_key_mgmt_set_pmk(struct wpa_sm *sm, const u8 * pmk, int pmk_len)
{
  
  return 0;
}



static void pmksa_cache_set_expiration(struct rsn_pmksa_cache *pmksa)
{
  return ;
}

void eap_notify_success(struct eap_sm *sm)
{
  if (sm) {
    sm->decision = DECISION_COND_SUCC;
    sm->EAP_state = EAP_SUCCESS;
  }
}



const u8 *eap_get_eapKeyData(struct eap_sm *sm, size_t * len)
{
  if (sm == NULL || sm->eapKeyData == NULL) {
    *len = 0;
    return NULL;
  }
  
  *len = sm->eapKeyDataLen;
  return sm->eapKeyData;
}


int eapol_sm_get_key(struct eapol_sm *sm, u8 * key, size_t len)
{
  const u8 *eap_key;
  size_t eap_len;
  
  if (sm == NULL ) {
    wpa_printf(MSG_DEBUG, "EAPOL: EAP key not available");
    return -1;
  }
  eap_key = eap_get_eapKeyData(sm->eap, &eap_len);
  if (eap_key == NULL) {
    wpa_printf(MSG_DEBUG, "EAPOL: Failed to get eapKeyData");
    return -1;
  }
  if (len > eap_len) {
    wpa_printf(MSG_DEBUG, "EAPOL: Requested key length (%lu) not "
               "available (len=%lu)",
               (unsigned long)len, (unsigned long)eap_len);
    return eap_len;
  }
  os_memcpy(key, eap_key, len);
  wpa_printf(MSG_DEBUG, "EAPOL: Successfully fetched key (len=%lu)",
             (unsigned long)len);
  return 0;
}


void eapol_sm_notify_cached(struct eapol_sm *sm)
{
  if (sm == NULL)
    return;
  wpa_printf(MSG_DEBUG, "EAPOL: PMKSA caching was used - skip EAPOL");
  sm->eapSuccess = TRUE;
  eap_notify_success(sm->eap);
}


void eapol_sm_notify_tx_eapol_key(struct eapol_sm *sm)
{
  if (sm)
    sm->dot1xSuppEapolFramesTx++;
}
/**
* wpa_eapol_key_send - Send WPA/RSN EAPOL-Key message
* @sm: Pointer to WPA state machine data from wpa_sm_init()
* @kck: Key Confirmation Key (KCK, part of PTK)
* @kck_len: KCK length in octets
* @ver: Version field from Key Info
* @dest: Destination address for the frame
* @proto: Ethertype (usually ETH_P_EAPOL)
* @msg: EAPOL-Key message
* @msg_len: Length of message
* @key_mic: Pointer to the buffer to which the EAPOL-Key MIC is written
* Returns: >= 0 on success, < 0 on failure
*/
int wpa_eapol_key_send(struct wpa_sm *sm, const u8 *kck, size_t kck_len,
		       int ver, const u8 *dest, u16 proto,
		       u8 *msg, size_t msg_len, u8 *key_mic)
{
  int ret = -1;
  size_t mic_len = wpa_mic_len(sm->key_mgmt);
  
  int i=0;
  if (is_zero_ether_addr(dest) && is_zero_ether_addr(sm->bssid)) {
    /*
    * Association event was not yet received; try to fetch
    * BSSID from the driver.
    */
    
    dest = sm->bssid;
    
    
  }
  
  if (key_mic &&
      wpa_eapol_key_mic(kck, kck_len, 2, 1, msg, msg_len,
                        key_mic)) {
                          wpa_msg(sm->ctx->msg_ctx, MSG_ERROR,
                                  "WPA: Failed to generate EAPOL-Key version %d key_mgmt 0x%x MIC",
                                  ver, sm->key_mgmt);
                          goto out;
                        }
  
  printf( "WPA: KCK %d\n",  kck_len);
  printf( "WPA: Derived Key MIC %d\n",  mic_len);
  printf( "WPA: TX EAPOL-Key %d\n",  msg_len);
  
  ret = wpa_sm_ether_send(sm, dest, proto, msg, msg_len);
  eapol_sm_notify_tx_eapol_key(sm->eapol);
out:
  os_free(msg);
  return ret;
}



int wpa_config_add_prio_network(struct wpa_config *config,
                                struct wpa_ssid *ssid)
{
  int prio;
  struct wpa_ssid *prev, **nlist;
  
  for (prio = 0; prio < config->num_prio; prio++) {
    prev = config->pssid[prio];
    if (prev->priority == ssid->priority) {
      while (prev->pnext)
        prev = prev->pnext;
      prev->pnext = ssid;
      return 0;
    }
  }
  
  nlist = os_realloc_array(config->pssid, config->num_prio + 1,
                           sizeof(struct wpa_ssid *));
  if (nlist == NULL)
    return -1;
  
  for (prio = 0; prio < config->num_prio; prio++) {
    if (nlist[prio]->priority < ssid->priority) {
      os_memmove(&nlist[prio + 1], &nlist[prio],
                 (config->num_prio - prio) * sizeof(struct wpa_ssid *));
      break;
    }
  }
  
  nlist[prio] = ssid;
  config->num_prio++;
  config->pssid = nlist;
  
  return 0;
}



int wpa_config_update_prio_list(struct wpa_config *config)
{
  struct wpa_ssid *ssid;
  int ret = 0;
  
  os_free(config->pssid);
  config->pssid = NULL;
  config->num_prio = 0;
  
  ssid = config->ssid;
  while (ssid) {
    ssid->pnext = NULL;
    if (wpa_config_add_prio_network(config, ssid) < 0)
      ret = -1;
    ssid = ssid->next;
  }
  
  return ret;
}

struct wpa_ssid *wpa_config_add_network(struct wpa_config *config)
{
  int id;
  struct wpa_ssid *ssid, *last = NULL;
  
  id = -1;
  ssid = config->ssid;
  while (ssid) {
    if (ssid->id > id)
      id = ssid->id;
    last = ssid;
    ssid = ssid->next;
  }
  id++;
  
  ssid = os_zalloc(sizeof(*ssid));
  if (ssid == NULL)
    return NULL;
  ssid->id = id;
  dl_list_init(&ssid->psk_list);
  if (last)
    last->next = ssid;
  else
    config->ssid = ssid;
  
  wpa_config_update_prio_list(config);
  
  return ssid;
}


void wpa_config_set_network_defaults(struct wpa_ssid *ssid)
{
  ssid->proto = DEFAULT_PROTO;
  ssid->pairwise_cipher = DEFAULT_PAIRWISE;
  ssid->group_cipher = DEFAULT_GROUP;
  ssid->key_mgmt = 2;
  ssid->bg_scan_period = DEFAULT_BG_SCAN_PERIOD;
#ifdef IEEE8021X_EAPOL
  ssid->eapol_flags = DEFAULT_EAPOL_FLAGS;
  ssid->eap_workaround = DEFAULT_EAP_WORKAROUND;
  ssid->eap.fragment_size = DEFAULT_FRAGMENT_SIZE;
  ssid->eap.sim_num = DEFAULT_USER_SELECTED_SIM;
#endif
#ifdef CONFIG_MESH
  ssid->dot11MeshMaxRetries = DEFAULT_MESH_MAX_RETRIES;
  ssid->dot11MeshRetryTimeout = DEFAULT_MESH_RETRY_TIMEOUT;
  ssid->dot11MeshConfirmTimeout = DEFAULT_MESH_CONFIRM_TIMEOUT;
  ssid->dot11MeshHoldingTimeout = DEFAULT_MESH_HOLDING_TIMEOUT;
#endif
#ifdef CONFIG_HT_OVERRIDES
  ssid->disable_ht = DEFAULT_DISABLE_HT;
  ssid->disable_ht40 = DEFAULT_DISABLE_HT40;
  ssid->disable_sgi = DEFAULT_DISABLE_SGI;
  ssid->disable_ldpc = DEFAULT_DISABLE_LDPC;
  ssid->disable_max_amsdu = DEFAULT_DISABLE_MAX_AMSDU;
  ssid->ampdu_factor = DEFAULT_AMPDU_FACTOR;
  ssid->ampdu_density = DEFAULT_AMPDU_DENSITY;
#endif
#ifdef CONFIG_VHT_OVERRIDES
  ssid->vht_rx_mcs_nss_1 = -1;
  ssid->vht_rx_mcs_nss_2 = -1;
  ssid->vht_rx_mcs_nss_3 = -1;
  ssid->vht_rx_mcs_nss_4 = -1;
  ssid->vht_rx_mcs_nss_5 = -1;
  ssid->vht_rx_mcs_nss_6 = -1;
  ssid->vht_rx_mcs_nss_7 = -1;
  ssid->vht_rx_mcs_nss_8 = -1;
  ssid->vht_tx_mcs_nss_1 = -1;
  ssid->vht_tx_mcs_nss_2 = -1;
  ssid->vht_tx_mcs_nss_3 = -1;
  ssid->vht_tx_mcs_nss_4 = -1;
  ssid->vht_tx_mcs_nss_5 = -1;
  ssid->vht_tx_mcs_nss_6 = -1;
  ssid->vht_tx_mcs_nss_7 = -1;
  ssid->vht_tx_mcs_nss_8 = -1;
#endif
  ssid->proactive_key_caching = -1;
#ifdef CONFIG_IEEE80211W
  ssid->ieee80211w = MGMT_FRAME_PROTECTION_DEFAULT;
#endif
  ssid->mac_addr = -1;
}



int wpa_supplicant_ctrl_iface_add_network(struct wpa_supplicant *wpa_s)
{
  struct wpa_ssid *ssid;
  int ret = 0;
  
  ssid = wpa_config_add_network(wpa_s->conf);
  if (ssid == NULL)
    return -1;
  
  ssid->disabled = 1;
  wpa_config_set_network_defaults(ssid);
  wpa_s->wpa->proto = DEFAULT_PROTO;
  return ret;
}


static u8 * wpa_sm_alloc_eapol(const struct wpa_supplicant *wpa_s, u8 type,
                               const void *data, u16 data_len,
                               size_t *msg_len, void **data_pos)
{
  struct ieee802_1x_hdr *hdr;
  
  *msg_len = sizeof(*hdr) + data_len;
  hdr = os_malloc(*msg_len);
  if (hdr == NULL)
    return NULL;
  hdr->version = 1;
  hdr->type = type;
  hdr->length = host_to_be16(data_len);
  
  if (data)
    os_memcpy(hdr + 1, data, data_len);
  else
    os_memset(hdr + 1, 0, data_len);
  
  
  if (data_pos)
    *data_pos = hdr + 1;
  
  return (u8 *) hdr;
}



/**
* wpa_sm_key_request - Send EAPOL-Key Request
* @sm: Pointer to WPA state machine data from wpa_sm_init()
* @error: Indicate whether this is an Michael MIC error report
* @pairwise: 1 = error report for pairwise packet, 0 = for group packet
*
* Send an EAPOL-Key Request to the current authenticator. This function is
* used to request rekeying and it is usually called when a local Michael MIC
* failure is detected.
*/
void wpa_sm_key_request(struct wpa_sm *sm, int error, int pairwise)
{
  size_t mic_len, hdrlen, rlen;
  struct wpa_eapol_key *reply;
  struct wpa_eapol_key_192 *reply192;
  int key_info, ver;
  u8 bssid[ETH_ALEN], *rbuf, *key_mic;
  
  if (sm->key_mgmt == WPA_KEY_MGMT_OSEN ||
      wpa_key_mgmt_suite_b(sm->key_mgmt))
    ver = WPA_KEY_INFO_TYPE_AKM_DEFINED;
  else if (wpa_key_mgmt_ft(sm->key_mgmt) ||
           wpa_key_mgmt_sha256(sm->key_mgmt))
    ver = WPA_KEY_INFO_TYPE_AES_128_CMAC;
  else if (sm->pairwise_cipher != WPA_CIPHER_TKIP)
    ver = WPA_KEY_INFO_TYPE_HMAC_SHA1_AES;
  else
    ver = WPA_KEY_INFO_TYPE_HMAC_MD5_RC4;
  
  if (wpa_sm_get_bssid(sm, bssid) < 0) {
    wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
            "Failed to read BSSID for EAPOL-Key request");
    return;
  }
  
  mic_len = wpa_mic_len(sm->key_mgmt);
  hdrlen = mic_len == 24 ? sizeof(*reply192) : sizeof(*reply);
  rbuf = wpa_sm_alloc_eapol((struct wpa_supplicant*)sm, IEEE802_1X_TYPE_EAPOL_KEY, NULL,
                            hdrlen, &rlen, (void *) &reply);
  if (rbuf == NULL)
    return;
  reply192 = (struct wpa_eapol_key_192 *) reply;
  
  reply->type = (sm->proto == WPA_PROTO_RSN ||
                 sm->proto == WPA_PROTO_OSEN) ?
EAPOL_KEY_TYPE_RSN : EAPOL_KEY_TYPE_WPA;
key_info = WPA_KEY_INFO_REQUEST | ver;
if (sm->ptk_set)
key_info |= WPA_KEY_INFO_MIC | WPA_KEY_INFO_SECURE;
if (error)
key_info |= WPA_KEY_INFO_ERROR;
if (pairwise)
key_info |= WPA_KEY_INFO_KEY_TYPE;
WPA_PUT_BE16(reply->key_info, key_info);
WPA_PUT_BE16(reply->key_length, 0);
os_memcpy(reply->replay_counter, sm->request_counter,
          WPA_REPLAY_COUNTER_LEN);
inc_byte_array(sm->request_counter, WPA_REPLAY_COUNTER_LEN);

if (mic_len == 24)
WPA_PUT_BE16(reply192->key_data_length, 0);
else
WPA_PUT_BE16(reply->key_data_length, 0);
if (!(key_info & WPA_KEY_INFO_MIC))
key_mic = NULL;
else
key_mic = reply192->key_mic; /* same offset in reply */

wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
        "WPA: Sending EAPOL-Key Request (error=%d "
          "pairwise=%d ptk_set=%d len=%lu)",
          error, pairwise, sm->ptk_set, (unsigned long) rlen);
wpa_eapol_key_send(sm, sm->ptk.kck, sm->ptk.kck_len, ver, bssid,
                   ETH_P_EAPOL, rbuf, rlen, key_mic);
}


static void wpa_supplicant_key_mgmt_set_pmk(struct wpa_sm *sm)
{
#ifdef CONFIG_IEEE80211R
  if (sm->key_mgmt == WPA_KEY_MGMT_FT_IEEE8021X) {
    if (wpa_sm_key_mgmt_set_pmk(sm, sm->xxkey, sm->xxkey_len))
      wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
              "RSN: Cannot set low order 256 bits of MSK for key management offload");
  } else {
#endif /* CONFIG_IEEE80211R */
    if (wpa_sm_key_mgmt_set_pmk(sm, sm->pmk, sm->pmk_len))
      wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
              "RSN: Cannot set PMK for key management offload");
#ifdef CONFIG_IEEE80211R
  }
#endif /* CONFIG_IEEE80211R */
}



/**
* pmksa_cache_get - Fetch a PMKSA cache entry
* @pmksa: Pointer to PMKSA cache data from pmksa_cache_init()
* @aa: Authenticator address or %NULL to match any
* @pmkid: PMKID or %NULL to match any
* @network_ctx: Network context or %NULL to match any
* Returns: Pointer to PMKSA cache entry or %NULL if no match was found
*/
struct rsn_pmksa_cache_entry * pmksa_cache_get(struct rsn_pmksa_cache *pmksa,
					       const u8 *aa, const u8 *pmkid,
					       const void *network_ctx)
{
  struct rsn_pmksa_cache_entry *entry = pmksa->pmksa;
  while (entry) {
    if ((aa == NULL || os_memcmp(entry->aa, aa, ETH_ALEN) == 0) &&
        (pmkid == NULL ||
         os_memcmp(entry->pmkid, pmkid, PMKID_LEN) == 0) &&
          (network_ctx == NULL || network_ctx == entry->network_ctx))
      return entry;
    entry = entry->next;
  }
  return NULL;
}

static void _pmksa_cache_free_entry(struct rsn_pmksa_cache_entry *entry)
{
  bin_clear_free(entry, sizeof(*entry));
}   

static void pmksa_cache_free_entry(struct rsn_pmksa_cache *pmksa,
                                   struct rsn_pmksa_cache_entry *entry,
                                   enum pmksa_free_reason reason)
{
  
  pmksa->pmksa_count--;
  pmksa->free_cb(entry, pmksa->ctx, reason);
  _pmksa_cache_free_entry(entry);
}


void pmksa_cache_clear_current(struct wpa_sm *sm)
{
  if (sm == NULL)
    return;
  sm->cur_pmksa = NULL;
}

/**
* pmksa_cache_flush - Flush PMKSA cache entries for a specific network
* @pmksa: Pointer to PMKSA cache data from pmksa_cache_init()
* @network_ctx: Network configuration context or %NULL to flush all entries
* @pmk: PMK to match for or %NYLL to match all PMKs
* @pmk_len: PMK length
*/
void pmksa_cache_flush(struct rsn_pmksa_cache *pmksa, void *network_ctx,
                       const u8 *pmk, size_t pmk_len)
{
  struct rsn_pmksa_cache_entry *entry, *prev = NULL, *tmp;
  int removed = 0;
  
  entry = pmksa->pmksa;
  while (entry) {
    if ((entry->network_ctx == network_ctx ||
         network_ctx == NULL) &&
        (pmk == NULL ||
         (pmk_len == entry->pmk_len &&
          os_memcmp(pmk, entry->pmk, pmk_len) == 0))) {
            wpa_printf(MSG_DEBUG, "RSN: Flush PMKSA cache entry "
                       "for " MACSTR, MAC2STR(entry->aa));
            if (prev)
              prev->next = entry->next;
            else
              pmksa->pmksa = entry->next;
            tmp = entry;
            entry = entry->next;
            pmksa_cache_free_entry(pmksa, tmp, PMKSA_FREE);
            removed++;
          } else {
            prev = entry;
            entry = entry->next;
          }
  }
  
}



/**
* pmksa_cache_add - Add a PMKSA cache entry
* @pmksa: Pointer to PMKSA cache data from pmksa_cache_init()
* @pmk: The new pairwise master key
* @pmk_len: PMK length in bytes, usually PMK_LEN (32)
* @pmkid: Calculated PMKID
* @kck: Key confirmation key or %NULL if not yet derived
* @kck_len: KCK length in bytes
* @aa: Authenticator address
* @spa: Supplicant address
* @network_ctx: Network configuration context for this PMK
* @akmp: WPA_KEY_MGMT_* used in key derivation
* Returns: Pointer to the added PMKSA cache entry or %NULL on error
*
* This function create a PMKSA entry for a new PMK and adds it to the PMKSA
* cache. If an old entry is already in the cache for the same Authenticator,
* this entry will be replaced with the new entry. PMKID will be calculated
* based on the PMK and the driver interface is notified of the new PMKID.
*/
struct rsn_pmksa_cache_entry *
pmksa_cache_add(struct rsn_pmksa_cache *pmksa, const u8 *pmk, size_t pmk_len,
                const u8 *pmkid, const u8 *kck, size_t kck_len,
                const u8 *aa, const u8 *spa, void *network_ctx, int akmp)
{
  struct rsn_pmksa_cache_entry *entry, *pos, *prev;
  struct os_reltime now;
  
  if (pmk_len > PMK_LEN_MAX)
    return NULL;
  
  if (wpa_key_mgmt_suite_b(akmp) && !kck)
    return NULL;
  
  entry = os_zalloc(sizeof(*entry));
  if (entry == NULL)
    return NULL;
  os_memcpy(entry->pmk, pmk, pmk_len);
  entry->pmk_len = pmk_len;
  if (pmkid)
    os_memcpy(entry->pmkid, pmkid, PMKID_LEN);
  else if (akmp == WPA_KEY_MGMT_IEEE8021X_SUITE_B_192)
    rsn_pmkid_suite_b_192(kck, kck_len, aa, spa, entry->pmkid);
  else if (wpa_key_mgmt_suite_b(akmp))
    rsn_pmkid_suite_b(kck, kck_len, aa, spa, entry->pmkid);
  else
    rsn_pmkid(pmk, pmk_len, aa, spa, entry->pmkid,
              wpa_key_mgmt_sha256(akmp));
  os_get_reltime(&now);
  entry->expiration = now.sec + pmksa->sm->dot11RSNAConfigPMKLifetime;
  entry->reauth_time = now.sec + pmksa->sm->dot11RSNAConfigPMKLifetime *
    pmksa->sm->dot11RSNAConfigPMKReauthThreshold / 100;
  entry->akmp = akmp;
  os_memcpy(entry->aa, aa, ETH_ALEN);
  entry->network_ctx = network_ctx;
  
  /* Replace an old entry for the same Authenticator (if found) with the
  * new entry */
  pos = pmksa->pmksa;
  prev = NULL;
  while (pos) {
    if (os_memcmp(aa, pos->aa, ETH_ALEN) == 0) {
      if (pos->pmk_len == pmk_len &&
          os_memcmp_const(pos->pmk, pmk, pmk_len) == 0 &&
            os_memcmp_const(pos->pmkid, entry->pmkid,
                            PMKID_LEN) == 0) {
                              wpa_printf(MSG_DEBUG, "WPA: reusing previous "
                                         "PMKSA entry");
                              os_free(entry);
                              return pos;
                            }
      if (prev == NULL)
        pmksa->pmksa = pos->next;
      else
        prev->next = pos->next;
      
      /*
      * If OKC is used, there may be other PMKSA cache
      * entries based on the same PMK. These needs to be
      * flushed so that a new entry can be created based on
      * the new PMK. Only clear other entries if they have a
      * matching PMK and this PMK has been used successfully
      * with the current AP, i.e., if opportunistic flag has
      * been cleared in wpa_supplicant_key_neg_complete().
      */
      wpa_printf(MSG_DEBUG, "RSN: Replace PMKSA entry for "
                 "the current AP and any PMKSA cache entry "
                   "that was based on the old PMK");
      if (!pos->opportunistic)
        pmksa_cache_flush(pmksa, network_ctx, pos->pmk,
                          pos->pmk_len);
      pmksa_cache_free_entry(pmksa, pos, PMKSA_REPLACE);
      break;
    }
    prev = pos;
    pos = pos->next;
  }
  
  if (pmksa->pmksa_count >= 32 && pmksa->pmksa) {
    /* Remove the oldest entry to make room for the new entry */
    pos = pmksa->pmksa;
    
    if (pos == pmksa->sm->cur_pmksa) {
      /*
      * Never remove the current PMKSA cache entry, since
      * it's in use, and removing it triggers a needless
      * deauthentication.
      */
      pos = pos->next;
      pmksa->pmksa->next = pos ? pos->next : NULL;
    } else
      pmksa->pmksa = pos->next;
    
    if (pos) {
      wpa_printf(MSG_DEBUG, "RSN: removed the oldest idle "
                 "PMKSA cache entry (for " MACSTR ") to "
                   "make room for new one",
                   MAC2STR(pos->aa));
      pmksa_cache_free_entry(pmksa, pos, PMKSA_FREE);
    }
  }
  
  /* Add the new entry; order by expiration time */
  pos = pmksa->pmksa;
  prev = NULL;
  while (pos) {
    if (pos->expiration > entry->expiration)
      break;
    prev = pos;
    pos = pos->next;
  }
  if (prev == NULL) {
    entry->next = pmksa->pmksa;
    pmksa->pmksa = entry;
    pmksa_cache_set_expiration(pmksa);
  } else {
    entry->next = prev->next;
    prev->next = entry;
  }
  pmksa->pmksa_count++;
  wpa_printf(MSG_DEBUG, "RSN: Added PMKSA cache entry for " MACSTR
             " network_ctx=%p", MAC2STR(entry->aa), network_ctx);
  
  return entry;
}



/**
* wpa_sm_set_pmk_from_pmksa - Set PMK based on the current PMKSA
* @sm: Pointer to WPA state machine data from wpa_sm_init()
*
* Take the PMK from the current PMKSA into use. If no PMKSA is active, the PMK
* will be cleared.
*/
void wpa_sm_set_pmk_from_pmksa(struct wpa_sm *sm)
{
  if (sm == NULL)
    return;
  
  if (sm->cur_pmksa) {
    sm->pmk_len = sm->cur_pmksa->pmk_len;
    os_memcpy(sm->pmk, sm->cur_pmksa->pmk, sm->pmk_len);
  } else {
    sm->pmk_len = PMK_LEN;
    os_memset(sm->pmk, 0, PMK_LEN);
  }
}


#if 0
static int wpa_supplicant_get_pmk(struct wpa_sm *sm,
				  const unsigned char *src_addr,
				  const u8 *pmkid)
{
  int abort_cached = 0;
  
  if (pmkid && !sm->cur_pmksa) {
    /* When using drivers that generate RSN IE, wpa_supplicant may
    * not have enough time to get the association information
    * event before receiving this 1/4 message, so try to find a
    * matching PMKSA cache entry here. */
    sm->cur_pmksa = pmksa_cache_get(sm->pmksa, src_addr, pmkid,
                                    NULL);
    if (sm->cur_pmksa) {
      wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
              "RSN: found matching PMKID from PMKSA cache");
    } else {
      wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
              "RSN: no matching PMKID found");
      abort_cached = 1;
    }
  }
  
  if (pmkid && sm->cur_pmksa &&
      os_memcmp_const(pmkid, sm->cur_pmksa->pmkid, PMKID_LEN) == 0) {
        wpa_hexdump(MSG_DEBUG, "RSN: matched PMKID", pmkid, PMKID_LEN);
        wpa_sm_set_pmk_from_pmksa(sm);
        wpa_hexdump_key(MSG_DEBUG, "RSN: PMK from PMKSA cache",
                        sm->pmk, sm->pmk_len);
        eapol_sm_notify_cached(sm->eapol);
#ifdef CONFIG_IEEE80211R
        sm->xxkey_len = 0;
#endif /* CONFIG_IEEE80211R */
      } else if (wpa_key_mgmt_wpa_ieee8021x(sm->key_mgmt) && sm->eapol) {
        int res, pmk_len;
        
        if (sm->key_mgmt & WPA_KEY_MGMT_IEEE8021X_SUITE_B_192)
          pmk_len = PMK_LEN_SUITE_B_192;
        else
          pmk_len = PMK_LEN;
        res = eapol_sm_get_key(sm->eapol, sm->pmk, pmk_len);
        //res = 0;
        if (res) {
          if (pmk_len == PMK_LEN) {
            /*
            * EAP-LEAP is an exception from other EAP
            * methods: it uses only 16-byte PMK.
            */
            res = eapol_sm_get_key(sm->eapol, sm->pmk, 16);
            pmk_len = 16;
          }
        } else {
#ifdef CONFIG_IEEE80211R
          u8 buf[2 * PMK_LEN];
          if (eapol_sm_get_key(sm->eapol, buf, 2 * PMK_LEN) == 0)
          {
            os_memcpy(sm->xxkey, buf + PMK_LEN, PMK_LEN);
            sm->xxkey_len = PMK_LEN;
            os_memset(buf, 0, sizeof(buf));
          }
#endif /* CONFIG_IEEE80211R */
        }
        if (res == 0) {
          struct rsn_pmksa_cache_entry *sa = NULL;
          wpa_hexdump_key(MSG_DEBUG, "WPA: PMK from EAPOL state "
                          "machines", sm->pmk, pmk_len);
          sm->pmk_len = pmk_len;
          wpa_supplicant_key_mgmt_set_pmk(sm);
          if (sm->proto == WPA_PROTO_RSN &&
              !wpa_key_mgmt_suite_b(sm->key_mgmt) &&
                !wpa_key_mgmt_ft(sm->key_mgmt)) {
                  sa = pmksa_cache_add(sm->pmksa,
                                       sm->pmk, pmk_len, NULL,
                                       NULL, 0,
                                       src_addr, sm->own_addr,
                                       sm->network_ctx,
                                       sm->key_mgmt);
                }
          if (!sm->cur_pmksa && pmkid &&
              pmksa_cache_get(sm->pmksa, src_addr, pmkid, NULL))
          {
            wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
                    "RSN: the new PMK matches with the "
                      "PMKID");
            abort_cached = 0;
          } else if (sa && !sm->cur_pmksa && pmkid) {
            /*
            * It looks like the authentication server
            * derived mismatching MSK. This should not
            * really happen, but bugs happen.. There is not
            * much we can do here without knowing what
            * exactly caused the server to misbehave.
            */
            wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
                    "RSN: PMKID mismatch - authentication server may have derived different MSK?!");
            return -1;
          }
          
          if (!sm->cur_pmksa)
            sm->cur_pmksa = sa;
        } else {
          wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
                  "WPA: Failed to get master session key from "
                    "EAPOL state machines - key handshake "
                      "aborted");
          if (sm->cur_pmksa) {
            wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
                    "RSN: Cancelled PMKSA caching "
                      "attempt");
            sm->cur_pmksa = NULL;
            abort_cached = 1;
          } else if (!abort_cached) {
            return -1;
          }
        }
      }
  
  if (abort_cached && wpa_key_mgmt_wpa_ieee8021x(sm->key_mgmt) &&
      !wpa_key_mgmt_suite_b(sm->key_mgmt) &&
        !wpa_key_mgmt_ft(sm->key_mgmt) && sm->key_mgmt != WPA_KEY_MGMT_OSEN)
  {
    /* Send EAPOL-Start to trigger full EAP authentication. */
    u8 *buf;
    size_t buflen;
    
    wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
            "RSN: no PMKSA entry found - trigger "
              "full EAP authentication");
    buf = wpa_sm_alloc_eapol((struct wpa_supplicant *)sm, IEEE802_1X_TYPE_EAPOL_START,
                             NULL, 0, &buflen, NULL);
    if (buf) {
      wpa_sm_ether_send(sm, sm->bssid, ETH_P_EAPOL,
                        buf, buflen);
      os_free(buf);
      return -2;
    }
    
    return -1;
  }
  
  return 0;
}
#endif

/**
* wpa_supplicant_send_2_of_4 - Send message 2 of WPA/RSN 4-Way Handshake
* @sm: Pointer to WPA state machine data from wpa_sm_init()
* @dst: Destination address for the frame
* @key: Pointer to the EAPOL-Key frame header
* @ver: Version bits from EAPOL-Key Key Info
* @nonce: Nonce value for the EAPOL-Key frame
* @wpa_ie: WPA/RSN IE
* @wpa_ie_len: Length of the WPA/RSN IE
* @ptk: PTK to use for keyed hash and encryption
* Returns: >= 0 on success, < 0 on failure
*/
int wpa_supplicant_send_2_of_4(struct wpa_sm *sm, const unsigned char *dst,
			       const struct wpa_eapol_key *key,
			       int ver, const u8 *nonce,
			       const u8 *wpa_ie, size_t wpa_ie_len,
			       struct wpa_ptk *ptk)
{
  size_t mic_len, hdrlen, rlen;
  struct wpa_eapol_key *reply;
  struct wpa_eapol_key_192 *reply192;
  u8 *rbuf, *key_mic;
  u8 *rsn_ie_buf = NULL;
  int i=0;
  if (wpa_ie == NULL) {
    wpa_msg(sm->ctx->msg_ctx, MSG_WARNING, "WPA: No wpa_ie set - "
            "cannot generate msg 2/4");
    return -1;
  }
  
#ifdef CONFIG_IEEE80211R
  if (wpa_key_mgmt_ft(sm->key_mgmt)) {
    int res;
    
    /*
    * Add PMKR1Name into RSN IE (PMKID-List) and add MDIE and
    * FTIE from (Re)Association Response.
    */
    rsn_ie_buf = os_malloc(wpa_ie_len + 2 + 2 + PMKID_LEN +
                           sm->assoc_resp_ies_len);
    if (rsn_ie_buf == NULL)
      return -1;
    os_memcpy(rsn_ie_buf, wpa_ie, wpa_ie_len);
    res = wpa_insert_pmkid(rsn_ie_buf, &wpa_ie_len,
                           sm->pmk_r1_name);
    if (res < 0) {
      os_free(rsn_ie_buf);
      return -1;
    }
    
    if (sm->assoc_resp_ies) {
      os_memcpy(rsn_ie_buf + wpa_ie_len, sm->assoc_resp_ies,
                sm->assoc_resp_ies_len);
      wpa_ie_len += sm->assoc_resp_ies_len;
    }
    
    wpa_ie = rsn_ie_buf;
  }
#endif /* CONFIG_IEEE80211R */
  
  wpa_hexdump(MSG_DEBUG, "WPA: WPA IE for msg 2/4", wpa_ie, wpa_ie_len);
  
  mic_len = wpa_mic_len(sm->key_mgmt);
  hdrlen = mic_len == 24 ? sizeof(*reply192) : sizeof(*reply);
  rbuf = wpa_sm_alloc_eapol((struct wpa_supplicant *)sm, IEEE802_1X_TYPE_EAPOL_KEY,
                            NULL, hdrlen + wpa_ie_len,
                            &rlen, (void *) &reply);
  if (rbuf == NULL) {
    os_free(rsn_ie_buf);
    return -1;
  }
  reply192 = (struct wpa_eapol_key_192 *) reply;
  
  reply->type = (sm->proto == WPA_PROTO_RSN ||
                 sm->proto == WPA_PROTO_OSEN) ?
EAPOL_KEY_TYPE_RSN : EAPOL_KEY_TYPE_WPA;
WPA_PUT_BE16(reply->key_info,
             ver | WPA_KEY_INFO_KEY_TYPE | WPA_KEY_INFO_MIC);
if (sm->proto == WPA_PROTO_RSN || sm->proto == WPA_PROTO_OSEN)
WPA_PUT_BE16(reply->key_length, 0);
else
os_memcpy(reply->key_length, key->key_length, 2);
os_memcpy(reply->replay_counter, key->replay_counter,
          WPA_REPLAY_COUNTER_LEN);
wpa_hexdump(MSG_DEBUG, "WPA: Replay Counter", reply->replay_counter,
            WPA_REPLAY_COUNTER_LEN);

key_mic = reply192->key_mic; /* same offset for reply and reply192 */

if (mic_len == 24) {
  WPA_PUT_BE16(reply192->key_data_length, wpa_ie_len);
  os_memcpy(reply192 + 1, wpa_ie, wpa_ie_len);
} else {
  WPA_PUT_BE16(reply->key_data_length, wpa_ie_len);
  os_memcpy(reply + 1, wpa_ie, wpa_ie_len);
}

os_free(rsn_ie_buf);

os_memcpy(reply->key_nonce, nonce, WPA_NONCE_LEN);


wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "WPA: Sending EAPOL-Key 2/4");
return wpa_eapol_key_send(sm, ptk->kck, ptk->kck_len, ver, dst,
                          ETH_P_EAPOL, rbuf, rlen, key_mic);
}


static int wpa_derive_ptk(struct wpa_sm *sm, const unsigned char *src_addr,
			  const struct wpa_eapol_key *key, struct wpa_ptk *ptk)
{
  
#ifdef CONFIG_IEEE80211R
  if (wpa_key_mgmt_ft(sm->key_mgmt))
    return wpa_derive_ptk_ft(sm, src_addr, key, ptk);
#endif /* CONFIG_IEEE80211R */
  
  return wpa_pmk_to_ptk(sm->pmk, sm->pmk_len, "Pairwise key expansion",
                        sm->own_addr, src_addr, sm->snonce,
                        key->key_nonce, ptk, sm->key_mgmt,
                        sm->pairwise_cipher);
}

void *wpa_sm_get_network_ctx(struct wpa_sm *sm)
{
  
  return NULL;
}


static int wpa_parse_generic(const u8 * pos, const u8 * end,
                             struct wpa_eapol_ie_parse *ie)
{
  if (pos[1] == 0)
    return 1;
  
  if (pos[1] >= 6 &&
      RSN_SELECTOR_GET(pos + 2) == WPA_OUI_TYPE &&
        pos[2 + WPA_LEN_SELECTOR] == 1 && pos[2 + WPA_LEN_SELECTOR + 1] == 0) {
          ie->wpa_ie = pos;
          ie->wpa_ie_len = pos[1] + 2;
          wpa_hexdump(MSG_DEBUG, "WPA: WPA IE in EAPOL-Key",
                      ie->wpa_ie, ie->wpa_ie_len);
          return 0;
	}
  
  if (pos + 1 + _SELECTOR_LEN_TO_RSN < end &&
      pos[1] >= _SELECTOR_LEN_TO_RSN + TK_PMK_ID_LEN &&
        RSN_SELECTOR_GET(pos + 2) == RSN_KEY_DATA_PMKID) {
          ie->pmkid = pos + 2 + _SELECTOR_LEN_TO_RSN;
          wpa_hexdump(MSG_DEBUG, "WPA: PMKID in EAPOL-Key", pos, pos[1] + 2);
          return 0;
	}
  
  if (pos[1] > _SELECTOR_LEN_TO_RSN + 2 &&
      RSN_SELECTOR_GET(pos + 2) == RSN_KEY_DATA_GROUPKEY) {
        ie->gtk = pos + 2 + _SELECTOR_LEN_TO_RSN;
        ie->gtk_len = pos[1] - _SELECTOR_LEN_TO_RSN;
        wpa_hexdump_key(MSG_DEBUG, "WPA: GTK in EAPOL-Key", pos, pos[1] + 2);
        return 0;
      }
  
  if (pos[1] > _SELECTOR_LEN_TO_RSN + 2 &&
      RSN_SELECTOR_GET(pos + 2) == RSN_KEY_DATA_MAC_ADDR) {
        ie->mac_addr = pos + 2 + _SELECTOR_LEN_TO_RSN;
        ie->mac_addr_len = pos[1] - _SELECTOR_LEN_TO_RSN;
        wpa_hexdump(MSG_DEBUG, "WPA: MAC Address in EAPOL-Key",
                    pos, pos[1] + 2);
        return 0;
      }
#ifdef CONFIG_PEERKEY
  if (pos[1] > _SELECTOR_LEN_TO_RSN + 2 &&
      RSN_SELECTOR_GET(pos + 2) == RSN_KEY_DATA_SMK) {
        ie->smk = pos + 2 + _SELECTOR_LEN_TO_RSN;
        ie->smk_len = pos[1] - _SELECTOR_LEN_TO_RSN;
        wpa_hexdump_key(MSG_DEBUG, "WPA: SMK in EAPOL-Key", pos, pos[1] + 2);
        return 0;
      }
  
  if (pos[1] > _SELECTOR_LEN_TO_RSN + 2 &&
      RSN_SELECTOR_GET(pos + 2) == RSN_KEY_DATA_NONCE) {
        ie->nonce = pos + 2 + _SELECTOR_LEN_TO_RSN;
        ie->nonce_len = pos[1] - _SELECTOR_LEN_TO_RSN;
        wpa_hexdump(MSG_DEBUG, "WPA: Nonce in EAPOL-Key", pos, pos[1] + 2);
        return 0;
      }
  
  if (pos[1] > _SELECTOR_LEN_TO_RSN + 2 &&
      RSN_SELECTOR_GET(pos + 2) == RSN_KEY_DATA_LIFETIME) {
        ie->lifetime = pos + 2 + _SELECTOR_LEN_TO_RSN;
        ie->lifetime_len = pos[1] - _SELECTOR_LEN_TO_RSN;
        wpa_hexdump(MSG_DEBUG, "WPA: Lifetime in EAPOL-Key", pos, pos[1] + 2);
        return 0;
      }
  
  if (pos[1] > _SELECTOR_LEN_TO_RSN + 2 &&
      RSN_SELECTOR_GET(pos + 2) == RSN_KEY_DATA_ERROR) {
        ie->error = pos + 2 + _SELECTOR_LEN_TO_RSN;
        ie->error_len = pos[1] - _SELECTOR_LEN_TO_RSN;
        wpa_hexdump(MSG_DEBUG, "WPA: Error in EAPOL-Key", pos, pos[1] + 2);
        return 0;
      }
#endif
  
#ifdef CONFIG_IEEE80211W
  if (pos[1] > _SELECTOR_LEN_TO_RSN + 2 &&
      RSN_SELECTOR_GET(pos + 2) == RSN_KEY_DATA_IGTK) {
        ie->igtk = pos + 2 + _SELECTOR_LEN_TO_RSN;
        ie->igtk_len = pos[1] - _SELECTOR_LEN_TO_RSN;
        wpa_hexdump_key(MSG_DEBUG, "WPA: IGTK in EAPOL-Key", pos, pos[1] + 2);
        return 0;
      }
#endif
  
#ifdef CONFIG_P2P
  if (pos[1] >= _SELECTOR_LEN_TO_RSN + 1 &&
      RSN_SELECTOR_GET(pos + 2) == WFA_KEY_DATA_IP_ADDR_REQ) {
        ie->ip_addr_req = pos + 2 + _SELECTOR_LEN_TO_RSN;
        wpa_hexdump(MSG_DEBUG, "WPA: IP Address Request in EAPOL-Key",
                    ie->ip_addr_req, pos[1] - _SELECTOR_LEN_TO_RSN);
        return 0;
      }
  
  if (pos[1] >= _SELECTOR_LEN_TO_RSN + 3 * 4 &&
      RSN_SELECTOR_GET(pos + 2) == WFA_KEY_DATA_IP_ADDR_ALLOC) {
        ie->ip_addr_alloc = pos + 2 + _SELECTOR_LEN_TO_RSN;
        wpa_hexdump(MSG_DEBUG,
                    "WPA: IP Address Allocation in EAPOL-Key",
                    ie->ip_addr_alloc, pos[1] - _SELECTOR_LEN_TO_RSN);
        return 0;
      }
#endif
  
  return 0;
}


static int wpa_parse_vendor_specific(const u8 * pos, const u8 * end,
                                     struct wpa_eapol_ie_parse *ie)
{
  unsigned int oui;
  
  if (pos[1] < 4) {
    wpa_printf(MSG_MSGDUMP, "Too short vendor specific IE ignored (len=%u)",
               pos[1]);
    return 1;
  }
  
  oui = WPA_GET_BE24(&pos[2]);
  if (oui == OUI_MS && pos[5] == WMM_TYP_OUI && pos[1] > 4) {
    if (pos[6] == WMM_CHILDTYPE_INFORMATION_ELEMENT_OUI) {
      ie->wmm = &pos[2];
      ie->wmm_len = pos[1];
      wpa_hexdump(MSG_DEBUG, "WPA: WMM IE", ie->wmm, ie->wmm_len);
    } else if (pos[6] == WMM_CHILDTYPE_PARAMETER_ELEMENT_OUI) {
      ie->wmm = &pos[2];
      ie->wmm_len = pos[1];
      wpa_hexdump(MSG_DEBUG, "WPA: WMM Parameter Element",
                  ie->wmm, ie->wmm_len);
    }
  }
  return 0;
}



int wpa_supplicant_parse_ies(const u8 * buf, size_t len,
                             struct wpa_eapol_ie_parse *ie)
{
  const u8 *pos, *end;
  int ret = 0;
  
  os_memset(ie, 0, sizeof(*ie));
  for (pos = buf, end = pos + len; pos + 1 < end; pos += 2 + pos[1]) {
    if (pos[0] == 0xdd && ((pos == buf + len - 1) || pos[1] == 0)) {
      
      break;
    }
    if (pos + 2 + pos[1] > end) {
      wpa_printf(MSG_DEBUG, "WPA: EAPOL-Key Key Data "
                 "underflow (ie=%d len=%d pos=%d)",
                 pos[0], pos[1], (int)(pos - buf));
      wpa_hexdump_key(MSG_DEBUG, "WPA: Key Data", buf, len);
      ret = -1;
      break;
    }
    if (*pos == WLAN_RSN_EID) {
      ie->rsn_ie = pos;
      ie->rsn_ie_len = pos[1] + 2;
      
    } else if (*pos == WLAN_MOBILITY_FIELD_EID &&
               pos[1] >= sizeof(struct rsn_mdie)) {
                 ie->mdie = pos;
                 ie->mdie_len = pos[1] + 2;
                 
               } else if (*pos == WLAN_FAST_BSS_CHANGE_EID &&
                          pos[1] >= sizeof(struct rsn_ftie)) {
                            ie->ftie = pos;
                            ie->ftie_len = pos[1] + 2;
                            
                          } else if (*pos == WLAN_OVERTIME_INTERVAL_EID && pos[1] >= 5) {
                            if (pos[2] == WLAN_TIMEOUT_REASSOC_DEADLINE) {
                              ie->reassoc_deadline = pos;
                              wpa_hexdump(MSG_DEBUG, "WPA: Reassoc Deadline "
                                          "in EAPOL-Key", ie->reassoc_deadline, pos[1] + 2);
                            } else if (pos[2] == WLAN_TIMEOUT_KEY_LIFETIME) {
                              ie->key_lifetime = pos;
                              wpa_hexdump(MSG_DEBUG, "WPA: KeyLifetime "
                                          "in EAPOL-Key", ie->key_lifetime, pos[1] + 2);
                            } else {
                              wpa_hexdump(MSG_DEBUG, "WPA: Unrecognized "
                                          "EAPOL-Key Key Data IE", pos, 2 + pos[1]);
                            }
                          } else if (*pos == WLAN_EID_LINK_ID) {
                            if (pos[1] >= 18) {
                              ie->lnkid = pos;
                              ie->lnkid_len = pos[1] + 2;
                            }
                          } else if (*pos == WLAN_EID_EXT_CAPAB) {
                            ie->ext_capab = pos;
                            ie->ext_capab_len = pos[1] + 2;
                          } else if (*pos == WLAN_SUPP_RATES_EID) {
                            ie->supp_rates = pos;
                            ie->supp_rates_len = pos[1] + 2;
                          } else if (*pos == WLAN_EXT_SUPP_RATES_EID) {
                            ie->ext_supp_rates = pos;
                            ie->ext_supp_rates_len = pos[1] + 2;
                          } else if (*pos == WLAN_HT_CAP_EID &&
                                     pos[1] >= sizeof(struct ieee80211_ht_capabilities)) {
                                       ie->ht_capabilities = pos + 2;
                                     } else if (*pos == WLAN_EID_VHT_AID) {
                                       if (pos[1] >= 2)
                                         ie->aid = WPA_GET_LE16(pos + 2) & 0x3fff;
                                     } else if (*pos == WLAN_EID_VHT_CAP &&
                                                pos[1] >= sizeof(struct ieee80211_vht_capabilities)) {
                                                  ie->vht_capabilities = pos + 2;
                                                } else if (*pos == WLAN_EID_QOS && pos[1] >= 1) {
                                                  ie->qosinfo = pos[2];
                                                } else if (*pos == WLAN_SUSTAINS_CHANNELS_EID) {
                                                  ie->supp_channels = pos + 2;
                                                  ie->supp_channels_len = pos[1];
                                                } else if (*pos == WLAN_EID_SUPPORTED_OPERATING_CLASSES) {
                                                  
                                                  if (pos[1] >= 2 && pos[1] <= 253) {
                                                    ie->supp_oper_classes = pos + 2;
                                                    ie->supp_oper_classes_len = pos[1];
                                                  }
                                                } else if (*pos == WLAN_VENDOR_SPECIFIC_EID) {
                                                  ret = wpa_parse_generic(pos, end, ie);
                                                  if (ret < 0)
                                                    break;
                                                  if (ret > 0) {
                                                    ret = 0;
                                                    break;
                                                  }
                                                  
                                                  ret = wpa_parse_vendor_specific(pos, end, ie);
                                                  if (ret < 0)
                                                    break;
                                                  if (ret > 0) {
                                                    ret = 0;
                                                    break;
                                                  }
                                                } else {
                                                  wpa_hexdump(MSG_DEBUG, "WPA: Unrecognized EAPOL-Key "
                                                              "Key Data IE", pos, 2 + pos[1]);
                                                }
  }
  
  return ret;
}


static void wpa_supplicant_process_1_of_4(struct wpa_sm *sm,
					  const unsigned char *src_addr,
					  const struct wpa_eapol_key *key,
					  u16 ver, const u8 *key_data,
					  size_t key_data_len)
{
  struct wpa_eapol_ie_parse ie;
  struct wpa_ptk *ptk;
  int res;
  u8 *kde, *kde_buf = NULL;
  size_t kde_len;
  nic_info_st *pnic_info = (nic_info_st *)sm->pnic_info;
  sec_info_st *sec_info = pnic_info->sec_info;
  wf_80211_mgmt_ie_t *pie = NULL;
  int i=0;
  wpa_sm_set_state(sm, WPA_4WAY_HANDSHAKE);
  
  os_memset(&ie, 0, sizeof(ie));
  
  if (sm->proto == WPA_PROTO_RSN || sm->proto == WPA_PROTO_OSEN) {
    /* RSN: msg 1/4 should contain PMKID for the selected PMK */
    wpa_hexdump(MSG_DEBUG, "RSN: msg 1/4 key data",
                key_data, key_data_len);
    if (wpa_supplicant_parse_ies(key_data, key_data_len, &ie) < 0)
      goto failed;
    if (ie.pmkid) {
      wpa_hexdump(MSG_DEBUG, "RSN: PMKID from "
                  "Authenticator", ie.pmkid, PMKID_LEN);
    }
  }
  
#if 0
  res = wpa_supplicant_get_pmk(sm, src_addr, ie.pmkid);
  if (res == -2) {
    wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "RSN: Do not reply to "
            "msg 1/4 - requesting full EAP authentication");
    return;
  }
  if (res)
    return;
#endif
  if (sm->renew_snonce) {
    if (random_get_bytes(sm->snonce, WPA_NONCE_LEN)) {
      wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
              "WPA: Failed to get random data for SNonce");
      goto failed;
    }
    sm->renew_snonce = 0;
    wpa_hexdump(MSG_DEBUG, "WPA: Renewed SNonce",
                sm->snonce, WPA_NONCE_LEN);
  }
  
  /* Calculate PTK which will be stored as a temporary PTK until it has
  * been verified when processing message 3/4. */
  ptk = &sm->tptk;
  wpa_derive_ptk(sm, src_addr, key, ptk);
  if (sm->pairwise_cipher == WPA_CIPHER_TKIP) {
    u8 buf[8];
    /* Supplicant: swap tx/rx Mic keys */
    os_memcpy(buf, &ptk->tk[16], 8);
    os_memcpy(&ptk->tk[16], &ptk->tk[24], 8);
    os_memcpy(&ptk->tk[24], buf, 8);
    os_memset(buf, 0, sizeof(buf));
  }
  sm->tptk_set = 1;
  pie = (wf_80211_mgmt_ie_t *)sec_info->supplicant_ie;
  
  sm->assoc_wpa_ie_len = pie->len +2;
  
  //memcpy(sm->assoc_wpa_ie,sec_info->supplicant_ie,sm->assoc_wpa_ie_len);
  sm->assoc_wpa_ie = sec_info->supplicant_ie;
  kde = sm->assoc_wpa_ie ;
  kde_len = sm->assoc_wpa_ie_len;
  
#ifdef CONFIG_P2P
  if (sm->p2p) {
    kde_buf = os_malloc(kde_len + 2 + RSN_SELECTOR_LEN + 1);
    if (kde_buf) {
      u8 *pos;
      wpa_printf(MSG_DEBUG, "P2P: Add IP Address Request KDE "
                 "into EAPOL-Key 2/4");
      os_memcpy(kde_buf, kde, kde_len);
      kde = kde_buf;
      pos = kde + kde_len;
      *pos++ = WLAN_EID_VENDOR_SPECIFIC;
      *pos++ = RSN_SELECTOR_LEN + 1;
      RSN_SELECTOR_PUT(pos, WFA_KEY_DATA_IP_ADDR_REQ);
      pos += RSN_SELECTOR_LEN;
      *pos++ = 0x01;
      kde_len = pos - kde;
    }
  }
#endif /* CONFIG_P2P */
  
  if (wpa_supplicant_send_2_of_4(sm, sm->bssid, key, ver, sm->snonce,
                                 kde, kde_len, ptk) < 0)
    goto failed;
  
  os_free(kde_buf);
  os_memcpy(sm->anonce, key->key_nonce, WPA_NONCE_LEN);
  return;
  
failed:
  os_free(kde_buf);
  
}


static void wpa_sm_start_preauth(void *eloop_ctx, void *timeout_ctx)
{
  return;
}

static void wpa_sm_cancel_auth_timeout(struct wpa_sm *sm)
{
  return;
}

static void wpa_supplicant_key_neg_complete(struct wpa_sm *sm,
					    const u8 *addr, int secure)
{
  wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
          "WPA: Key negotiation completed with "
            MACSTR " [PTK=%s GTK=%s]", MAC2STR(addr),
            wpa_cipher_txt(sm->pairwise_cipher),
            wpa_cipher_txt(sm->group_cipher));
  wpa_sm_cancel_auth_timeout(sm);
  wpa_sm_set_state(sm, WPA_COMPLETED);
  
  
  if (sm->cur_pmksa && sm->cur_pmksa->opportunistic) {
    wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
            "RSN: Authenticator accepted "
              "opportunistic PMKSA entry - marking it valid");
    sm->cur_pmksa->opportunistic = 0;
  }
  
#ifdef CONFIG_IEEE80211R
  if (wpa_key_mgmt_ft(sm->key_mgmt)) {
    /* Prepare for the next transition */
    wpa_ft_prepare_auth_request(sm, NULL);
  }
#endif /* CONFIG_IEEE80211R */
}


static void wpa_sm_rekey_ptk(void *eloop_ctx, void *timeout_ctx)
{
  struct wpa_sm *sm = eloop_ctx;
  wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "WPA: Request PTK rekeying");
  wpa_sm_key_request(sm, 0, 1);
}

static int wpa_sm_set_key(struct wpa_sm *sm,enum wpa_alg alg,
                          const u8 * addr, int key_idx, int set_tx,
                          const u8 * seq, size_t seq_len,
                          const u8 * key, size_t key_len)
{
  return wpa_s_obj.driver->set_key(sm->pnic_info,key_idx, set_tx, addr, (u32) alg, seq,
                                   seq_len, key, key_len);
}

static int wpa_supplicant_install_ptk(struct wpa_sm *sm,
				      const struct wpa_eapol_key *key)
{
  int keylen, rsclen;
  enum wpa_alg alg;
  const u8 *key_rsc;
  
  
  
  wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
          "WPA: Installing PTK to the driver");
  
  if (sm->pairwise_cipher == WPA_CIPHER_NONE) {
    wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "WPA: Pairwise Cipher "
            "Suite: NONE - do not use pairwise keys");
    return 0;
  }
  
  if (!wpa_cipher_valid_pairwise(sm->pairwise_cipher)) {
    wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
            "WPA: Unsupported pairwise cipher %d",
            sm->pairwise_cipher);
    return -1;
  }
  
  alg = wpa_cipher_to_alg(sm->pairwise_cipher);
  keylen = wpa_cipher_key_len(sm->pairwise_cipher);
  rsclen = wpa_cipher_rsc_len(sm->pairwise_cipher);
  
  if (sm->proto == WPA_PROTO_RSN || sm->proto == WPA_PROTO_OSEN) {
    key_rsc = null_rsc;
  } else {
    key_rsc = key->key_rsc;
    wpa_hexdump(MSG_DEBUG, "WPA: RSC", key_rsc, rsclen);
  }
  
  if (wpa_sm_set_key(sm, alg, sm->bssid, 0, 1, key_rsc, rsclen,
                     sm->ptk.tk, keylen) < 0) {
                       wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
                               "WPA: Failed to set PTK to the "
                                 "driver (alg=%d keylen=%d bssid=" MACSTR ")",
                                 alg, keylen, MAC2STR(sm->bssid));
                       return -1;
                     }
  
  /* TK is not needed anymore in supplicant */
  os_memset(sm->ptk.tk, 0, WPA_TK_MAX_LEN);
  
  
  return 0;
}


static int wpa_supplicant_check_group_cipher(struct wpa_sm *sm,
					     int group_cipher,
					     int keylen, int maxkeylen,
					     int *key_rsc_len,
					     enum wpa_alg *alg)
{
  int klen;
  
  *alg = wpa_cipher_to_alg(group_cipher);
  if (*alg == WPA_ALG_NONE) {
    wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
            "WPA: Unsupported Group Cipher %d",
            group_cipher);
    return -1;
  }
  *key_rsc_len = wpa_cipher_rsc_len(group_cipher);
  
  klen = wpa_cipher_key_len(group_cipher);
  if (keylen != klen || maxkeylen < klen) {
    wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
            "WPA: Unsupported %s Group Cipher key length %d (%d)",
            wpa_cipher_txt(group_cipher), keylen, maxkeylen);
    return -1;
  }
  return 0;
}



static int wpa_supplicant_install_gtk(struct wpa_sm *sm,
				      const struct wpa_gtk_data *gd,
				      const u8 *key_rsc)
{
  const u8 *_gtk = gd->gtk;
  u8 gtk_buf[32];
  
  wpa_hexdump_key(MSG_DEBUG, "WPA: Group Key", gd->gtk, gd->gtk_len);
  wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
          "WPA: Installing GTK to the driver (keyidx=%d tx=%d len=%d)",
          gd->keyidx, gd->tx, gd->gtk_len);
  wpa_hexdump(MSG_DEBUG, "WPA: RSC", key_rsc, gd->key_rsc_len);
  if (sm->group_cipher == WPA_CIPHER_TKIP) {
    /* Swap Tx/Rx keys for Michael MIC */
    os_memcpy(gtk_buf, gd->gtk, 16);
    os_memcpy(gtk_buf + 16, gd->gtk + 24, 8);
    os_memcpy(gtk_buf + 24, gd->gtk + 16, 8);
    _gtk = gtk_buf;
  }
  if (sm->pairwise_cipher == WPA_CIPHER_NONE) {
    if (wpa_sm_set_key(sm, gd->alg, NULL,
                       gd->keyidx, 1, key_rsc, gd->key_rsc_len,
                       _gtk, gd->gtk_len) < 0) {
                         wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
                                 "WPA: Failed to set GTK to the driver "
                                   "(Group only)");
                         os_memset(gtk_buf, 0, sizeof(gtk_buf));
                         return -1;
                       }
  } else if (wpa_sm_set_key(sm, gd->alg, broadcast_ether_addr,
                            gd->keyidx, gd->tx, key_rsc, gd->key_rsc_len,
                            _gtk, gd->gtk_len) < 0) {
                              wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
                                      "WPA: Failed to set GTK to "
                                        "the driver (alg=%d keylen=%d keyidx=%d)",
                                        gd->alg, gd->gtk_len, gd->keyidx);
                              os_memset(gtk_buf, 0, sizeof(gtk_buf));
                              return -1;
                            }
  os_memset(gtk_buf, 0, sizeof(gtk_buf));
  
  return 0;
}


static int wpa_supplicant_gtk_tx_bit_workaround(const struct wpa_sm *sm,
						int tx)
{
  if (tx && sm->pairwise_cipher != WPA_CIPHER_NONE) {
    /* Ignore Tx bit for GTK if a pairwise key is used. One AP
    * seemed to set this bit (incorrectly, since Tx is only when
    * doing Group Key only APs) and without this workaround, the
    * data connection does not work because wpa_supplicant
    * configured non-zero keyidx to be used for unicast. */
    wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
            "WPA: Tx bit set for GTK, but pairwise "
              "keys are used - ignore Tx bit");
    return 0;
  }
  return tx;
}


static int wpa_supplicant_rsc_relaxation(const struct wpa_sm *sm,
					 const u8 *rsc)
{
  int rsclen;
  
  
  rsclen = wpa_cipher_rsc_len(sm->group_cipher);
  
  /*
  * Try to detect RSC (endian) corruption issue where the AP sends
  * the RSC bytes in EAPOL-Key message in the wrong order, both if
  * it's actually a 6-byte field (as it should be) and if it treats
  * it as an 8-byte field.
  * An AP model known to have this bug is the Sapido RB-1632.
  */
  if (rsclen == 6 && ((rsc[5] && !rsc[0]) || rsc[6] || rsc[7])) {
    wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
            "RSC %02x%02x%02x%02x%02x%02x%02x%02x is likely bogus, using 0",
            rsc[0], rsc[1], rsc[2], rsc[3],
            rsc[4], rsc[5], rsc[6], rsc[7]);
    
    return 1;
  }
  
  return 0;
}


static int wpa_supplicant_pairwise_gtk(struct wpa_sm *sm,
				       const struct wpa_eapol_key *key,
				       const u8 *gtk, size_t gtk_len,
				       int key_info)
{
  struct wpa_gtk_data gd;
  const u8 *key_rsc;
  
  /*
  * IEEE Std 802.11i-2004 - 8.5.2 EAPOL-Key frames - Figure 43x
  * GTK KDE format:
  * KeyID[bits 0-1], Tx [bit 2], Reserved [bits 3-7]
  * Reserved [bits 0-7]
  * GTK
  */
  
  os_memset(&gd, 0, sizeof(gd));
  wpa_hexdump_key(MSG_DEBUG, "RSN: received GTK in pairwise handshake",
                  gtk, gtk_len);
  
  if (gtk_len < 2 || gtk_len - 2 > sizeof(gd.gtk))
    return -1;
  
  gd.keyidx = gtk[0] & 0x3;
  gd.tx = wpa_supplicant_gtk_tx_bit_workaround(sm,
                                               !!(gtk[0] & BIT(2)));
  gtk += 2;
  gtk_len -= 2;
  
  os_memcpy(gd.gtk, gtk, gtk_len);
  gd.gtk_len = gtk_len;
  
  key_rsc = key->key_rsc;
  if (wpa_supplicant_rsc_relaxation(sm, key->key_rsc))
    key_rsc = null_rsc;
  
  if (sm->group_cipher != WPA_CIPHER_GTK_NOT_USED &&
      (wpa_supplicant_check_group_cipher(sm, sm->group_cipher,
                                         gtk_len, gtk_len,
                                         &gd.key_rsc_len, &gd.alg) ||
       wpa_supplicant_install_gtk(sm, &gd, key_rsc))) {
         wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
                 "RSN: Failed to install GTK");
         os_memset(&gd, 0, sizeof(gd));
         return -1;
       }
  os_memset(&gd, 0, sizeof(gd));
  
  wpa_supplicant_key_neg_complete(sm, sm->bssid,
                                  key_info & WPA_KEY_INFO_SECURE);
  return 0;
}


static int ieee80211w_set_keys(struct wpa_sm *sm,
			       struct wpa_eapol_ie_parse *ie)
{
#ifdef CONFIG_IEEE80211W
  if (!wpa_cipher_valid_mgmt_group(sm->mgmt_group_cipher))
    return 0;
  
  if (ie->igtk) {
    size_t len;
    const struct wpa_igtk_kde *igtk;
    u16 keyidx;
    len = wpa_cipher_key_len(sm->mgmt_group_cipher);
    if (ie->igtk_len != WPA_IGTK_KDE_PREFIX_LEN + len)
      return -1;
    igtk = (const struct wpa_igtk_kde *) ie->igtk;
    keyidx = WPA_GET_LE16(igtk->keyid);
    wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "WPA: IGTK keyid %d "
            "pn %02x%02x%02x%02x%02x%02x",
            keyidx, MAC2STR(igtk->pn));
    wpa_hexdump_key(MSG_DEBUG, "WPA: IGTK",
                    igtk->igtk, len);
    if (keyidx > 4095) {
      wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
              "WPA: Invalid IGTK KeyID %d", keyidx);
      return -1;
    }
    if (wpa_sm_set_key(sm, wpa_cipher_to_alg(sm->mgmt_group_cipher),
                       broadcast_ether_addr,
                       keyidx, 0, igtk->pn, sizeof(igtk->pn),
                       igtk->igtk, len) < 0) {
                         wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
                                 "WPA: Failed to configure IGTK to the driver");
                         return -1;
                       }
  }
  
  return 0;
#else /* CONFIG_IEEE80211W */
  return 0;
#endif /* CONFIG_IEEE80211W */
}


static void wpa_report_ie_mismatch(struct wpa_sm *sm,
				   const char *reason, const u8 *src_addr,
				   const u8 *wpa_ie, size_t wpa_ie_len,
				   const u8 *rsn_ie, size_t rsn_ie_len)
{
  wpa_msg(sm->ctx->msg_ctx, MSG_WARNING, "WPA: %s (src=" MACSTR ")",
          reason, MAC2STR(src_addr));
  
  if (sm->ap_wpa_ie) {
    wpa_hexdump(MSG_INFO, "WPA: WPA IE in Beacon/ProbeResp",
                sm->ap_wpa_ie, sm->ap_wpa_ie_len);
  }
  if (wpa_ie) {
    if (!sm->ap_wpa_ie) {
      wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
              "WPA: No WPA IE in Beacon/ProbeResp");
    }
    wpa_hexdump(MSG_INFO, "WPA: WPA IE in 3/4 msg",
                wpa_ie, wpa_ie_len);
  }
  
  if (sm->ap_rsn_ie) {
    wpa_hexdump(MSG_INFO, "WPA: RSN IE in Beacon/ProbeResp",
                sm->ap_rsn_ie, sm->ap_rsn_ie_len);
  }
  if (rsn_ie) {
    if (!sm->ap_rsn_ie) {
      wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
              "WPA: No RSN IE in Beacon/ProbeResp");
    }
    wpa_hexdump(MSG_INFO, "WPA: RSN IE in 3/4 msg",
                rsn_ie, rsn_ie_len);
  }
  
}


#ifdef CONFIG_IEEE80211R

static int ft_validate_mdie(struct wpa_sm *sm,
			    const unsigned char *src_addr,
			    struct wpa_eapol_ie_parse *ie,
			    const u8 *assoc_resp_mdie)
{
  struct rsn_mdie *mdie;
  
  mdie = (struct rsn_mdie *) (ie->mdie + 2);
  if (ie->mdie == NULL || ie->mdie_len < 2 + sizeof(*mdie) ||
      os_memcmp(mdie->mobility_domain, sm->mobility_domain,
                MOBILITY_DOMAIN_ID_LEN) != 0) {
                  wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "FT: MDIE in msg 3/4 did "
                          "not match with the current mobility domain");
                  return -1;
                }
  
  if (assoc_resp_mdie &&
      (assoc_resp_mdie[1] != ie->mdie[1] ||
       os_memcmp(assoc_resp_mdie, ie->mdie, 2 + ie->mdie[1]) != 0)) {
         wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "FT: MDIE mismatch");
         wpa_hexdump(MSG_DEBUG, "FT: MDIE in EAPOL-Key msg 3/4",
                     ie->mdie, 2 + ie->mdie[1]);
         wpa_hexdump(MSG_DEBUG, "FT: MDIE in (Re)Association Response",
                     assoc_resp_mdie, 2 + assoc_resp_mdie[1]);
         return -1;
       }
  
  return 0;
}


static int ft_validate_ftie(struct wpa_sm *sm,
			    const unsigned char *src_addr,
			    struct wpa_eapol_ie_parse *ie,
			    const u8 *assoc_resp_ftie)
{
  if (ie->ftie == NULL) {
    wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
            "FT: No FTIE in EAPOL-Key msg 3/4");
    return -1;
  }
  
  if (assoc_resp_ftie == NULL)
    return 0;
  
  if (assoc_resp_ftie[1] != ie->ftie[1] ||
      os_memcmp(assoc_resp_ftie, ie->ftie, 2 + ie->ftie[1]) != 0) {
        wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "FT: FTIE mismatch");
        wpa_hexdump(MSG_DEBUG, "FT: FTIE in EAPOL-Key msg 3/4",
                    ie->ftie, 2 + ie->ftie[1]);
        wpa_hexdump(MSG_DEBUG, "FT: FTIE in (Re)Association Response",
                    assoc_resp_ftie, 2 + assoc_resp_ftie[1]);
        return -1;
      }
  
  return 0;
}


static int ft_validate_rsnie(struct wpa_sm *sm,
			     const unsigned char *src_addr,
			     struct wpa_eapol_ie_parse *ie)
{
  struct wpa_ie_data rsn;
  
  if (!ie->rsn_ie)
    return 0;
  
  /*
  * Verify that PMKR1Name from EAPOL-Key message 3/4
  * matches with the value we derived.
  */
  if (wpa_parse_wpa_ie_rsn(ie->rsn_ie, ie->rsn_ie_len, &rsn) < 0 ||
      rsn.num_pmkid != 1 || rsn.pmkid == NULL) {
        wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "FT: No PMKR1Name in "
                "FT 4-way handshake message 3/4");
        return -1;
      }
  
  if (os_memcmp_const(rsn.pmkid, sm->pmk_r1_name, WPA_PMK_NAME_LEN) != 0)
  {
    wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
            "FT: PMKR1Name mismatch in "
              "FT 4-way handshake message 3/4");
    wpa_hexdump(MSG_DEBUG, "FT: PMKR1Name from Authenticator",
                rsn.pmkid, WPA_PMK_NAME_LEN);
    wpa_hexdump(MSG_DEBUG, "FT: Derived PMKR1Name",
                sm->pmk_r1_name, WPA_PMK_NAME_LEN);
    return -1;
  }
  
  return 0;
}


static int wpa_supplicant_validate_ie_ft(struct wpa_sm *sm,
					 const unsigned char *src_addr,
					 struct wpa_eapol_ie_parse *ie)
{
  const u8 *pos, *end, *mdie = NULL, *ftie = NULL;
  
  if (sm->assoc_resp_ies) {
    pos = sm->assoc_resp_ies;
    end = pos + sm->assoc_resp_ies_len;
    while (end - pos > 2) {
      if (2 + pos[1] > end - pos)
        break;
      switch (*pos) {
      case WLAN_EID_MOBILITY_DOMAIN:
        mdie = pos;
        break;
      case WLAN_EID_FAST_BSS_TRANSITION:
        ftie = pos;
        break;
      }
      pos += 2 + pos[1];
    }
  }
  
  if (ft_validate_mdie(sm, src_addr, ie, mdie) < 0 ||
      ft_validate_ftie(sm, src_addr, ie, ftie) < 0 ||
        ft_validate_rsnie(sm, src_addr, ie) < 0)
    return -1;
  
  return 0;
}

#endif /* CONFIG_IEEE80211R */


static int wpa_supplicant_validate_ie(struct wpa_sm *sm,
				      const unsigned char *src_addr,
				      struct wpa_eapol_ie_parse *ie)
{
  if (sm->ap_wpa_ie == NULL && sm->ap_rsn_ie == NULL) {
    wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
            "WPA: No WPA/RSN IE for this AP known. "
              "Trying to get from scan results");
    
  }
  
  if (ie->wpa_ie == NULL && ie->rsn_ie == NULL &&
      (sm->ap_wpa_ie || sm->ap_rsn_ie)) {
        wpa_report_ie_mismatch(sm, "IE in 3/4 msg does not match "
                               "with IE in Beacon/ProbeResp (no IE?)",
                               src_addr, ie->wpa_ie, ie->wpa_ie_len,
                               ie->rsn_ie, ie->rsn_ie_len);
        return -1;
      }
  
  if ((ie->wpa_ie && sm->ap_wpa_ie &&
       (ie->wpa_ie_len != sm->ap_wpa_ie_len ||
        os_memcmp(ie->wpa_ie, sm->ap_wpa_ie, ie->wpa_ie_len) != 0)) ||
      (ie->rsn_ie && sm->ap_rsn_ie &&
       wpa_compare_rsn_ie(wpa_key_mgmt_ft(sm->key_mgmt),
                          sm->ap_rsn_ie, sm->ap_rsn_ie_len,
                          ie->rsn_ie, ie->rsn_ie_len))) {
                            wpa_report_ie_mismatch(sm, "IE in 3/4 msg does not match "
                                                   "with IE in Beacon/ProbeResp",
                                                   src_addr, ie->wpa_ie, ie->wpa_ie_len,
                                                   ie->rsn_ie, ie->rsn_ie_len);
                            return -1;
                          }
  
  if (sm->proto == WPA_PROTO_WPA &&
      ie->rsn_ie && sm->ap_rsn_ie == NULL && sm->rsn_enabled) {
        wpa_report_ie_mismatch(sm, "Possible downgrade attack "
                               "detected - RSN was enabled and RSN IE "
                                 "was in msg 3/4, but not in "
                                   "Beacon/ProbeResp",
                                   src_addr, ie->wpa_ie, ie->wpa_ie_len,
                                   ie->rsn_ie, ie->rsn_ie_len);
        return -1;
      }
  
#ifdef CONFIG_IEEE80211R
  if (wpa_key_mgmt_ft(sm->key_mgmt) &&
      wpa_supplicant_validate_ie_ft(sm, src_addr, ie) < 0)
    return -1;
#endif /* CONFIG_IEEE80211R */
  
  return 0;
}


/**
* wpa_supplicant_send_4_of_4 - Send message 4 of WPA/RSN 4-Way Handshake
* @sm: Pointer to WPA state machine data from wpa_sm_init()
* @dst: Destination address for the frame
* @key: Pointer to the EAPOL-Key frame header
* @ver: Version bits from EAPOL-Key Key Info
* @key_info: Key Info
* @ptk: PTK to use for keyed hash and encryption
* Returns: >= 0 on success, < 0 on failure
*/
int wpa_supplicant_send_4_of_4(struct wpa_sm *sm, const unsigned char *dst,
			       const struct wpa_eapol_key *key,
			       u16 ver, u16 key_info,
			       struct wpa_ptk *ptk)
{
  size_t mic_len, hdrlen, rlen;
  struct wpa_eapol_key *reply;
  struct wpa_eapol_key_192 *reply192;
  u8 *rbuf, *key_mic;
  
  mic_len = wpa_mic_len(sm->key_mgmt);
  hdrlen = mic_len == 24 ? sizeof(*reply192) : sizeof(*reply);
  rbuf = wpa_sm_alloc_eapol((struct wpa_supplicant *)sm, IEEE802_1X_TYPE_EAPOL_KEY, NULL,
                            hdrlen, &rlen, (void *) &reply);
  if (rbuf == NULL)
    return -1;
  reply192 = (struct wpa_eapol_key_192 *) reply;
  
  reply->type = (sm->proto == WPA_PROTO_RSN ||
                 sm->proto == WPA_PROTO_OSEN) ?
EAPOL_KEY_TYPE_RSN : EAPOL_KEY_TYPE_WPA;
key_info &= WPA_KEY_INFO_SECURE;
key_info |= ver | WPA_KEY_INFO_KEY_TYPE | WPA_KEY_INFO_MIC;
WPA_PUT_BE16(reply->key_info, key_info);
if (sm->proto == WPA_PROTO_RSN || sm->proto == WPA_PROTO_OSEN)
WPA_PUT_BE16(reply->key_length, 0);
else
os_memcpy(reply->key_length, key->key_length, 2);
os_memcpy(reply->replay_counter, key->replay_counter,
          WPA_REPLAY_COUNTER_LEN);

key_mic = reply192->key_mic; /* same offset for reply and reply192 */
if (mic_len == 24)
WPA_PUT_BE16(reply192->key_data_length, 0);
else
WPA_PUT_BE16(reply->key_data_length, 0);

wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "WPA: Sending EAPOL-Key 4/4");
return wpa_eapol_key_send(sm, ptk->kck, ptk->kck_len, ver, dst,
                          ETH_P_EAPOL, rbuf, rlen, key_mic);
}


static void wpa_supplicant_process_3_of_4(struct wpa_sm *sm,
					  const struct wpa_eapol_key *key,
					  u16 ver, const u8 *key_data,
					  size_t key_data_len)
{
  u16 key_info, keylen;
  struct wpa_eapol_ie_parse ie;
  
  wpa_sm_set_state(sm, WPA_4WAY_HANDSHAKE);
  wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "WPA: RX message 3 of 4-Way "
          "Handshake from " MACSTR " (ver=%d)", MAC2STR(sm->bssid), ver);
  
  key_info = WPA_GET_BE16(key->key_info);
  
  wpa_hexdump(MSG_DEBUG, "WPA: IE KeyData", key_data, key_data_len);
  if (wpa_supplicant_parse_ies(key_data, key_data_len, &ie) < 0)
    goto failed;
  if (ie.gtk && !(key_info & WPA_KEY_INFO_ENCR_KEY_DATA)) {
    wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
            "WPA: GTK IE in unencrypted key data");
    goto failed;
  }
#ifdef CONFIG_IEEE80211W
  if (ie.igtk && !(key_info & WPA_KEY_INFO_ENCR_KEY_DATA)) {
    wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
            "WPA: IGTK KDE in unencrypted key data");
    goto failed;
  }
  
  if (ie.igtk &&
      wpa_cipher_valid_mgmt_group(sm->mgmt_group_cipher) &&
        ie.igtk_len != WPA_IGTK_KDE_PREFIX_LEN +
          (unsigned int) wpa_cipher_key_len(sm->mgmt_group_cipher)) {
            wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
                    "WPA: Invalid IGTK KDE length %lu",
                    (unsigned long) ie.igtk_len);
            goto failed;
          }
#endif /* CONFIG_IEEE80211W */
  
  if (wpa_supplicant_validate_ie(sm, sm->bssid, &ie) < 0)
    goto failed;
  
  if (os_memcmp(sm->anonce, key->key_nonce, WPA_NONCE_LEN) != 0) {
    wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
            "WPA: ANonce from message 1 of 4-Way Handshake "
              "differs from 3 of 4-Way Handshake - drop packet (src="
                MACSTR ")", MAC2STR(sm->bssid));
    goto failed;
  }
  
  keylen = WPA_GET_BE16(key->key_length);
  if (keylen != wpa_cipher_key_len(sm->pairwise_cipher)) {
    wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
            "WPA: Invalid %s key length %d (src=" MACSTR
              ")", wpa_cipher_txt(sm->pairwise_cipher), keylen,
              MAC2STR(sm->bssid));
    goto failed;
  }
  
#ifdef CONFIG_P2P
  if (ie.ip_addr_alloc) {
    os_memcpy(sm->p2p_ip_addr, ie.ip_addr_alloc, 3 * 4);
    wpa_hexdump(MSG_DEBUG, "P2P: IP address info",
                sm->p2p_ip_addr, sizeof(sm->p2p_ip_addr));
  }
#endif /* CONFIG_P2P */
  
  if (wpa_supplicant_send_4_of_4(sm, sm->bssid, key, ver, key_info,
                                 &sm->ptk) < 0) {
                                   goto failed;
                                 }
  
  /* SNonce was successfully used in msg 3/4, so mark it to be renewed
  * for the next 4-Way Handshake. If msg 3 is received again, the old
  * SNonce will still be used to avoid changing PTK. */
  sm->renew_snonce = 1;
  
  if (key_info & WPA_KEY_INFO_INSTALL) {
    if (wpa_supplicant_install_ptk(sm, key))
      goto failed;
  }
  
  
  wpa_sm_set_state(sm, WPA_GROUP_HANDSHAKE);
  
  if (sm->group_cipher == WPA_CIPHER_GTK_NOT_USED) {
    wpa_supplicant_key_neg_complete(sm, sm->bssid,
                                    key_info & WPA_KEY_INFO_SECURE);
  } else if (ie.gtk &&
             wpa_supplicant_pairwise_gtk(sm, key,
                                         ie.gtk, ie.gtk_len, key_info) < 0) {
                                           wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
                                                   "RSN: Failed to configure GTK");
                                           goto failed;
                                         }
  
  if (ieee80211w_set_keys(sm, &ie) < 0) {
    wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
            "RSN: Failed to configure IGTK");
    goto failed;
  }
  
  
  if (sm->proto == WPA_PROTO_RSN && wpa_key_mgmt_suite_b(sm->key_mgmt)) {
    struct rsn_pmksa_cache_entry *sa;
    
    sa = pmksa_cache_add(sm->pmksa, sm->pmk, sm->pmk_len, NULL,
                         sm->ptk.kck, sm->ptk.kck_len,
                         sm->bssid, sm->own_addr,
                         sm->network_ctx, sm->key_mgmt);
    if (!sm->cur_pmksa)
      sm->cur_pmksa = sa;
  }
  
  sm->msg_3_of_4_ok = 1;
  return;
  
failed:
  return;
}


static int wpa_supplicant_process_1_of_2_rsn(struct wpa_sm *sm,
					     const u8 *keydata,
					     size_t keydatalen,
					     u16 key_info,
					     struct wpa_gtk_data *gd)
{
  int maxkeylen;
  struct wpa_eapol_ie_parse ie;
  
  wpa_hexdump(MSG_DEBUG, "RSN: msg 1/2 key data", keydata, keydatalen);
  if (wpa_supplicant_parse_ies(keydata, keydatalen, &ie) < 0)
    return -1;
  if (ie.gtk && !(key_info & WPA_KEY_INFO_ENCR_KEY_DATA)) {
    wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
            "WPA: GTK IE in unencrypted key data");
    return -1;
  }
  if (ie.gtk == NULL) {
    wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
            "WPA: No GTK IE in Group Key msg 1/2");
    return -1;
  }
  maxkeylen = gd->gtk_len = ie.gtk_len - 2;
  
  if (wpa_supplicant_check_group_cipher(sm, sm->group_cipher,
                                        gd->gtk_len, maxkeylen,
                                        &gd->key_rsc_len, &gd->alg))
    return -1;
  
  wpa_hexdump_key(MSG_DEBUG, "RSN: received GTK in group key handshake",
                  ie.gtk, ie.gtk_len);
  gd->keyidx = ie.gtk[0] & 0x3;
  gd->tx = wpa_supplicant_gtk_tx_bit_workaround(sm,
                                                !!(ie.gtk[0] & BIT(2)));
  if (ie.gtk_len - 2 > sizeof(gd->gtk)) {
    wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
            "RSN: Too long GTK in GTK IE (len=%lu)",
            (unsigned long) ie.gtk_len - 2);
    return -1;
  }
  os_memcpy(gd->gtk, ie.gtk + 2, ie.gtk_len - 2);
  
  if (ieee80211w_set_keys(sm, &ie) < 0)
    wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
            "RSN: Failed to configure IGTK");
  
  return 0;
}


static int wpa_supplicant_process_1_of_2_wpa(struct wpa_sm *sm,
					     const struct wpa_eapol_key *key,
					     const u8 *key_data,
					     size_t key_data_len, u16 key_info,
					     u16 ver, struct wpa_gtk_data *gd)
{
  size_t maxkeylen;
  u16 gtk_len;
  
  gtk_len = WPA_GET_BE16(key->key_length);
  maxkeylen = key_data_len;
  if (ver == WPA_KEY_INFO_TYPE_HMAC_SHA1_AES) {
    if (maxkeylen < 8) {
      wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
              "WPA: Too short maxkeylen (%lu)",
              (unsigned long) maxkeylen);
      return -1;
    }
    maxkeylen -= 8;
  }
  
  if (gtk_len > maxkeylen ||
      wpa_supplicant_check_group_cipher(sm, sm->group_cipher,
                                        gtk_len, maxkeylen,
                                        &gd->key_rsc_len, &gd->alg))
    return -1;
  
  gd->gtk_len = gtk_len;
  gd->keyidx = (key_info & WPA_KEY_INFO_KEY_INDEX_MASK) >>
    WPA_KEY_INFO_KEY_INDEX_SHIFT;
  if (ver == WPA_KEY_INFO_TYPE_HMAC_MD5_RC4 && sm->ptk.kek_len == 16) {
#ifdef CONFIG_NO_RC4
    wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
            "WPA: RC4 not supported in the build");
    return -1;
#else /* CONFIG_NO_RC4 */
    u8 ek[32];
    if (key_data_len > sizeof(gd->gtk)) {
      wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
              "WPA: RC4 key data too long (%lu)",
              (unsigned long) key_data_len);
      return -1;
    }
    os_memcpy(ek, key->key_iv, 16);
    os_memcpy(ek + 16, sm->ptk.kek, sm->ptk.kek_len);
    os_memcpy(gd->gtk, key_data, key_data_len);
    if (rc4_skip(ek, 32, 256, gd->gtk, key_data_len)) {
      os_memset(ek, 0, sizeof(ek));
      wpa_msg(sm->ctx->msg_ctx, MSG_ERROR,
              "WPA: RC4 failed");
      return -1;
    }
    os_memset(ek, 0, sizeof(ek));
#endif /* CONFIG_NO_RC4 */
  } else if (ver == WPA_KEY_INFO_TYPE_HMAC_SHA1_AES) {
    if (maxkeylen % 8) {
      wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
              "WPA: Unsupported AES-WRAP len %lu",
              (unsigned long) maxkeylen);
      return -1;
    }
    if (maxkeylen > sizeof(gd->gtk)) {
      wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
              "WPA: AES-WRAP key data "
                "too long (keydatalen=%lu maxkeylen=%lu)",
                (unsigned long) key_data_len,
                (unsigned long) maxkeylen);
      return -1;
    }
    if (aes_unwrap(sm->ptk.kek, sm->ptk.kek_len, maxkeylen / 8,
                   key_data, gd->gtk)) {
                     wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
                             "WPA: AES unwrap failed - could not decrypt "
                               "GTK");
                     return -1;
                   }
  } else {
    wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
            "WPA: Unsupported key_info type %d", ver);
    return -1;
  }
  gd->tx = wpa_supplicant_gtk_tx_bit_workaround(
                                                sm, !!(key_info & WPA_KEY_INFO_TXRX));
  return 0;
}


static int wpa_supplicant_send_2_of_2(struct wpa_sm *sm,
				      const struct wpa_eapol_key *key,
				      int ver, u16 key_info)
{
  size_t mic_len, hdrlen, rlen;
  struct wpa_eapol_key *reply;
  struct wpa_eapol_key_192 *reply192;
  u8 *rbuf, *key_mic;
  
  mic_len = wpa_mic_len(sm->key_mgmt);
  hdrlen = mic_len == 24 ? sizeof(*reply192) : sizeof(*reply);
  rbuf = wpa_sm_alloc_eapol((struct wpa_supplicant *)sm, IEEE802_1X_TYPE_EAPOL_KEY, NULL,
                            hdrlen, &rlen, (void *) &reply);
  if (rbuf == NULL)
    return -1;
  reply192 = (struct wpa_eapol_key_192 *) reply;
  
  reply->type = (sm->proto == WPA_PROTO_RSN ||
                 sm->proto == WPA_PROTO_OSEN) ?
EAPOL_KEY_TYPE_RSN : EAPOL_KEY_TYPE_WPA;
key_info &= WPA_KEY_INFO_KEY_INDEX_MASK;
key_info |= ver | WPA_KEY_INFO_MIC | WPA_KEY_INFO_SECURE;
WPA_PUT_BE16(reply->key_info, key_info);
if (sm->proto == WPA_PROTO_RSN || sm->proto == WPA_PROTO_OSEN)
WPA_PUT_BE16(reply->key_length, 0);
else
os_memcpy(reply->key_length, key->key_length, 2);
os_memcpy(reply->replay_counter, key->replay_counter,
          WPA_REPLAY_COUNTER_LEN);

key_mic = reply192->key_mic; /* same offset for reply and reply192 */
if (mic_len == 24)
WPA_PUT_BE16(reply192->key_data_length, 0);
else
WPA_PUT_BE16(reply->key_data_length, 0);

wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "WPA: Sending EAPOL-Key 2/2");
return wpa_eapol_key_send(sm, sm->ptk.kck, sm->ptk.kck_len, ver,
                          sm->bssid, ETH_P_EAPOL, rbuf, rlen, key_mic);
}


static void wpa_supplicant_process_1_of_2(struct wpa_sm *sm,
					  const unsigned char *src_addr,
					  const struct wpa_eapol_key *key,
					  const u8 *key_data,
					  size_t key_data_len, u16 ver)
{
  u16 key_info;
  int rekey, ret;
  struct wpa_gtk_data gd;
  const u8 *key_rsc;
  
  if (!sm->msg_3_of_4_ok) {
    wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
            "WPA: Group Key Handshake started prior to completion of 4-way handshake");
    goto failed;
  }
  
  os_memset(&gd, 0, sizeof(gd));
  
  rekey = wpa_sm_get_state() == WPA_COMPLETED;
  wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "WPA: RX message 1 of Group Key "
          "Handshake from " MACSTR " (ver=%d)", MAC2STR(src_addr), ver);
  
  key_info = WPA_GET_BE16(key->key_info);
  
  if (sm->proto == WPA_PROTO_RSN || sm->proto == WPA_PROTO_OSEN) {
    ret = wpa_supplicant_process_1_of_2_rsn(sm, key_data,
                                            key_data_len, key_info,
                                            &gd);
  } else {
    ret = wpa_supplicant_process_1_of_2_wpa(sm, key, key_data,
                                            key_data_len,
                                            key_info, ver, &gd);
  }
  
  wpa_sm_set_state(sm, WPA_GROUP_HANDSHAKE);
  
  if (ret)
    goto failed;
  
  key_rsc = key->key_rsc;
  if (wpa_supplicant_rsc_relaxation(sm, key->key_rsc))
    key_rsc = null_rsc;
  
  if (wpa_supplicant_install_gtk(sm, &gd, key_rsc) ||
      wpa_supplicant_send_2_of_2(sm, key, ver, key_info) < 0)
    goto failed;
  os_memset(&gd, 0, sizeof(gd));
  
  if (rekey) {
    wpa_msg(sm->ctx->msg_ctx, MSG_INFO, "WPA: Group rekeying "
            "completed with " MACSTR " [GTK=%s]",
            MAC2STR(sm->bssid), wpa_cipher_txt(sm->group_cipher));
    wpa_sm_cancel_auth_timeout(sm);
    wpa_sm_set_state(sm, WPA_COMPLETED);
  } else {
    wpa_supplicant_key_neg_complete(sm, sm->bssid,
                                    key_info &
                                      WPA_KEY_INFO_SECURE);
  }
  
  
  return;
  
failed:
  os_memset(&gd, 0, sizeof(gd));
}


static int wpa_supplicant_verify_eapol_key_mic(struct wpa_sm *sm,
					       struct wpa_eapol_key_192 *key,
					       u16 ver,
					       const u8 *buf, size_t len)
{
  u8 mic[WPA_EAPOL_KEY_MIC_MAX_LEN];
  int ok = 0;
  size_t mic_len = wpa_mic_len(sm->key_mgmt);
  int i=0;
  
  os_memcpy(mic, key->key_mic, mic_len);
  if (sm->tptk_set) {
    os_memset(key->key_mic, 0, mic_len);
    
    
    wpa_eapol_key_mic(sm->tptk.kck, sm->tptk.kck_len, sm->key_mgmt,
                      ver, buf, len, key->key_mic);
    
    
    if (os_memcmp_const(mic, key->key_mic, mic_len) != 0) {
      wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
              "WPA: Invalid EAPOL-Key MIC "
                "when using TPTK - ignoring TPTK");
    } else {
      ok = 1;
      sm->tptk_set = 0;
      sm->ptk_set = 1;
      os_memcpy(&sm->ptk, &sm->tptk, sizeof(sm->ptk));
      os_memset(&sm->tptk, 0, sizeof(sm->tptk));
    }
  }
  
  if (!ok && sm->ptk_set) {
    os_memset(key->key_mic, 0, mic_len);
    wpa_eapol_key_mic(sm->ptk.kck, sm->ptk.kck_len, sm->key_mgmt,
                      ver, buf, len, key->key_mic);
    if (os_memcmp_const(mic, key->key_mic, mic_len) != 0) {
      wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
              "WPA: Invalid EAPOL-Key MIC - "
                "dropping packet");
      return -1;
    }
    ok = 1;
  }
  
  if (!ok) {
    wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
            "WPA: Could not verify EAPOL-Key MIC - "
              "dropping packet");
    return -1;
  }
  
  os_memcpy(sm->rx_replay_counter, key->replay_counter,
            WPA_REPLAY_COUNTER_LEN);
  sm->rx_replay_counter_set = 1;
  return 0;
}


/* Decrypt RSN EAPOL-Key key data (RC4 or AES-WRAP) */
static int wpa_supplicant_decrypt_key_data(struct wpa_sm *sm,
					   struct wpa_eapol_key *key, u16 ver,
					   u8 *key_data, size_t *key_data_len)
{
  printf("RSN: encrypted key data");
  if (!sm->ptk_set) {
    wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
            "WPA: PTK not available, cannot decrypt EAPOL-Key Key "
              "Data");
    return -1;
  }
  
  /* Decrypt key data here so that this operation does not need
  * to be implemented separately for each message type. */
  if (ver == WPA_KEY_INFO_TYPE_HMAC_MD5_RC4 && sm->ptk.kek_len == 16) {
#ifdef CONFIG_NO_RC4
    wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
            "WPA: RC4 not supported in the build");
    return -1;
#else /* CONFIG_NO_RC4 */
    u8 ek[32];
    os_memcpy(ek, key->key_iv, 16);
    os_memcpy(ek + 16, sm->ptk.kek, sm->ptk.kek_len);
    if (rc4_skip(ek, 32, 256, key_data, *key_data_len)) {
      os_memset(ek, 0, sizeof(ek));
      wpa_msg(sm->ctx->msg_ctx, MSG_ERROR,
              "WPA: RC4 failed");
      return -1;
    }
    os_memset(ek, 0, sizeof(ek));
#endif /* CONFIG_NO_RC4 */
  } else if (ver == WPA_KEY_INFO_TYPE_HMAC_SHA1_AES ||
             ver == WPA_KEY_INFO_TYPE_AES_128_CMAC ||
               sm->key_mgmt == WPA_KEY_MGMT_OSEN ||
                 wpa_key_mgmt_suite_b(sm->key_mgmt)) {
                   u8 *buf;
                   if (*key_data_len < 8 || *key_data_len % 8) {
                     wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
                             "WPA: Unsupported AES-WRAP len %u",
                             (unsigned int) *key_data_len);
                     return -1;
                   }
                   *key_data_len -= 8; /* AES-WRAP adds 8 bytes */
                   buf = os_malloc(*key_data_len);
                   if (buf == NULL) {
                     wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
                             "WPA: No memory for AES-UNWRAP buffer");
                     return -1;
                   }
                   if (aes_unwrap(sm->ptk.kek, sm->ptk.kek_len, *key_data_len / 8,
                                  key_data, buf)) {
                                    bin_clear_free(buf, *key_data_len);
                                    wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
                                            "WPA: AES unwrap failed - "
                                              "could not decrypt EAPOL-Key key data");
                                    return -1;
                                  }
                   os_memcpy(key_data, buf, *key_data_len);
                   bin_clear_free(buf, *key_data_len);
                   WPA_PUT_BE16(key->key_data_length, *key_data_len);
                 } else {
                   wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
                           "WPA: Unsupported key_info type %d", ver);
                   return -1;
                 }
  wpa_hexdump_key(MSG_DEBUG, "WPA: decrypted EAPOL-Key key data",
                  key_data, *key_data_len);
  return 0;
}


/**
* wpa_sm_aborted_cached - Notify WPA that PMKSA caching was aborted
* @sm: Pointer to WPA state machine data from wpa_sm_init()
*/
void wpa_sm_aborted_cached(struct wpa_sm *sm)
{
  if (sm && sm->cur_pmksa) {
    wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
            "RSN: Cancelling PMKSA caching attempt");
    sm->cur_pmksa = NULL;
  }
}


static void wpa_eapol_key_dump(struct wpa_sm *sm,
			       const struct wpa_eapol_key *key,
			       unsigned int key_data_len,
			       const u8 *mic, unsigned int mic_len)
{
#ifndef CONFIG_NO_STDOUT_DEBUG
  u16 key_info = WPA_GET_BE16(key->key_info);
  
  wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "  EAPOL-Key type=%d", key->type);
  wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
          "  key_info 0x%x (ver=%d keyidx=%d rsvd=%d %s%s%s%s%s%s%s%s)",
          key_info, key_info & WPA_KEY_INFO_TYPE_MASK,
          (key_info & WPA_KEY_INFO_KEY_INDEX_MASK) >>
            WPA_KEY_INFO_KEY_INDEX_SHIFT,
            (key_info & (BIT(13) | BIT(14) | BIT(15))) >> 13,
            key_info & WPA_KEY_INFO_KEY_TYPE ? "Pairwise" : "Group",
            key_info & WPA_KEY_INFO_INSTALL ? " Install" : "",
            key_info & WPA_KEY_INFO_ACK ? " Ack" : "",
            key_info & WPA_KEY_INFO_MIC ? " MIC" : "",
            key_info & WPA_KEY_INFO_SECURE ? " Secure" : "",
            key_info & WPA_KEY_INFO_ERROR ? " Error" : "",
            key_info & WPA_KEY_INFO_REQUEST ? " Request" : "",
            key_info & WPA_KEY_INFO_ENCR_KEY_DATA ? " Encr" : "");
  wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
          "  key_length=%u key_data_length=%u",
          WPA_GET_BE16(key->key_length), key_data_len);
  wpa_hexdump(MSG_DEBUG, "  replay_counter",
              key->replay_counter, WPA_REPLAY_COUNTER_LEN);
  wpa_hexdump(MSG_DEBUG, "  key_nonce", key->key_nonce, WPA_NONCE_LEN);
  wpa_hexdump(MSG_DEBUG, "  key_iv", key->key_iv, 16);
  wpa_hexdump(MSG_DEBUG, "  key_rsc", key->key_rsc, 8);
  wpa_hexdump(MSG_DEBUG, "  key_id (reserved)", key->key_id, 8);
  wpa_hexdump(MSG_DEBUG, "  key_mic", mic, mic_len);
#endif /* CONFIG_NO_STDOUT_DEBUG */
}


static int wpa_supplicant_process_smk_error(struct wpa_sm *sm,
                                            const unsigned char *src_addr,
                                            const struct wpa_eapol_key *key,
                                            size_t extra_len)
{
  struct wpa_eapol_ie_parse kde;
  struct rsn_error_kde error;
  u8 peer[ETH_ALEN];
  u16 error_type;
  
  wpa_printf(MSG_DEBUG, "RSN: Received SMK Error");
  
  if (!sm->peerkey_enabled || sm->proto != WPA_RSN_PROTO) {
    wpa_printf(MSG_DEBUG, "RSN: SMK handshake not allowed for "
               "the current network");
    return -1;
  }
  
  if (wpa_supplicant_parse_ies((const u8 *)(key + 1), extra_len, &kde) < 0) {
    wpa_printf(MSG_INFO, "RSN: Failed to parse KDEs in SMK Error");
    return -1;
  }
  
  if (kde.error == NULL || kde.error_len < sizeof(error)) {
    wpa_printf(MSG_INFO, "RSN: No Error KDE in SMK Error");
    return -1;
  }
  
  if (kde.mac_addr && kde.mac_addr_len >= ETH_ALEN)
    os_memcpy(peer, kde.mac_addr, ETH_ALEN);
  else
    os_memset(peer, 0, ETH_ALEN);
  os_memcpy(&error, kde.error, sizeof(error));
  error_type = be_to_host16(error.error_type);
  wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
          "RSN: SMK Error KDE received: MUI %d error_type %d peer "
            MACSTR, be_to_host16(error.mui), error_type, MAC2STR(peer));
  
  
  return 0;
}

static u8 *wpa_add_kde(u8 * pos, u32 kde, const u8 * data, size_t data_len)
{
  *pos++ = WLAN_VENDOR_SPECIFIC_EID;
  *pos++ = _SELECTOR_LEN_TO_RSN + data_len;
  TILK_SELE_CTOR_RSN__PUT(pos, kde);
  pos += _SELECTOR_LEN_TO_RSN;
  os_memcpy(pos, data, data_len);
  pos += data_len;
  return pos;
}

static int wpa_supplicant_send_smk_error(struct wpa_sm *sm, const u8 * dst,
                                         const u8 * peer,
                                         u16 mui, u16 error_type, int ver)
{
  size_t rlen;
  struct wpa_eapol_key *err;
  struct wpa_eapol_key_192 *err192;
  struct rsn_error_kde error;
  u8 *rbuf, *pos;
  size_t kde_len;
  u16 key_info;
  
  kde_len = 2 + _SELECTOR_LEN_TO_RSN + sizeof(error);
  if (peer)
    kde_len += 2 + _SELECTOR_LEN_TO_RSN + ETH_ALEN;
  
  rbuf = wpa_sm_alloc_eapol((const struct wpa_supplicant *)sm, IEEE802_1X_TYPE_EAPOL_KEY,
                            NULL, sizeof(*err) + kde_len, &rlen,
                            (void *)&err);
  if (rbuf == NULL)
    return -1;
  err192 = (struct wpa_eapol_key_192 *)err;
  
  err->type = EAPOL_KEY_TYPE_RSN;
  key_info = ver | WPA_KEY_INFO_SMK_MESSAGE | WPA_KEY_INFO_MIC |
    WPA_KEY_INFO_SECURE | WPA_KEY_INFO_ERROR | WPA_KEY_INFO_REQUEST;
  WPA_PUT_BE16(err->key_info, key_info);
  WPA_PUT_BE16(err->key_length, 0);
  os_memcpy(err->replay_counter, sm->request_counter, WPA_REPLAY_COUNTER_LEN);
  inc_byte_array(sm->request_counter, WPA_REPLAY_COUNTER_LEN);
  
  WPA_PUT_BE16(err->key_data_length, (u16) kde_len);
  pos = (u8 *) (err + 1);
  
  if (peer) {
    
    pos = wpa_add_kde(pos, RSN_KEY_DATA_MAC_ADDR, peer, ETH_ALEN);
  }
  
  error.mui = host_to_be16(mui);
  error.error_type = host_to_be16(error_type);
  wpa_add_kde(pos, RSN_KEY_DATA_ERROR, (u8 *) & error, sizeof(error));
  
  if (peer) {
    wpa_printf(MSG_DEBUG, "RSN: Sending EAPOL-Key SMK Error (peer "
               MACSTR " mui %d error_type %d)",
               MAC2STR(peer), mui, error_type);
  } else {
    wpa_printf(MSG_DEBUG, "RSN: Sending EAPOL-Key SMK Error "
               "(mui %d error_type %d)", mui, error_type);
  }
  
  wpa_eapol_key_send(sm, sm->ptk.kck, sm->ptk.kck_len, ver, dst,
                     ETH_P_EAPOL, rbuf, rlen, err192->key_mic);
  
  return 0;
}



static u8 *wpa_add_ie(u8 * pos, const u8 * ie, size_t ie_len)
{
  os_memcpy(pos, ie, ie_len);
  return pos + ie_len;
}

static int wpa_supplicant_send_smk_m3(struct wpa_sm *sm,
                                      const unsigned char *src_addr,
                                      const struct wpa_eapol_key *key,
                                      int ver, struct wpa_peerkey *peerkey)
{
  size_t rlen;
  struct wpa_eapol_key *reply;
  struct wpa_eapol_key_192 *reply192;
  u8 *rbuf, *pos;
  size_t kde_len;
  u16 key_info;
  
  kde_len = peerkey->rsnie_p_len +
    2 + _SELECTOR_LEN_TO_RSN + ETH_ALEN +
      2 + _SELECTOR_LEN_TO_RSN + WPA_LEN_NONCE;
  
  rbuf = wpa_sm_alloc_eapol((const struct wpa_supplicant *)sm, IEEE802_1X_TYPE_EAPOL_KEY,
                            NULL, sizeof(*reply) + kde_len, &rlen,
                            (void *)&reply);
  if (rbuf == NULL)
    return -1;
  reply192 = (struct wpa_eapol_key_192 *)reply;
  
  reply->type = EAPOL_KEY_TYPE_RSN;
  key_info = ver | WPA_KEY_INFO_SMK_MESSAGE | WPA_KEY_INFO_MIC |
    WPA_KEY_INFO_SECURE;
  WPA_PUT_BE16(reply->key_info, key_info);
  WPA_PUT_BE16(reply->key_length, 0);
  os_memcpy(reply->replay_counter, key->replay_counter,
            WPA_REPLAY_COUNTER_LEN);
  
  os_memcpy(reply->key_nonce, peerkey->pnonce, WPA_LEN_NONCE);
  
  WPA_PUT_BE16(reply->key_data_length, (u16) kde_len);
  pos = (u8 *) (reply + 1);
  
  pos = wpa_add_ie(pos, peerkey->rsnie_p, peerkey->rsnie_p_len);
  
  pos = wpa_add_kde(pos, RSN_KEY_DATA_MAC_ADDR, peerkey->addr, ETH_ALEN);
  
  wpa_add_kde(pos, RSN_KEY_DATA_NONCE, peerkey->inonce, WPA_LEN_NONCE);
  
  wpa_printf(MSG_DEBUG, "RSN: Sending EAPOL-Key SMK M3");
  wpa_eapol_key_send(sm, sm->ptk.kck, sm->ptk.kck_len, ver, src_addr,
                     ETH_P_EAPOL, rbuf, rlen, reply192->key_mic);
  
  return 0;
}


static int wpa_supplicant_process_smk_m2(struct wpa_sm *sm,
                                         const unsigned char *src_addr,
                                         const struct wpa_eapol_key *key,
                                         size_t extra_len, int ver)
{
  struct wpa_peerkey *peerkey;
  struct wpa_eapol_ie_parse kde;
  struct wpa_ie_data ie;
  int cipher;
  struct rsn_hdr_ie *hdr;
  u8 *pos;
  
  wpa_printf(MSG_DEBUG, "RSN: Received SMK M2");
  
  if (!sm->peerkey_enabled || sm->proto != WPA_RSN_PROTO) {
    wpa_printf(MSG_INFO, "RSN: SMK handshake not allowed for "
               "the current network");
    return -1;
  }
  
  if (wpa_supplicant_parse_ies((const u8 *)(key + 1), extra_len, &kde) < 0) {
    wpa_printf(MSG_INFO, "RSN: Failed to parse KDEs in SMK M2");
    return -1;
  }
  
  if (kde.rsn_ie == NULL || kde.mac_addr == NULL ||
      kde.mac_addr_len < ETH_ALEN) {
        wpa_printf(MSG_INFO, "RSN: No RSN IE or MAC address KDE in " "SMK M2");
        return -1;
      }
  
  wpa_printf(MSG_DEBUG, "RSN: SMK M2 - SMK initiator " MACSTR,
             MAC2STR(kde.mac_addr));
  
  if (kde.rsn_ie_len > PEERKEY_MAX_IE_LEN) {
    wpa_printf(MSG_INFO, "RSN: Too long Initiator RSN IE in SMK " "M2");
    return -1;
  }
  
  if (wpa_parse_wpa_ie_rsn(kde.rsn_ie, kde.rsn_ie_len, &ie) < 0) {
    wpa_printf(MSG_INFO, "RSN: Failed to parse RSN IE in SMK M2");
    return -1;
  }
  
  cipher = wpa_pick_pairwise_cipher(ie.pairwise_cipher &
                                    sm->allowed_pairwise_cipher, 0);
  if (cipher < 0) {
    wpa_printf(MSG_INFO, "RSN: No acceptable cipher in SMK M2");
    wpa_supplicant_send_smk_error(sm, src_addr, kde.mac_addr,
                                  STK_MUI_SMK, STK_ERR_CPHR_NS, ver);
    return -1;
  }
  wpa_printf(MSG_DEBUG, "RSN: Using %s for PeerKey", wpa_cipher_txt(cipher));
  
  peerkey = os_zalloc(sizeof(*peerkey));
  if (peerkey == NULL)
    return -1;
  os_memcpy(peerkey->addr, kde.mac_addr, ETH_ALEN);
  os_memcpy(peerkey->inonce, key->key_nonce, WPA_LEN_NONCE);
  os_memcpy(peerkey->rsnie_i, kde.rsn_ie, kde.rsn_ie_len);
  peerkey->rsnie_i_len = kde.rsn_ie_len;
  peerkey->cipher = cipher;
  peerkey->akmp = ie.key_mgmt;
  
  if (os_get_random(peerkey->pnonce, WPA_LEN_NONCE)) {
    wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
            "WPA: Failed to get random data for PNonce");
    os_free(peerkey);
    return -1;
  }
  
  hdr = (struct rsn_hdr_ie *)peerkey->rsnie_p;
  hdr->elem_id = WLAN_RSN_EID;
  TILK_WPA_TO_LE16(hdr->version, RSN_VERSION);
  pos = (u8 *) (hdr + 1);
  
  TILK_SELE_CTOR_RSN__PUT(pos, TILK_CIPHER_SUITE_CCMP_FOR_RSN);
  pos += _SELECTOR_LEN_TO_RSN;
  
  TILK_WPA_TO_LE16(pos, 1);
  pos += 2;
  TILK_SELE_CTOR_RSN__PUT(pos, wpa_cipher_to_suite(WPA_RSN_PROTO, cipher));
  pos += _SELECTOR_LEN_TO_RSN;
  
  hdr->len = (pos - peerkey->rsnie_p) - 2;
  peerkey->rsnie_p_len = pos - peerkey->rsnie_p;
  wpa_hexdump(MSG_DEBUG, "WPA: RSN IE for SMK handshake",
              peerkey->rsnie_p, peerkey->rsnie_p_len);
  
  wpa_supplicant_send_smk_m3(sm, src_addr, key, ver, peerkey);
  
  peerkey->next = sm->peerkey;
  sm->peerkey = peerkey;
  
  return 0;
}


static int wpa_supplicant_process_smk_m5(struct wpa_sm *sm,
                                         const unsigned char *src_addr,
                                         const struct wpa_eapol_key *key,
                                         int ver,
                                         struct wpa_peerkey *peerkey,
                                         struct wpa_eapol_ie_parse *kde)
{
  int cipher;
  struct wpa_ie_data ie;
  
  wpa_printf(MSG_DEBUG, "RSN: Received SMK M5 (Peer " MACSTR ")",
             MAC2STR(kde->mac_addr));
  if (kde->rsn_ie == NULL || kde->rsn_ie_len > PEERKEY_MAX_IE_LEN ||
      wpa_parse_wpa_ie_rsn(kde->rsn_ie, kde->rsn_ie_len, &ie) < 0) {
        wpa_printf(MSG_INFO, "RSN: No RSN IE in SMK M5");
        
        return -1;
      }
  
  if (os_memcompare(key->key_nonce, peerkey->inonce, WPA_LEN_NONCE) != 0) {
    wpa_printf(MSG_INFO, "RSN: Key Nonce in SMK M5 does "
               "not match with INonce used in SMK M1");
    return -1;
  }
  
  if (os_memcompare(kde->smk + PMK_LEN, peerkey->inonce, WPA_LEN_NONCE) != 0) {
    wpa_printf(MSG_INFO, "RSN: INonce in SMK KDE does not "
               "match with the one used in SMK M1");
    return -1;
  }
  
  os_memcpy(peerkey->rsnie_p, kde->rsn_ie, kde->rsn_ie_len);
  peerkey->rsnie_p_len = kde->rsn_ie_len;
  os_memcpy(peerkey->pnonce, kde->nonce, WPA_LEN_NONCE);
  
  cipher = wpa_pick_pairwise_cipher(ie.pairwise_cipher &
                                    sm->allowed_pairwise_cipher, 0);
  if (cipher < 0) {
    wpa_printf(MSG_INFO, "RSN: SMK Peer STA " MACSTR " selected "
               "unacceptable cipher", MAC2STR(kde->mac_addr));
    wpa_supplicant_send_smk_error(sm, src_addr, kde->mac_addr,
                                  STK_MUI_SMK, STK_ERR_CPHR_NS, ver);
    
    return -1;
  }
  wpa_printf(MSG_DEBUG, "RSN: Using %s for PeerKey", wpa_cipher_txt(cipher));
  peerkey->cipher = cipher;
  
  return 0;
}


static int wpa_supplicant_process_smk_m4(struct wpa_peerkey *peerkey,
                                         struct wpa_eapol_ie_parse *kde)
{
  wpa_printf(MSG_DEBUG, "RSN: Received SMK M4 (Initiator " MACSTR ")",
             MAC2STR(kde->mac_addr));
  
  if (os_memcompare(kde->smk + PMK_LEN, peerkey->pnonce, WPA_LEN_NONCE) != 0) {
    wpa_printf(MSG_INFO, "RSN: PNonce in SMK KDE does not "
               "match with the one used in SMK M3");
    return -1;
  }
  
  if (os_memcompare(kde->nonce, peerkey->inonce, WPA_LEN_NONCE) != 0) {
    wpa_printf(MSG_INFO, "RSN: INonce in SMK M4 did not "
               "match with the one received in SMK M2");
    return -1;
  }
  
  return 0;
}


static void rsn_smkid(const u8 * smk, const u8 * pnonce, const u8 * mac_p,
                      const u8 * inonce, const u8 * mac_i, u8 * smkid, int akmp)
{
  char *title = "SMK Name";
  const u8 *addr[5];
  const size_t len[5] = { 8, WPA_LEN_NONCE, ETH_ALEN, WPA_LEN_NONCE,
  ETH_ALEN
  };
  unsigned char hash[32];
  
  addr[0] = (u8 *) title;
  addr[1] = pnonce;
  addr[2] = mac_p;
  addr[3] = inonce;
  addr[4] = mac_i;
  
#ifdef CONFIG_IEEE80211W
  if (wpa_key_mgmt_sha256(akmp))
    vector_hmac_sha256(smk, PMK_LEN, 5, addr, len, hash);
  else
#endif
    hmac_sha1_vector(smk, PMK_LEN, 5, addr, len, hash);
  os_memcpy(smkid, hash, TK_PMK_ID_LEN);
}


static void wpa_supplicant_send_stk_1_of_4(struct wpa_sm *sm,
                                           struct wpa_peerkey *peerkey)
{
  size_t mlen;
  struct wpa_eapol_key *msg;
  u8 *mbuf;
  size_t kde_len;
  u16 key_info, ver;
  
  kde_len = 2 + _SELECTOR_LEN_TO_RSN + TK_PMK_ID_LEN;
  
  mbuf = wpa_sm_alloc_eapol((const struct wpa_supplicant *)sm, IEEE802_1X_TYPE_EAPOL_KEY, NULL,
                            sizeof(*msg) + kde_len, &mlen,
                            (void *)&msg);
  if (mbuf == NULL)
    return;
  
  msg->type = EAPOL_KEY_TYPE_RSN;
  
  if (peerkey->cipher != WPA_CIPHER_TKIP)
    ver = WPA_KEY_INFO_TYPE_HMAC_SHA1_AES;
  else
    ver = WPA_KEY_INFO_TYPE_HMAC_MD5_RC4;
  
  key_info = ver | WPA_KEY_INFO_KEY_TYPE | WPA_KEY_INFO_ACK;
  WPA_PUT_BE16(msg->key_info, key_info);
  
  if (peerkey->cipher != WPA_CIPHER_TKIP)
    WPA_PUT_BE16(msg->key_length, 16);
  else
    WPA_PUT_BE16(msg->key_length, 32);
  
  os_memcpy(msg->replay_counter, peerkey->replay_counter,
            WPA_REPLAY_COUNTER_LEN);
  inc_byte_array(peerkey->replay_counter, WPA_REPLAY_COUNTER_LEN);
  
  WPA_PUT_BE16(msg->key_data_length, kde_len);
  wpa_add_kde((u8 *) (msg + 1), RSN_KEY_DATA_PMKID,
              peerkey->smkid, TK_PMK_ID_LEN);
  
  if (os_get_random(peerkey->inonce, WPA_LEN_NONCE)) {
    wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
            "RSN: Failed to get random data for INonce (STK)");
    os_free(mbuf);
    return;
  }
  wpa_hexdump(MSG_DEBUG, "RSN: INonce for STK 4-Way Handshake",
              peerkey->inonce, WPA_LEN_NONCE);
  os_memcpy(msg->key_nonce, peerkey->inonce, WPA_LEN_NONCE);
  
  wpa_printf(MSG_DEBUG, "RSN: Sending EAPOL-Key STK 1/4 to " MACSTR,
             MAC2STR(peerkey->addr));
  wpa_eapol_key_send(sm, NULL, 0, ver, peerkey->addr, ETH_P_EAPOL,
                     mbuf, mlen, NULL);
}


static int wpa_supplicant_process_smk_m45(struct wpa_sm *sm,
                                          const unsigned char *src_addr,
                                          const struct wpa_eapol_key *key,
                                          size_t extra_len, int ver)
{
  struct wpa_peerkey *peerkey;
  struct wpa_eapol_ie_parse kde;
  u32 lifetime;
  
  if (!sm->peerkey_enabled || sm->proto != WPA_RSN_PROTO) {
    wpa_printf(MSG_DEBUG, "RSN: SMK handshake not allowed for "
               "the current network");
    return -1;
  }
  
  if (wpa_supplicant_parse_ies((const u8 *)(key + 1), extra_len, &kde) < 0) {
    wpa_printf(MSG_INFO, "RSN: Failed to parse KDEs in SMK M4/M5");
    return -1;
  }
  
  if (kde.mac_addr == NULL || kde.mac_addr_len < ETH_ALEN ||
      kde.nonce == NULL || kde.nonce_len < WPA_LEN_NONCE ||
        kde.smk == NULL || kde.smk_len < PMK_LEN + WPA_LEN_NONCE ||
          kde.lifetime == NULL || kde.lifetime_len < 4) {
            wpa_printf(MSG_INFO, "RSN: No MAC Address, Nonce, SMK, or "
                       "Lifetime KDE in SMK M4/M5");
            return -1;
          }
  
  for (peerkey = sm->peerkey; peerkey; peerkey = peerkey->next) {
    if (os_memcompare(peerkey->addr, kde.mac_addr, ETH_ALEN) == 0 &&
        os_memcompare(peerkey->initiator ? peerkey->inonce :
                        peerkey->pnonce, key->key_nonce, WPA_LEN_NONCE) == 0)
      break;
  }
  if (peerkey == NULL) {
    wpa_printf(MSG_INFO, "RSN: No matching SMK handshake found "
               "for SMK M4/M5: peer " MACSTR, MAC2STR(kde.mac_addr));
    return -1;
  }
  
  if (peerkey->initiator) {
    if (wpa_supplicant_process_smk_m5(sm, src_addr, key, ver,
                                      peerkey, &kde) < 0)
      return -1;
  } else {
    if (wpa_supplicant_process_smk_m4(peerkey, &kde) < 0)
      return -1;
  }
  
  os_memcpy(peerkey->smk, kde.smk, PMK_LEN);
  peerkey->smk_complete = 1;
  wpa_hexdump_key(MSG_DEBUG, "RSN: SMK", peerkey->smk, PMK_LEN);
  lifetime = TILK_WPA_READ_BE32(kde.lifetime);
  wpa_printf(MSG_DEBUG, "RSN: SMK lifetime %u seconds", lifetime);
  if (lifetime > 1000000000)
    lifetime = 1000000000;
  peerkey->lifetime = lifetime;
  if (peerkey->initiator) {
    rsn_smkid(peerkey->smk, peerkey->pnonce, peerkey->addr,
              peerkey->inonce, sm->own_addr, peerkey->smkid, peerkey->akmp);
    wpa_supplicant_send_stk_1_of_4(sm, peerkey);
  } else {
    rsn_smkid(peerkey->smk, peerkey->pnonce, sm->own_addr,
              peerkey->inonce, peerkey->addr, peerkey->smkid,
              peerkey->akmp);
  }
  wpa_hexdump(MSG_DEBUG, "RSN: SMKID", peerkey->smkid, TK_PMK_ID_LEN);
  
  return 0;
}


void peerkey_rx_eapol_smk(struct wpa_sm *sm, const u8 * src_addr,
                          struct wpa_eapol_key *key, size_t extra_len,
                          u16 key_info, u16 ver)
{
  if (key_info & WPA_KEY_INFO_ERROR) {
    
    wpa_supplicant_process_smk_error(sm, src_addr, key, extra_len);
  } else if (key_info & WPA_KEY_INFO_ACK) {
    
    wpa_supplicant_process_smk_m2(sm, src_addr, key, extra_len, ver);
  } else {
    
    wpa_supplicant_process_smk_m45(sm, src_addr, key, extra_len, ver);
  }
}


/**
* wpa_sm_rx_eapol - Process received WPA EAPOL frames
* @sm: Pointer to WPA state machine data from wpa_sm_init()
* @src_addr: Source MAC address of the EAPOL packet
* @buf: Pointer to the beginning of the EAPOL data (EAPOL header)
* @len: Length of the EAPOL frame
* Returns: 1 = WPA EAPOL-Key processed, 0 = not a WPA EAPOL-Key, -1 failure
*
* This function is called for each received EAPOL frame. Other than EAPOL-Key
* frames can be skipped if filtering is done elsewhere. wpa_sm_rx_eapol() is
* only processing WPA and WPA2 EAPOL-Key frames.
*
* The received EAPOL-Key packets are validated and valid packets are replied
* to. In addition, key material (PTK, GTK) is configured at the end of a
* successful key handshake.
*/
int wpa_sm_rx_eapol(struct wpa_sm *sm, const u8 *src_addr,
		    const u8 *buf, size_t len)
{
  size_t plen, data_len, key_data_len;
  const struct ieee802_1x_hdr *hdr;
  struct wpa_eapol_key *key;
  struct wpa_eapol_key_192 *key192;
  u16 key_info, ver;
  u8 *tmp = NULL;
  int ret = -1;
  struct wpa_peerkey *peerkey = NULL;
  u8 *key_data;
  size_t mic_len, keyhdrlen;
  
  
#ifdef CONFIG_IEEE80211R
  sm->ft_completed = 0;
#endif /* CONFIG_IEEE80211R */
  
  mic_len = wpa_mic_len(sm->key_mgmt);
  keyhdrlen = mic_len == 24 ? sizeof(*key192) : sizeof(*key);
  
  if (len < sizeof(*hdr) + keyhdrlen) {
    wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
            "WPA: EAPOL frame too short to be a WPA "
              "EAPOL-Key (len %lu, expecting at least %lu)",
              (unsigned long) len,
              (unsigned long) sizeof(*hdr) + keyhdrlen);
    return 0;
  }
  
  hdr = (const struct ieee802_1x_hdr *) buf;
  plen = be_to_host16(hdr->length);
  data_len = plen + sizeof(*hdr);
  wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
          "IEEE 802.1X RX: version=%d type=%d length=%lu",
          hdr->version, hdr->type, (unsigned long) plen);
  
  if (hdr->version < EAPOL_VERSION) {
    /* TODO: backwards compatibility */
  }
  if (hdr->type != IEEE802_1X_TYPE_EAPOL_KEY) {
    wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
            "WPA: EAPOL frame (type %u) discarded, "
              "not a Key frame", hdr->type);
    ret = 0;
    goto out;
  }
  wpa_hexdump(MSG_MSGDUMP, "WPA: RX EAPOL-Key", buf, len);
  if (plen > len - sizeof(*hdr) || plen < keyhdrlen) {
    wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
            "WPA: EAPOL frame payload size %lu "
              "invalid (frame size %lu)",
              (unsigned long) plen, (unsigned long) len);
    ret = 0;
    goto out;
  }
  if (data_len < len) {
    wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
            "WPA: ignoring %lu bytes after the IEEE 802.1X data",
            (unsigned long) len - data_len);
  }
  
  /*
  * Make a copy of the frame since we need to modify the buffer during
  * MAC validation and Key Data decryption.
  */
  tmp = os_malloc(data_len);
  if (tmp == NULL)
    goto out;
  os_memcpy(tmp, buf, data_len);
  key = (struct wpa_eapol_key *) (tmp + sizeof(struct ieee802_1x_hdr));
  key192 = (struct wpa_eapol_key_192 *)
    (tmp + sizeof(struct ieee802_1x_hdr));
  if (mic_len == 24)
    key_data = (u8 *) (key192 + 1);
  else
    key_data = (u8 *) (key + 1);
  
  if (key->type != EAPOL_KEY_TYPE_WPA && key->type != EAPOL_KEY_TYPE_RSN)
  {
    wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
            "WPA: EAPOL-Key type (%d) unknown, discarded",
            key->type);
    ret = 0;
    goto out;
  }
  
  if (mic_len == 24)
    key_data_len = WPA_GET_BE16(key192->key_data_length);
  else
    key_data_len = WPA_GET_BE16(key->key_data_length);
  wpa_eapol_key_dump(sm, key, key_data_len, key192->key_mic, mic_len);
  
  if (key_data_len > plen - keyhdrlen) {
    wpa_msg(sm->ctx->msg_ctx, MSG_INFO, "WPA: Invalid EAPOL-Key "
            "frame - key_data overflow (%u > %u)",
            (unsigned int) key_data_len,
            (unsigned int) (plen - keyhdrlen));
    goto out;
  }
  key_info = WPA_GET_BE16(key->key_info);
  ver = key_info & WPA_KEY_INFO_TYPE_MASK;
  
  //if ((key_info & WPA_KEY_INFO_MIC) && !peerkey &&)
  wpa_supplicant_verify_eapol_key_mic(sm, key192, ver, tmp, data_len);
  //goto out;
  
  
  if ((sm->proto == WPA_PROTO_RSN || sm->proto == WPA_PROTO_OSEN) &&
      (key_info & WPA_KEY_INFO_ENCR_KEY_DATA)) {
        if (wpa_supplicant_decrypt_key_data(sm, key, ver, key_data,
                                            &key_data_len))
          goto out;
      }
  
  if (key_info & WPA_KEY_INFO_KEY_TYPE) {
    if (key_info & WPA_KEY_INFO_KEY_INDEX_MASK) {
      wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
              "WPA: Ignored EAPOL-Key (Pairwise) with "
                "non-zero key index");
      goto out;
    }
    if (key_info & WPA_KEY_INFO_MIC) {
      /* 3/4 4-Way Handshake */
      wpa_supplicant_process_3_of_4(sm, key, ver, key_data,
                                    key_data_len);
    } else {
      /* 1/4 4-Way Handshake */
      wpa_supplicant_process_1_of_4(sm, src_addr, key,
                                    ver, key_data,
                                    key_data_len);
    }
  } else if (key_info & WPA_KEY_INFO_SMK_MESSAGE) {
    /* PeerKey SMK Handshake */
    peerkey_rx_eapol_smk(sm, src_addr, key, key_data_len, key_info,
                         ver);
  } else {
    if (key_info & WPA_KEY_INFO_MIC) {
      /* 1/2 Group Key Handshake */
      wpa_supplicant_process_1_of_2(sm, src_addr, key,
                                    key_data, key_data_len,
                                    ver);
    } else {
      wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
              "WPA: EAPOL-Key (Group) without Mic bit - "
                "dropped");
    }
  }
  
  ret = 1;
  
out:
  bin_clear_free(tmp, data_len);
  return ret;
}


#ifdef CONFIG_CTRL_IFACE
static u32 wpa_key_mgmt_suite(struct wpa_sm *sm)
{
  switch (sm->key_mgmt) {
  case WPA_KEY_MGMT_IEEE8021X:
    return ((sm->proto == WPA_PROTO_RSN ||
             sm->proto == WPA_PROTO_OSEN) ?
  RSN_AUTH_KEY_MGMT_UNSPEC_802_1X :
               WPA_AUTH_KEY_MGMT_UNSPEC_802_1X);
  case WPA_KEY_MGMT_PSK:
    return (sm->proto == WPA_PROTO_RSN ?
  RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X :
              WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X);
#ifdef CONFIG_IEEE80211R
  case WPA_KEY_MGMT_FT_IEEE8021X:
    return RSN_AUTH_KEY_MGMT_FT_802_1X;
  case WPA_KEY_MGMT_FT_PSK:
    return RSN_AUTH_KEY_MGMT_FT_PSK;
#endif /* CONFIG_IEEE80211R */
#ifdef CONFIG_IEEE80211W
  case WPA_KEY_MGMT_IEEE8021X_SHA256:
    return RSN_AUTH_KEY_MGMT_802_1X_SHA256;
  case WPA_KEY_MGMT_PSK_SHA256:
    return RSN_AUTH_KEY_MGMT_PSK_SHA256;
#endif /* CONFIG_IEEE80211W */
  case WPA_KEY_MGMT_CCKM:
    return (sm->proto == WPA_PROTO_RSN ?
  RSN_AUTH_KEY_MGMT_CCKM:
              WPA_AUTH_KEY_MGMT_CCKM);
  case WPA_KEY_MGMT_WPA_NONE:
    return WPA_AUTH_KEY_MGMT_NONE;
  case WPA_KEY_MGMT_IEEE8021X_SUITE_B:
    return RSN_AUTH_KEY_MGMT_802_1X_SUITE_B;
  case WPA_KEY_MGMT_IEEE8021X_SUITE_B_192:
    return RSN_AUTH_KEY_MGMT_802_1X_SUITE_B_192;
  default:
    return 0;
  }
}


#define RSN_SUITE "%02x-%02x-%02x-%d"
#define RSN_SUITE_ARG(s) \
((s) >> 24) & 0xff, ((s) >> 16) & 0xff, ((s) >> 8) & 0xff, (s) & 0xff

/**
* wpa_sm_get_mib - Dump text list of MIB entries
* @sm: Pointer to WPA state machine data from wpa_sm_init()
* @buf: Buffer for the list
* @buflen: Length of the buffer
* Returns: Number of bytes written to buffer
*
* This function is used fetch dot11 MIB variables.
*/
int wpa_sm_get_mib(struct wpa_sm *sm, char *buf, size_t buflen)
{
  char pmkid_txt[PMKID_LEN * 2 + 1];
  int rsna, ret;
  size_t len;
  
  if (sm->cur_pmksa) {
    wpa_snprintf_hex(pmkid_txt, sizeof(pmkid_txt),
                     sm->cur_pmksa->pmkid, PMKID_LEN);
  } else
    pmkid_txt[0] = '\0';
  
  if ((wpa_key_mgmt_wpa_psk(sm->key_mgmt) ||
       wpa_key_mgmt_wpa_ieee8021x(sm->key_mgmt)) &&
      sm->proto == WPA_PROTO_RSN)
    rsna = 1;
  else
    rsna = 0;
  
  ret = os_snprintf(buf, buflen,
                    "dot11RSNAOptionImplemented=TRUE\n"
                      "dot11RSNAPreauthenticationImplemented=TRUE\n"
                        "dot11RSNAEnabled=%s\n"
			  "dot11RSNAPreauthenticationEnabled=%s\n"
                            "dot11RSNAConfigVersion=%d\n"
                              "dot11RSNAConfigPairwiseKeysSupported=5\n"
                                "dot11RSNAConfigGroupCipherSize=%d\n"
                                  "dot11RSNAConfigPMKLifetime=%d\n"
                                    "dot11RSNAConfigPMKReauthThreshold=%d\n"
                                      "dot11RSNAConfigNumberOfPTKSAReplayCounters=1\n"
                                        "dot11RSNAConfigSATimeout=%d\n",
                                        rsna ? "TRUE" : "FALSE",
                                        rsna ? "TRUE" : "FALSE",
                                        RSN_VERSION,
                                        wpa_cipher_key_len(sm->group_cipher) * 8,
                                        sm->dot11RSNAConfigPMKLifetime,
                                        sm->dot11RSNAConfigPMKReauthThreshold,
                                        sm->dot11RSNAConfigSATimeout);
  if (os_snprintf_error(buflen, ret))
    return 0;
  len = ret;
  
  ret = os_snprintf(
                    buf + len, buflen - len,
                    "dot11RSNAAuthenticationSuiteSelected=" RSN_SUITE "\n"
                      "dot11RSNAPairwiseCipherSelected=" RSN_SUITE "\n"
                        "dot11RSNAGroupCipherSelected=" RSN_SUITE "\n"
                          "dot11RSNAPMKIDUsed=%s\n"
                            "dot11RSNAAuthenticationSuiteRequested=" RSN_SUITE "\n"
                              "dot11RSNAPairwiseCipherRequested=" RSN_SUITE "\n"
                                "dot11RSNAGroupCipherRequested=" RSN_SUITE "\n"
                                  "dot11RSNAConfigNumberOfGTKSAReplayCounters=0\n"
                                    "dot11RSNA4WayHandshakeFailures=%u\n",
                                    RSN_SUITE_ARG(wpa_key_mgmt_suite(sm)),
                                    RSN_SUITE_ARG(wpa_cipher_to_suite(sm->proto,
                                                                      sm->pairwise_cipher)),
                                    RSN_SUITE_ARG(wpa_cipher_to_suite(sm->proto,
                                                                      sm->group_cipher)),
                                    pmkid_txt,
                                    RSN_SUITE_ARG(wpa_key_mgmt_suite(sm)),
                                    RSN_SUITE_ARG(wpa_cipher_to_suite(sm->proto,
                                                                      sm->pairwise_cipher)),
                                    RSN_SUITE_ARG(wpa_cipher_to_suite(sm->proto,
                                                                      sm->group_cipher)),
                                    sm->dot11RSNA4WayHandshakeFailures);
  if (!os_snprintf_error(buflen - len, ret))
    len += ret;
  
  return (int) len;
}
#endif /* CONFIG_CTRL_IFACE */


static void wpa_sm_pmksa_free_cb(struct rsn_pmksa_cache_entry *entry,
				 void *ctx, enum pmksa_free_reason reason)
{
  struct wpa_sm *sm = ctx;
  int deauth = 0;
  
  wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "RSN: PMKSA cache entry free_cb: "
          MACSTR " reason=%d", MAC2STR(entry->aa), reason);
  
  if (sm->cur_pmksa == entry) {
    wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
            "RSN: %s current PMKSA entry",
            reason == PMKSA_REPLACE ? "replaced" : "removed");
    pmksa_cache_clear_current(sm);
    
    /*
    * If an entry is simply being replaced, there's no need to
    * deauthenticate because it will be immediately re-added.
    * This happens when EAP authentication is completed again
    * (reauth or failed PMKSA caching attempt).
    */
    if (reason != PMKSA_REPLACE)
      deauth = 1;
  }
  
  if (reason == PMKSA_EXPIRE &&
      (sm->pmk_len == entry->pmk_len &&
       os_memcmp(sm->pmk, entry->pmk, sm->pmk_len) == 0)) {
         wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
                 "RSN: deauthenticating due to expired PMK");
         pmksa_cache_clear_current(sm);
         deauth = 1;
       }
  
  if (deauth) {
    os_memset(sm->pmk, 0, sizeof(sm->pmk));
  }
}


struct rsn_pmksa_cache
*pmksa_cache_init(void (*free_cb)
                  (struct rsn_pmksa_cache_entry * entry, void *ctx,
                   enum pmksa_free_reason reason), void *ctx,
                  struct wpa_sm *sm)
{
  struct rsn_pmksa_cache *pmksa;
  
  pmksa = os_zalloc(sizeof(*pmksa));
  if (pmksa) {
    pmksa->free_cb = free_cb;
    pmksa->ctx = ctx;
    pmksa->sm = sm;
  }
  
  return pmksa;
}



/**
* wpa_sm_init - Initialize WPA state machine
* @ctx: Context pointer for callbacks; this needs to be an allocated buffer
* Returns: Pointer to the allocated WPA state machine data
*
* This function is used to allocate a new WPA state machine and the returned
* value is passed to all WPA state machine calls.
*/
struct wpa_sm * wpa_sm_init(struct wpa_supplicant *wpa_s)
{
  struct wpa_sm *sm;
  
  sm = os_zalloc(sizeof(*sm));
  if (sm == NULL)
    return NULL;
  dl_list_init(&sm->pmksa_candidates);
  sm->renew_snonce = 1;
  
  sm->dot11RSNAConfigPMKLifetime = 43200;
  sm->dot11RSNAConfigPMKReauthThreshold = 70;
  sm->dot11RSNAConfigSATimeout = 60;
  
  sm->pmksa = pmksa_cache_init(wpa_sm_pmksa_free_cb, sm, sm);
  if (sm->pmksa == NULL) {
    wpa_msg(sm->ctx->msg_ctx, MSG_ERROR,
            "RSN: PMKSA cache initialization failed");
    os_free(sm);
    return NULL;
  }
  
  return sm;
}


void wpa_sm_drop_sa(struct wpa_sm *sm)
{
  wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "WPA: Clear old PMK and PTK");
  sm->ptk_set = 0;
  sm->tptk_set = 0;
  os_memset(sm->pmk, 0, sizeof(sm->pmk));
  os_memset(&sm->ptk, 0, sizeof(sm->ptk));
  os_memset(&sm->tptk, 0, sizeof(sm->tptk));
#ifdef CONFIG_IEEE80211R
  os_memset(sm->xxkey, 0, sizeof(sm->xxkey));
  os_memset(sm->pmk_r0, 0, sizeof(sm->pmk_r0));
  os_memset(sm->pmk_r1, 0, sizeof(sm->pmk_r1));
#endif /* CONFIG_IEEE80211R */
}


/**
* wpa_sm_deinit - Deinitialize WPA state machine
* @sm: Pointer to WPA state machine data from wpa_sm_init()
*/
void wpa_sm_deinit(struct wpa_sm *sm)
{
  if (sm == NULL)
    return;
  
  os_free(sm->assoc_wpa_ie);
  os_free(sm->ap_wpa_ie);
  os_free(sm->ap_rsn_ie);
  wpa_sm_drop_sa(sm);
  os_free(sm->ctx);
#ifdef CONFIG_IEEE80211R
  os_free(sm->assoc_resp_ies);
#endif /* CONFIG_IEEE80211R */
#ifdef CONFIG_TESTING_OPTIONS
  wpabuf_free(sm->test_assoc_ie);
#endif /* CONFIG_TESTING_OPTIONS */
  os_free(sm);
}


/**
* wpa_sm_notify_assoc - Notify WPA state machine about association
* @sm: Pointer to WPA state machine data from wpa_sm_init()
* @bssid: The BSSID of the new association
*
* This function is called to let WPA state machine know that the connection
* was established.
*/
void wpa_sm_notify_assoc(struct wpa_sm *sm, const u8 *bssid)
{
  int clear_ptk = 1;
  
  if (sm == NULL)
    return;
  
  wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
          "WPA: Association event - clear replay counter");
  os_memcpy(sm->bssid, bssid, ETH_ALEN);
  os_memset(sm->rx_replay_counter, 0, WPA_REPLAY_COUNTER_LEN);
  sm->rx_replay_counter_set = 0;
  sm->renew_snonce = 1;
  
  
#ifdef CONFIG_IEEE80211R
  if (wpa_ft_is_completed(sm)) {
    /*
    * Clear portValid to kick EAPOL state machine to re-enter
    * AUTHENTICATED state to get the EAPOL port Authorized.
    */
    eapol_sm_notify_portValid(sm->eapol, FALSE);
    wpa_supplicant_key_neg_complete(sm, sm->bssid, 1);
    
    /* Prepare for the next transition */
    wpa_ft_prepare_auth_request(sm, NULL);
    
    clear_ptk = 0;
  }
#endif /* CONFIG_IEEE80211R */
  
  if (clear_ptk) {
    /*
    * IEEE 802.11, 8.4.10: Delete PTK SA on (re)association if
    * this is not part of a Fast BSS Transition.
    */
    wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "WPA: Clear old PTK");
    sm->ptk_set = 0;
    os_memset(&sm->ptk, 0, sizeof(sm->ptk));
    sm->tptk_set = 0;
    os_memset(&sm->tptk, 0, sizeof(sm->tptk));
  }
  
#ifdef CONFIG_TDLS
  wpa_tdls_assoc(sm);
#endif /* CONFIG_TDLS */
  
#ifdef CONFIG_P2P
  os_memset(sm->p2p_ip_addr, 0, sizeof(sm->p2p_ip_addr));
#endif /* CONFIG_P2P */
}


/**
* wpa_sm_notify_disassoc - Notify WPA state machine about disassociation
* @sm: Pointer to WPA state machine data from wpa_sm_init()
*
* This function is called to let WPA state machine know that the connection
* was lost. This will abort any existing pre-authentication session.
*/
void wpa_sm_notify_disassoc(struct wpa_sm *sm)
{
  ;
  pmksa_cache_clear_current(sm);
  if (wpa_sm_get_state() == WPA_4WAY_HANDSHAKE)
    sm->dot11RSNA4WayHandshakeFailures++;
#ifdef CONFIG_TDLS
  wpa_tdls_disassoc(sm);
#endif /* CONFIG_TDLS */
  
  /* Keys are not needed in the WPA state machine anymore */
  wpa_sm_drop_sa(sm);
  
  sm->msg_3_of_4_ok = 0;
}


/**
* wpa_sm_set_pmk - Set PMK
* @sm: Pointer to WPA state machine data from wpa_sm_init()
* @pmk: The new PMK
* @pmk_len: The length of the new PMK in bytes
* @pmkid: Calculated PMKID
* @bssid: AA to add into PMKSA cache or %NULL to not cache the PMK
*
* Configure the PMK for WPA state machine.
*/
void wpa_sm_set_pmk(struct wpa_sm *sm, const u8 *pmk, size_t pmk_len,
		    const u8 *pmkid, const u8 *bssid)
{
  if (sm == NULL)
    return;
  
  sm->pmk_len = pmk_len;
  os_memcpy(sm->pmk, pmk, pmk_len);
  
#ifdef CONFIG_IEEE80211R
  /* Set XXKey to be PSK for FT key derivation */
  sm->xxkey_len = pmk_len;
  os_memcpy(sm->xxkey, pmk, pmk_len);
#endif /* CONFIG_IEEE80211R */
  
  if (bssid) {
    pmksa_cache_add(sm->pmksa, pmk, pmk_len, pmkid, NULL, 0,
                    bssid, sm->own_addr,
                    sm->network_ctx, sm->key_mgmt);
  }
}


/**
* wpa_sm_set_fast_reauth - Set fast reauthentication (EAP) enabled/disabled
* @sm: Pointer to WPA state machine data from wpa_sm_init()
* @fast_reauth: Whether fast reauthentication (EAP) is allowed
*/
void wpa_sm_set_fast_reauth(struct wpa_sm *sm, int fast_reauth)
{
  if (sm)
    sm->fast_reauth = fast_reauth;
}


/**
* wpa_sm_set_scard_ctx - Set context pointer for smartcard callbacks
* @sm: Pointer to WPA state machine data from wpa_sm_init()
* @scard_ctx: Context pointer for smartcard related callback functions
*/
void wpa_sm_set_scard_ctx(struct wpa_sm *sm, void *scard_ctx)
{
  if (sm == NULL)
    return;
  sm->scard_ctx = scard_ctx;
}


/**
* wpa_sm_set_config - Notification of current configration change
* @sm: Pointer to WPA state machine data from wpa_sm_init()
* @config: Pointer to current network configuration
*
* Notify WPA state machine that configuration has changed. config will be
* stored as a backpointer to network configuration. This can be %NULL to clear
* the stored pointed.
*/
void wpa_sm_set_config(struct wpa_sm *sm, struct rsn_supp_config *config)
{
  if (!sm)
    return;
  
  if (config) {
    sm->network_ctx = config->network_ctx;
    sm->peerkey_enabled = config->peerkey_enabled;
    sm->allowed_pairwise_cipher = config->allowed_pairwise_cipher;
    sm->proactive_key_caching = config->proactive_key_caching;
    sm->eap_workaround = config->eap_workaround;
    sm->eap_conf_ctx = config->eap_conf_ctx;
    if (config->ssid) {
      os_memcpy(sm->ssid, config->ssid, config->ssid_len);
      sm->ssid_len = config->ssid_len;
    } else
      sm->ssid_len = 0;
    sm->wpa_ptk_rekey = config->wpa_ptk_rekey;
    sm->p2p = config->p2p;
  } else {
    sm->network_ctx = NULL;
    sm->peerkey_enabled = 0;
    sm->allowed_pairwise_cipher = 0;
    sm->proactive_key_caching = 0;
    sm->eap_workaround = 0;
    sm->eap_conf_ctx = NULL;
    sm->ssid_len = 0;
    sm->wpa_ptk_rekey = 0;
    sm->p2p = 0;
  }
}


/**
* wpa_sm_set_own_addr - Set own MAC address
* @sm: Pointer to WPA state machine data from wpa_sm_init()
* @addr: Own MAC address
*/
void wpa_sm_set_own_addr(struct wpa_sm *sm, const u8 *addr)
{
  if (sm)
    os_memcpy(sm->own_addr, addr, ETH_ALEN);
}


/**
* wpa_sm_set_ifname - Set network interface name
* @sm: Pointer to WPA state machine data from wpa_sm_init()
* @ifname: Interface name
* @bridge_ifname: Optional bridge interface name (for pre-auth)
*/
void wpa_sm_set_ifname(struct wpa_sm *sm, const char *ifname,
		       const char *bridge_ifname)
{
  if (sm) {
    sm->ifname = ifname;
    sm->bridge_ifname = bridge_ifname;
  }
}


/**
* wpa_sm_set_eapol - Set EAPOL state machine pointer
* @sm: Pointer to WPA state machine data from wpa_sm_init()
* @eapol: Pointer to EAPOL state machine allocated with eapol_sm_init()
*/
void wpa_sm_set_eapol(struct wpa_sm *sm, struct eapol_sm *eapol)
{
  if (sm)
    sm->eapol = eapol;
}


/**
* wpa_sm_set_param - Set WPA state machine parameters
* @sm: Pointer to WPA state machine data from wpa_sm_init()
* @param: Parameter field
* @value: Parameter value
* Returns: 0 on success, -1 on failure
*/
int wpa_sm_set_param(struct wpa_sm *sm, enum wpa_sm_conf_params param,
		     unsigned int value)
{
  int ret = 0;
  
  if (sm == NULL)
    return -1;
  
  switch (param) {
  case RSNA_PMK_LIFETIME:
    if (value > 0)
      sm->dot11RSNAConfigPMKLifetime = value;
    else
      ret = -1;
    break;
  case RSNA_PMK_REAUTH_THRESHOLD:
    if (value > 0 && value <= 100)
      sm->dot11RSNAConfigPMKReauthThreshold = value;
    else
      ret = -1;
    break;
  case RSNA_SA_TIMEOUT:
    if (value > 0)
      sm->dot11RSNAConfigSATimeout = value;
    else
      ret = -1;
    break;
  case WPA_PARAM_PROTO:
    sm->proto = value;
    break;
  case WPA_PARAM_PAIRWISE:
    sm->pairwise_cipher = value;
    break;
  case WPA_PARAM_GROUP:
    sm->group_cipher = value;
    break;
  case WPA_PARAM_KEY_MGMT:
    sm->key_mgmt = value;
    break;
#ifdef CONFIG_IEEE80211W
  case WPA_PARAM_MGMT_GROUP:
    sm->mgmt_group_cipher = value;
    break;
#endif /* CONFIG_IEEE80211W */
  case WPA_PARAM_RSN_ENABLED:
    sm->rsn_enabled = value;
    break;
  case WPA_PARAM_MFP:
    sm->mfp = value;
    break;
  default:
    break;
  }
  
  return ret;
}


/**
* wpa_sm_get_status - Get WPA state machine
* @sm: Pointer to WPA state machine data from wpa_sm_init()
* @buf: Buffer for status information
* @buflen: Maximum buffer length
* @verbose: Whether to include verbose status information
* Returns: Number of bytes written to buf.
*
* Query WPA state machine for status information. This function fills in
* a text area with current status information. If the buffer (buf) is not
* large enough, status information will be truncated to fit the buffer.
*/
int wpa_sm_get_status(struct wpa_sm *sm, char *buf, size_t buflen,
		      int verbose)
{
  char *pos = buf, *end = buf + buflen;
  int ret;
  
  ret = os_snprintf(pos, end - pos,
                    "pairwise_cipher=%s\n"
                      "group_cipher=%s\n"
                        "key_mgmt=%s\n",
                        wpa_cipher_txt(sm->pairwise_cipher),
                        wpa_cipher_txt(sm->group_cipher),
                        wpa_key_mgmt_txt(sm->key_mgmt, sm->proto));
  if (os_snprintf_error(end - pos, ret))
    return pos - buf;
  pos += ret;
  
  if (sm->mfp != NO_MGMT_FRAME_PROTECTION && sm->ap_rsn_ie) {
    struct wpa_ie_data rsn;
    if (wpa_parse_wpa_ie_rsn(sm->ap_rsn_ie, sm->ap_rsn_ie_len, &rsn)
        >= 0 &&
          rsn.capabilities & (WPA_CAPABILITY_MFPR |
                              WPA_CAPABILITY_MFPC)) {
                                ret = os_snprintf(pos, end - pos, "pmf=%d\n",
                                                  (rsn.capabilities &
                                                   WPA_CAPABILITY_MFPR) ? 2 : 1);
                                if (os_snprintf_error(end - pos, ret))
                                  return pos - buf;
                                pos += ret;
                              }
  }
  
  return pos - buf;
}


int wpa_sm_pmf_enabled(struct wpa_sm *sm)
{
  struct wpa_ie_data rsn;
  
  if (sm->mfp == NO_MGMT_FRAME_PROTECTION || !sm->ap_rsn_ie)
    return 0;
  
  if (wpa_parse_wpa_ie_rsn(sm->ap_rsn_ie, sm->ap_rsn_ie_len, &rsn) >= 0 &&
      rsn.capabilities & (WPA_CAPABILITY_MFPR | WPA_CAPABILITY_MFPC))
    return 1;
  
  return 0;
}


static int wpa_gen_wpa_ie_rsn(u8 * rsn_ie, size_t rsn_ie_len,
                              int pairwise_cipher, int group_cipher,
                              int key_mgmt, int mgmt_group_cipher,
                              struct wpa_sm *sm)
{
  u8 *pos;
  struct rsn_hdr_ie *hdr;
  u16 capab;
  u32 suite;
  
  if (rsn_ie_len < sizeof(*hdr) + _SELECTOR_LEN_TO_RSN +
      2 + _SELECTOR_LEN_TO_RSN + 2 + _SELECTOR_LEN_TO_RSN + 2 +
        (sm->cur_pmksa ? 2 + TK_PMK_ID_LEN : 0)) {
          wpa_printf(MSG_DEBUG, "RSN: Too short IE buffer (%lu bytes)",
                     (unsigned long)rsn_ie_len);
          return -1;
	}
  
  hdr = (struct rsn_hdr_ie *)rsn_ie;
  hdr->elem_id = WLAN_RSN_EID;
  TILK_WPA_TO_LE16(hdr->version, RSN_VERSION);
  pos = (u8 *) (hdr + 1);
  
  suite = wpa_cipher_to_suite(WPA_RSN_PROTO, group_cipher);
  if (suite == 0) {
    wpa_printf(MSG_WARNING, "Invalid group cipher (%d).", group_cipher);
    return -1;
  }
  TILK_SELE_CTOR_RSN__PUT(pos, suite);
  pos += _SELECTOR_LEN_TO_RSN;
  
  *pos++ = 1;
  *pos++ = 0;
  suite = wpa_cipher_to_suite(WPA_RSN_PROTO, pairwise_cipher);
  if (suite == 0 ||
      (!wpa_cipher_valid_pairwise(pairwise_cipher) &&
       pairwise_cipher != WPA_NONE_CIPHER)) {
         wpa_printf(MSG_WARNING, "Invalid pairwise cipher (%d).",
                    pairwise_cipher);
         return -1;
       }
  TILK_SELE_CTOR_RSN__PUT(pos, suite);
  pos += _SELECTOR_LEN_TO_RSN;
  
  *pos++ = 1;
  *pos++ = 0;
  if (key_mgmt == WPA_IEEE8021X_KEY_MGMT) {
    TILK_SELE_CTOR_RSN__PUT(pos, TILK_AKMGMT_UNSPEC_802_1X_FOR_RSN);
  } else if (key_mgmt == WPA_PSK_KEY_MGMT) {
    TILK_SELE_CTOR_RSN__PUT(pos, TILK_AKM_PSK_OVER_802_1X_FOR_RSN);
  } else if (key_mgmt == WPA_KEY_MGMT_CCKM) {
    TILK_SELE_CTOR_RSN__PUT(pos, RSN_AUTH_KEY_MGMT_CCKM);
  } else if (key_mgmt == WPA_KEY_MGMT_IEEE8021X_SUITE_B_192) {
    TILK_SELE_CTOR_RSN__PUT(pos, RSN_AUTH_KEY_MGMT_802_1X_SUITE_B_192);
  } else if (key_mgmt == WPA_KEY_MGMT_IEEE8021X_SUITE_B) {
    TILK_SELE_CTOR_RSN__PUT(pos, RSN_AUTH_KEY_MGMT_802_1X_SUITE_B);
  } else {
    wpa_printf(MSG_WARNING, "Invalid key management type (%d).", key_mgmt);
    return -1;
  }
  pos += _SELECTOR_LEN_TO_RSN;
  
  capab = 0;
  TILK_WPA_TO_LE16(pos, capab);
  pos += 2;
  
  if (sm->cur_pmksa) {
    
    *pos++ = 1;
    *pos++ = 0;
    
    os_memcpy(pos, sm->cur_pmksa->pmkid, TK_PMK_ID_LEN);
    pos += TK_PMK_ID_LEN;
  }
  
  hdr->len = (pos - rsn_ie) - 2;
  
  WPA_ASSERT((size_t) (pos - rsn_ie) <= rsn_ie_len);
  
  return pos - rsn_ie;
}


static int wpa_gen_wpa_ie_wpa(u8 * wpa_ie, size_t wpa_ie_len,
                              int pairwise_cipher, int group_cipher,
                              int key_mgmt)
{
  u8 *pos;
  struct wpa_hdr_lan *hdr;
  u32 suite;
  
  if (wpa_ie_len < sizeof(*hdr) + WPA_LEN_SELECTOR +
      2 + WPA_LEN_SELECTOR + 2 + WPA_LEN_SELECTOR)
    return -1;
  
  hdr = (struct wpa_hdr_lan *)wpa_ie;
  hdr->elem_id = WLAN_VENDOR_SPECIFIC_EID;
  TILK_SELE_CTOR_RSN__PUT(hdr->oui, WPA_OUI_TYPE);
  TILK_WPA_TO_LE16(hdr->version, WPA_VERSION);
  pos = (u8 *) (hdr + 1);
  
  suite = wpa_cipher_to_suite(WPA_WPA_PROTO, group_cipher);
  if (suite == 0) {
    wpa_printf(MSG_WARNING, "Invalid group cipher (%d).", group_cipher);
    return -1;
  }
  TILK_SELE_CTOR_RSN__PUT(pos, suite);
  pos += WPA_LEN_SELECTOR;
  
  *pos++ = 1;
  *pos++ = 0;
  suite = wpa_cipher_to_suite(WPA_WPA_PROTO, pairwise_cipher);
  if (suite == 0 ||
      (!wpa_cipher_valid_pairwise(pairwise_cipher) &&
       pairwise_cipher != WPA_NONE_CIPHER)) {
         wpa_printf(MSG_WARNING, "Invalid pairwise cipher (%d).",
                    pairwise_cipher);
         return -1;
       }
  TILK_SELE_CTOR_RSN__PUT(pos, suite);
  pos += WPA_LEN_SELECTOR;
  
  *pos++ = 1;
  *pos++ = 0;
  if (key_mgmt == WPA_IEEE8021X_KEY_MGMT) {
    TILK_SELE_CTOR_RSN__PUT(pos, _AUTH_KEY_MGMT_UNSPEC_802_1X_TO_WPA);
  } else if (key_mgmt == WPA_PSK_KEY_MGMT) {
    TILK_SELE_CTOR_RSN__PUT(pos, _AUTH_KEY_MGMT_PSK_OVER_802_1X_TO_WPA);
  } else if (key_mgmt == WPA_WPA_NONE_KEY_MGMT) {
    TILK_SELE_CTOR_RSN__PUT(pos, _AUTH_KEY_MGMT_NONE_TO_WPA);
  } else if (key_mgmt == WPA_KEY_MGMT_CCKM) {
    TILK_SELE_CTOR_RSN__PUT(pos, WPA_AUTH_KEY_MGMT_CCKM);
  } else {
    wpa_printf(MSG_WARNING, "Invalid key management type (%d).", key_mgmt);
    return -1;
  }
  pos += WPA_LEN_SELECTOR;
  
  hdr->len = (pos - wpa_ie) - 2;
  
  WPA_ASSERT((size_t) (pos - wpa_ie) <= wpa_ie_len);
  
  return pos - wpa_ie;
}


int wpa_gen_wpa_ie(struct wpa_sm *sm, u8 * wpa_ie, size_t wpa_ie_len)
{
  if (sm->proto == WPA_RSN_PROTO)
    return wpa_gen_wpa_ie_rsn(wpa_ie, wpa_ie_len,
                              sm->pairwise_cipher,
                              sm->group_cipher,
                              sm->key_mgmt, sm->mgmt_group_cipher, sm);
  else
    return wpa_gen_wpa_ie_wpa(wpa_ie, wpa_ie_len,
                              sm->pairwise_cipher,
                              sm->group_cipher, sm->key_mgmt);
}



/**
* wpa_sm_set_assoc_wpa_ie_default - Generate own WPA/RSN IE from configuration
* @sm: Pointer to WPA state machine data from wpa_sm_init()
* @wpa_ie: Pointer to buffer for WPA/RSN IE
* @wpa_ie_len: Pointer to the length of the wpa_ie buffer
* Returns: 0 on success, -1 on failure
*/
int wpa_sm_set_assoc_wpa_ie_default(struct wpa_sm *sm, u8 *wpa_ie,
				    size_t *wpa_ie_len)
{
  int res;
  
  if (sm == NULL)
    return -1;
  
#ifdef CONFIG_TESTING_OPTIONS
  if (sm->test_assoc_ie) {
    wpa_printf(MSG_DEBUG,
               "TESTING: Replace association WPA/RSN IE");
    if (*wpa_ie_len < wpabuf_len(sm->test_assoc_ie))
      return -1;
    os_memcpy(wpa_ie, wpabuf_head(sm->test_assoc_ie),
              wpabuf_len(sm->test_assoc_ie));
    res = wpabuf_len(sm->test_assoc_ie);
  } else
#endif /* CONFIG_TESTING_OPTIONS */
    res = wpa_gen_wpa_ie(sm, wpa_ie, *wpa_ie_len);
  if (res < 0)
    return -1;
  *wpa_ie_len = res;
  
  wpa_hexdump(MSG_DEBUG, "WPA: Set own WPA IE default",
              wpa_ie, *wpa_ie_len);
  
  if (sm->assoc_wpa_ie == NULL) {
    /*
    * Make a copy of the WPA/RSN IE so that 4-Way Handshake gets
    * the correct version of the IE even if PMKSA caching is
    * aborted (which would remove PMKID from IE generation).
    */
    sm->assoc_wpa_ie = os_malloc(*wpa_ie_len);
    if (sm->assoc_wpa_ie == NULL)
      return -1;
    
    os_memcpy(sm->assoc_wpa_ie, wpa_ie, *wpa_ie_len);
    sm->assoc_wpa_ie_len = *wpa_ie_len;
  }
  
  return 0;
}


/**
* wpa_sm_set_assoc_wpa_ie - Set own WPA/RSN IE from (Re)AssocReq
* @sm: Pointer to WPA state machine data from wpa_sm_init()
* @ie: Pointer to IE data (starting from id)
* @len: IE length
* Returns: 0 on success, -1 on failure
*
* Inform WPA state machine about the WPA/RSN IE used in (Re)Association
* Request frame. The IE will be used to override the default value generated
* with wpa_sm_set_assoc_wpa_ie_default().
*/
int wpa_sm_set_assoc_wpa_ie(struct wpa_sm *sm, const u8 *ie, size_t len)
{
  if (sm == NULL)
    return -1;
  
  os_free(sm->assoc_wpa_ie);
  if (ie == NULL || len == 0) {
    wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
            "WPA: clearing own WPA/RSN IE");
    sm->assoc_wpa_ie = NULL;
    sm->assoc_wpa_ie_len = 0;
  } else {
    wpa_hexdump(MSG_DEBUG, "WPA: set own WPA/RSN IE", ie, len);
    sm->assoc_wpa_ie = os_malloc(len);
    if (sm->assoc_wpa_ie == NULL)
      return -1;
    
    os_memcpy(sm->assoc_wpa_ie, ie, len);
    sm->assoc_wpa_ie_len = len;
  }
  
  return 0;
}


/**
* wpa_sm_set_ap_wpa_ie - Set AP WPA IE from Beacon/ProbeResp
* @sm: Pointer to WPA state machine data from wpa_sm_init()
* @ie: Pointer to IE data (starting from id)
* @len: IE length
* Returns: 0 on success, -1 on failure
*
* Inform WPA state machine about the WPA IE used in Beacon / Probe Response
* frame.
*/
int wpa_sm_set_ap_wpa_ie(struct wpa_sm *sm, const u8 *ie, size_t len)
{
  if (sm == NULL)
    return -1;
  
  os_free(sm->ap_wpa_ie);
  if (ie == NULL || len == 0) {
    wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
            "WPA: clearing AP WPA IE");
    sm->ap_wpa_ie = NULL;
    sm->ap_wpa_ie_len = 0;
  } else {
    wpa_hexdump(MSG_DEBUG, "WPA: set AP WPA IE", ie, len);
    sm->ap_wpa_ie = os_malloc(len);
    if (sm->ap_wpa_ie == NULL)
      return -1;
    
    os_memcpy(sm->ap_wpa_ie, ie, len);
    sm->ap_wpa_ie_len = len;
  }
  
  return 0;
}


/**
* wpa_sm_set_ap_rsn_ie - Set AP RSN IE from Beacon/ProbeResp
* @sm: Pointer to WPA state machine data from wpa_sm_init()
* @ie: Pointer to IE data (starting from id)
* @len: IE length
* Returns: 0 on success, -1 on failure
*
* Inform WPA state machine about the RSN IE used in Beacon / Probe Response
* frame.
*/
int wpa_sm_set_ap_rsn_ie(struct wpa_sm *sm, const u8 *ie, size_t len)
{
  if (sm == NULL)
    return -1;
  
  os_free(sm->ap_rsn_ie);
  if (ie == NULL || len == 0) {
    wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
            "WPA: clearing AP RSN IE");
    sm->ap_rsn_ie = NULL;
    sm->ap_rsn_ie_len = 0;
  } else {
    wpa_hexdump(MSG_DEBUG, "WPA: set AP RSN IE", ie, len);
    sm->ap_rsn_ie = os_malloc(len);
    if (sm->ap_rsn_ie == NULL)
      return -1;
    
    os_memcpy(sm->ap_rsn_ie, ie, len);
    sm->ap_rsn_ie_len = len;
  }
  
  return 0;
}


int wpa_parse_wpa_ie(const u8 * wpa_ie, size_t wpa_ie_len,
                     struct wpa_ie_data *data)
{
  if (wpa_ie_len >= 1 && wpa_ie[0] == WLAN_RSN_EID)
    return wpa_parse_wpa_ie_rsn(wpa_ie, wpa_ie_len, data);
  if (wpa_ie_len >= 6 && wpa_ie[0] == WLAN_VENDOR_SPECIFIC_EID &&
      wpa_ie[1] >= 4 && TILK_WPA_READ_BE32(&wpa_ie[2]) == OSEN_IE_VENDOR_TYPE)
    return wpa_parse_wpa_ie_rsn(wpa_ie, wpa_ie_len, data);
  else
    return wpa_parse_wpa_ie_wpa(wpa_ie, wpa_ie_len, data);
}


/**
* wpa_sm_parse_own_wpa_ie - Parse own WPA/RSN IE
* @sm: Pointer to WPA state machine data from wpa_sm_init()
* @data: Pointer to data area for parsing results
* Returns: 0 on success, -1 if IE is not known, or -2 on parsing failure
*
* Parse the contents of the own WPA or RSN IE from (Re)AssocReq and write the
* parsed data into data.
*/
int wpa_sm_parse_own_wpa_ie(struct wpa_sm *sm, struct wpa_ie_data *data)
{
  if (sm == NULL)
    return -1;
  
  if (sm->assoc_wpa_ie == NULL) {
    wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
            "WPA: No WPA/RSN IE available from association info");
    return -1;
  }
  if (wpa_parse_wpa_ie(sm->assoc_wpa_ie, sm->assoc_wpa_ie_len, data))
    return -2;
  return 0;
}


int wpa_sm_pmksa_cache_list(struct wpa_sm *sm, char *buf, size_t len)
{
  return 0;
}


int wpa_sm_has_ptk(struct wpa_sm *sm)
{
  if (sm == NULL)
    return 0;
  return sm->ptk_set;
}


void wpa_sm_update_replay_ctr(struct wpa_sm *sm, const u8 *replay_ctr)
{
  os_memcpy(sm->rx_replay_counter, replay_ctr, WPA_REPLAY_COUNTER_LEN);
}


void wpa_sm_pmksa_cache_flush(struct wpa_sm *sm, void *network_ctx)
{
  pmksa_cache_flush(sm->pmksa, network_ctx, NULL, 0);
}


#ifdef CONFIG_WNM
int wpa_wnmsleep_install_key(struct wpa_sm *sm, u8 subelem_id, u8 *buf)
{
  u16 keyinfo;
  u8 keylen;  /* plaintext key len */
  u8 *key_rsc;
  
  if (subelem_id == WNM_SLEEP_SUBELEM_GTK) {
    struct wpa_gtk_data gd;
    
    os_memset(&gd, 0, sizeof(gd));
    keylen = wpa_cipher_key_len(sm->group_cipher);
    gd.key_rsc_len = wpa_cipher_rsc_len(sm->group_cipher);
    gd.alg = wpa_cipher_to_alg(sm->group_cipher);
    if (gd.alg == WPA_ALG_NONE) {
      wpa_printf(MSG_DEBUG, "Unsupported group cipher suite");
      return -1;
    }
    
    key_rsc = buf + 5;
    keyinfo = WPA_GET_LE16(buf + 2);
    gd.gtk_len = keylen;
    if (gd.gtk_len != buf[4]) {
      wpa_printf(MSG_DEBUG, "GTK len mismatch len %d vs %d",
                 gd.gtk_len, buf[4]);
      return -1;
    }
    gd.keyidx = keyinfo & 0x03; /* B0 - B1 */
    gd.tx = wpa_supplicant_gtk_tx_bit_workaround(
                                                 sm, !!(keyinfo & WPA_KEY_INFO_TXRX));
    
    os_memcpy(gd.gtk, buf + 13, gd.gtk_len);
    
    wpa_hexdump_key(MSG_DEBUG, "Install GTK (WNM SLEEP)",
                    gd.gtk, gd.gtk_len);
    if (wpa_supplicant_install_gtk(sm, &gd, key_rsc)) {
      os_memset(&gd, 0, sizeof(gd));
      wpa_printf(MSG_DEBUG, "Failed to install the GTK in "
                 "WNM mode");
      return -1;
    }
    os_memset(&gd, 0, sizeof(gd));
  } else {
    wpa_printf(MSG_DEBUG, "Unknown element id");
    return -1;
  }
  
  return 0;
}
#endif /* CONFIG_WNM */


#ifdef CONFIG_PEERKEY
int wpa_sm_rx_eapol_peerkey(struct wpa_sm *sm, const u8 *src_addr,
			    const u8 *buf, size_t len)
{
  struct wpa_peerkey *peerkey;
  
  for (peerkey = sm->peerkey; peerkey; peerkey = peerkey->next) {
    if (os_memcmp(peerkey->addr, src_addr, ETH_ALEN) == 0)
      break;
  }
  
  if (!peerkey)
    return 0;
  
  wpa_sm_rx_eapol(sm, src_addr, buf, len);
  
  return 1;
}
#endif /* CONFIG_PEERKEY */


#ifdef CONFIG_P2P

int wpa_sm_get_p2p_ip_addr(struct wpa_sm *sm, u8 *buf)
{
  if (sm == NULL || WPA_GET_BE32(sm->p2p_ip_addr) == 0)
    return -1;
  os_memcpy(buf, sm->p2p_ip_addr, 3 * 4);
  return 0;
}

#endif /* CONFIG_P2P */


void wpa_sm_set_rx_replay_ctr(struct wpa_sm *sm, const u8 *rx_replay_counter)
{
  if (rx_replay_counter == NULL)
    return;
  
  os_memcpy(sm->rx_replay_counter, rx_replay_counter,
            WPA_REPLAY_COUNTER_LEN);
  sm->rx_replay_counter_set = 1;
  wpa_printf(MSG_DEBUG, "Updated key replay counter");
}


void wpa_sm_set_ptk_kck_kek(struct wpa_sm *sm,
			    const u8 *ptk_kck, size_t ptk_kck_len,
			    const u8 *ptk_kek, size_t ptk_kek_len)
{
  if (ptk_kck && ptk_kck_len <= WPA_KCK_MAX_LEN) {
    os_memcpy(sm->ptk.kck, ptk_kck, ptk_kck_len);
    sm->ptk.kck_len = ptk_kck_len;
    wpa_printf(MSG_DEBUG, "Updated PTK KCK");
  }
  if (ptk_kek && ptk_kek_len <= WPA_KEK_MAX_LEN) {
    os_memcpy(sm->ptk.kek, ptk_kek, ptk_kek_len);
    sm->ptk.kek_len = ptk_kek_len;
    wpa_printf(MSG_DEBUG, "Updated PTK KEK");
  }
  sm->ptk_set = 1;
}

int eapol_sm_rx_eapol(struct eapol_sm *sm, const u8 * src, const u8 * buf,
                      size_t len)
{
  const struct ieee802_1x_hdr *hdr;
  const struct ieee802_1x_eapol_key *key;
  int data_len;
  int res = 1;
  size_t plen;
  
  if (sm == NULL)
    return 0;
  sm->dot1xSuppEapolFramesRx++;
  if (len < sizeof(*hdr)) {
    sm->dot1xSuppInvalidEapolFramesRx++;
    return 0;
  }
  hdr = (const struct ieee802_1x_hdr *)buf;
  sm->dot1xSuppLastEapolFrameVersion = hdr->version;
  os_memcpy(sm->dot1xSuppLastEapolFrameSource, src, ETH_ALEN);
  if (hdr->version < EAPOL_VERSION) {
    
  }
  plen = be_to_host16(hdr->length);
  if (plen > len - sizeof(*hdr)) {
    sm->dot1xSuppEapLengthErrorFramesRx++;
    return 0;
  }
  data_len = plen + sizeof(*hdr);
  
  switch (hdr->type) {
  case IEEE802_1X_TYPE_EAP_PACKET:
    if (sm->conf.workaround) {
      
      const struct eap_hdr *ehdr = (const struct eap_hdr *)(hdr + 1);
      if (plen >= sizeof(*ehdr) && ehdr->code == 10) {
        break;
      }
    }
    
    wpabuf_free(sm->eapReqData);
    sm->eapReqData = wpabuf_alloc_copy(hdr + 1, plen);
    if (sm->eapReqData) {
      wpa_printf(MSG_DEBUG, "EAPOL: Received EAP-Packet " "frame");
      sm->eapolEap = TRUE;
      
    }
    break;
  case IEEE802_1X_TYPE_EAPOL_KEY:
    if (plen < sizeof(*key)) {
      wpa_printf(MSG_DEBUG, "EAPOL: Too short EAPOL-Key "
                 "frame received");
      break;
    }
    key = (const struct ieee802_1x_eapol_key *)(hdr + 1);
    if (key->type == EAPOL_KEY_TYPE_WPA || key->type == EAPOL_KEY_TYPE_RSN) {
      
      wpa_printf(MSG_DEBUG, "EAPOL: Ignoring WPA EAPOL-Key "
                 "frame in EAPOL state machines");
      res = 0;
      break;
    }
    if (key->type != EAPOL_KEY_TYPE_RC4) {
      wpa_printf(MSG_DEBUG, "EAPOL: Ignored unknown "
                 "EAPOL-Key type %d", key->type);
      break;
    }
    os_free(sm->last_rx_key);
    sm->last_rx_key = os_malloc(data_len);
    if (sm->last_rx_key) {
      wpa_printf(MSG_DEBUG, "EAPOL: Received EAPOL-Key " "frame");
      os_memcpy(sm->last_rx_key, buf, data_len);
      sm->last_rx_key_len = data_len;
      sm->rxKey = TRUE;
      
    }
    break;
  default:
    wpa_printf(MSG_DEBUG, "EAPOL: Received unknown EAPOL type %d",
               hdr->type);
    sm->dot1xSuppInvalidEapolFramesRx++;
    break;
  }
  
  return res;
}

void wpa_supplicant_rx_eapol(struct wpa_supplicant *wpa_s, const u8 * src_addr,
                             const u8 * buf, size_t len)
{
  
  wpa_dbg(wpa_s, MSG_DEBUG, "RX EAPOL from " MACSTR, MAC2STR(src_addr));
  wpa_hexdump(MSG_MSGDUMP, "RX EAPOL", buf, len);
  
#if 0 
  if (wpa_s->wpa_state > WPA_ASSOCIATED && wpa_s->current_ssid &&
      wpa_s->current_ssid->peerkey &&
        !(wpa_s->drv_flags & WPA_DRIVER_FLAGS_4WAY_HANDSHAKE) &&
          wpa_sm_rx_eapol_peerkey(wpa_s->wpa, src_addr, buf, len) == 1) {
            wpa_dbg(wpa_s, MSG_DEBUG, "RSN: Processed PeerKey EAPOL-Key");
            return;
          }
#endif
  
  wpa_s->last_eapol_matches_bssid =
    os_memcompare(src_addr, wpa_s->bssid, ETH_ALEN) == 0;
  
  //if (wpa_s->ap_mode_start) {
  //wpa_supplicant_ap_rx_eapol(wpa_s, src_addr, buf, len);
  //return;
  //}
  
  if (wpa_s->key_mgmt == WPA_NONE_KEY_MGMT) {
    wpa_dbg(wpa_s, MSG_DEBUG, "Ignored received EAPOL frame since "
            "no key management is configured");
    return;
  }
  
  if (wpa_s->eapol_received == 0 &&
      (!(wpa_s->drv_flags & WPA_DRIVER_FLAGS_4WAY_HANDSHAKE) ||
       !wpa_key_mgmt_wpa_psk(wpa_s->key_mgmt) ||
         wpa_s->wpa_state != WPA_COMPLETED) &&
        (wpa_s->current_ssid == NULL ||
         wpa_s->current_ssid->mode != IEEE80211_MODE_IBSS)) {
           
           int timeout = 10;
           
           if (wpa_key_mgmt_wpa_ieee8021x(wpa_s->key_mgmt) ||
               wpa_s->key_mgmt == WPA_IEEE8021X_NO_WPA_KEY_MGMT ||
                 wpa_s->key_mgmt == WPA_KEY_MGMT_WPS) {
                   
                   timeout = 70;
                 }
#ifdef CONFIG_WPS
           if (wpa_s->current_ssid && wpa_s->current_bss &&
               (wpa_s->current_ssid->key_mgmt & WPA_KEY_MGMT_WPS) &&
                 eap_is_wps_pin_enrollee(&wpa_s->current_ssid->eap)) {
                   
                   struct wpabuf *wps_ie;
                   
                   wps_ie =
                     wpa_bss_get_vendor_ie_multi(wpa_s->current_bss,
                                                 WPS_IE_VENDOR_TYPE);
                   if (wps_ie && !wps_is_addr_authorized(wps_ie, wpa_s->own_addr, 1))
                     timeout = 10;
                   wpabuf_free(wps_ie);
                 }
#endif
           timeout = timeout;
#ifdef NOT_FIXED
           wpa_supplicant_req_auth_timeout(wpa_s, timeout, 0);
#endif
         }
  wpa_s->eapol_received++;
  
  if (wpa_s->countermeasures) {
    wpa_msg(wpa_s, MSG_INFO, "WPA: Countermeasures - dropped "
            "EAPOL packet");
    return;
  }
  
  os_memcpy(wpa_s->last_eapol_src, src_addr, ETH_ALEN);
  if (!wpa_key_mgmt_wpa_psk(wpa_s->key_mgmt) &&
      eapol_sm_rx_eapol(wpa_s->eapol, src_addr, (buf + 34), (len - 34)) > 0)
    return;
  if (!(wpa_s->drv_flags & WPA_DRIVER_FLAGS_4WAY_HANDSHAKE))
    wpa_sm_rx_eapol(wpa_s->wpa, src_addr, (buf + 34), (len -34));
  else if (wpa_key_mgmt_wpa_ieee8021x(wpa_s->key_mgmt)) {
    
  }
}


