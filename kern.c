#include "defs.h"
#ifdef RAW_OUTPUT_IS_RAW
int curttl = 0;
#endif

void k_init_pim(int socket)
{//初始化组播的socket，用于vif.c中
    int v=1;
    
    if (setsockopt(socket,IPPROTO_IP,MRT_INIT,(char *)&v,sizeof(int))<0)
	log(LOG_ERR,"cannot enable multicast routing in kernel");//初始化PIM的socket
    
    if(setsockopt(socket,IPPROTO_IP, MRT_ASSERT,(char *)&v,sizeof(int))<0)
	log(LOG_ERR,"cannot set ASSERT flag in kernel");//启用断言

	log(LOG_INFO,"Init Kernel multicast option success!");
}
void k_set_rcvbuf(int socket,int bufsize,int minsize)
{//设定缓存，理想是bufsize，最小要minsize，否则报错
    int delta = bufsize / 2;
    int iter = 0;
    
    if (setsockopt(socket, SOL_SOCKET, SO_RCVBUF,(char *)&bufsize, sizeof(bufsize)) < 0) 
    {
			bufsize -= delta;
			while (1) {
	    	iter++;
	   	  if (delta > 1)
	      	delta = delta/2;
	    
	    	if (setsockopt(socket, SOL_SOCKET, SO_RCVBUF,(char *)&bufsize, sizeof(bufsize)) < 0) {
					bufsize -= delta;
	    	} else {
				if (delta < 1024)
		    	break;
					bufsize += delta;
	    	}
			}
	if (bufsize < minsize) {
	    log(LOG_ERR,"OS-allowed buffer size %u < app min %u",
		bufsize, minsize);
	    /*NOTREACHED*/
	}
    }
}
void k_hdr_include(socket, bool)
    int socket;
    int bool;
{//设置是否包括报文头部
#ifdef IP_HDRINCL
    if (setsockopt(socket, IPPROTO_IP, IP_HDRINCL,
		   (char *)&bool, sizeof(bool)) < 0)
	log(LOG_ERR,  "setsockopt IP_HDRINCL %u", bool);
#endif
}
void k_set_ttl(socket, t)
    int socket;
    int t;
{
#ifdef RAW_OUTPUT_IS_RAW
    curttl = t;
#else
    u_char ttl;
    
    ttl = t;
    if (setsockopt(socket, IPPROTO_IP, IP_MULTICAST_TTL,
		   (char *)&ttl, sizeof(ttl)) < 0)
	log(LOG_ERR,"setsockopt IP_MULTICAST_TTL %u", ttl);
#endif
}
void k_set_loop(socket, flag)
    int socket;
    int flag;
{//报文是否发送LOOP
    u_char loop;

    loop = flag;
    if (setsockopt(socket, IPPROTO_IP, IP_MULTICAST_LOOP,
		   (char *)&loop, sizeof(loop)) < 0)
	log(LOG_ERR,"setsockopt IP_MULTICAST_LOOP %u", loop);
}

void k_set_if(int socket,u_int32 ifa)
{//设置socket，只在地址为ifa的接口发送该报文
    struct in_addr adr;

    adr.s_addr=ifa;
    if (setsockopt(socket,IPPROTO_IP,IP_MULTICAST_IF,(char *)&adr,sizeof(adr))<0)
			log(LOG_ERR,"setsockopt IP_MULTICAST_IF %s",inet_fmt(ifa, s1));
}
void k_join(int socket,u_int32 grp,u_int32 ifa)
{//给定组播地址和网络接口地址，将这个网络接口加入到指定的组播组中，接收该组播组的报文
    struct ip_mreq mreq;
    
    mreq.imr_multiaddr.s_addr = grp;
    mreq.imr_interface.s_addr = ifa;

    if (setsockopt(socket,IPPROTO_IP,IP_ADD_MEMBERSHIP,(char *)&mreq, sizeof(mreq))<0)
		log(LOG_WARNING,"cannot join group %s on interface %s",inet_fmt(grp, s1),inet_fmt(ifa, s2));
	log(LOG_INFO,"Interface %s join group %s success!",inet_fmt(ifa, s2),inet_fmt(grp, s1));
}
void k_leave(int socket,u_int32 grp,u_int32 ifa)
{//跟上面正好相反的操作
    struct ip_mreq mreq;

    mreq.imr_multiaddr.s_addr = grp;
    mreq.imr_interface.s_addr = ifa;
    
    if (setsockopt(socket,IPPROTO_IP,IP_DROP_MEMBERSHIP,(char *)&mreq,sizeof(mreq))<0)
	log(LOG_WARNING,"cannot leave group %s on interface %s",inet_fmt(grp, s1), inet_fmt(ifa, s2));
}
void k_add_vif(int socket,vifi_t vif,struct myvif *v)
{//添加内核虚拟接口
    struct vifctl vc;

    vc.vifc_vifi            =vif;
    vc.vifc_flags           =0;
    vc.vifc_threshold       =v->threshold;
    vc.vifc_rate_limit	    =v->rate_limit;
    vc.vifc_lcl_addr.s_addr =v->address;
    vc.vifc_rmt_addr.s_addr =0;

    if(setsockopt(socket,IPPROTO_IP,MRT_ADD_VIF,(char *)&vc,sizeof(vc))<0)
		log(LOG_ERR,"setsockopt MRT_ADD_VIF on vif %d Failed",vif);
}
int delete_mfc(int socket,u_int32 source,u_int32 group)
{/*删除一个MFC,0表示删除失败，1表示删除成功*/
    struct mfcctl mc;

    mc.mfcc_origin.s_addr=source;
    mc.mfcc_mcastgrp.s_addr=group;
	
    if (setsockopt(socket,IPPROTO_IP,MRT_DEL_MFC,(char *)&mc,sizeof(mc))<0)
	{
		log(LOG_INFO,"setsockopt k_del_mfc error!!");
		return 0;
    }
	log(LOG_INFO,"Deleted MFC entry: src %s, grp %s",inet_fmt(mc.mfcc_origin.s_addr, s1),inet_fmt(mc.mfcc_mcastgrp.s_addr, s2));
    return 1;
}
int change_mfc(int socket,u_int32 source,u_int32 group,vifi_t incoming)
{/*根据源地址，组播地址，组播报文的来源接口改变内核的MFC*/
    struct mfcctl mc;
    vifi_t vif;
    struct myvif *myv;
    struct mrt *mymrt;

    mymrt=find_sg(source,group);
    //log(LOG_INFO,"mymrt's out number is %d",mymrt->outnumber);
    if(mymrt==NULL)
    {
        return 0;
    }
    mc.mfcc_origin.s_addr=source;/*这个组播的来源是？*/
    mc.mfcc_mcastgrp.s_addr=group;/*这个组播的组播地址是？*/
    mc.mfcc_parent=incoming;/*本设备接收到这个组播报文的接口是？*/
    for(vif=1;vif<=maxvif;vif++)
    {
		if(mymrt->outvif[vif]==1)
		{
			mc.mfcc_ttls[vif]=1;
			log(LOG_INFO,"Change MFC,Forwarding on vif %d",vif);
		}
		else
		{
			mc.mfcc_ttls[vif]=0;
		}
    }    
    if (setsockopt(socket,IPPROTO_IP,MRT_ADD_MFC,(char *)&mc,sizeof(mc))<0) 
    {
    	log(LOG_INFO,"ADD MFC for source %s and group %s Error",inet_fmt(source, s1), inet_fmt(group, s2));
        return 0;
    }
    log(LOG_INFO,"ADD MFC for source %s and group %s Success",inet_fmt(source, s1), inet_fmt(group, s2));
    return 1;
}
int k_get_sg_cnt(int socket,u_int32 source,u_int32 group)
{//成功返回1，否则返回0，是统计距上一次统计，总共转发了多少报文
    struct sioc_sg_req sgreq;
    
    sgreq.src.s_addr=source;
    sgreq.grp.s_addr=group;
    if ((ioctl(socket,SIOCGETSGCNT,(char *)&sgreq)<0)||(sgreq.wrong_if==0xffffffff)) 
    {	 
		return	0;
    }
    return sgreq.pktcnt;
}
