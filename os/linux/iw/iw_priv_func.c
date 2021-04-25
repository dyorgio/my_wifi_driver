#include "common.h"
#include "hif.h"
#include "wf_debug.h"
#include "iw_priv_func.h"
#include "mcu_cmd.h"
#include "nic_io.h"
#include "mp.h"

#if 0
#define IW_PRIV_DBG(fmt, ...)        LOG_D("[%s]"fmt, __func__, ##__VA_ARGS__)
#define IW_PRIV_WARN(fmt, ...)       LOG_E("[%s]"fmt, __func__, ##__VA_ARGS__)
#define IW_PRIV_ARRAY(data, len)     log_array(data, len)
#else
#define IW_PRIV_DBG(fmt, ...)
#define IW_PRIV_WARN(fmt, ...)
#define IW_PRIV_ARRAY(data, len)
#endif


int wf_iw_priv_data_xmit(nic_info_st * pnic_info);
int wf_iw_priv_phy_reg(nic_info_st *nic_info, wf_u32 time);
int wf_iw_priv_mac_reg(nic_info_st *nic_info, int time);


#define IW_PRV_DEBUG

//int wf_iw_priv_mp_set(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
//{
//	ndev_priv_st *ndev_priv = netdev_priv(dev);
//	nic_info_st *pnic_info = ndev_priv->nic;
//	local_info_st * plocal_info = (local_info_st *)pnic_info->local_info;
//	struct iw_point *p = NULL;
//	wf_u8  *tmp_buf       = NULL;
//    u16 len     = 0;
//	char cmd_string[512]={0};
//	char *test;
//	char *token, *cmd[3] = {0, 0, 0};
//	u16 addr = 0xFF, cnts = 0;
//	int err, efuse_code = 1000;
//	int i = 0;
//	p = &wrqu->data;
//    len = p->length;

//#ifdef IW_PRV_DEBUG
//    LOG_D("[WLAN_IW_PRIV] : %s", __func__);
//#endif

//    if(0 == len)
//    {
//        return -EINVAL;
//    }

//    tmp_buf = wf_kzalloc(len + 1);
//    if(NULL == tmp_buf)
//    {
//        return -ENOMEM;
//    }


//    if(copy_from_user(tmp_buf,p->pointer,len))
//    {
//        wf_kfree(tmp_buf);
//        return -EFAULT;
//    }

//	if(sscanf(tmp_buf,"%s",cmd_string)!= 1)
//	{
//		LOG_D("iwpriv wlan(X) set help                    :display all cmd");
//		wf_kfree(tmp_buf);
//        return -EFAULT;
//	}

//    test = cmd_string;

//	while ((token = strsep(&test, "=")) != NULL)
//    {
//        cmd[i] = token;
//        i++;
//	}

//    LOG_D("cmd0: %s",cmd[0]);
//    LOG_D("cmd1: %s",cmd[1]);

//    if (cmd[0] == NULL)
//    {
//		LOG_D("iwpriv wlan(X) set help                    :display all cmd");
//		wf_kfree(tmp_buf);
//        return -EFAULT;
//	}

//	if(!strcmp(cmd[0],"help"))
//	{
//	    LOG_D("\n\n\n");
//	    LOG_D("-----------------------------------------------------------");
//		LOG_D("-                 iwpriv set help function                 -");
//		LOG_D("-----------------------------------------------------------");
//		LOG_D("iwpriv wlan(X) set test=start                    :start TEST mode");
//		LOG_D("iwpriv wlan(X) set test=stop                     :stop  TEST mode");
//		LOG_D("iwpriv wlan(X) set freq=[01~07][70~FF]           :write Freq value");
//		LOG_D("iwpriv wlan(X) set thermal                       :start Thermal test");
//		LOG_D("iwpriv wlan(X) set tx_ant=[0~2]                  :set tx antenna (0:all ant on  1:ant_1 on  2:ant_2 on)");
//        LOG_D("iwpriv wlan(X) set rx_ant=[0~2]                  :set tx antenna (0:all ant on  1:ant_1 on  2:ant_2 on)");
//		LOG_D("iwpriv wlan(X) set tx_power0=[0~63]              :set tx power level");
//		LOG_D("iwpriv wlan(X) set channel=[1~14]                :set channel");
//		LOG_D("iwpriv wlan(X) set bw=[0,1]                      :set bw   (0:20MHz  1:40MHz)");
//		LOG_D("iwpriv wlan(X) set gi=[0,1]                      :set tx guard interval   (0:short  1:long)");
//		LOG_D("iwpriv wlan(X) set rate=[0~3,128~135,256~263]    :set data rate   (referance rate table)");
//        LOG_D("iwpriv wlan(X) set tx=frame                      :start tx frame");
//        LOG_D("iwpriv wlan(X) set tx=frame,len=[]               :start tx frame with length");
//        LOG_D("iwpriv wlan(X) set tx=frame,count=[]             :start tx number of packets frame");
//        LOG_D("iwpriv wlan(X) set tx=frame,len=[],count=[]      :start tx number of packets frame with length");
//        LOG_D("iwpriv wlan(X) set tx=carr                       :start sending carrier suppression");
//        LOG_D("iwpriv wlan(X) set tx=single                     :start sending single tone signal");
//        LOG_D("iwpriv wlan(X) set tx=stop                       :stop tx frame");
//        LOG_D("iwpriv wlan(X) set rx=start                      :start rx frame");
//        LOG_D("iwpriv wlan(X) set rx=stop                       :stop rx frame");
//        LOG_D("iwpriv wlan(X) set stats=query                   :get the statistics");
//        LOG_D("iwpriv wlan(X) set stats=reset                   :reset statistics");
//	}
//	else if (!strcmp(cmd[0], "test"))
//    {
//		mp_test_ctrl(dev, cmd[1]);
//	}
//	else if (!strcmp(cmd[0], "channel"))
//    {
//        mp_set_channel(dev, cmd[1]);
//	}
//    else if (!strcmp(cmd[0], "freq"))
//    {
//	}
//	else if (!strcmp(cmd[0], "bw"))
//    {
//	}
//	else if (!strcmp(cmd[0], "gi"))
//    {
//	}
//	else if (!strcmp(cmd[0], "tx_ant"))
//    {
//	}
//	else if (!strcmp(cmd[0], "tx_power0"))
//    {
//	}
//	else if (!strcmp(cmd[0], "rate"))
//    {
//	}
//	else if (!strcmp(cmd[0], "tx"))
//    {
//	}
//	else if (!strcmp(cmd[0], "stats"))
//    {
//	}
//	else if (!strcmp(cmd[0], "rx_ant"))
//    {
//	}
//	else if (!strcmp(cmd[0], "rx"))
//    {
//	}
//	else if (!strcmp(cmd[0], "thermal"))
//    {
//	}
//    else
//    {
//        LOG_D("iwpriv wlan(X) set help                    :display all cmd");
//    }

//    wf_kfree(tmp_buf);
//	return 0;
//}



//int wf_iw_priv_mp_get(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
//{
//	ndev_priv_st *ndev_priv = netdev_priv(dev);
//	nic_info_st *pnic_info = ndev_priv->nic;
//	local_info_st * plocal_info = (local_info_st *)pnic_info->local_info;
//	struct iw_point *p = NULL;
//	wf_u8  *tmp_buf       = NULL;
//    u16 len     = 0;
//	char cmd_string[512]={0};
//	char *test;
//	char *token, *cmd[3] = {0, 0, 0};
//	u16 addr = 0xFF, cnts = 0;
//	int err, efuse_code = 1000;
//	int i = 0;
//	p = &wrqu->data;
//    len = p->length;

//#ifdef IW_PRV_DEBUG
//    LOG_D("[WLAN_IW_PRIV] : %s", __func__);
//#endif

//    if(0 == len)
//    {
//        return -EINVAL;
//    }

//    tmp_buf = wf_kzalloc(len + 1);
//    if(NULL == tmp_buf)
//    {
//        return -ENOMEM;
//    }


//    if(copy_from_user(tmp_buf,p->pointer,len))
//    {
//        wf_kfree(tmp_buf);
//        return -EFAULT;
//    }

//	if(sscanf(tmp_buf,"%s",cmd_string)!= 1)
//	{
//		LOG_D("iwpriv wlan(X) get help                    :display all cmd");
//		wf_kfree(tmp_buf);
//        return -EFAULT;
//	}

//    test = cmd_string;


//    while ((token = strsep(&test, "=")) != NULL)
//    {
//        cmd[i] = token;
//        i++;
//    }

//    LOG_D("cmd0: %s",cmd[0]);
//    LOG_D("cmd1: %s",cmd[1]);

//    if (cmd[0] == NULL)
//    {
//		LOG_D("iwpriv wlan(X) get help                    :display all cmd");
//		wf_kfree(tmp_buf);
//        return -EFAULT;
//	}

//	if(!strcmp(cmd[0],"help"))
//	{
//	    LOG_D("\n\n\n");
//	    LOG_D("-----------------------------------------------------------");
//		LOG_D("-                 iwpriv get help function                 -");
//		LOG_D("-----------------------------------------------------------");
//		LOG_D("iwpriv wlan(X) get freq                          :read Freq, return a value");
//		LOG_D("iwpriv wlan(X) get thermal                       :read Thermal, return a value");
//	}
//	else if (!strcmp(cmd[0], "bw"))
//	{
//	    wf_u8 bw;

//	    mp_get_bw(dev,&bw);
//	}
//	else if (!strcmp(cmd[0], "channel") == 0)
//	{
//	}
//	else if (!strcmp(cmd[0], "freq") == 0)
//	{
//	}
//	else if (!strcmp(cmd[0], "thermal") == 0)
//    {
//	}
//	else if (!strcmp(cmd[0], "blinked") == 0)
//    {
//	}
//	else if (!strcmp(cmd[0], "drv_version") == 0)
//    {
//	}
//    else
//    {
//        LOG_D("iwpriv wlan(X) get help                    :display all cmd");
//    }

//    wf_kfree(tmp_buf);
//	return 0;
//}



//int wf_iw_priv_mp_read_efuse(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
//{
//	ndev_priv_st *ndev_priv = netdev_priv(dev);
//	nic_info_st *pnic_info = ndev_priv->nic;
//	local_info_st * plocal_info = (local_info_st *)pnic_info->local_info;
//	struct iw_point *p = NULL;
//	wf_u8  *tmp_buf       = NULL;
//    u16 len     = 0;
//	char cmd_string[512]={0};
//	char *test;
//	char *token, *cmd[3] = {0, 0, 0};
//	u16 addr = 0xFF, cnts = 0;
//	int err, efuse_code = 1000;
//	int i = 0;
//	p = &wrqu->data;
//    len = p->length;

//#ifdef IW_PRV_DEBUG
//    LOG_D("[WLAN_IW_PRIV] : %s", __func__);
//#endif

//    if(0 == len)
//    {
//        return -EINVAL;
//    }

//    tmp_buf = wf_kzalloc(len + 1);
//    if(NULL == tmp_buf)
//    {
//        return -ENOMEM;
//    }


//    if(copy_from_user(tmp_buf,p->pointer,len))
//    {
//        wf_kfree(tmp_buf);
//        return -EFAULT;
//    }

//	if(sscanf(tmp_buf,"%s",cmd_string)!= 1)
//	{
//		LOG_D("iwpriv wlan(X) read_efuse help                    :display all cmd");
//		wf_kfree(tmp_buf);
//        return -EFAULT;
//	}

//    test = cmd_string;
//    while ((token = strsep(&test, "=")) != NULL)
//    {
//        cmd[i] = token;
//        i++;
//    }

//    LOG_D("cmd0: %s",cmd[0]);
//    LOG_D("cmd1: %s",cmd[1]);

//    if (cmd[0] == NULL)
//    {
//		LOG_D("iwpriv wlan(X) read_efuse help                    :display all cmd");
//		wf_kfree(tmp_buf);
//        return -EFAULT;
//	}

//	if(!strcmp(cmd[0],"help"))
//	{
//	    LOG_D("\n\n\n");
//	    LOG_D("-------------------------------------------------------------------");
//		LOG_D("-                 iwpriv read_efuse help function                 -");
//		LOG_D("-------------------------------------------------------------------");
//        LOG_D("iwpriv wlan(X) read_efuse phy_freespace            :get the remaining physpace,eg:0xe0");
//        LOG_D("iwpriv wlan(X) read_efuse mac                      :get mac address,eg: 0xb4 0x04 0x18 0x00 0x00 0x04");
//        LOG_D("iwpriv wlan(X) read_efuse vid                      :get vid value, eg: 0xE7 0x02");
//        LOG_D("iwpriv wlan(X) read_efuse pid                      :get pid value eg :0x86 0x90");
//        LOG_D("iwpriv wlan(X) read_efuse manufacture              :read Manufacture value ,only usb");
//        LOG_D("iwpriv wlan(X) read_efuse product                  :read Product value ,only usb");
//        LOG_D("iwpriv wlan(X) read_efuse freqcal                  :read Freqcal value");
//        LOG_D("iwpriv wlan(X) read_efuse tempcal                  :read tempCal value");
//        LOG_D("iwpriv wlan(X) read_efuse channelplan              :read channeplan value");
//        LOG_D("iwpriv wlan(X) read_efuse powercal                 :read PowerCal value");
//        LOG_D("iwpriv wlan(X) read_efuse customer[1-5]            :read tempCal value");
//	}
//	else if (!strcmp(cmd[0], "phy_freespace"))
//    {
//		efuse_code = EFUSE_PHYSPACE;
//	}
//	else if (!strcmp(cmd[0], "mac"))
//    {
//		efuse_code = WLAN_EEPORM_MAC;
//	}
//	else if (!strcmp(cmd[0], "vid"))
//    {
//		efuse_code = EFUSE_VID;
//	}
//	else if (!strcmp(cmd[0], "pid"))
//    {
//		efuse_code = EFUSE_PID;
//	}
//	else if (!strcmp(cmd[0], "manufacture"))
//    {
//		efuse_code = EFUSE_MANU;
//	}
//	else if (!strcmp(cmd[0], "product"))
//    {
//		efuse_code = EFUSE_PRODUCT;
//	}
//	else if (!strcmp(cmd[0], "freqcal"))
//    {
//		efuse_code = EFUSE_FREQCAL;
//	}
//	else if (!strcmp(cmd[0], "tempcal"))
//    {
//		efuse_code = EFUSE_TEMPCAL;
//	}
//	else if (!strcmp(cmd[0], "channelplan"))
//    {
//		efuse_code = EFUSE_CHANNELPLAN;
//	}
//	else if (!strcmp(cmd[0], "powercal"))
//    {
//		efuse_code = EFUSE_POWERCAL;
//    }
//    else
//    {
//        LOG_D("iwpriv wlan(X) read_efuse help                    :display all cmd");
//    }

//    wf_kfree(tmp_buf);
//	return 0;
//}


//int wf_iw_priv_mp_write_efuse(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
//{
//	ndev_priv_st *ndev_priv = netdev_priv(dev);
//	nic_info_st *pnic_info = ndev_priv->nic;
//	local_info_st * plocal_info = (local_info_st *)pnic_info->local_info;
//	struct iw_point *p = NULL;
//	wf_u8  *tmp_buf       = NULL;
//    u16 len     = 0;
//	char cmd_string[512]={0};
//	char *test;
//	char *token, *cmd[3] = {0, 0, 0};
//	u16 addr = 0xFF, cnts = 0;
//	int err, efuse_code = 1000;
//	int i = 0;
//	p = &wrqu->data;
//    len = p->length;

//#ifdef IW_PRV_DEBUG
//    LOG_D("[WLAN_IW_PRIV] : %s", __func__);
//#endif

//    if(0 == len)
//    {
//        return -EINVAL;
//    }

//    tmp_buf = wf_kzalloc(len + 1);
//    if(NULL == tmp_buf)
//    {
//        return -ENOMEM;
//    }


//    if(copy_from_user(tmp_buf,p->pointer,len))
//    {
//        wf_kfree(tmp_buf);
//        return -EFAULT;
//    }

//	if(sscanf(tmp_buf,"%s",cmd_string)!= 1)
//	{
//		LOG_D("iwpriv wlan(X) write_efuse help                    :display all cmd");
//		wf_kfree(tmp_buf);
//        return -EFAULT;
//	}

//    test = cmd_string;
//    while ((token = strsep(&test, "=")) != NULL)
//    {
//        cmd[i] = token;
//        i++;
//    }

//    LOG_D("cmd0: %s",cmd[0]);
//    LOG_D("cmd1: %s",cmd[1]);

//    if (cmd[0] == NULL)
//    {
//		LOG_D("iwpriv wlan(X) write_efuse help                    :display all cmd");
//		wf_kfree(tmp_buf);
//        return -EFAULT;
//	}

//	if(!strcmp(cmd[0],"help"))
//	{
//	    LOG_D("\n\n\n");
//	    LOG_D("--------------------------------------------------------------------");
//		LOG_D("-                 iwpriv write_efuse help function                 -");
//		LOG_D("--------------------------------------------------------------------");
//        LOG_D("iwpriv wlan(X) write_efuse check=id                :id data check");
//        LOG_D("iwpriv wlan(X) write_efuse check=phy               :phy data check");
//        LOG_D("iwpriv wlan(X) write_efuse check=fix               :write fixed data");
//        LOG_D("iwpriv wlan(X) write_efuse mac=[b40418000007]      :write mac address");
//        LOG_D("iwpriv wlan(X) write_efuse vid=[e702]              :write vid value");
//        LOG_D("iwpriv wlan(X) write_efuse pid=[8690]              :write pid value");
//        LOG_D("iwpriv wlan(X) write_efuse manufacture=[0101]      :write Manufacture value ,only usb");
//        LOG_D("iwpriv wlan(X) write_efuse product=[9188]          :write Product value ,only usb");
//        LOG_D("iwpriv wlan(X) write_efuse freqcal=[01~07][70~FF]  :write Freqcal value");
//        LOG_D("iwpriv wlan(X) write_efuse tempcal=[20]            :write TempCal value");
//        LOG_D("iwpriv wlan(X) write_efuse channelplan=[20]        :write channelplan value");
//        LOG_D("iwpriv wlan(X) write_efuse powercal=[xxxx]         :write powercal value");

//	}
//	else if (strncmp(cmd[0], "mac", 3) == 0)
//    {
//		efuse_code = WLAN_EEPORM_MAC;
//	}
//	else if (!strncmp(cmd[0], "vid", 3))
//    {
//		efuse_code = EFUSE_VID;
//	}
//	else if (!strncmp(cmd[0], "pid", 3))
//    {
//		efuse_code = EFUSE_PID;
//	}
//	else if (!strncmp(cmd[0], "manufacture", strlen("manufacture")))
//    {
//		efuse_code = EFUSE_MANU;
//	}
//	else if (!strncmp(cmd[0], "product", strlen("product")))
//    {
//		efuse_code = EFUSE_PRODUCT;
//	}
//	else if (!strncmp(cmd[0], "freqcal", strlen("freqcal")))
//    {
//		efuse_code = EFUSE_FREQCAL;
//	}
//	else if (!strncmp(cmd[0], "tempcal", strlen("tempcal")))
//    {
//		efuse_code = EFUSE_TEMPCAL;
//	}
//	else if (!strncmp(cmd[0], "channelplan", strlen("channelplan")))
//    {
//		efuse_code = EFUSE_CHANNELPLAN;
//	}
//	else if (!strncmp(cmd[0], "powercal", strlen("powercal")))
//    {
//		efuse_code = EFUSE_POWERCAL;
//	}
//	else if (!strncmp(cmd[0], "id", 2))
//    {
//		efuse_code = EFUSE_HEADERCHECK;
//	}
//	else if (strncmp(cmd[0], "fix", 3))
//    {
//		efuse_code = WLAN_EEPORM_BASEVALUE2;
//	}
//	else if (strncmp(cmd[0], "phy", 3))
//    {
//		efuse_code = EFUSE_PHYCFGCHECK;
//	}
//	else
//    {
//        LOG_D("iwpriv wlan(X) write_efuse help                    :display all cmd");
//    }

//    wf_kfree(tmp_buf);
//	return 0;
//}



#ifdef IW_PRV_DEBUG
static void usb_register_all_range(nic_info_st *nic_info, wf_u32 start, wf_u32 end)
{
    wf_u32 addr;
    for(addr=start; addr<=end; addr++)
    {
        if(addr%16 == 0)
        {
            printk("\n%08x ", addr);
			wf_msleep(10);
        }
        printk("%02x ", wf_io_read8(nic_info, addr, NULL));
    }
    printk("\n");
}

int wf_iw_priv_reg(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
	ndev_priv_st *pndev_priv = netdev_priv(dev);
    nic_info_st *pnic_info = pndev_priv->nic;
	hif_node_st *hif_info  = pnic_info->hif_node;
	struct iw_point *wdata;
	wf_u8 *input;
	wf_u32 ret = 0;
	wf_u16 input_len = 0;
	wf_u32 type;
    wf_u32 addr = 0;
	wf_u32 data = 0;
	int bytes = 0;
	int i = 0 ,virt;

	wdata = &wrqu->data;

	input_len = wdata->length;
	if(0 == input_len)
	{
		return -ENOMEM;
	}

	input = wf_kzalloc(input_len + 1);
	if (NULL == input)
	{
		ret = -ENOMEM;
		goto exit;
	}

	if (copy_from_user(input, wdata->pointer, input_len))
	{
		ret = -EFAULT;
		goto exit;
	}

	virt = sscanf(input, "%d,%x,%x", &type, &addr, &bytes);
	if(virt == 1 && type == 0)
	{
		LOG_D("******************************************************************");
		LOG_D("----------------------register help func--------------------------");
		LOG_D("------------------------------------------------------------------");
		LOG_D("cmd: iwpriv wlan0 reg cmd1,cmd2,cmd3");
		LOG_D("cmd1:");
		LOG_D("cmd : if cmd1 = 0 --------------------------------------------help");
		LOG_D("cmd : if cmd1 = 1 -------------------------------------------write");
		LOG_D("cmd : if cmd1 = 2 --------------------------------------------read");
		LOG_D("cmd : if cmd1 = 3 -------------------------------read all register");
		LOG_D("cmd2: Register address");
		LOG_D("cmd3: used for cmd1 == 2, bytes --Number of bytes read");
		LOG_D("cmd3: used for cmd1 == 1, data --The value written to the register");
		LOG_D("help  --------------eg: iwpriv wlan0 reg 0------------------------");
		LOG_D("write --------------eg: iwpriv wlan0 reg 1,123,1------------------");
		LOG_D("read  --------------eg: iwpriv wlan0 reg 2,123,1------------------");
		LOG_D("read all -----------eg: iwpriv wlan0 reg 3------------------------");
		LOG_D("******************************************************************");
		sprintf(extra, "--help");
		goto exit;
	}
	else if(type == 1 && virt == 3)
	{
		hif_io_write32(hif_info, addr, bytes);
		sprintf(extra, "%x --%02X", addr, bytes);
		goto exit;
	}
	else if(type == 2 && virt == 3)
	{
		switch(bytes)
		{
			case 1:
				data = hif_io_read8(hif_info, addr, NULL);
				snprintf(extra, 50, "%x --%02X", addr, data);
				break;
			case 2:
				data = hif_io_read16(hif_info, addr, NULL);
				sprintf(extra, "%x --%04X", addr, data);
				break;
			case 4:
				data = hif_io_read32(hif_info, addr, NULL);
				sprintf(extra, "%x --%08X", addr, data);
				break;
			default:
				break;
		}
		goto exit;
	}
	else if(type == 3 && virt == 1)
	{
		LOG_D("---------------------------------------------\n");
		usb_register_all_range(pnic_info, 0, 0x2ff);
	    LOG_D("---------------------------------------------\n");
	    usb_register_all_range(pnic_info, 0x300, 0x7ff);
	    LOG_D("---------------------------------------------\n");
	    usb_register_all_range(pnic_info, 0x800, 0xfff);
	    LOG_D("---------------------------------------------\n");
		sprintf(extra, "--read all");
		goto exit;
	}
	else
	{
		LOG_D("help  --------------eg: iwpriv wlan0 reg 0------------------------");
		sprintf(extra, "--error");
		ret = -1;
		goto exit;
	}

exit:
	wf_kfree(input);
	return ret;
}
#endif





int wf_iw_reg_write(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wdata, char *extra)
{
    ndev_priv_st *pndev_priv = netdev_priv(dev);
    nic_info_st *pnic_info = pndev_priv->nic;
    struct iw_point *wrqu;
    wf_u8 *pch;
    wf_u32 byte,addr,data;
    wf_u32 ret;
    wrqu = (struct iw_point *)wdata;

    if (copy_from_user(extra, wrqu->pointer, wrqu->length))
    {
        LOG_D("copy_from_user fail");
		return WF_RETURN_FAIL;
    }

    pch = extra;
    LOG_D("in = %s",extra);

    sscanf(pch,"%d,%x,%x",&byte,&addr,&data);
    switch(byte)
    {
        case 1:
            ret = wf_io_write8(pnic_info,addr, data);
            break;
        case 2:
            ret = wf_io_write16(pnic_info,addr, data);
            break;
        case 4:
            ret = wf_io_write32(pnic_info,addr, data);
            break;
        default:
            LOG_D("byte error");
            ret = WF_RETURN_FAIL;
            break;
    }
    if(ret == WF_RETURN_OK)
    {
        sprintf(extra, "%s" "%s", extra, " ok");
    }
    else if(ret == WF_RETURN_FAIL)
    {
        sprintf(extra, "%s" "%s", extra, " ok");
    }
    wrqu->length = strlen(extra);
    return 0;
}

int wf_iw_phy(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wdata, char *extra)
{
	ndev_priv_st *pndev_priv = netdev_priv(dev);
    nic_info_st *pnic_info = pndev_priv->nic;
	wf_u32 loopback;
	loopback = wf_io_read32(pnic_info, 0xd00,NULL);
	LOG_D("loopback:%x",loopback);
	loopback |= 0x1000000;
	LOG_D("loopback:%x",loopback);
	wf_io_write32(pnic_info, 0xd00, loopback);
	return 0;
}


int wf_iw_reg_read(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wdata, char *extra)
{
    ndev_priv_st *pndev_priv = netdev_priv(dev);
    nic_info_st *pnic_info = pndev_priv->nic;
    struct iw_point *wrqu;
    wf_u8 *pch;
    wf_u32 byte,addr,data;
    wf_u32 ret=WF_RETURN_OK;
    wrqu = (struct iw_point *)wdata;

    if (copy_from_user(extra, wrqu->pointer, wrqu->length))
    {
        LOG_D("copy_from_user fail");
		return WF_RETURN_FAIL;
    }

    pch = extra;
    LOG_D("in = %s",extra);

    sscanf(pch,"%d,%x",&byte,&addr);
    LOG_D("byte:%d addr:%x",byte,addr);
    switch(byte)
    {
        case 1:
            data = wf_io_read8(pnic_info,addr, NULL);
            break;
        case 2:
            data = wf_io_read16(pnic_info,addr, NULL);
            break;
        case 4:
            data = wf_io_read32(pnic_info,addr, NULL);
            break;
        default:
            LOG_D("byte error");
            ret = WF_RETURN_FAIL;
            break;
    }
    LOG_D("reg:%x --%08X", addr, data);
    if(ret != WF_RETURN_FAIL)
    {
        sprintf(extra, "reg:%x --%08X", addr, data);
    }
    wrqu->length = strlen(extra);
    return 0;

}

static wf_inline void generate_rand_data (wf_u32 *pdata, wf_u16 len)
{
    wf_u8 i;

    for (i = 0; i < len; i++)
    {
        pdata[i++] = wf_os_api_rand32();
    }
}

int isspace(int x)
{
 if(x==' '||x=='\t'||x=='\n'||x=='\f'||x=='\b'||x=='\r')
  return 1;
 else
  return 0;
}
int isdigit(int x)
{
 if(x<='9'&&x>='0')
  return 1;
 else
  return 0;

}
int atoi(const char *nptr)
{
        int c;              /* current char */
        int total;         /* current total */
        int sign;           /* if '-', then negative, otherwise positive */

        /* skip whitespace */
        while ( isspace((int)(unsigned char)*nptr) )
            ++nptr;

        c = (int)(unsigned char)*nptr++;
        sign = c;           /* save sign indication */
        if (c == '-' || c == '+')
            c = (int)(unsigned char)*nptr++;    /* skip sign */

        total = 0;

        while (isdigit(c)) {
            total = 10 * total + (c - '0');     /* accumulate digit */
            c = (int)(unsigned char)*nptr++;    /* get next char */
        }

        if (sign == '-')
            return -total;
        else
            return total;   /* return result, negated if necessary */
}


int wf_iw_fwdl(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
	ndev_priv_st *ndev_priv = netdev_priv(dev);
	nic_info_st *pnic_info = ndev_priv->nic;
	wf_u32 failed_times = 0;
	struct iw_point *p = NULL;
    wf_u8  *tmp_buf       = NULL;
    u16 len     = 0;
	char temp_func[128];
	char *test;
	char *temp_param[2] = {0,0};
	wf_u32 i = 0, j = 0, ret;
	wf_u32 out_buf[57] = {0};
	wf_u32 in_buf[57] = {0};
	wf_u32 inbox_len = 0;
	wf_u32 input_len = 0;
	char *token;
	wf_u32 rand_data[57];
	int flag;
	p = &wrqu->data;
    len = p->length;

#ifdef IW_PRV_DEBUG
	LOG_D("[WLAN_IW_PRIV] : %s", __func__);
#endif

    if(0 == len)
    {
        return -EINVAL;
    }

    tmp_buf = wf_kzalloc(len+1);
    if(NULL == tmp_buf)
    {
        return -ENOMEM;
    }


    if(copy_from_user(tmp_buf,p->pointer,len))
    {
        wf_kfree(tmp_buf);
        return -EFAULT;
    }

	if(sscanf(tmp_buf,"%s",temp_func)!= 1)
	{
		LOG_D("the input parameter must be characters");
		wf_kfree(tmp_buf);
        return -EFAULT;
	}
	test = temp_func;
	while ((token = strsep(&test, ",")) != NULL) {
		if (i > 1)
			break;
		temp_param[i] = token;
		i++;
	}

	LOG_D("------------------fw download test-------------------");

	if(!strcmp(temp_param[0],"help"))
	{
		LOG_D("---------------fw download help func--------------");
		LOG_D("--------------------------------------------------");
		LOG_D("cmd: iwpriv wlan0 fwdl cmd1,cmd2");
		LOG_D("cmd1:");
		LOG_D("cmd : check the cmd up and down is right");
		LOG_D("fwdownload : check the false time during 10000 times of load and unload");
		LOG_D("cmd2: used for cmd1 == cmd, input less than 228 bits");
		LOG_D("cmd2: used for cmd1 == fwdownload, fwdownload for cmd2 times");
	}
	else if(!strcmp(temp_param[0],"fwdownload"))
	{
		LOG_D("---------------fw download fwdownload func--------------");
		if(temp_param[1] != 0)
		{
			for(i = (int)*temp_param[1];i > 0;i--)
			{
				ret = wf_fw_download(ndev_priv->nic);
				if(ret != 0)
				{
					LOG_D("wf_fw_download fail!!!!");
					failed_times++;
				}
				msleep(5);
			}
		}
		else
		{
			for(i = 10000;i > 0;i--)
			{
				ret = wf_fw_download(ndev_priv->nic);
				if(ret != 0)
				{
					LOG_D("wf_fw_download fail!!!!");
					failed_times++;
				}
				msleep(5);
			}
		}
		LOG_D("fw_download failed %d times", failed_times);
	}
	else if(!strcmp(temp_param[0],"cmd"))
	{
		LOG_D("---------------fw download cmd func--------------");
		input_len = atoi(temp_param[1]);
		if(input_len < 1 || input_len > 57)/*  256 - 4*7  */
		{
			LOG_E("inbox_len must be less than 57");
			return -EFAULT;
		}

		wf_memset(in_buf, 0, input_len);
		for(i = 0; i < input_len; i++)
		{
			generate_rand_data(rand_data, WF_ARRAY_SIZE(rand_data));
			flag = rand_data[i];
			if(flag%2) {
				in_buf[j++]='A' + flag%26;
			}
			else {
				in_buf[j++]='a' + flag%26;
			}
		}
		LOG_D("input : %s",(char *)in_buf);
		LOG_D("input_len: %d",input_len);


		inbox_len = (input_len + 3)/4;
		ret = mcu_cmd_communicate(pnic_info, UMSG_OPS_CMD_TEST, in_buf, inbox_len, out_buf, inbox_len);
		if (WF_RETURN_FAIL == ret)
    	{
    		LOG_E("[%s] UMSG_OPS_CMD_TEST failed", __func__);
        	return ret;
    	}

		LOG_D("-----------------output : %s-----------------",(char *)out_buf);
		if (memcmp(in_buf,out_buf,input_len) == 0)
		{
			LOG_E("[%s] error.....", __func__);
		}

	}
	LOG_D("---------------------test end----------------------");
    wf_kfree(tmp_buf);
	return 0;
}


int wf_iw_priv_tx(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
	ndev_priv_st *pndev_priv = netdev_priv(dev);
	nic_info_st *pnic_info = pndev_priv->nic;
	struct iw_point *p;
	wf_u8 *tmp_buf;
	wf_u32 len;
	char cmd[512];
	char *test;
	char *param1[3] = {0x00,0x00,0x00};
	wf_u32 times,loopback,buff_addr;
	int i = 0;
	char *token;
	int param_times;
#ifdef IW_PRV_DEBUG
	LOG_D("[WLAN_IW_PRIV] : %s", __func__);
#endif
	p = &wrqu->data;
	len = p->length;
	if (0 == len)
		return -1;

    tmp_buf = wf_kzalloc(len+1);
    if(NULL == tmp_buf)
    {
        return -1;
    }
	if (copy_from_user(tmp_buf, p->pointer, len)) {
		return -1;
	}

	sscanf(tmp_buf, "%s", cmd);
	test = cmd;
	while ((token = strsep(&test, ",")) != NULL) {
		LOG_D("token = %s", token);
		if (i > 2)
			break;
		param1[i] = token;
		i++;
	}

	if(strcmp(param1[0],"help") == 0){
	    LOG_D("\n\n\n");
	    LOG_D("--------------------------------------------------------------------");
		LOG_D("-                 iwpriv tx help function                 -");
		LOG_D("--------------------------------------------------------------------");
        LOG_D("iwpriv wlan(X) tx mac                :mac loopback");
        LOG_D("iwpriv wlan(X) tx phy                :phy loopback");
        LOG_D("iwpriv wlan(X) tx data,(n)           :send data frame n times");
        LOG_D("iwpriv wlan(X) tx mgmt,(n)           :send mgmt frame n times");

        wf_kfree(tmp_buf);
		return 0;

	}

	if(strcmp(param1[0],"data") == 0){
		times = atoi(param1[1]);
		param_times = times;
		for(;times > 0;times--){
			wf_iw_priv_data_xmit(pnic_info);
			wf_msleep(2);
		}
		LOG_D("send null data frame times:%d",param_times);
		wf_kfree(tmp_buf);
		return 0;
	}
	if(strcmp(param1[0],"mgmt") == 0){
		times = atoi(param1[1]);
		param_times = times;
		for(;times > 0;times--){
			wf_scan_probe_send(pnic_info);
			wf_msleep(2);
		}
		LOG_D("send null probe request frame times:%d",param_times);
		wf_kfree(tmp_buf);
		return 0;
	}
	if(strcmp(param1[0],"mac") == 0){
		loopback = wf_io_read32(pnic_info, 0x100,NULL);
		LOG_D("loopback:%x",loopback);
		loopback |= (BIT(24) | BIT(25) | BIT(27));
//		loopback &= (~(BIT(27)));
//		loopback = 0x0b000000;
		LOG_D("loopback:%x",loopback);
		wf_io_write32(pnic_info, 0x100, loopback);
//		buff_addr = wf_io_read32(pnic_info, 0x45D,NULL);
//		LOG_D("buff_addr:%x",buff_addr);
//		buff_addr |= 0x03;
//		LOG_D("buff_addr:%x",buff_addr);
//		wf_io_write32(pnic_info, 0x45D, buff_addr);
		wf_kfree(tmp_buf);
		return 0;
	}
	if(strcmp(param1[0],"phy") == 0){
        loopback = wf_io_read32(pnic_info, 0xd00,NULL);
		LOG_D("loopback:%x",loopback);
		loopback |= 0x1000000;
//		loopback &= (~(BIT(27)));
//		loopback = 0x0b000000;
		LOG_D("loopback:%x",loopback);
		wf_io_write32(pnic_info, 0xd00, loopback);
//		buff_addr = wf_io_read32(pnic_info, 0x45D,NULL);
//		LOG_D("buff_addr:%x",buff_addr);
//		buff_addr |= 0x03;
//		LOG_D("buff_addr:%x",buff_addr);
//		wf_io_write32(pnic_info, 0x45D, buff_addr);
		wf_kfree(tmp_buf);
		return 0;
	}
	LOG_E("param error");
	wf_kfree(tmp_buf);
	return 0;
}

int wf_iw_priv_status(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
	ndev_priv_st *pndev_priv = netdev_priv(dev);
	nic_info_st *pnic_info = pndev_priv->nic;
	rx_info_t *prx_info;
	struct iw_point *p;
	wf_u8 *tmp_buf;
	wf_u32 len;
	char cmd[512];
	char *test;
	char *param1[3] = {0x00,0x00,0x00};
	wf_u32 time,time_start,i = 0;
	wf_u64 rx_pkt;
	char *token;
#ifdef IW_PRV_DEBUG
	LOG_D("[WLAN_IW_PRIV] : %s", __func__);
#endif
	prx_info = pnic_info->rx_info;
	p = &wrqu->data;
	len = p->length;
	if (0 == len)
		return -1;

    tmp_buf = wf_kzalloc(len+1);
    if(NULL == tmp_buf)
    {
        return -1;
    }
	if (copy_from_user(tmp_buf, p->pointer, len)) {
		return -1;
	}

	sscanf(tmp_buf, "%s", cmd);
	test = cmd;
	while ((token = strsep(&test, ",")) != NULL) {
		if (i > 2)
			break;
		param1[i] = token;
		i++;
	}

	if(strcmp(param1[0],"help") == 0){
		LOG_D("cmd :");
		LOG_D("iwpriv wlan0 tx cmd1,cmd2");
		LOG_D("cmd1:help/phy/rx");
		LOG_D("cmd2:phy interval time");
		wf_kfree(tmp_buf);
		return 0;
	}
	if(strcmp(param1[0],"rx") == 0){
		if(param1[1] == NULL){
			LOG_E("time not get");
			wf_kfree(tmp_buf);
			return 0;
		}
		time = atoi(param1[1]);
		LOG_D("time:%d",time);
		rx_pkt = prx_info->rx_total_pkts;
		wf_msleep(time*1000);
		LOG_D("time interval:%d s rx_total_pkts:%lld",time,(prx_info->rx_total_pkts - rx_pkt));

		wf_kfree(tmp_buf);
		return 0;
	}
	if(strcmp(param1[0],"phy") == 0){
		time = atoi(param1[1]);
		if(param1[1] == NULL){
			LOG_E("time not get");
			wf_kfree(tmp_buf);
			return 0;
		}
		wf_iw_priv_phy_reg(pnic_info,time);
		wf_kfree(tmp_buf);
		return 0;
	}
	LOG_E("param error");
	wf_kfree(tmp_buf);
	return 0;
}


int wf_iw_priv_test(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
    ndev_priv_st *ndev_priv = netdev_priv(dev);

	LOG_D("[WLAN_IW_PRIV] : %s\n", __func__);

    #if 1
    wf_hw_info_set_channnel_bw(ndev_priv->nic, 1, CHANNEL_WIDTH_20, HAL_PRIME_CHNL_OFFSET_DONT_CARE);
    wf_msleep(300);
    wf_scan_probe_send(ndev_priv->nic);
    #endif

    return 0;
}

int wf_iw_priv_phy_reg(nic_info_st *nic_info, wf_u32 time)
{
	int ret = 0;
	int cnt_parity_fail1;//0x0da0
	int cnt_rate_fail1,cnt_crc8_fail1;//0x0da4
	int cnt_mcs_fail1;//0x0da8
	int cnt_fast_fsync1,cnt_sb_search_fail1;//0x0cf0
	int cnt_ofdm_fail1;

	int cnt_ofdm_ht_crc_ok1,cnt_ofdm_ht_crc_fail1;//0x0f90
	int cnt_ofdm_crc_ok1,cnt_ofdm_crc_fail1;//0x0f94
	int cnt_ofdm_crc_fail_all1;
	int cnt_ofdm_crc_ok_all1;

	int cnt_cck_crc_fail1;
	int cnt_cck_crc_ok1;

	int cnt_crc_fail_all1,cnt_crc_ok_all1;

	int cnt_cck_fail1;

	int ofdm_fail_probability1;
	int cck_fail_probability1;
	int cnt_parity_fail2;//0x0da0
	int cnt_rate_fail2,cnt_crc8_fail2;//0x0da4
	int cnt_mcs_fail2;//0x0da8
	int cnt_fast_fsync2,cnt_sb_search_fail2;//0x0cf0
	int cnt_ofdm_fail2;

	int cnt_ofdm_ht_crc_ok2,cnt_ofdm_ht_crc_fail2;//0x0f90
	int cnt_ofdm_crc_ok2,cnt_ofdm_crc_fail2;//0x0f94
	int cnt_ofdm_crc_fail_all2;
	int cnt_ofdm_crc_ok_all2;

	int cnt_cck_crc_fail2;
	int cnt_cck_crc_ok2;

	int cnt_crc_fail_all2,cnt_crc_ok_all2;

	int cnt_cck_fail2;

	int ofdm_fail_probability2;
	int cck_fail_probability2;

	int cnt_ofdm_fail_interval;
	int cnt_ofdm_crc_fail_all;
	int cnt_ofdm_crc_ok_all;

	int cnt_cck_crc_fail_all;
	int cnt_cck_crc_ok_all;
	int cnt_cck_fail;
	int val = 65535;
	ret = wf_io_read32(nic_info,0x09cc,NULL);
//	LOG_D("cnt_txpkton_ofdm:%d",(ret & 0xffff));
//	LOG_D("cnt_ofdmtxen:%d",((ret & 0xffff0000) >> 16));
	ret = wf_io_read32(nic_info,0x09d0,NULL);
//	LOG_D("cnt_txpkton_cck:%d",(ret & 0xffff));
//	LOG_D("cnt_ccktxen:%d",((ret & 0xffff0000) >> 16));
//	ret = wf_io_read32(nic_info,0x0f14,NULL);
//	LOG_D("r_txpkt_cnt_ofdm_ext:%d",((ret & 0xf0000) >> 16));
//	ret = wf_io_read32(nic_info,0x0f18,NULL);
//	LOG_D("r_txpkt_cnt_ofdm_old:%d",(ret & 0xffff));
//	LOG_D("r_txpkt_cnt_cck:%d",((ret & 0xffff0000) >> 16));
	ret = wf_io_read32(nic_info,0x0da0,NULL);
//	LOG_D("cnt_parity_fail:%d",((ret & 0xffff0000) >> 16));
	cnt_parity_fail1 = (ret & 0xffff0000) >> 16;
	ret = wf_io_read32(nic_info,0x0da4,NULL);
	cnt_rate_fail1 = ret & 0xffff;
	cnt_crc8_fail1 = (ret & 0xffff0000) >> 16;
//	LOG_D("cnt_rate_fail:%d",(ret & 0xffff));
//	LOG_D("cnt_crc8_fail:%d",((ret & 0xffff0000) >> 16));
	ret = wf_io_read32(nic_info,0x0da8,NULL);
	cnt_mcs_fail1 = ret & 0xffff;
	ret = wf_io_read32(nic_info,0x0cf0,NULL);
	cnt_fast_fsync1 = ret & 0xffff;
	cnt_sb_search_fail1 = (ret & 0xffff0000) >> 16;
	cnt_ofdm_fail1 = cnt_parity_fail1 + cnt_rate_fail1 + cnt_crc8_fail1 + cnt_mcs_fail1 + cnt_fast_fsync1 + cnt_sb_search_fail1;
//	LOG_D("RX cnt_ofdm_fail:%d",cnt_ofdm_fail1);
	ret = wf_io_read32(nic_info,0x0f90,NULL);
	cnt_ofdm_ht_crc_ok1 = ret & 0xffff;
	cnt_ofdm_ht_crc_fail1 = (ret & 0xffff0000) >> 16;
//	LOG_D("cnt_ofdmrx_ht_crc_ok:%d",(ret & 0xffff));
//	LOG_D("cck_ofdmrx_ht_crc_err:%d",((ret & 0xffff0000) >> 16));
	ret = wf_io_read32(nic_info,0x0f94,NULL);
	cnt_ofdm_crc_ok1 = ret & 0xffff;
	cnt_ofdm_crc_fail1 = (ret & 0xffff0000) >> 16;
	cnt_ofdm_crc_fail_all1 = cnt_ofdm_crc_fail1 + cnt_ofdm_ht_crc_fail1;
	cnt_ofdm_crc_ok_all1 = cnt_ofdm_crc_ok1 + cnt_ofdm_ht_crc_ok1;
//	LOG_D("cnt_ofdmrx_l_crc_ok:%d",(ret & 0xffff));
//	LOG_D("cck_ofdmrx_l_crc_err:%d",((ret & 0xffff0000) >> 16));
//	LOG_D("cnt_ofdm_crc_fail_all:%d",cnt_ofdm_crc_fail_all1);
//	LOG_D("cnt_ofdm_crc_ok_all:%d",cnt_ofdm_crc_ok_all1);
	ofdm_fail_probability1 = ((cnt_ofdm_crc_fail_all1*100)/(cnt_ofdm_crc_fail_all1 + cnt_ofdm_crc_ok_all1));
//	LOG_D("ofdm fail_probability:%d",ofdm_fail_probability);
//	ret = wf_io_read32(nic_info,0x0fa0,NULL);
//	LOG_D("cck_cca_cnt:%d",((ret & 0xffff0000) >> 16));
	ret = wf_io_read32(nic_info,0x0a5c,NULL);
	cnt_cck_fail1 = ret & 0xff;
	ret = wf_io_read32(nic_info,0x0a58,NULL);
	cnt_cck_fail1 += ((ret & 0xff000000) >> 16);
//	LOG_D("cnt_cck_fail:%d",cnt_cck_fail1);
	ret = wf_io_read32(nic_info,0x0f84,NULL);
	cnt_cck_crc_fail1 = ret;
//	LOG_D("cnt_cck_crc_fail:%d",cnt_cck_crc_fail1);
	ret = wf_io_read32(nic_info,0x0f88,NULL);
	cnt_cck_crc_ok1 = ret;
//	LOG_D("cnt_ cck_crc32ok:%d",cnt_cck_crc_ok1);
	cnt_crc_fail_all1 = cnt_cck_crc_fail1 + cnt_ofdm_crc_fail_all1;
//	LOG_D("cnt_crc_fail_all:%d",cnt_crc_fail_all1);
	cnt_crc_ok_all1 = cnt_cck_crc_ok1 + cnt_ofdm_crc_ok_all1;
//	LOG_D("cnt_crc_ok_all:%d",cnt_crc_ok_all1);
	cck_fail_probability1 = ((cnt_crc_fail_all1*100)/(cnt_crc_fail_all1 + cnt_crc_ok_all1));
//	LOG_D("cck fail_probability:%d",cck_fail_probability);
	wf_msleep(time*1000);
	ret = wf_io_read32(nic_info,0x09cc,NULL);
//	LOG_D("cnt_txpkton_ofdm:%d",(ret & 0xffff));
//	LOG_D("cnt_ofdmtxen:%d",((ret & 0xffff0000) >> 16));
	ret = wf_io_read32(nic_info,0x09d0,NULL);
//	LOG_D("cnt_txpkton_cck:%d",(ret & 0xffff));
//	LOG_D("cnt_ccktxen:%d",((ret & 0xffff0000) >> 16));
//	ret = wf_io_read32(nic_info,0x0f14,NULL);
//	LOG_D("r_txpkt_cnt_ofdm_ext:%d",((ret & 0xf0000) >> 16));
//	ret = wf_io_read32(nic_info,0x0f18,NULL);
//	LOG_D("r_txpkt_cnt_ofdm_old:%d",(ret & 0xffff));
//	LOG_D("r_txpkt_cnt_cck:%d",((ret & 0xffff0000) >> 16));
	ret = wf_io_read32(nic_info,0x0da0,NULL);
//	LOG_D("cnt_parity_fail:%d",((ret & 0xffff0000) >> 16));
	cnt_parity_fail2 = (ret & 0xffff0000) >> 16;
	ret = wf_io_read32(nic_info,0x0da4,NULL);
	cnt_rate_fail2 = ret & 0xffff;
	cnt_crc8_fail2 = (ret & 0xffff0000) >> 16;
//	LOG_D("cnt_rate_fail:%d",(ret & 0xffff));
//	LOG_D("cnt_crc8_fail:%d",((ret & 0xffff0000) >> 16));
	ret = wf_io_read32(nic_info,0x0da8,NULL);
	cnt_mcs_fail2 = ret & 0xffff;
	ret = wf_io_read32(nic_info,0x0cf0,NULL);
	cnt_fast_fsync2 = ret & 0xffff;
	cnt_sb_search_fail2 = (ret & 0xffff0000) >> 16;
	cnt_ofdm_fail2 = cnt_parity_fail2 + cnt_rate_fail2 + cnt_crc8_fail2 + cnt_mcs_fail2 + cnt_fast_fsync2 + cnt_sb_search_fail2;
	LOG_D("-----------------test------------------");
	LOG_D("cnt_parity_fail2:%d",(cnt_parity_fail2-cnt_parity_fail1));
	LOG_D("cnt_rate_fail2:%d",(cnt_rate_fail2-cnt_rate_fail1));
	LOG_D("cnt_crc8_fail2:%d",(cnt_crc8_fail2-cnt_crc8_fail1));
	LOG_D("cnt_mcs_fail2:%d",(cnt_mcs_fail2-cnt_mcs_fail1));
	LOG_D("cnt_fast_fsync2:%d",(cnt_fast_fsync2-cnt_fast_fsync1));
	LOG_D("cnt_sb_search_fail2:%d",(cnt_sb_search_fail2-cnt_sb_search_fail1));
	LOG_D("-----------------test------------------");
//	LOG_D("RX cnt_ofdm_fail:%d",cnt_ofdm_fail2);
	ret = wf_io_read32(nic_info,0x0f90,NULL);
	cnt_ofdm_ht_crc_ok2 = ret & 0xffff;
	cnt_ofdm_ht_crc_fail2 = (ret & 0xffff0000) >> 16;
//	LOG_D("cnt_ofdmrx_ht_crc_ok:%d",(ret & 0xffff));
//	LOG_D("cck_ofdmrx_ht_crc_err:%d",((ret & 0xffff0000) >> 16));
	ret = wf_io_read32(nic_info,0x0f94,NULL);
	cnt_ofdm_crc_ok2 = ret & 0xffff;
	cnt_ofdm_crc_fail2 = (ret & 0xffff0000) >> 16;
	cnt_ofdm_crc_fail_all2 = cnt_ofdm_crc_fail2 + cnt_ofdm_ht_crc_fail2;
	cnt_ofdm_crc_ok_all2 = cnt_ofdm_crc_ok2 + cnt_ofdm_ht_crc_ok2;
//	LOG_D("cnt_ofdmrx_l_crc_ok:%d",(ret & 0xffff));
//	LOG_D("cck_ofdmrx_l_crc_err:%d",((ret & 0xffff0000) >> 16));
//	LOG_D("cnt_ofdm_crc_fail_all2:%d",cnt_ofdm_crc_fail_all2);
//	LOG_D("cnt_ofdm_crc_ok_all2:%d",cnt_ofdm_crc_ok_all2);
	ofdm_fail_probability2 = ((cnt_ofdm_crc_fail_all2*100)/(cnt_ofdm_crc_fail_all2 + cnt_ofdm_crc_ok_all2));
//	LOG_D("ofdm fail_probability:%d",ofdm_fail_probability);
//	ret = wf_io_read32(nic_info,0x0fa0,NULL);
//	LOG_D("cck_cca_cnt:%d",((ret & 0xffff0000) >> 16));
	ret = wf_io_read32(nic_info,0x0a5c,NULL);
	cnt_cck_fail2 = ret & 0xff;
	ret = wf_io_read32(nic_info,0x0a58,NULL);
	cnt_cck_fail2 += ((ret & 0xff000000) >> 16);
//	LOG_D("cnt_cck_fail:%d",cnt_cck_fail2);
	ret = wf_io_read32(nic_info,0x0f84,NULL);
	cnt_cck_crc_fail2 = ret;
//	LOG_D("cnt_cck_crc_fail:%d",cnt_cck_crc_fail2);
	ret = wf_io_read32(nic_info,0x0f88,NULL);
	cnt_cck_crc_ok2 = ret;
//	LOG_D("cnt_ cck_crc32ok:%d",cnt_cck_crc_ok2);
	cnt_crc_fail_all2 = cnt_cck_crc_fail2 + cnt_ofdm_crc_fail_all2;
//	LOG_D("cnt_crc_fail_all:%d",cnt_crc_fail_all2);
	cnt_crc_ok_all2 = cnt_cck_crc_ok2 + cnt_ofdm_crc_ok_all2;
//	LOG_D("cnt_crc_ok_all:%d",cnt_crc_ok_all2);
	cck_fail_probability2 = ((cnt_crc_fail_all2*100)/(cnt_crc_fail_all2 + cnt_crc_ok_all2));
	cnt_ofdm_fail_interval = cnt_ofdm_fail2 - cnt_ofdm_fail1;
	if(cnt_ofdm_fail_interval < 0){
		cnt_ofdm_fail_interval = cnt_ofdm_fail1 - cnt_ofdm_fail2;
//		cnt_ofdm_fail_interval = (cnt_ofdm_fail2 + val) - cnt_ofdm_fail1;
//		LOG_D("reg reset");
	}

	cnt_ofdm_crc_fail_all = cnt_ofdm_crc_fail_all2 - cnt_ofdm_crc_fail_all1;
	if(cnt_ofdm_crc_fail_all < 0){
		cnt_ofdm_crc_fail_all = cnt_ofdm_crc_fail_all1 - cnt_ofdm_crc_fail_all2;
//		cnt_ofdm_crc_fail_all = (cnt_ofdm_crc_fail_all2 + val) - cnt_ofdm_crc_fail_all1;
//		LOG_D("reg reset");
	}
	cnt_ofdm_crc_ok_all = cnt_ofdm_crc_ok_all2 - cnt_ofdm_crc_ok_all1;
	if(cnt_ofdm_crc_ok_all < 0){
		cnt_ofdm_crc_ok_all = cnt_ofdm_crc_ok_all1 - cnt_ofdm_crc_ok_all2;
//		cnt_ofdm_crc_ok_all = (cnt_ofdm_crc_ok_all2 + val) - cnt_ofdm_crc_ok_all1;
//		LOG_D("reg reset");
	}
	cnt_cck_crc_ok_all = cnt_cck_crc_ok2 - cnt_cck_crc_ok1;
	if(cnt_cck_crc_ok_all < 0){
		cnt_cck_crc_ok_all = cnt_cck_crc_ok1 - cnt_cck_crc_ok2;
//		cnt_cck_crc_ok_all = (cnt_cck_crc_ok2 + val) - cnt_cck_crc_ok1;
//		LOG_D("reg reset");
	}
	cnt_cck_crc_fail_all = cnt_cck_crc_fail2 - cnt_cck_crc_fail1;
	if(cnt_cck_crc_fail_all < 0){
		cnt_cck_crc_fail_all = cnt_cck_crc_fail1 - cnt_cck_crc_fail2;
//		cnt_cck_crc_fail_all = (cnt_cck_crc_fail2 + val) - cnt_cck_crc_fail1;
//		LOG_D("reg reset");
	}
	cnt_cck_fail = cnt_cck_fail2 - cnt_cck_fail1;
	if(cnt_cck_fail < 0){
		cnt_cck_fail = cnt_cck_fail1 - cnt_cck_fail2;
//		cnt_cck_fail = (cnt_cck_fail2 + val) - cnt_cck_fail1;
//		LOG_D("reg reset");
	}
	LOG_D("ofdm fail interval %d s : %d",time,cnt_ofdm_fail_interval);
	LOG_D("ofdm crc fail interval %d s : %d",time,cnt_ofdm_crc_fail_all);
	LOG_D("ofdm crc ok interval %d s : %d",time,cnt_ofdm_crc_ok_all);

	LOG_D("cck fail interval %d s : %d",time,cnt_cck_fail);
	LOG_D("cck crc fail interval %d s : %d",time,cnt_cck_crc_fail_all);
	LOG_D("cck crc ok interval %d s : %d",time,cnt_cck_crc_ok_all);
	LOG_D("\n");
	LOG_D("\n");
	wf_iw_priv_mac_reg(nic_info, time);

	return 0;
}


int wf_iw_priv_mac_reg(nic_info_st *nic_info, int time)
{
	int rx_mac_num1,rx_mac_num2,rx_mac_num;
	int ret ;
	int val = 65535;
	ret = wf_io_read32(nic_info,0x04ec,NULL);
	rx_mac_num1 = ret & 0xffff;
	wf_msleep(time*1000);
	ret = wf_io_read32(nic_info,0x04ec,NULL);
	rx_mac_num2 = ret & 0xffff;
	rx_mac_num = rx_mac_num2 - rx_mac_num1;
	if(rx_mac_num < 0){
		rx_mac_num = (rx_mac_num2 + val) - rx_mac_num1;
	}

	LOG_D("mac reg[0x04ec] interval %d s val:%d",time,rx_mac_num);
	return 0;
}


int wf_iw_priv_fw_dowlond_test(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
	ndev_priv_st *ndev_priv = netdev_priv(dev);
	int number = (int)*extra;
	int ret;
	LOG_D("[WLAN_IW_PRIV] : %s\n", __func__);
	for(;number > 0;number--){
		ret = wf_fw_download(ndev_priv->nic);
		if(ret != 0){
			LOG_D("wf_fw_download fail!!!!");
			return -1;
		}
	}

	return 0;
}


int wf_iw_priv_send_test(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
	ndev_priv_st *ndev_priv = netdev_priv(dev);
	int cmd = (int)*extra;
	LOG_D("[WLAN_IW_PRIV] : %s\n", __func__);

	switch(cmd){
	case 0:
		LOG_D("tx mgmt frame");
		wf_scan_probe_send(ndev_priv->nic);
		break;
	case 1:
		LOG_D("tx data frame");
		wf_iw_priv_data_xmit(ndev_priv->nic);
		break;
	default:
		LOG_D("type error");
		break;
	}
	return 0;
}

int wf_iw_priv_write32(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
	LOG_D("[WLAN_IW_PRIV] : %s\n", __func__);
    return 0;
}

int wf_iw_priv_read32(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
    ndev_priv_st *pndev_priv = netdev_priv(dev);
    nic_info_st *pnic_info = pndev_priv->nic;
    hif_node_st *hif_info  = pnic_info->hif_node;
    hif_sdio_st *sd = &hif_info->u.sdio;
    struct iw_point *p = NULL;
    wf_u8  *tmp_buf       = NULL;
    u16 len     = 0;
    int bytes    = 0;
    wf_u32 addr    = 0;
    wf_u32 data    = 0;

    LOG_D("[WLAN_IW_PRIV] : %s\n", __func__);
    p = &wrqu->data;
    len = p->length;

    if(0 == len)
    {
        return -EINVAL;
    }

    tmp_buf = wf_kzalloc(len+1);
    if(NULL == tmp_buf)
    {
        return -ENOMEM;
    }

    if(copy_from_user(tmp_buf,p->pointer,len))
    {
        wf_kfree(tmp_buf);
        return -EFAULT;
    }

    if(2 != sscanf(tmp_buf,"%d,%x",&bytes,&addr)){
		LOG_E("2 != sscanf");
		wf_kfree(tmp_buf);
		return -EFAULT;
    }

    switch (bytes)
    {
        case 1:
        data = hif_io_read8(hif_info, addr,NULL);
        sprintf(extra, "0x%x-----0x%02X", addr,data);
        break;
        case 2:
        data = hif_io_read16(hif_info, addr,NULL);
        sprintf(extra, "0x%x-----0x%04X", addr,data);
        break;
        case 4:
			LOG_W("wf_io_read32");
	        data = hif_io_read32(hif_info, addr,NULL);
	        sprintf(extra, "0x%x-----0x%08X", addr,data);
	        break;

        default:
            LOG_I("%s: usage> read [bytes],[address(hex)]\n", __func__);
            break;
    }

    wf_kfree(tmp_buf);

    return 0;
}

int wf_iw_priv_set_rf(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
	LOG_D("[WLAN_IW_PRIV] : %s\n", __func__);
    return 0;
}

int wf_iw_priv_get_rf(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
	LOG_D("[WLAN_IW_PRIV] : %s\n", __func__);
    return 0;
}

int wf_iw_priv_set_p2p(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
	LOG_D("[WLAN_IW_PRIV] : %s\n", __func__);
    return 0;
}

int wf_iw_priv_get_p2p(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
	LOG_D("[WLAN_IW_PRIV] : %s\n", __func__);
    return 0;
}

int wf_iw_priv_get_ap_Info(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
	LOG_D("[WLAN_IW_PRIV] : %s\n", __func__);
    return 0;
}

int wf_iw_priv_get_sensitivity(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
	LOG_D("[WLAN_IW_PRIV] : %s\n", __func__);
    return 0;
}

int wf_iw_priv_set_pm(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
	LOG_D("[WLAN_IW_PRIV] : %s\n", __func__);
    return 0;
}

int wf_iw_priv_set_pid(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wrqu, char *extra)
{
	LOG_D("[WLAN_IW_PRIV] : %s\n", __func__);
    return 0;
}


inline wf_u32 wf_get_freq(nic_info_st * nic_info)
{
	local_info_st * local = (local_info_st *)nic_info->local_info;

	return (wf_u32)local->bw;
}



void null_data_wlan_hdr(nic_info_st * pnic_info, struct xmit_buf * pxmit_buf)
{
	wf_u8 * pframe;
	struct wl_ieee80211_hdr * pwlanhdr;
	mlme_info_t * pmlme_info = (mlme_info_t *)pnic_info->mlme_info;;

	pframe = pxmit_buf->pbuf + TXDESC_OFFSET;
	pwlanhdr = (struct wl_ieee80211_hdr *)pframe;

	pwlanhdr->frame_ctl = 0;
	SetFrameType(pframe,WIFI_DATA_TYPE);
	SetFrameSubType(pframe, WIFI_DATA_NULL);  /* set subtype */
 }


 /*
 tx data test frame
 */
 int wf_iw_priv_data_xmit(nic_info_st * pnic_info)
 {
	 wf_u8 * pframe;
	 wf_u8 * phead;
	 wf_u8 element_id;
	 wf_u8 length;
	 int i = 0;
	 struct wl_ieee80211_hdr * pwlanhdr;
	 struct xmit_buf * pxmit_buf;
	 wf_u32 pkt_len;
	 tx_info_st *	  ptx_info;
	 wf_scan_info_t *scan_info = pnic_info->scan_info;
	 hw_info_st *hw_info = pnic_info->hw_info;
	 wf_wlan_ssid_t *pSsid;
	 int ret = 0;
	 wf_pkt *pkt;
	 wf_u8 bmc_addr[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
	 wf_wlan_info_t *wlan_info = (wf_wlan_info_t *)pnic_info->wlan_info;
	wf_u8 test[10] = {1,1,1,1,1,1,1,1,1,1};
	wf_u8 icmp[] = {0x14,0xe6,0xe4,0x38,0x6f,0x3c,0xb4,0x4,0x18,0xc6,0x75,0xf7,0x8,0x0,0x45,0x0,0x0,0x54,0xb8
,0xcf,0x40,0x0,0x40,0x1,0xfe,0x13,0xc0,0xa8,0x1,0x74,0xc0,0xa8,0x1,0x1,0x8,0x0,0x4d,0xaf,0x26
,0x27,0x0,0x10,0xf3,0xcf,0xf,0x60,0x0,0x0,0x0,0x0,0xbc,0x16,0x6,0x0,0x0,0x0,0x0,0x0,0x10,0x11,0x12,0x13
,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27
,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37 };
	 ptx_info =  (tx_info_st *)pnic_info->tx_info;
	 /* alloc xmit_buf */
	 pxmit_buf = wf_xmit_extbuf_new(ptx_info);
	 if(pxmit_buf == NULL){
		 LOG_E("[%s]: pxmit_buf is NULL",__func__);
		 return -1;
	 }
	 wf_memset(pxmit_buf->pbuf, 0, WLANHDR_OFFSET + TXDESC_OFFSET);


	 null_data_wlan_hdr(pnic_info, pxmit_buf);

	 /* set txd at tx module */
	 pframe = pxmit_buf->pbuf + TXDESC_OFFSET; /* pframe point to wlan_hdr */
	 phead = pframe;
	 pwlanhdr = (struct wl_ieee80211_hdr *)pframe;

	 pkt_len = sizeof(struct wl_ieee80211_hdr_3addr);
	 pframe += pkt_len; /* point to iv or frame body */

	 wf_memset(pwlanhdr->addr1, 0xff, MAC_ADDR_LEN);
	 wf_memset(pwlanhdr->addr3, 0xff, MAC_ADDR_LEN);

	 wf_memcpy(pwlanhdr->addr2, nic_to_local_addr(pnic_info), MAC_ADDR_LEN);

	 wf_memcpy(wlan_info->cur_network.bssid,bmc_addr,MAC_ADDR_LEN);
#ifdef SCAN_DEBUG
	 LOG_D("wlanhdr:addr1="WF_MAC_FMT,WF_MAC_ARG(pwlanhdr->addr1));
	 LOG_D("wlanhdr:addr2="WF_MAC_FMT,WF_MAC_ARG(pwlanhdr->addr2));
	 LOG_D("wlanhdr:addr3="WF_MAC_FMT,WF_MAC_ARG(pwlanhdr->addr3));
#endif
//	set_fixed_ie(pframe,10,test,&pkt_len);
	 pxmit_buf->pkt_len = pkt_len;

	pkt = wf_kzalloc(sizeof(wf_pkt));
//	wf_memcpy(&pkt->data,pframe,pkt_len);
//for(;i<pkt_len;i++)
//{
//	LOG_D("%x",*(phead + i));
//}
	LOG_D("-----------------------------------------------------------");

	pkt->data = icmp;
	pkt->len = sizeof(icmp);
	if(wf_wdn_find_info(pnic_info,bmc_addr) == NULL)
	{
		wf_wdn_add(pnic_info,bmc_addr);
	}

 	//ret = wf_tx_msdu(pnic_info,);


	LOG_D("%s ret=%d",__func__,ret);


	if(pxmit_buf != NULL){
		wf_xmit_extbuf_delete(ptx_info,pxmit_buf);
	}
	 if(0 != ret)
	 {
		 LOG_E("[%s] failed, ret=%d",__func__,ret);
	 }
	 return 0;
 }


 void wf_iw_deauth_frame_wlan_hdr (nic_info_st *pnic_info, struct xmit_buf *pxmit_buf)
 {
     wf_u8 *pframe;
     struct wl_ieee80211_hdr *pwlanhdr;
     mlme_info_t *pmlme_info = (mlme_info_t *)pnic_info->mlme_info;;
     pframe = pxmit_buf->pbuf + TXDESC_OFFSET;
     pwlanhdr = (struct wl_ieee80211_hdr *)pframe;

     pwlanhdr->frame_ctl = 0;
     SetFrameType(pframe, WIFI_MGT_TYPE);
     SetFrameSubType(pframe, WIFI_DEAUTH);  /* set subtype */
 }

 int wf_iw_deauth_ap (nic_info_st *pnic_info, wf_u16 reason)
 {
     wf_u8 bc_addr[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
     wf_u8 *pframe;
     wf_u16 start_seq;
     struct wl_ieee80211_hdr *pwlanhdr;
     struct xmit_buf *pxmit_buf;
     wf_u32 pkt_len, i;
     tx_info_st *ptx_info;
     wdn_net_info_st *wdn_info;
     mlme_info_t *mlme_info;
     wf_wlan_info_t *pwlan_info = pnic_info->wlan_info;
     wf_wlan_network_t *pcur_network = &pwlan_info->cur_network;
     hw_info_st *phw_info = (hw_info_st *)pnic_info->hw_info;
     wf_u8 category = WF_WLAN_CATEGORY_BACK;

     wdn_info = wf_wdn_find_info(pnic_info, pcur_network->mac_addr);
     if (wdn_info == NULL)
     {
         return -1;
     }

     ptx_info    = (tx_info_st *)pnic_info->tx_info;
     mlme_info   = (mlme_info_t *)pnic_info->mlme_info;

     /* alloc xmit_buf */
     pxmit_buf = wf_xmit_extbuf_new(ptx_info);
     if(pxmit_buf == NULL)
     {
         LOG_E("pxmit_buf is NULL");
         return -1;
     }
     wf_memset(pxmit_buf->pbuf, 0, WLANHDR_OFFSET + TXDESC_OFFSET);

     /* type of management is 0100 */
     wf_iw_deauth_frame_wlan_hdr(pnic_info, pxmit_buf);

     /* set txd at tx module */
     pframe = pxmit_buf->pbuf + TXDESC_OFFSET; /* pframe point to wlan_hdr */
     pwlanhdr = (struct wl_ieee80211_hdr *)pframe;

     /* copy addr1/2/3 */
     wf_memcpy(pwlanhdr->addr1, pcur_network->mac_addr, MAC_ADDR_LEN);
     wf_memcpy(pwlanhdr->addr2, nic_to_local_addr(pnic_info), MAC_ADDR_LEN);
     wf_memcpy(pwlanhdr->addr3, wdn_info->bssid, MAC_ADDR_LEN);

     pkt_len = sizeof(struct wl_ieee80211_hdr_3addr);
     pframe += pkt_len; /* point to iv or frame body */

     reason = wf_cpu_to_le16(reason);
     pframe = set_fixed_ie(pframe, 2, (wf_u8 *)&reason, &pkt_len);
     wf_nic_mgmt_frame_xmit(pnic_info, wdn_info, pxmit_buf, pxmit_buf->pkt_len);

     return 0;
 }

int wf_iw_all_mac(struct net_device *dev, struct iw_request_info *info, union iwreq_data *wdata, char *extra)
{
	ndev_priv_st *pndev_priv = netdev_priv(dev);
	nic_info_st *pnic_info = pndev_priv->nic;
	wf_u32 loopback;
	loopback = wf_io_read32(pnic_info, 0x100,NULL);
	LOG_D("loopback:%x",loopback);
	loopback |= (BIT(24) | BIT(25) | BIT(27));

	LOG_D("loopback:%x",loopback);
	wf_io_write32(pnic_info, 0x100, loopback);

	return 0;

}
