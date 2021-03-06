#include "defs.h"
char *pim_receive_buf;
char *pim_send_buf;
u_int32 allpimrouters;
int pimsocket;
void pim_input();
void receive_join_prune(u_int32 src,u_int32 dst,char *data,int length);
void receive_assert(u_int32 src,u_int32 dst,char *data,int length);
void receive_pim_graft(u_int32 src,u_int32 dst,char *data,int length);
void receive_pim_graft_ack(u_int32 src,u_int32 dst,char *data,int length);
void receive_hello(u_int32 src,u_int32 dst,char *data,int length);
void init_pim()
{//初始化
	allpimrouters=htonl(INADDR_ALLPIM_ROUTERS_GROUP);
	pim_receive_buf=malloc(64*1024);
	pim_send_buf=malloc(64*1024);
	
	if((pimsocket=socket(AF_INET,SOCK_RAW,IPPROTO_PIM))<0)
		log(LOG_ERR,"igmp socket create failed!");

	k_hdr_include(pimsocket,1);//发送的时候包括IP头部
    	k_set_rcvbuf(pimsocket,256*1024,48*1024);
    	k_set_ttl(pimsocket,1);//设置IGMP报文的TTL为1
    	k_set_loop(pimsocket,0);//决定发送的多点广播包不应该被回送

	if(register_input_handler(pimsocket,pim_input)<0)
		log(LOG_ERR,"Couldn't register pim_input as an input handler");
}
void pim_input()
{//接收，检查报文类型，交由相应的函数处理
	int length;
	u_int32 src,dst;
	struct ip *ip;
	struct pim *pim;
	int iphdrlen,pimlen;
	
	length=recvfrom(pimsocket,pim_receive_buf,64*1024,0,NULL,NULL);

	if(length<sizeof(struct ip))
	{
		log(LOG_INFO,"PIM packet too short,receive failed!");
	}

	ip=(struct ip *)pim_receive_buf;
	src=ip->ip_src.s_addr;
	dst=ip->ip_dst.s_addr;
	iphdrlen=ip->ip_hl<<2;

	pim=(struct pim *)(pim_receive_buf+iphdrlen);
	pimlen=length-iphdrlen;

	if (pimlen<sizeof(pim)) 
	{
		log(LOG_INFO,"PIM packet too short,receive failed!");
		return;
    }
    
    switch (pim->pim_type) 
	{
		case PIM_HELLO:
			receive_hello(src,dst,(char *)(pim),pimlen);
		return;
	
		case PIM_JOIN_PRUNE:
			receive_join_prune(src,dst,(char *)(pim),pimlen);
		return;
	
		case PIM_ASSERT:
			receive_assert(src,dst,(char *)(pim),pimlen);
		return;
		
		case PIM_GRAFT:
			receive_pim_graft(src,dst,(char *)(pim),pimlen);
		return;
		
		case PIM_GRAFT_ACK:
			receive_pim_graft_ack(src,dst,(char *)(pim),pimlen);
		return;

		default:
			log(LOG_INFO,"ignoring unused PIM packet from %s to %s",inet_fmt(src, s1), inet_fmt(dst, s2));
		return;
    }
}
int pimdm_output(char *buf,u_int32 src,u_int32 dst,int type,int length)
{//提供缓存，源、目的、类型和长度，发送PIM报文
	struct sockaddr_in sockdst;
	struct ip *ip;
	struct pim *pim;
	int sendlen;
	int setloop=0;
	ip=(struct ip*)buf;
	ip->ip_v=4;
	ip->ip_hl=(sizeof(struct ip)>>2);
	ip->ip_tos=0;
	ip->ip_id=0;
	ip->ip_off=0;
	ip->ip_p=IPPROTO_PIM;
	ip->ip_sum=0;
	ip->ip_len=sizeof(struct ip)+sizeof(struct pim)+length;
	ip->ip_src.s_addr=src;
	ip->ip_dst.s_addr=dst;
	ip->ip_ttl=1;
	sendlen=ip->ip_len;

	pim=(struct pim *)(buf+sizeof(struct ip));
	pim->pim_type=type;
	pim->pim_vers=2;
	pim->pim_cksum=0;
	pim->pim_cksum=inet_cksum((u_int16 *)pim,sizeof(struct pim)+length);

	if(IN_MULTICAST(ntohl(dst)))
	{
		k_set_if(pimsocket,src);
		setloop=1;
		k_set_loop(pimsocket,1);
	}
	bzero((void *)&sockdst,sizeof(sockdst));
    	sockdst.sin_family=AF_INET;
    	sockdst.sin_len=sizeof(sockdst);
	sockdst.sin_addr.s_addr=dst;
    
	if (sendto(pimsocket,pim_send_buf,sendlen,0,(struct sockaddr *)&sockdst,sizeof(sockdst))<0) 
	{	
		if (setloop)
	    		k_set_loop(pimsocket,0);
		return 0;    
	}

	if (setloop)
		k_set_loop(pimsocket,0);
	return 1;
}
int valid_packet(u_int32 src,char *packet,int length)
{//检验包的校验和 源地址 接口等信息，确定该包是否有效，有效返回1，否则返回0
	vifi_t vif;
	struct myvif *myv;
	if(inet_cksum((u_int16 *)packet,length))
	{
		log(LOG_INFO,0,"Checking sum error!");
		return 0;
	}
	vif=find_vif_direct(src);
	if(vif==0)
	{
		//log(LOG_INFO,"Source address error!Don't handle!");
		return 0;
	}
	myv=&myvifs[vif];
	if(myv->is_active==0)
	{
		log(LOG_INFO,"Interface is not active!");
		return 0;
	}
	return 1;
}
//############################################################
//处理Hello报文
//Handle hello packet
//############################################################
int get_hold_time(char *data,int length)
{//从Hello报文中获得holdtime的值,如果获取失败返回-1，否则返回相应的holdtime值
	short *holdtime;
	char *mydata;
	struct pim_hello_header *option;

	mydata=data+sizeof(struct pim);
	length-=sizeof(struct pim);
	while(length>=sizeof(struct pim_hello_header))
	{
		option=(struct pim_hello_header *)mydata;
		if(ntohs(option->option_type)==1)
		{
			if(ntohs(option->option_length)!=2)
			{
				log(LOG_INFO,"Error!Invalid Hello Option length");
				return -1;
			}
			mydata=mydata+sizeof(struct pim_hello_header);
			holdtime=(short *)mydata;
			return ntohs(*holdtime);
		}
		mydata=mydata+sizeof(struct pim_hello_header)+ntohs(option->option_length);
		length=length-sizeof(struct pim_hello_header)-ntohs(option->option_length);
	}
	return -1;
}
void pim_hello_output(void *arg)
{//给定Holdtime，发送一条PIM的Hello报文
	struct pim_hello_output_data *hello_data;//参考实验指导书
	struct pim_hello_header *option;
	char *mydata;
	short *hold_time;
	int length;
	
	hello_data=(struct pim_hello_output_data *)arg;
	mydata=(char *)(pim_send_buf+sizeof(struct ip)+sizeof(struct pim));

	option=(struct pim_hello_header *)mydata;
	option->option_type=htons(1);
	option->option_length=htons(2);

	mydata=mydata+sizeof(struct pim_hello_header);
	hold_time=(short *)mydata;
	*hold_time=htons(hello_data->holdtime);

	length=sizeof(struct pim_hello_header)+sizeof(short);

	
	if(hello_data->regular==1)
	
	{
		
		timer_setTimer(PIM_HELLO_PERIOD,pim_hello_output,hello_data);
	
	}
	if(pimdm_output(pim_send_buf,myvifs[hello_data->vif].address,allpimrouters,PIM_HELLO,length)!=1)
	{
		log(LOG_INFO,"Send Pim Hello Packet On %s(%s) Error!",myvifs[hello_data->vif].name,inet_fmt(myvifs[hello_data->vif].address,s1));
	}
	else
	{
		log(LOG_INFO,"Send Pim Hello Packet On %s(%s) Success!",myvifs[hello_data->vif].name,inet_fmt(myvifs[hello_data->vif].address,s1));
	
	}
	return;
}
void delete_pim_neighbor(void *arg)
{//删除一个邻居
	struct pim_neighbor *myentry,*temp,*pimneighbor;
	vifi_t vif;
	myentry=arg;

	vif=find_vif_direct(myentry->address);
	if(vif==0)
	{
		return;
	}
	for(pimneighbor=myvifs[vif].neighbor;pimneighbor!=NULL;pimneighbor=pimneighbor->next)
	{
		if(pimneighbor->address==myentry->address)
		{
			if(myvifs[vif].neighbor->address==myentry->address)
			{
				myvifs[vif].neighbor=NULL;
			}
			temp->next=pimneighbor->next;
			break;
		}
		temp=pimneighbor;
	}
	log(LOG_INFO,"Holdtime=0,delete this neighbor");
	refresh_mrt(vif,DEL_NEIGHBOR);
}
void add_pim_neighbor(u_int32 src,int holdtime,vifi_t thevif)
{//添加一个邻居
	vifi_t vif;
	vif=thevif;
	struct pim_neighbor *newentry;
	
	newentry=(struct pim_neighbor *)malloc(sizeof(struct pim_neighbor));
	newentry->next=myvifs[vif].neighbor;
	newentry->address=src;
	newentry->holdtime=holdtime;
	newentry->timeoutid=timer_setTimer(holdtime,delete_pim_neighbor,newentry);

	myvifs[vif].neighbor=newentry;
	log(LOG_INFO,"It's a new neighbor,add this neighbor on vif %s",myvifs[vif].name);
	refresh_mrt(vif,ADD_NEIGHBOR);/*1 add 0 delete*/
}
void receive_hello(u_int32 src,u_int32 dst,char *data,int length)
{//收到Hello报文
	vifi_t vif;
	struct pim_neighbor *pimneighbor;
	
	int holdtime;
	vif=find_vif_direct(src);
	if(valid_packet(src,data,length)==0)
	{
		return;
	
	}
	holdtime=get_hold_time(data,length);
	if(holdtime==-1)
	{
		log(LOG_INFO,"Get hold time error!");
	}
	log(LOG_INFO,"Receive PIM Hello message,get Hold time=%d from %s",holdtime,inet_fmt(src,s1));
	for(pimneighbor=myvifs[vif].neighbor;pimneighbor!=NULL;pimneighbor=pimneighbor->next)
	{
		if(pimneighbor->address==src)
		{
			if(holdtime==0)
			{
				delete_pim_neighbor(pimneighbor);
				return;
			}
			pimneighbor->holdtime=holdtime;
			if(pimneighbor->timeoutid!=-1)
			{
				timer_clearTimer(pimneighbor->timeoutid);
			}
			pimneighbor->timeoutid=timer_setTimer(holdtime,delete_pim_neighbor,pimneighbor);
			log(LOG_INFO,"Refresh hold time of neighbor %s to %d",inet_fmt(pimneighbor->address,s1),holdtime);
			return;
		}
	}
	add_pim_neighbor(src,holdtime,vif);
	return;
	//根据所得到的时间，采取相应的操作*/
}
//############################################################
//处理JOIN和PRUNE报文
//Handle join/prune
//############################################################
void pim_join_prune_output(struct mrt *mrtdata,int packet_type,int holdtime)
{//发送Join/Prune报文，填充其特定格式，参见指导书
	struct pim_join_prune_header *jpheader;
	struct encode_unicast_option *unicast_option;
	struct pim_join_prune_group *jpgroup;
	struct encode_source_address *source_address;
	struct mrt *mymrt;
	char *mydata;
	int length;

	mymrt=mrtdata;
	mydata=(char *)(pim_send_buf+sizeof(struct ip)+sizeof(struct pim));
	unicast_option=(struct encode_unicast_option *)mydata;
	unicast_option->address_family=1;//协议族是ipv4
	unicast_option->encode_type=0;
	mydata=mydata+sizeof(struct encode_unicast_option);
	jpheader=(struct pim_join_prune_header *)mydata;
	jpheader->upstream=mymrt->upstream;
	jpheader->reserved=0;
	jpheader->num_groups=0;
	jpheader->num_groups=1;
	jpheader->holdtime=0;
	jpheader->holdtime=holdtime;
	
	mydata=mydata+sizeof(struct pim_join_prune_header);
	jpgroup=(struct pim_join_prune_group *)mydata;
	jpgroup->group_address.address_family=1;
	jpgroup->group_address.encode_type=0;
	jpgroup->group_address.reserved=0;
	jpgroup->group_address.mask_len=32;
	jpgroup->group_address.group_address=mymrt->group;

	mydata=mydata+sizeof(struct pim_join_prune_group);
	if(packet_type==PIM_JOIN)
	{
		jpgroup->join_number=htons(1);
		jpgroup->prune_number=htons(0);
	}
	else
	{
		jpgroup->join_number=htons(0);
		jpgroup->prune_number=htons(1);
	}
	source_address=(struct encode_source_address *)mydata;
	source_address->address_family=1;
	source_address->encode_type=0;
	source_address->reserved=0;
	source_address->mask_len=32;
	source_address->source_address=mymrt->source;

	length=sizeof(struct encode_unicast_option)+sizeof(struct pim_join_prune_header)+sizeof(struct pim_join_prune_group)+sizeof(struct encode_source_address);
	
	if(pimdm_output(pim_send_buf,myvifs[mymrt->incoming].address,allpimrouters,PIM_JOIN_PRUNE,length)!=1)
	{
		log(LOG_INFO,"Send Pim Join/Prune Packet On %s(%s) Error!",myvifs[mymrt->incoming].name,inet_fmt(myvifs[mymrt->incoming].address,s1));
	}
	else
	{
		log(LOG_INFO,"Send Pim Join/Prune Packet On %s(%s) Success!",myvifs[mymrt->incoming].name,inet_fmt(myvifs[mymrt->incoming].address,s1));
	}
	return;
}
struct pim_neighbor *findneighbor(u_int32 address,vifi_t vif)
{//给定地址和接口，寻找这个地址是否是该接口的邻居
	struct pim_neighbor *result;
	result=myvifs[vif].neighbor;
	while(result!=NULL)
	{
		if(result->address==address)
		{
			return result;
		}
		result=result->next;
	}
	return NULL;
}
void send_join(void *data)
{//发送Join消息的函数
	struct pim_join_data *joindata;
	struct mrt *mymrt;
	joindata=(struct pim_join_data *)data;
	mymrt=find_sg(joindata->source,joindata->group);
	if(mymrt==NULL)
	{
		return;
	}
	else
	{
		timer_clearTimer(mymrt->join_delay_timer_id);
		mymrt->join_delay_timer_id=0;
		pim_join_prune_output(mymrt,PIM_JOIN,0);
	}
}
void prune_time_out(void *data)
{
	struct pim_prune_data *prunedata;
	struct mrt *mymrt;
	prunedata=(struct pim_prune_data *)data;
	mymrt=find_sg(prunedata->source,prunedata->group);
	log(LOG_INFO,"Prune timeout,Forwarding multicast data again!");
	if(mymrt==NULL)
	{//发送Prune消息
		return;
	}
	else
	{
		mymrt->prune_timer_id[prunedata->vif]=0;
		mymrt->outvif[prunedata->vif]=1;
		change_mfc(igmpsocket,mymrt->source,mymrt->group,mymrt->incoming);
		check_pim_state(ADD_LEAF);
		print_sg();
	}
}
void set_prune(void *data)
{//在prune计时器超时后，执行操作
	struct pim_prune_data *prunedata;
	struct mrt *mymrt;
	log(LOG_INFO,"Begin Prune");
	prunedata=(struct pim_prune_data *)data;
	mymrt=find_sg(prunedata->source,prunedata->group);
	log(LOG_INFO,"Prune source:%s Group:%s",inet_fmt(prunedata->source,s1),inet_fmt(prunedata->group,s2));
	if(mymrt==NULL)
	{
		return;
	}
	else
	{
		mymrt->prune_delay_timer_id[prunedata->vif]=0;
		mymrt->outvif[prunedata->vif]=0;
		change_mfc(igmpsocket,mymrt->source,mymrt->group,mymrt->incoming);
		check_pim_state(DEL_LEAF);
		log(LOG_INFO,"miss,handle set the prune");
		mymrt->prune_timer_id[prunedata->vif]=timer_setTimer(5,prune_time_out,prunedata);
	}
}
void schedule_join(struct mrt *mymrt,u_int32 dst)
{//设置join计时器
	struct pim_join_data *joindata;
	joindata=(struct pim_join_data *)malloc(sizeof(struct pim_join_data));
	joindata->source=mymrt->source;
	joindata->group=mymrt->group;
	joindata->destination=dst;
	mymrt->join_delay_timer_id=timer_setTimer(1,send_join,joindata);
}
void schedule_prune(struct mrt *mrtdata,vifi_t vif,int holdtime)
{//设置prune计时器
	struct pim_prune_data *prunedata;
	struct mrt *mymrt;
	
	mymrt=mrtdata;
	prunedata=(struct pim_prune_data *)malloc(sizeof(struct pim_prune_data));
	prunedata->source=mymrt->source;
	prunedata->group=mymrt->group;
	prunedata->destination=mymrt->upstream;
	prunedata->vif=vif;
	prunedata->holdtime=holdtime;
	mymrt->prune_delay_timer_id[vif]=timer_setTimer(PIM_PRUNE_DELAY,set_prune,prunedata);

	log(LOG_INFO,"schedule_prune!!!");
}
void receive_join_prune(u_int32 src,u_int32 dst,char *data,int length)
{//收到join或prune
	int unicast_address;
	struct encode_group_address group_address;
	struct encode_source_address *source_address;
	vifi_t vif;
	struct pim_neighbor neighbor;
	struct pim_join_prune_header *jp_header;
	struct pim_join_prune_group *jp_group_header;
	struct mrt *mymrt;
	char *mydata;
	int num_groups;
	int holdtime;
	int join_number;
	int prune_number;
	int i,j,k;

	vif=find_vif_direct(src);
	if(vif==0)
	{
		return;
	}
	if(valid_packet(src,data,length)==0)
	{
		return;
	}
	mydata=data+sizeof(struct pim)+sizeof(struct encode_unicast_option);
	jp_header=(struct pim_join_prune_header *)mydata;
	unicast_address=jp_header->upstream;
	num_groups=jp_header->num_groups;
	holdtime=jp_header->holdtime;
	if(num_groups==0)
	{
		return;
	}

	log(LOG_INFO,"Receive PIM Join/Prune from %s to %s",inet_fmt(src,s1),inet_fmt(dst,s2));
	mydata=mydata+sizeof(struct pim_join_prune_header);
	log(LOG_INFO,"Receive Upstream interface:%s",inet_fmt(unicast_address,s1));
	if((unicast_address!=myvifs[vif].address)&&(unicast_address!=INADDR_ANY_N))
	{
		//我不是这条消息的接收者
		//如果是join，如果此时我有延迟发送join消息的计划，则取消发送join的计划
		//如果是prune，此时我还要接收的话，延迟一段时间后发送join消息
		if(findneighbor(unicast_address,vif)==NULL)
		{//看接收方是不是我的PIM邻居
			return;
		}log(LOG_INFO,"Num of groups:%d",num_groups);
		for(i=0;i<num_groups;i++)
		{
			jp_group_header=(struct pim_join_prune_group *)mydata;
			mydata=mydata+sizeof(struct pim_join_prune_group);
			group_address=jp_group_header->group_address;
			join_number=ntohs(jp_group_header->join_number);
			prune_number=ntohs(jp_group_header->prune_number);
log(LOG_INFO,"Receive Group number:%d Join number:%d Prune number:%d",inet_fmt(group_address.group_address,s1),join_number,prune_number);
			if(!IN_MULTICAST(ntohl(group_address.group_address)))
			{
				mydata=mydata+(join_number+prune_number)*sizeof(struct encode_source_address);
				continue;
			}
			for(j=0;j<join_number;j++)
			{
				source_address=(struct encode_source_address *)mydata;
log(LOG_INFO,"Receive Source_address:%s",inet_fmt(source_address->source_address,s1));
				if(!inet_valid_host(source_address->source_address))
				{
					mydata=mydata+sizeof(struct encode_source_address);
					continue;
				}
				mymrt=find_sg(source_address->source_address,group_address.group_address);
				if(mymrt==NULL)
				{//没有该组播路由表项，则不处理
					mydata=mydata+sizeof(struct encode_source_address);
					continue;
				}
				if(mymrt->join_delay_timer_id!=0)
				{//如果有join延迟的话，取消置为0即可
					log(LOG_INFO,"Join form %s for %s,Canceling delay join",inet_fmt(source_address->source_address,s1),inet_fmt(group_address.group_address,s2));
					timer_clearTimer(mymrt->join_delay_timer_id);
					mymrt->join_delay_timer_id=0;
				}
				mydata=mydata+(join_number+prune_number)*sizeof(struct encode_source_address);
			}
			for(k=0;k<prune_number;k++)
			{
				source_address=(struct encode_source_address *)mydata;
log(LOG_INFO,"Receive Source_address:%s",inet_fmt(source_address->source_address,s1));
				if(!inet_valid_host(source_address->source_address))
				{
					mydata=mydata+sizeof(struct encode_source_address);
					continue;
				}
				mymrt=find_sg(source_address->source_address,group_address.group_address);
				if(mymrt==NULL)
				{//路由表是空的情况下,不处理
					mydata=mydata+sizeof(struct encode_source_address);
					continue;
				}
				if(mymrt->outnumber!=0)
				{
					if(mymrt->upstream!=mymrt->source)
					{
						log(LOG_INFO,"We are receiving Src %s Group %s,Scheduling and send Join",inet_fmt(source_address->source_address,s1),inet_fmt(group_address.group_address,s2));
						schedule_join(mymrt,unicast_address);
					}
					mydata=mydata+sizeof(struct encode_source_address);
					continue;
				}
				mydata=mydata+(join_number+prune_number)*sizeof(struct encode_source_address);
			}
		}
		return;
	}
	else
	{//我是Join/Prune报文的接收方
		//收到Join报文，取消我的延迟剪枝计时器
		//收到Prune报文，回送Prune报文，启动计时器，超时后剪枝
log(LOG_INFO,"Num of groups:%d",num_groups);
		for(i=0;i<num_groups;i++)
		{
			jp_group_header=(struct pim_join_prune_group *)mydata;
			mydata=mydata+sizeof(struct pim_join_prune_group);
			group_address=jp_group_header->group_address;
			join_number=ntohs(jp_group_header->join_number);
			prune_number=ntohs(jp_group_header->prune_number);
log(LOG_INFO,"Receive Group address:%s Join number:%d Prune number:%d",inet_fmt(group_address.group_address,s1),join_number,prune_number);
			if(!IN_MULTICAST(ntohl(group_address.group_address)))
			{
				mydata=mydata+(join_number+prune_number)*sizeof(struct encode_source_address);
				continue;
			}
			for(j=0;j<join_number;j++)
			{
				source_address=(struct encode_source_address *)mydata;
log(LOG_INFO,"Join Message,Receive Source_address:%s",inet_fmt(source_address->source_address,s1));
				if(!inet_valid_host(source_address->source_address))
				{
					mydata=mydata+sizeof(struct encode_source_address);
					continue;
				}
				mymrt=find_sg(source_address->source_address,group_address.group_address);
				if(mymrt==NULL)
				{//没有该组播路由表项，则不处理
					mydata=mydata+sizeof(struct encode_source_address);
					continue;
				}
				if(mymrt->prune_delay_timer_id[vif]!=0)
				{//如果有join延迟的话，取消置为0即可
					log(LOG_INFO,"Join form %s for %s,Canceling delay prune",inet_fmt(source_address->source_address,s1),inet_fmt(group_address.group_address,s2));
					timer_clearTimer(mymrt->prune_delay_timer_id[vif]);
					mymrt->prune_delay_timer_id[vif]=0;
				}
				mydata=mydata+(join_number+prune_number)*sizeof(struct encode_source_address);
			}
			for(k=0;k<prune_number;k++)
			{
				source_address=(struct encode_source_address *)mydata;
log(LOG_INFO,"Prune message,Receive Source_address:%s",inet_fmt(source_address->source_address,s1));
				if(!inet_valid_host(source_address->source_address))
				{
					mydata=mydata+sizeof(struct encode_source_address);
					continue;
				}
				mymrt=find_sg(source_address->source_address,group_address.group_address);
				if(mymrt==NULL)
				{//路由表是空的情况下,不处理
					mydata=mydata+sizeof(struct encode_source_address);
					continue;
				}
				if(mymrt->upstream!=mymrt->source)
				{
					log(LOG_INFO,"We are receiving Src %s Group %s,Scheduling and echo Prune",inet_fmt(source_address->source_address,s1),inet_fmt(group_address.group_address,s2));
					bcopy(data,pim_send_buf+sizeof(struct ip),length);
					if(pimdm_output(pim_send_buf,myvifs[vif].address,allpimrouters,PIM_JOIN_PRUNE,length-sizeof(struct pim))!=1)
					{
						log(LOG_INFO,"Echo Prune Packet error!");
					}
					schedule_prune(mymrt,vif,holdtime);
				}
				mydata=mydata+(join_number+prune_number)*sizeof(struct encode_source_address);
			}
		}
	}
	return;
}
//############################################################
//处理Assert报文
//Handle Assert
//############################################################
int assert_compare(u_int32 local_preference,u_int32 local_metric,u_int32 local_address,u_int32 remote_preference,u_int32 remote_metric,u_int32 remote_address)
{//如果前者胜，返回1，否则返回0
    if (remote_preference > local_preference)
	return 1;
    if (remote_preference < local_preference)
	return 0;
    if (remote_metric > local_metric)
	return 1;
    if (remote_metric < local_metric)
	return 0;
    if (ntohl(local_address) > ntohl(remote_address))
	return 1;
    return 0;
}
void pim_assert_output(u_int32 source,u_int32 group,vifi_t vif,u_int32 preference,u_int32 metric)
{//发送assert
	char *mydata;
	struct encode_unicast_option *unicast_option;
	struct encode_group_address *group_address;
	u_int32 *mypreference;
	u_int32 *mymetric;
	u_int32 *unicast_address;
	int length;

	mydata=(char *)(pim_send_buf+sizeof(struct ip)+sizeof(struct pim));
	group_address=(struct encode_group_address *)mydata;
	group_address->address_family=1;
	group_address->encode_type=0;
	group_address->reserved=0;
	group_address->mask_len=32;
	group_address->group_address=group;
	
	mydata=mydata+sizeof(struct encode_group_address);
	unicast_option=(struct encode_unicast_option *)mydata;
	unicast_option->address_family=1;//协议族是ipv4
	unicast_option->encode_type=0;
	mydata=mydata+sizeof(struct encode_unicast_option);
	unicast_address=(u_int32 *)mydata;
	*unicast_address=source;

	mydata=mydata+sizeof(u_int32);
	mypreference=(u_int32 *)mydata;
	*mypreference=preference;
	
	mydata=mydata+sizeof(u_int32);
	mymetric=(u_int32 *)mydata;
	*mymetric=metric;

	length=sizeof(struct encode_group_address)+sizeof(struct encode_group_address)+2*sizeof(u_int32);
	if(pimdm_output(pim_send_buf,myvifs[vif].address,allpimrouters,PIM_ASSERT,length)!=1)
	{
		log(LOG_INFO,"Send Pim Assert Packet On %s(%s) Error!",myvifs[vif].name,inet_fmt(myvifs[vif].address,s1));
	}
	else
	{
		log(LOG_INFO,"Send Pim Assert Packet On %s(%s) Success!",myvifs[vif].name,inet_fmt(myvifs[vif].address,s1));
	
	}	
	return;
}
void active_forward(void *arg)
{//自己不是断言的胜利者，但一段时间过去了，继续转发报文
	vifi_t *vif;
	struct mrt *mymrt;
	vif=arg;
	
	for(mymrt=themrt;mymrt!=NULL;mymrt=mymrt->next)
	{
		mymrt->outvif[(int)vif]=1;
		change_mfc(igmpsocket,mymrt->source,mymrt->group,mymrt->incoming);
	}
	check_pim_state(ADD_LEAF);
}
void receive_assert(u_int32 src,u_int32 dst,char *data,int length)
{//
	vifi_t vif;
	vifi_t *vifdata;
	u_int32 *unicast_address;
	struct encode_group_address *group_address;
	u_int32 source,group;
	struct mrt *mymrt;
	char *mydata;
	u_int32 *preference;
	u_int32 *metric;

	vif=find_vif_direct(src);
	if(vif==0)
	{
		return;
	}
	if(valid_packet(src,data,length)==0)
	{
		return;
	}
	mydata=data+sizeof(struct pim);
	group_address=(struct encode_group_address *)mydata;
	group=group_address->group_address;
	
	mydata=mydata+sizeof(struct encode_group_address)+sizeof(struct encode_unicast_option);
	unicast_address=(u_int32 *)mydata;
	source=*unicast_address;

	mydata=mydata+sizeof(u_int32);
	preference=(u_int32 *)mydata;

	mydata=mydata+sizeof(u_int32);
	metric=(u_int32 *)mydata;

	log(LOG_INFO,"Receive PIM Assert from %s to %s",inet_fmt(src,s1),inet_fmt(dst,s2));
	mymrt=find_sg(source,group);
	if(mymrt==NULL)
	{
		return;
	}
	if(vif==mymrt->incoming)
	{
		if(source==mymrt->upstream)
		{
			return;
		}
		else
		{
			if(assert_compare(mymrt->preference,mymrt->metric,mymrt->upstream,*preference,*metric,source)==1)
			{
				return;
			}
			else
			{
				log(LOG_INFO,"PIM Assert!:Change upstream!");
				mymrt->preference=*preference;
				mymrt->metric=*metric;
				mymrt->upstream=source;
				return;
			}
		}
	}
	else
	{
		if(mymrt->outvif[vif]==0)
		{
			return;
		}
		if(assert_compare(mymrt->preference,mymrt->metric,mymrt->upstream,*preference,*metric,source)==1)
		{
			pim_assert_output(source,group,vif,mymrt->preference,mymrt->metric);
			return;
		}
		else
		{
			vifdata=(vifi_t *)malloc(sizeof(vifi_t));
			*vifdata=vif;
			mymrt->prune_delay_timer_id[vif]=timer_setTimer(PIM_ASSERT_TIMER,active_forward,vifdata);
			//changeinterface();将该接口置为不转发状态
		}
	}
}
//############################################################
//处理Graft报文
//Handle Graft
//############################################################

void receive_pim_graft(u_int32 src,u_int32 dst,char *data,int length)
{//
	u_int32 unicast_address;
	struct encode_group_address group_address;
	struct encode_source_address *source_address;
	vifi_t vif;
	struct pim_neighbor neighbor;
	struct pim_join_prune_header *graft_header;
	struct pim_join_prune_group *graft_group_header;
	int i,j;
	int join_number;
	char *mydata,*temp;
	int num_groups;
	struct mrt *mymrt;

	temp=data;

	vif=find_vif_direct(src);
	if(vif==0)
	{
		return;
	}
	if(valid_packet(src,data,length)==0)
	{
		return;
	}
	mydata=data+sizeof(struct pim)+sizeof(struct encode_unicast_option);
	graft_header=(struct pim_join_prune_header *)mydata;
	unicast_address=graft_header->upstream;
	num_groups=graft_header->num_groups;
	if(num_groups==0)
	{
		return;
	}

	mydata=mydata+sizeof(struct pim_join_prune_header);
	for(i=0;i<num_groups;i++)
	{
		graft_group_header=(struct pim_join_prune_group *)mydata;
		mydata=mydata+sizeof(struct pim_join_prune_group);
		group_address=graft_group_header->group_address;
		join_number=ntohs(graft_group_header->join_number);
		if(!IN_MULTICAST(ntohl(group_address.group_address)))
		{
			mydata=mydata+(join_number)*sizeof(struct encode_source_address);
			continue;
		}
		for(j=0;j<join_number;j++)
		{
			source_address=(struct encode_source_address *)mydata;
			if(!inet_valid_host(source_address->source_address))
			{
				mydata=mydata+sizeof(struct encode_source_address);
				continue;
			}
			log(LOG_INFO,"Receive Graft form %s for group %s,Forwarding data in this interface",inet_fmt(source_address->source_address,s1),inet_fmt(group_address.group_address,s2));
			bcopy(temp,pim_send_buf+sizeof(struct ip),length);
			pimdm_output(pim_send_buf,myvifs[vif].address,src,PIM_GRAFT_ACK,length-sizeof(struct pim));
			mymrt=find_sg(source_address->source_address,group_address.group_address);
			if(mymrt==NULL)
			{//没有该组播路由表项，则不处理
				mydata=mydata+sizeof(struct encode_source_address);				
				continue;
			}
			
			if(mymrt->prune_delay_timer_id[vif]!=0)
			{
				timer_clearTimer(mymrt->prune_delay_timer_id[vif]);
				mymrt->prune_delay_timer_id[vif]=0;
			}
			if(mymrt->outvif[vif]==0)
			{
				mymrt->outvif[vif]=1;
				change_mfc(igmpsocket,mymrt->source,mymrt->group,mymrt->incoming);
				//bcopy(data,pim_send_buf+sizeof(struct ip),length);
			}
			//pimdm_output(pim_send_buf,myvifs[vif].address,src,PIM_GRAFT_ACK,length-sizeof(struct pim));
		}
		mydata=mydata+sizeof(struct encode_source_address);
	}
}
void receive_pim_graft_ack(u_int32 src,u_int32 dst,char *data,int length)
{//
	vifi_t vif;
	char *mydata;
	struct pim_join_prune_header *graft_header;
	struct pim_join_prune_group *graft_group_header;
	u_int32 unicast_address;
	struct encode_group_address group_address;
	struct encode_source_address *source_address;
	int num_groups;
	int join_number;
	int i,j;
	struct mrt *mymrt;

	vif=find_vif_direct(src);
	if(vif==0)
	{
		return;
	}
	if(valid_packet(src,data,length)==0)
	{
		return;
	}
	mydata=data+sizeof(struct pim)+sizeof(struct encode_unicast_option);
	graft_header=(struct pim_join_prune_header *)mydata;
	unicast_address=graft_header->upstream;
	num_groups=graft_header->num_groups;
	if(num_groups==0)
	{
		return;
	}

	mydata=mydata+sizeof(struct pim_join_prune_header);
	for(i=0;i<num_groups;i++)
	{
		graft_group_header=(struct pim_join_prune_group *)mydata;
		mydata=mydata+sizeof(struct pim_join_prune_group);
		group_address=graft_group_header->group_address;
		join_number=graft_group_header->join_number;
		if(!IN_MULTICAST(ntohl(group_address.group_address)))
		{
			mydata=mydata+(join_number)*sizeof(struct encode_source_address);
			continue;
		}
		for(j=0;j<join_number;j++)
		{
			source_address=(struct encode_source_address *)mydata;
			if(!inet_valid_host(source_address->source_address))
			{
				mydata=mydata+sizeof(struct encode_source_address);
				continue;
			}
			mymrt=find_sg(source_address->source_address,group_address.group_address);
			if(mymrt==NULL)
			{//没有该组播路由表项，则不处理
				mydata=mydata+sizeof(struct encode_source_address);
				continue;
			}
			if(mymrt->graft_timer_id!=0)
			{
				log(LOG_INFO,"Receive PIM Graft ack from %s to %s,Graft success",inet_fmt(src,s1),inet_fmt(src,s2));
				timer_clearTimer(mymrt->graft_timer_id);
				mymrt->graft_timer_id=0;
			}
		}
	}
}
void pim_graft_output(struct mrt *mrtdata)
{//
	struct mrt *mymrt;
	struct pim_join_prune_header *graftheader;
	struct pim_join_prune_group *graftgroup;
	struct encode_source_address *source_address;
	struct encode_unicast_option *unicast_option;
	int length;
	char *mydata;

	mymrt=mrtdata;
	mydata=(char *)(pim_send_buf+sizeof(struct ip)+sizeof(struct pim));
	unicast_option=(struct encode_unicast_option *)mydata;
	unicast_option->address_family=1;//协议族是ipv4
	unicast_option->encode_type=0;
	
	mydata=mydata+sizeof(struct encode_unicast_option);
	graftheader=(struct pim_join_prune_header *)mydata;
	graftheader->upstream=mymrt->upstream;
	graftheader->reserved=0;
	graftheader->num_groups=1;
	graftheader->holdtime=0;
	
	mydata=mydata+sizeof(struct pim_join_prune_header);
	graftgroup=(struct pim_join_prune_group *)mydata;
	graftgroup->group_address.address_family=1;
	graftgroup->group_address.encode_type=0;
	graftgroup->group_address.reserved=0;
	graftgroup->group_address.mask_len=32;
	graftgroup->group_address.group_address=mymrt->group;

	mydata=mydata+sizeof(struct pim_join_prune_group);
	graftgroup->join_number=htons(1);
	graftgroup->prune_number=0;
	
	source_address=(struct encode_source_address *)mydata;
	source_address->address_family=1;
	source_address->encode_type=0;
	source_address->reserved=0;
	source_address->mask_len=32;
	source_address->source_address=mymrt->source;

	length=sizeof(struct encode_unicast_option)+sizeof(struct pim_join_prune_header)+sizeof(struct pim_join_prune_group)+sizeof(struct encode_source_address);
	if(pimdm_output(pim_send_buf,myvifs[mymrt->incoming].address,mymrt->upstream,PIM_GRAFT,length)!=1)
	{
		log(LOG_INFO,"Send Pim Graft Packet On %s(%s) Error!",myvifs[mymrt->incoming].name,inet_fmt(myvifs[mymrt->incoming].address,s1));
	}
	else
	{
		log(LOG_INFO,"Send Pim Graft Packet On %s(%s) Success!",myvifs[mymrt->incoming].name,inet_fmt(myvifs[mymrt->incoming].address,s1));
	}	
}
void plan_graft(void *arg)
{//
	struct mrt *mymrt,*thismrt,*mrtdata;
	mymrt=(struct mrt *)arg;
	thismrt=find_sg(mymrt->source,mymrt->group);
	if(mymrt->upstream==mymrt->source)
	{
		return;
	}
	if(thismrt!=NULL)
	{
		pim_graft_output(mymrt);
		mymrt->graft_timer_id=timer_setTimer(PIM_GRAFT_TIMER,plan_graft,mymrt);
	}
}
