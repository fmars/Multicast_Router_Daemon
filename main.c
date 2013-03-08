#include "defs.h"
u_long virtual_time = 0;
int mysignal=0;
#define NHANDLERS       3

static struct ihandler 
{
    int fd;			/* File descriptor               */
    ihfunc_t func;		/* Function to call with &fd_set */
} ihandlers[NHANDLERS];//����ṹ��ע�ᱨ�����뺯����
static int nhandlers = 0;

void handler(int sig)
{
    switch(sig)
    {
    	case SIGINT://ctrl+c
    	case SIGTERM://��ֹ����     ������ֹ�ź�
	     mysignal=1; //exit
	     break;
    }
}
void timer()
{//���û�м�ʱ���¼���Ĭ��5��ѭ�����Σ�����5��ļ�ʱ��
    virtual_time += 5;
    timer_setTimer(5, timer, NULL);
}
int register_input_handler(int fd,ihfunc_t func)
{//ע�ᱨ�����봦������
    if (nhandlers >= NHANDLERS)
	return -1;
    
    ihandlers[nhandlers].fd = fd;
    ihandlers[nhandlers++].func = func;
    
    return 0;
}

int main(int argc,char *argv[])
{	
		struct timeval tv, difftime, curtime, lasttime, *timeout;
		fd_set rfds, readers;
		int nfds, n, i, secs;

		struct sigaction sa;
    

		setlinebuf(stderr);	
    if (geteuid()!= 0) 
    {
				log(LOG_INFO,"Error:Pogram must be run on root!\n");
				exit(1);
		}

    
    log(LOG_INFO,"PIM DM Daemon starting");
    
    srandom(gethostid());//�����

    callout_init();
    init_igmp();
    init_pim();
    init_route();
    init_mrt();
    init_vifs();//��ʼ��

    sa.sa_handler = handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,&sa,NULL);
    sigaction(SIGTERM,&sa,NULL);
    
    FD_ZERO(&readers);
    FD_SET(igmpsocket, &readers);
    nfds=igmpsocket+1;
    for (i=0;i<nhandlers; i++) 
    {
			FD_SET(ihandlers[i].fd,&readers);
			if (ihandlers[i].fd >= nfds)
					nfds = ihandlers[i].fd + 1;
    }//�ַ�������������
    
    timer_setTimer(5, timer, NULL);//��ʼ��һ����ʱ��
    
    difftime.tv_usec = 0;
    gettimeofday(&curtime, NULL);
    lasttime = curtime;//difftime��lasttime���������㴦�����Ļ��ʱ���¼��õ���ʱ�䣬�������1�룬Ҫ����Ƿ�Ӱ����һ����ʱ���¼�
    while(1)
    {	//��ʼѭ��
			//print_sg();
				bcopy((char *)&readers, (char *)&rfds, sizeof(rfds));
				secs = timer_nextTimer();
				if (secs ==-1)
						timeout = NULL;
				else 
        {
						timeout = &tv;
						timeout->tv_sec = secs;
						timeout->tv_usec = 0;
        }
        if(mysignal==1) 
				{
						break;//�Ƿ����˳��źţ�ctrl+c
				}
				if ((n = select(nfds, &rfds, NULL, NULL, timeout)) < 0) 
        {
						continue;//�����Ƿ��б���
				}
				do 
				{
						if (n == 0) 
						{
								curtime.tv_sec = lasttime.tv_sec + secs;
								curtime.tv_usec = lasttime.tv_usec;
								n = -1;	
						} 
						else
								gettimeofday(&curtime, NULL);
						difftime.tv_sec = curtime.tv_sec - lasttime.tv_sec;
						difftime.tv_usec += curtime.tv_usec - lasttime.tv_usec;
						while (difftime.tv_usec > 1000000) 
						{
								difftime.tv_sec++;
								difftime.tv_usec -= 1000000;
						}
						if (difftime.tv_usec < 0) 
            {
								difftime.tv_sec--;
								difftime.tv_usec += 1000000;
						}
						lasttime = curtime;
						if (secs == 0 || difftime.tv_sec > 0) {
								age_callout_queue(difftime.tv_sec);
						}
						secs = -1;
				} while (difftime.tv_sec > 0);
				if (n > 0) 
				{
						for (i = 0; i < nhandlers; i++) 
						{
								if (FD_ISSET(ihandlers[i].fd, &rfds)) 
								{
										(*ihandlers[i].func)(ihandlers[i].fd, &rfds);
								}
						}//������ʱ��
				}
		}
		log(LOG_INFO,"Program exiting");
		exit(0);
}