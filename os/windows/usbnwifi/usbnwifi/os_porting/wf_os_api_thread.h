#ifndef __WF_OS_API_THREAD_H__

typedef struct wf_thread_s
{
	void *obj;
	void *tid;
	wf_u8 stop;
}wf_thread_t;

void* wf_os_api_thread_create(void *ptid, char *name, void *func, void *param);
int wf_os_api_thread_wakeup(void *ptid);
int wf_os_api_thread_destory(void *ptid);
wf_bool wf_os_api_thread_wait_stop(void *ptid);
void wf_os_api_thread_exit(void *ptid);

#endif
