#include "defs.h"
struct mrt *themrt;
int packet_number=0;
void init_mrt()
{//初始化
	themrt=(struct mrt *)malloc(sizeof(struct mrt));
	themrt=NULL;
}
void check_pim_state(int judge)
{//在加入/删除PIM邻居或主机时，检查端口状态是否改变
	struct mrt *mymrt;
	
	for(mymrt=themrt;mymrt!=NULL;mymrt=mymrt->next)
	{
		if(mymrt->outnumber==0&&(judge==DEL_LEAF||judge==DEL_NEIGHBOR))
		{
			if(mymrt->upstream!=mymrt->source)
			{
				pim_join_prune_output(mymrt,PIM_PRUNE,mymrt->incoming,PIM_PRUNE_HOLD_TIME);
			}
		}
		if(mymrt->outnumber==1&&(judge==ADD_LEAF||judge==ADD_NEIGHBOR))
		{
			if(mymrt->upstream!=mymrt->source)
			{
				plan_graft(mymrt);
			}
		}
	}
		
	log(LOG_INFO,"checking state!");
}
void handle_sg_timeout(void *arg)
{//组播路由表项超时的处理函数
	struct mrt_timeout_data *mymrt_data;
	mymrt_data=(struct mrt_timeout_data *)arg;
	if(delete_sg(mymrt_data->source,mymrt_data->group)==1)
	{
		log(LOG_INFO,"MRT entry source:%s,group:%s timeout delete it",inet_fmt(mymrt_data->source,s1),inet_fmt(mymrt_data->group,s2));
	}
}
void check_sg(void *arg)
{//
	struct mrt_timeout_data *mymrt_data;
	struct mrt *mymrt;
	int nowdata;
	mymrt_data=(struct mrt_timeout_data *)arg;
	mymrt=find_sg(mymrt_data->source,mymrt_data->group);
	nowdata=k_get_sg_cnt(igmpsocket,mymrt_data->source,mymrt_data->group);
	if(nowdata>packet_number)
	{
		log(LOG_INFO,"!!!!!!!!!!!!!!!!!!!!!Last 30 seconds,we forwarding %d packet",nowdata-packet_number);
		packet_number=nowdata;
		timer_clearTimer(mymrt->timeout_id);
		mymrt->timeout_id=timer_setTimer(PIM_SG_TIMEOUT,handle_sg_timeout,mymrt_data);
	}
	timer_clearTimer(mymrt->check_timerid);
	mymrt->check_timerid=timer_setTimer(PIM_SG_CHECK,check_sg,mymrt_data);
}	
struct mrt *find_sg(u_int32 source,u_int32 group)
{//根据源地址和组播地址返回一个组播路由表项
	struct mrt *mymrt;
	mymrt=themrt;
	while(mymrt!=NULL)
	{
		if((mymrt->source==source)&&(mymrt->group==group))
		{
			return mymrt;
		}
		mymrt=mymrt->next;
	}
	return mymrt;
}
struct mrt *find_sg_group(u_int32 group)
{//根据组播地址返回组播表项指针
	struct mrt *mymrt;
	mymrt=themrt;
	while(mymrt!=NULL)
	{
		if(mymrt->group==group)
		{
			return mymrt;
		}
		mymrt=mymrt->next;
	}
	return mymrt;
}
int delete_sg(u_int32 source,u_int32 group)
{//根据源地址和组播地址删除一个组播表项,1表示成功，0表示不成功
	struct mrt *mymrt,*temp;
	mymrt=themrt;
	while(mymrt!=NULL)
	{
		if((mymrt->source==source)&&(mymrt->group==group))
		{
			if((themrt->source==source)&&(themrt->group==group))
			{
				delete_mfc(igmpsocket,source,group);
				timer_clearTimer(mymrt->check_timerid);
				themrt=themrt->next;
				print_sg();
				return 1;
			}
			delete_mfc(igmpsocket,source,group);
			timer_clearTimer(mymrt->check_timerid);
			temp->next=mymrt->next;
			print_sg();
			return 1;
		}
		temp=mymrt;
		mymrt=mymrt->next;
	}
	print_sg();
	return 0;
}
void add_sg(u_int32 source,u_int32 group,vifi_t incoming)
{//增加一个组播表项
	struct mrt *newmrt;
	struct igmp_group *igmpdata;
	struct mrt_timeout_data *mymrt_data;
	int i,number=0;
	newmrt=(struct mrt *)malloc(sizeof(struct mrt));
	newmrt->source=source;
	newmrt->group=group;
	newmrt->incoming=incoming;
	newmrt->upstream=get_upstream(source);
	log(LOG_INFO,"Upstream is %s",inet_fmt(newmrt->upstream,s1));
	newmrt->preference=0;
	newmrt->metric=0;

	newmrt->join_delay_timer_id=0;
	for(i=1;i<=maxvif;i++)
	{/*一开始，在没有建立邻居关系的情况下，所有接口都是不转发状态
	当有IGMP加入报文或者是PIM的Hello报文的时候，启用为转发状态*/
		newmrt->prune_timer_id[i]=0;
		newmrt->prune_delay_timer_id[i]=0;
		if(i!=incoming)
		{
			if(myvifs[i].neighbor!=NULL)
			{//如果有PIM邻居的话，转发状态
				newmrt->outvif[i]=1;
				number++;
				continue;
			}
			else
			{
				for(igmpdata=myvifs[i].groups;igmpdata!=NULL;igmpdata=igmpdata->next)
				{
					if(igmpdata->address==group)
					{
						log(LOG_INFO,"Out in vif %d",i);
						newmrt->outvif[i]=1;
						number++;
						break;
					}
				}
				if(newmrt->outvif[i]!=1)
				{
					newmrt->outvif[i]=0;
					continue;
				}
			}
		}
		else
		{
			newmrt->outvif[i]=0;
			continue;
		}
	}
	newmrt->outnumber=number;
	newmrt->graft_timer_id=0;
	mymrt_data=(struct mrt_timeout_data *)malloc(sizeof(struct mrt_timeout_data));
	mymrt_data->source=source;
	mymrt_data->group=group;
	newmrt->timeout_id=timer_setTimer(PIM_SG_TIMEOUT,handle_sg_timeout,mymrt_data);
	newmrt->check_timerid=timer_setTimer(PIM_SG_CHECK,check_sg,mymrt_data);
	if(themrt==NULL)
	{
		themrt=newmrt;
	}
	else
	{
		newmrt->next=themrt;
		themrt=newmrt;
	}
	if(newmrt->outnumber!=0)
	{
		if(newmrt->upstream!=source)
		{
			plan_graft(newmrt);
		}
	}
	else
	{
		if(newmrt->upstream!=source)
		{
			pim_join_prune_output(newmrt,PIM_PRUNE,newmrt->incoming,PIM_PRUNE_HOLD_TIME);
		}
	}
	print_sg();
}
void add_leaf(vifi_t vif,u_int32 group)
{//如果一个主机接收组播报文，在相应的端口上记录，添加，然后检查端口状态是否要改变
	struct mrt *mymrt;
	mymrt=find_sg_group(group);
	if(mymrt==NULL)
	{
		return;
	}
	if(mymrt->outvif[vif]!=1)
	{
		mymrt->outvif[vif]=1;
		mymrt->outnumber++;
	}
	change_mfc(igmpsocket,mymrt->source,group,mymrt->incoming);
	check_pim_state(ADD_LEAF);	
	print_sg();
}
void del_leaf(vifi_t vif,u_int32 group)
{//如果一个主机停止接收组播报文，在相应的端口上记录，删除，然后检查端口状态是否要改变
	struct mrt *mymrt;
	mymrt=find_sg_group(group);
	if(mymrt==NULL)
	{
		return;
	}
	mymrt->outvif[vif]=0;
	mymrt->outnumber--;
	change_mfc(igmpsocket,mymrt->source,group,mymrt->incoming);
	check_pim_state(DEL_LEAF);
	print_sg();
}
void refresh_mrt(vifi_t vif,int i)
{//刷心路由表，如果有端口状态变化的话
	struct mrt *mymrt;
	struct igmp_group *igmpdata;
	int temp;
	for(mymrt=themrt;mymrt!=NULL;mymrt=mymrt->next)
	{
		temp=0;
		if(i==ADD_NEIGHBOR)
		{
			if(mymrt->outvif[vif]==0)
			{
				mymrt->outvif[vif]=1;
				mymrt->outnumber++;
				change_mfc(igmpsocket,mymrt->source,mymrt->group,mymrt->incoming);
				check_pim_state(ADD_NEIGHBOR);
			}
		}
		if(i==DEL_NEIGHBOR)
		{
			for(igmpdata=myvifs[i].groups;igmpdata!=NULL;igmpdata=igmpdata->next)
			{
				if(igmpdata->address==mymrt->group)
				{
					temp=1;
					break;
				}
			}
			if(temp!=1)
			{	
				if(myvifs[i].neighbor==NULL)
				{
					mymrt->outvif[vif]=0;
					mymrt->outnumber--;
					change_mfc(igmpsocket,mymrt->source,mymrt->group,mymrt->incoming);
					check_pim_state(DEL_NEIGHBOR);
				}
			}
		}
	}
	print_sg();
}
void print_sg()
{//打印路由表
	struct mrt *mymrt;
	int i;
	mymrt=themrt;
	log(LOG_INFO,"###########PIM DM Routing Table###########");
	if(mymrt==NULL)
	{
		log(LOG_INFO,"NULL");
		return;
	}
	int number;
	while(mymrt!=NULL)
	{
		number=0;
		log(LOG_INFO,"(%s,%s)",inet_fmt(mymrt->source,s1),inet_fmt(mymrt->group,s2));
		log(LOG_INFO,"Upstream interface:%s",myvifs[mymrt->incoming].name);
		log(LOG_INFO,"Downstream interface list:");
		for(i=1;i<=maxvif;i++)
		{
			if(i!=mymrt->incoming)
			{
				if(mymrt->outvif[i]==0&&myvifs[i].neighbor!=NULL)
				{
					log(LOG_INFO,"[PRUNED]%s",myvifs[i].name);
					number++;
				}
				if(mymrt->outvif[i]==1)
				{
					log(LOG_INFO,"%s",myvifs[i].name);
					number++;
				}
			}
		}
		if(number==0)
		{
			log(LOG_INFO,"NULL");
		}
		mymrt=mymrt->next;
	}
	log(LOG_INFO,"#############################################");
}
