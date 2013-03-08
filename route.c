#include "defs.h"
#define BUFLEN ( sizeof(struct rt_msghdr)+512 )
int routesocket;
int seq;
struct rt_metrics rt_metrics;
u_long rtm_inits;
void init_route()
{
	routesocket=socket(PF_ROUTE,SOCK_RAW,0);
	if(routesocket<0)
	{
		log(LOG_INFO,"Create route socket error!");
	}
	log(LOG_INFO,"Create route socket success!");
	seq=0;
}
int get_upstream(int source)
{//给定目的地址，返回到达这个地址应该的下一跳地址，如果直连的话，返回该地址，具体参见指导书
	char *buf;
	vifi_t vif;
	pid_t pid;
	ssize_t n;
	char *data;
	int flags;
	struct in_addr in;
	struct rt_msghdr *rtm;
	struct sockaddr *sa;
	struct sockaddr_in *sin;

	vif=find_vif_direct(source);
	if(vif!=0)
	{
		return source;
	}

	flags=RTF_STATIC;
    	//flags |= RTF_GATE;
	buf = calloc(1,sizeof(struct rt_msghdr)+512);
	rtm = (struct rt_msghdr *)buf;
	rtm->rtm_msglen=sizeof(struct rt_msghdr)+sizeof(struct sockaddr_in);
	rtm->rtm_version = RTM_VERSION;
	rtm->rtm_type = RTM_GET;
	rtm->rtm_flags=flags;
	rtm->rtm_addrs |= RTA_DST;
	rtm->rtm_pid=pid=getpid();
	rtm->rtm_seq=seq;
	sin = (struct sockaddr_in *)(rtm+1);
	sin->sin_len=sizeof(struct sockaddr_in);
	sin->sin_family=AF_INET;
	inet_pton( AF_INET,inet_fmt(source,s1), &sin->sin_addr );
	if(write(routesocket,rtm,rtm->rtm_msglen)<0)
	{
		log(LOG_INFO,"Write Error!\n");
}
	do{
		n=read(routesocket,rtm,sizeof(struct rt_msghdr)+512);
	}while(rtm->rtm_type!=RTM_GET||rtm->rtm_seq!=seq||rtm->rtm_pid!=pid);
	if(n<0)
	{
		log(LOG_INFO,"Read Error!");
	}
	rtm=(struct rt_msghdr *)buf;
	data=(char *)(rtm+1);
	data=data+4*sizeof(long);
	sa=(struct sockaddr *)data;
	in=((struct sockaddr_in *)sa)->sin_addr;

	return in.s_addr;
}
int get_incoming_vif(int source)
{
	int neighbor;
	neighbor=get_upstream(source);
	if(neighbor==0)
	{
		exit(0);
	}
	return find_vif_direct(neighbor);
}
