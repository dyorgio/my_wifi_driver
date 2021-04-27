
#ifndef __WF_OS_API_H__
#define __WF_OS_API_H__

#include "wf_typedef.h"

/* LINUX Porting API */
#if defined(__linux__)
#include <linux/version.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/utsname.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/sem.h>
#include <linux/sched.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/wireless.h>
#include <net/iw_handler.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include "tx_linux.h"


#elif defined(_WIN32)
/* WINDOWS Porting API */
#include <stdio.h>
#include <stdlib.h>
#include <ndis.h>
#include <Wdm.h>


#elif defined (_WIN64)
//define something for Windows (64-bit only)
#endif

#include "os_priv.h"

/* all os use */
#define SIZE_PTR   SIZE_T
#define SSIZE_PTR  SSIZE_T

/**
 * mix
 */
#include "wf_os_api_mix.h"

/**
 * thread
 */
#include "wf_os_api_thread.h"

/**
 * workqueue
 */
#include "wf_os_api_workqueue.h"

/**
 * lock
 */
#include "wf_os_api_lock.h"

/**
 * semaphone
 */
#include "wf_os_api_sema.h"
/**
 * timer
 */
#include "wf_os_api_timer.h"

/**
 * file
 */
#include "wf_os_api_file.h"

#endif  /* END OF __WF_OS_API_H__ */
