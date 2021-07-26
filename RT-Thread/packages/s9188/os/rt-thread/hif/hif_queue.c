#include "common.h"
#include "hif_queue.h"
#include "hif.h"
#include "trx/rx_rtthread.h"
#include "wlan_dev/wf_wlan_dev.h"

#include "wf_debug.h"

static void wf_rx_workqueue(wf_work_struct *work, void *param);

static int wf_data_queue_full(wf_que_t *queue,int queue_node_num)
{
    return (queue_node_num == wf_que_count(queue));
}

int wf_data_queue_insert(wf_que_t *queue, data_queue_node_st *qnode)
{
    //LOG_I("[%s] num:%d",__func__,queue->num);
    wf_enque_tail(&qnode->node,queue);
    return queue->cnt;
}

data_queue_node_st * wf_data_queue_remove(wf_que_t *queue)
{
    wf_list_t    *head              = NULL;

    head    = wf_deque_head(queue);
    if(NULL != head)
    {
        return wf_list_entry(head, data_queue_node_st, node);
    }

    return NULL;
}

data_queue_node_st * wf_queue_node_malloc(int cnt)
{
    data_queue_node_st *node = NULL;

    node = wf_kzalloc(cnt * sizeof(data_queue_node_st));
    if(NULL == node)
    {
        LOG_E("[%s] wf_kzalloc failed ,check!!!!", __func__);
        return NULL;
    }

    return node;
}

static hif_netbuf_st * wf_netbuf_node_malloc(int cnt )
{
    hif_netbuf_st *node = NULL;

    node = wf_kzalloc(cnt * sizeof(hif_netbuf_st));
    if(NULL == node)
    {
        LOG_E("[%s] wf_kzalloc failed ,check!!!!", __func__);
        return NULL;
    }

    return node;
}

hif_netbuf_st *wf_netbuf_queue_remove(wf_que_t *queue)
{
    wf_list_t    *head              = NULL;

    head = wf_deque_head(queue);
    if(NULL != head)
    {
        return wf_list_entry(head, hif_netbuf_st, node);
    }

    return NULL;
}


int wf_data_queue_mngt_init(void *hif_node)
{
    int i                                           = 0;
    hif_netbuf_st      *netbuf_node                 = NULL;
    data_queue_node_st *recv_node                   = NULL;
    hif_node_st *hif_info                           = (hif_node_st *)hif_node;
    data_queue_mngt_st *data_queue_mngt             = &hif_info->trx_pipe;

    static wf_workqueue_func_param_st wq_rx_param   ={"rxwque",wf_rx_workqueue, NULL};
    wq_rx_param.param = hif_info;

    wf_que_init(&data_queue_mngt->netbuf_queue,WF_LOCK_TYPE_IRQ);
    wf_que_init(&data_queue_mngt->free_netbuf_queue,WF_LOCK_TYPE_IRQ);
    wf_que_init(&data_queue_mngt->free_rx_queue,WF_LOCK_TYPE_IRQ);
    
    /*tx queue init*/
    wf_os_api_workqueue_register(&data_queue_mngt->rx_wq,&wq_rx_param);

    /* netbuf alloc, used for recv data */
    hif_info->trx_pipe.all_netbuf_queue = wf_netbuf_node_malloc(HIF_QUEUE_ALLOC_MEM_NUM);
    for(i=0; i<HIF_QUEUE_ALLOC_MEM_NUM; i++)
    {
      netbuf_node = hif_info->trx_pipe.all_netbuf_queue + i;
      if(HIF_USB == hif_info->hif_type) {
          netbuf_node->mem = wf_kzalloc(WF_MAX_RECV_BUFF_LEN_USB);
      } else {
          netbuf_node->mem = wf_kzalloc(WF_MAX_RECV_BUFF_LEN_SDIO);
      }
      if(NULL == netbuf_node->mem) {
        LOG_E("[%s]:no memory for netbuf");
        return -WF_RETURN_FAIL;
      }
      netbuf_node->payload =  (wf_u8 *)WF_N_BYTE_ALIGMENT((SIZE_PTR)(netbuf_node->mem), HIF_QUEUE_ALLOC_MEM_ALIGN_SZ);
      netbuf_node->len = 0;
       
      wf_enque_tail(&netbuf_node->node, &data_queue_mngt->free_netbuf_queue);
      netbuf_node = NULL;
    }
    
    /*rx queue init */
    hif_info->trx_pipe.all_rx_queue = wf_queue_node_malloc(WF_RX_MAX_DATA_QUEUE_NODE_NUM);
    for(i=0; i<WF_RX_MAX_DATA_QUEUE_NODE_NUM; i++)
    {
        recv_node = hif_info->trx_pipe.all_rx_queue + i;

        //LOG_I("[%s] [%d] recv_node:%p",__func__,i,recv_node);
        recv_node->hif_node = hif_node;
        recv_node->real_size = 0;
        recv_node->node_id = i;
        if(HIF_USB == hif_info->hif_type)
        {
//            recv_node->u.purb       = usb_alloc_urb(0,GFP_KERNEL);
//            if(NULL == recv_node->u.purb)
//            {
//                LOG_E("[%s] usb_alloc_urb failed",__func__);
//                wf_free_skb( (struct sk_buff*)recv_node->buff);
//                wf_kfree(recv_node);
//                continue;
//            }
        }
        else if(HIF_SDIO == hif_info->hif_type)
        {
            recv_node->u.sd_func = hif_info->u.sdio.func;
        }

        wf_enque_tail(&recv_node->node, &data_queue_mngt->free_rx_queue);
        recv_node = NULL;
    }

    return WF_RETURN_OK;
}


int wf_data_queue_mngt_term(void *hif_node)
{
    wf_list_t *pos = NULL;
    wf_list_t *next = NULL;
    hif_netbuf_st      *netbuf_node = NULL;
    data_queue_node_st *data_node = NULL;
    hif_node_st  *hif_info      = (hif_node_st  *)hif_node;
    data_queue_mngt_st *trxq    = &hif_info->trx_pipe;

    if(trxq->rx_wq.ops)
    {
      if(trxq->rx_wq.ops->workqueue_term)
      {
        trxq->rx_wq.ops->workqueue_term(&trxq->rx_wq);
      }
    }
    else
    {
      return WF_RETURN_OK;
    }

    /*rx queue free */
    wf_list_for_each_safe(pos,next,&trxq->free_rx_queue.head)
    {
        data_node = wf_list_entry(pos, data_queue_node_st, node);
        if(data_node)
        {
            hif_info  = (hif_node_st  *)data_node->hif_node;
//            if(HIF_USB == hif_info->hif_type)
//            {
//                usb_kill_urb(data_node->u.purb);
//                usb_free_urb(data_node->u.purb);
//            }
            wf_list_delete(&data_node->node);
            data_node = NULL;
            trxq->free_rx_queue.cnt--;

        }

    }

    if (trxq->all_rx_queue)
    {
        wf_kfree(trxq->all_rx_queue);
        trxq->all_rx_queue = NULL;
    }
    
     /*netbuf free */
    wf_list_for_each_safe(pos,next,&trxq->free_netbuf_queue.head)
    {
        netbuf_node = wf_list_entry(pos, hif_netbuf_st, node);
        if(netbuf_node)
        {
            wf_list_delete(&netbuf_node->node);
            wf_kfree(netbuf_node->mem);
            netbuf_node = NULL;
            trxq->free_rx_queue.cnt--;
        }
    }
    wf_list_for_each_safe(pos,next,&trxq->netbuf_queue.head)
    {
        netbuf_node = wf_list_entry(pos, hif_netbuf_st, node);
        if(netbuf_node)
        {
            wf_list_delete(&netbuf_node->node);
            wf_kfree(netbuf_node->mem);
            netbuf_node = NULL;
            trxq->free_rx_queue.cnt--;
        }
    }

    if (trxq->all_netbuf_queue)
    {
        wf_kfree(trxq->all_netbuf_queue);
        trxq->all_netbuf_queue = NULL;
    }

    wf_que_deinit(&trxq->netbuf_queue);
    wf_que_deinit(&trxq->free_netbuf_queue);
    wf_que_deinit(&trxq->free_rx_queue);
    
    return WF_RETURN_OK;
}

int wf_tx_queue_empty(void *hif_info)
{
    return 1;
}

int wf_tx_queue_insert(void *hif_info,wf_u8 agg_num,char *buff, wf_u32 buff_len, wf_u32 addr, int (*tx_callback_func)(void*tx_info, void *param), void *tx_info, void *param)
{
    wf_s32 ret = 0;
    data_queue_node_st qnode_tx = {0};
    hif_node_st *hif_node       = hif_info;
    data_queue_node_st   *qnode = &qnode_tx;
    struct xmit_buf *pxmitbuf   = NULL;
    
    qnode->buff             = (wf_u8 *)buff;
    qnode->real_size        = buff_len;
    qnode->addr             = addr;
    qnode->tx_info          = tx_info;
    qnode->param            = param;
    qnode->tx_callback_func = tx_callback_func;
    qnode->agg_num          = agg_num;
    qnode->state            = TX_STATE_IDL;
    if(NULL != param)
    {
        pxmitbuf = param;
        qnode->pg_num = pxmitbuf->pg_num;
    }
    qnode->state            = TX_STATE_INSERT;
    
    
     
    if(HIF_USB == hif_node->hif_type)
    {
      hif_node->ops->hif_write(hif_node,WF_USB_NET_PIP,addr,(char*)qnode,qnode->real_size);
    }
    else
    {
      hif_node->ops->hif_write(hif_node,WF_SDIO_TRX_QUEUE_FLAG,addr,(char*)qnode,qnode->real_size);
    }
    
    return ret;
}

int hif_frame_dispath(hif_node_st *hif_info, hif_netbuf_st *netbuf_node)
{
  int num = 0;
  struct rt_wlan_device *wlan = NULL;
  struct rt_wlan_priv   *wlan_priv = NULL;
  wf_u8 *pdata = NULL;
#ifdef CONFIG_RICHV200
  struct rxd_detail_new *prxd = NULL;
#else
  struct rxd_detail_org *prxd = NULL;
#endif
  wf_u8 bc_addr[WF_ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  
#ifdef CONFIG_RICHV200
  prxd = (struct rxd_detail_new *)(netbuf_node->payload);
#else
  prxd = (struct rxd_detail_org *)(netbuf_node->payload);
#endif
  
  
  pdata = (netbuf_node->payload) + RXDESC_SIZE + 32;
  
  //  LOG_D("[hif_frame_dispath]:mac1:"WF_MAC_FMT,WF_MAC_ARG(get_ra(pdata)));
  //  LOG_D("[hif_frame_dispath]:ta:"WF_MAC_FMT,WF_MAC_ARG(get_ta(pdata)));
  while(num < hif_info->nic_number)
  {
    if(NULL == hif_info->nic_info[num])
    {
      LOG_E("%s  nic_info[%d] NULL",__func__,num);
      return WF_RETURN_FAIL;
    }
    
    wlan = hif_info->nic_info[num]->ndev;
    if(wlan == NULL)
    {
      LOG_E("%s  wlan[%d] NULL",__func__,num);
      return WF_RETURN_FAIL;
    }
    
    wlan_priv = wlan->user_data;
    if(wf_memcmp(wlan_priv->hw_addr,get_ra(pdata),WF_ETH_ALEN) == 0)
    {
      wf_rx_work(hif_info->nic_info[num]->ndev, netbuf_node);
      return 0;
      
    }
    else if(wf_memcmp(bc_addr,get_ra(pdata),WF_ETH_ALEN) == 0)
    {
      wf_rx_work(hif_info->nic_info[num]->ndev, netbuf_node);
    }
    num++;
  }
  
  return 0;
}

static void wf_rx_workqueue(wf_work_struct *work, void *param)
{
    hif_netbuf_st *netbuf_node  = NULL;
    data_queue_mngt_st *trxq    = NULL;
    wf_que_t *data_queue        = NULL;
    data_queue_node_st *qnode   = NULL;
    hif_node_st *hif_info       = NULL;
    wf_workqueue_mgnt_st *wq_mgt = NULL;
    
    hif_info = (hif_node_st *)param;
    trxq = &hif_info->trx_pipe;
    
    if(NULL == trxq)
    {
      LOG_E("[%s] rx data queue is null",__func__);
      return;
    }
    
    while(1)
    {
      netbuf_node = wf_netbuf_queue_remove(&trxq->netbuf_queue);
      if(NULL == netbuf_node) {
        goto wf_rx_workqueue_exit;
      }
      
      if (hm_get_mod_removed() == wf_true || hif_info->dev_removed == wf_true)
      {
        netbuf_node->len = 0;
        wf_enque_tail(&netbuf_node->node, &trxq->free_netbuf_queue);
        continue;
      }
      
      if(0 == netbuf_node->len) {
        wf_enque_tail(&netbuf_node->node, &trxq->free_netbuf_queue);
        continue;
      }
      
      hif_frame_dispath(hif_info,netbuf_node);
      netbuf_node->len = 0;
      wf_enque_tail(&netbuf_node->node, &trxq->free_netbuf_queue);
    }
    
wf_rx_workqueue_exit:
//      if(HIF_USB == hif_info->hif_type)
//      {
//        /* check urb node, if have free, used it */
//        if(NULL !=(qnode = wf_data_queue_remove(&hif_info->trx_pipe.free_rx_queue)))
//        {
//          hif_info->ops->hif_read(hif_info, WF_USB_NET_PIP, READ_QUEUE_INX, (wf_u8*)qnode,WF_MAX_RECV_BUFF_LEN_USB);
//        }
//      }
  return;
}

int wf_hif_queue_enable(hif_node_st *hif_node)
{
    data_queue_mngt_st *hqueue  = NULL;
    data_queue_node_st  *qnode  = NULL;

    LOG_D("[%s] begin",__func__);

    hqueue  = &hif_node->trx_pipe;

    /*rx queue*/
    if(HIF_USB == hif_node->hif_type )
    {
//        while(NULL !=(qnode = wf_data_queue_remove(&hif_node->trx_pipe.free_rx_queue)))
//        {
//            hif_node->ops->hif_read(hif_node, WF_USB_NET_PIP, READ_QUEUE_INX, (wf_u8*)qnode, WF_MAX_RECV_BUFF_LEN_USB);
//        }

        hif_node->hif_tr_ctrl = wf_true;
    }
    LOG_D("[%s] end",__func__);
    return WF_RETURN_OK;
}


wf_s32 wf_hif_queue_disable(hif_node_st *hif_node)
{
    return WF_RETURN_OK;
}

