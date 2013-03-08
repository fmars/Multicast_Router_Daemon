#include "defs.h"

char *igmp_send_buf;		//IGMP���ĵķ��ͻ���
char *igmp_receive_buf;		//IGMP���ĵĽ��ջ���

int igmpsocket;			//IGMPʹ�õ�socket
u_int32 allrouters;
u_int32 allhosts;
u_int32 allpimrouters;//temp
void igmp_input();
void leave_query(void *arg);
void init_igmp()
{//��ʼ��
	igmp_send_buf=malloc(64*1024);
	igmp_receive_buf=malloc(64*1024);

	allhosts=htonl(INADDR_ALLHOSTS_GROUP);
	allpimrouters=htonl(INADDR_ALLPIM_ROUTERS_GROUP);	
	allrouters=htonl(INADDR_ALLRTRS_GROUP);

	if((igmpsocket=socket(AF_INET,SOCK_RAW,IPPROTO_IGMP))<0)
		log(LOG_ERR,"igmp socket create failed!");

	k_hdr_include(igmpsocket,1);//���͵�ʱ�����IPͷ��
    	k_set_rcvbuf(igmpsocket,256*1024,48*1024);
    	k_set_ttl(igmpsocket,1);//����IGMP���ĵ�TTLΪ1
    	k_set_loop(igmpsocket,0);//�������͵Ķ��㲥����Ӧ�ñ�����

	if(register_input_handler(igmpsocket,igmp_input)<0)
		log(LOG_ERR,"Couldn't register igmp_input as an input handler");
}
void igmp_input()
{//IGMP�����뺯��
	int length;
	int temp=0;
	u_int32 src,dst,group;
	struct ip *ip;
	struct igmp *igmp;
	int ipdatalen,iphdrlen,igmpdatalen;
	length=recvfrom(igmpsocket,igmp_receive_buf,64*1024,0,NULL,&temp);

	if(length<sizeof(struct ip))
	{
		log(LOG_INFO,"IGMP packet too short,receive failed!");
	}

	ip=(struct ip *)igmp_receive_buf;
	src=ip->ip_src.s_addr;
	dst=ip->ip_dst.s_addr;

	if (ip->ip_p == 0) 
	//����δ֪������
	{
		if (src==0||dst==0)
			log(LOG_WARNING,"kernel request not accurate, src %s dst %s",inet_fmt(src, s1), inet_fmt(dst, s2));
		else
			multicast_packet_error(src);/*������鲥���������йص�һЩ���*/
		return;
    }

	iphdrlen=ip->ip_hl<<2;
	ipdatalen=ip->ip_len;

	if (iphdrlen + ipdatalen != length) 
	{
		log(LOG_WARNING,"received packet from %s shorter (%u bytes) than hdr+data length (%u+%u)",inet_fmt(src,s1),length,iphdrlen,ipdatalen);
		return;
    }
    
    igmp=(struct igmp *)(igmp_receive_buf + iphdrlen);
    group=igmp->igmp_group.s_addr;
    igmpdatalen=ipdatalen - IGMP_MINLEN;
    if (igmpdatalen<0) 
	{
		log(LOG_WARNING,"received IP data field too short (%u bytes) for IGMP, from %s",ipdatalen, inet_fmt(src, s1));
		return;
    }
    
    switch (igmp->igmp_type) 
	{
		case IGMP_MEMBERSHIP_QUERY:
			accept_membership_query(src, dst, group);
		return;
	
		case IGMP_V2_MEMBERSHIP_REPORT:
			accept_group_report(src,group);
		return;
	
		case IGMP_V2_LEAVE_GROUP:
			accept_leave_message(src,group);
		return;    
		default:
			log(LOG_INFO,"ignoring unknown IGMPv2 message type %x from %s to %s",igmp->igmp_type, inet_fmt(src, s1), inet_fmt(dst, s2));
		return;
    }
}
void igmp_output(char *buf,u_int32 src,u_int32 dst,int type,int code,u_int32 group,int datalen)
{
//IGMP���������    
	struct sockaddr_in sdst;
	struct ip *ip;
    
	struct igmp *igmp;
    
	int sendlen;
    
	int setloop = 0;

   
	ip 			    = (struct ip *)buf;

    ip->ip_len              = sizeof(struct ip) + IGMP_MINLEN + datalen;

	ip->ip_src.s_addr       = src;

    ip->ip_dst.s_addr       = dst;

	sendlen                 = ip->ip_len;

	ip->ip_v   = IPVERSION;

    ip->ip_hl  = (sizeof(struct ip) >> 2);

    ip->ip_tos = 0xc0;                  /* Internet Control   */

	ip->ip_id  = 0;                     /* let kernel fill in */

    ip->ip_off = 0;
    ip->ip_ttl = MAXTTL;		/* applies to unicasts only */
 
	ip->ip_p   = IPPROTO_IGMP;
    ip->ip_sum = 0;                     /* let kernel fill in               */


    igmp                    = (struct igmp *)(buf + sizeof(struct ip));

	igmp->igmp_type         = type;

	igmp->igmp_code         = code;

    igmp->igmp_group.s_addr = group;

    igmp->igmp_cksum        = 0;

    igmp->igmp_cksum        = inet_cksum((u_int16 *)igmp,
					 IGMP_MINLEN + datalen);


    if (IN_MULTICAST(ntohl(dst)))
	{
	
		k_set_if(igmpsocket, src);
		if (dst == allhosts)
		{
	    
			setloop = 1;
	    
			k_set_loop(igmpsocket,1);
	
		}
    
	}
    
   
	bzero((void *)&sdst, sizeof(sdst));
    
	sdst.sin_family = AF_INET;
    
	sdst.sin_len = sizeof(sdst);

    	sdst.sin_addr.s_addr = dst;
    
	if (sendto(igmpsocket, igmp_send_buf, sendlen, 0,(struct sockaddr *)&sdst, sizeof(sdst)) < 0) 
	{
	    //log(LOG_INFO, "sendto to %s on %s", inet_fmt(dst, s1), inet_fmt(src, s2));
	
		if (setloop)
	    		k_set_loop(igmpsocket,0);
		return;    
	}

	if (setloop)
		k_set_loop(igmpsocket,0);

	
	log(LOG_INFO,"SENT %s from %-15s to %s",packet_kind(IPPROTO_IGMP, type, code),inet_fmt(src, s1), inet_fmt(dst, s2));

}
void retry_query(void *arg)
{
//���Լ����Ǽ�ʱ��������£���ʱ����ʱ���ֳ�Ϊ��ʱ��������Query����
	log(LOG_INFO,"Querier is timeout,we are the querier now,send igmp query packet!");
	struct vif_data *mydata=(struct vif_data*)arg;
	vifi_t vif=mydata->vif;
	myvifs[vif].igmp_querytimer_id=timer_setTimer(IGMP_Query_Time,send_query,mydata);
	myvifs[vif].querier=0;
	myvifs[vif].querier_retry_id=0;
	
	igmp_output(igmp_send_buf,myvifs[vif].address,allhosts,IGMP_MEMBERSHIP_QUERY,100,0,0);
}
void send_query(void *arg)//��������Query����
{
	struct vif_data *mydata=(struct vif_data*)arg;
	vifi_t vif=mydata->vif;
	myvifs[vif].igmp_querytimer_id=timer_setTimer(IGMP_Query_Time,send_query,mydata);
	myvifs[vif].querier=0;
	myvifs[vif].querier_retry_id=0;

	igmp_output(igmp_send_buf,myvifs[vif].address,allhosts,IGMP_MEMBERSHIP_QUERY,100,0,0);
}
void accept_membership_query(u_int32 src,u_int32 dst,u_int32 group)
{//�յ���IGMP query����
	vifi_t vif;
	struct myvif *myv;
	struct igmp_group *g;
	struct vif_data *mydata,*retrydata;
	mydata=(struct vif_data *)malloc(sizeof(struct vif_data));
	retrydata=(struct vif_data *)malloc(sizeof(struct vif_data));
	myv=(struct myvif *)malloc(sizeof(struct myvif));

	if(is_local_address(src)!=0)
	//�Լ������Ĳ�ѯ���ģ�������
	{
		return;
	}

	vif=find_vif_direct(src);
	if(vif==0)
	//�������Լ��������������������ģ�������
	{
		return;
	}

	myv=&myvifs[vif];
	retrydata->vif=vif;
	retrydata->group=0;
	log(LOG_INFO,"Receive IGMP membership query from %s",inet_fmt(src,s1));

	if(myv->querier!=0)
	{
		timer_clearTimer(myv->querier_retry_id);
		myv->querier_retry_id=timer_setTimer(IGMP_Querier_Timeout,retry_query,retrydata);
	}
	if(myv->querier==0||myv->querier!=src)
	{
		//�����µ�·�����Ĳ�ѯ��Ϣ
		//�򱾽ӿھ��ǲ�ѯ��,Ҫ��Ƚ���ȷ���µ�IGMP��ѯ��
		if(myv->querier==0)
		{
			if(ntohl(src)<ntohl(myv->address))
			{
				log(LOG_INFO,"New IGMP querier %s",inet_fmt(src,s1));
				myv->querier=src;
				timer_clearTimer(myv->igmp_querytimer_id);
				myv->igmp_querytimer_id=0;
				myv->querier_retry_id=timer_setTimer(IGMP_Querier_Timeout,retry_query,retrydata);
			}
		}
		else
		{
			if(ntohl(src)<ntohl(myv->querier))
			{
				log(LOG_INFO,"New IGMP querier %s",inet_fmt(src,s1));
				myv->querier=src;
				timer_clearTimer(myv->querier_retry_id);
				myv->querier_retry_id=timer_setTimer(IGMP_Querier_Timeout,retry_query,retrydata);
			}
		}
	}
	if(myv->querier!=0&&group!=0)
	{
		//�Ҳ��ǲ�ѯ����������һ���ض����ѯ������ĳ���鲥�����Ҫ��ʧ���鿴�Ƿ�����Ҳ���е��飬����еĻ���������ʱ��������ʱ����ɾ������
		for(g=myv->groups;g!=NULL;g=g->next)
		{
			if(g->address==group)
			{
				mydata->vif=vif;
				mydata->group=g->address;
				g->leave_query=timer_setTimer(IGMP_Query_Time*2+10,leave_query,mydata);
				g->timeout=-1;
			}
		}
	}
}
void accept_group_report(u_int32 src,u_int32 group)
{//�յ������Ա��ϵ���ı���
	vifi_t vif;
	struct myvif *myv;
	struct igmp_group *g;

	vif=find_vif_direct(src);
	if(vif==0)
	//���Լ�����ͬһ�����Σ�������
	{
		return;
	}

	log(LOG_INFO,"receive IGMP group membership report from %s for group %s",inet_fmt(src, s1),inet_fmt(group, s2));
	
	myv=&myvifs[vif];

	for(g=myv->groups;g!=NULL;g=g->next)
	//��������鲥���Ƿ����뿪������µ��ض����ѯ״̬������ǵĻ���ȡ��
	{
		if(g->address==group)
		{
			//log(LOG_INFO,"Find this group!");
			if(g->leave_query!=0)
			{
				
				log(LOG_INFO,"This group is in leave_query!,so we cancel the leave_query");
				timer_clearTimer(g->leave_query);
				g->leave_query=0;
				g->timeout=2;
			}
			break;
		}
	}

	if(g==NULL)
	//û���ҵ��鲥����Ϣ������һ���µ�Ҫ������鲥��
	{
		//log(LOG_INFO,"This is a new Group!");
		g=(struct igmp_group *)malloc(sizeof(struct igmp_group));
		if(g==NULL)
			log(LOG_ERR,"Ran out of memory");

		g->address=group;
		g->leave_query=0;
		g->timeout=2;
		if(myv->groups!=NULL)
		{
			g->next=myv->groups;
		}
		else
		{
			g->next==NULL;
		}
		myv->groups=g;
		log(LOG_INFO,"add_leaf(%s,%s)",myvifs[vif].name,inet_fmt(group,s1));
		add_leaf(vif,group);
	}
}
void leave_query(void *arg)
{//�յ������뿪���ĺ󣬷����ض���ѯ����2�Σ�����10�룬����û�У���ֹͣ�鲥���ĵ�ת��
	struct vif_data *mydata=(struct vif_data*)arg;
	struct myvif *myv=&myvifs[mydata->vif];
	struct igmp_group *g,*temp;
	for(g=myv->groups;g!=NULL;g=g->next)
	{
		if(g->address==mydata->group)
		{
			if(g->timeout==1)
			{
				log(LOG_INFO,"Send specific group query message for group %s 2nd",inet_fmt(g->address,s1));
				igmp_output(igmp_send_buf,myv->address,g->address,IGMP_MEMBERSHIP_QUERY,100,g->address,0);
				g->leave_query=timer_setTimer(IGMP_Query_Time,leave_query,mydata);
				g->timeout=0;
			}
			else
			{
				if(g->timeout==0)
				{
					log(LOG_INFO,"Wait group report message for group %s for last 10 seconds",inet_fmt(g->address,s1));
					g->leave_query=timer_setTimer(IGMP_ResponseTime/10,leave_query,mydata);
					g->timeout=-1;
				}
				else
				{
					if(g!=myv->groups)
					{
						temp->next=g->next;
						free(g);
					}
					else
					{
						myv->groups=g->next;
					}
					log(LOG_INFO,"Time out,On vif %s delete the group %s",myvifs[mydata->vif].name,inet_fmt(g->address,s1));
					
				del_leaf(mydata->vif,g->address);
				}
			}
			break;
		}
		temp=g;
	}
}
void accept_leave_message(u_int32 src,u_int32 group)
{//�յ����뿪��Ϣ�Ĳ���
	vifi_t vif;
	struct myvif *myv;
	struct igmp_group *g;
	struct vif_data *mydata;
	
	mydata=(struct vif_data *)malloc(sizeof(struct vif_data));
	vif=find_vif_direct_local(src);
	if(vif==0)
	{
		return;
	}


	log(LOG_INFO,"receive IGMP leave message from %s for group %s",inet_fmt(src, s1),inet_fmt(group, s2));
	myv=&myvifs[vif];
	if(myv->querier!=0)
	{
		log(LOG_INFO,"We are not the querier,don't handle this packet");
		return;
	}
	for(g=myv->groups;g!=NULL;g=g->next)
	{
		if(g->address==group)
		{
			if(g->leave_query!=0)
			{
				timer_clearTimer(g->leave_query);
			}
			mydata->vif=vif;
			mydata->group=g->address;
			log(LOG_INFO,"Send specific group query message for group %s 1st",inet_fmt(g->address,s1));
			igmp_output(igmp_send_buf,myv->address,g->address,IGMP_MEMBERSHIP_QUERY,IGMP_ResponseTime,g->address,0);
			g->leave_query=timer_setTimer(IGMP_Query_Time,leave_query,mydata);
			g->timeout=1;
			break;
		}
	}
}
