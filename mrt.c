#include "defs.h"
struct mrt *themrt;
int packet_number=0;
void init_mrt()
{//��ʼ��
	themrt=(struct mrt *)malloc(sizeof(struct mrt));
	themrt=NULL;
}
void check_pim_state(int judge)
{//�ڼ���/ɾ��PIM�ھӻ�����ʱ�����˿�״̬�Ƿ�ı�
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
{//�鲥·�ɱ��ʱ�Ĵ�������
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
{//����Դ��ַ���鲥��ַ����һ���鲥·�ɱ���
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
{//�����鲥��ַ�����鲥����ָ��
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
{//����Դ��ַ���鲥��ַɾ��һ���鲥����,1��ʾ�ɹ���0��ʾ���ɹ�
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
{//����һ���鲥����
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
	{/*һ��ʼ����û�н����ھӹ�ϵ������£����нӿڶ��ǲ�ת��״̬
	����IGMP���뱨�Ļ�����PIM��Hello���ĵ�ʱ������Ϊת��״̬*/
		newmrt->prune_timer_id[i]=0;
		newmrt->prune_delay_timer_id[i]=0;
		if(i!=incoming)
		{
			if(myvifs[i].neighbor!=NULL)
			{//�����PIM�ھӵĻ���ת��״̬
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
{//���һ�����������鲥���ģ�����Ӧ�Ķ˿��ϼ�¼�����ӣ�Ȼ����˿�״̬�Ƿ�Ҫ�ı�
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
{//���һ������ֹͣ�����鲥���ģ�����Ӧ�Ķ˿��ϼ�¼��ɾ����Ȼ����˿�״̬�Ƿ�Ҫ�ı�
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
{//ˢ��·�ɱ�������ж˿�״̬�仯�Ļ�
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
{//��ӡ·�ɱ�
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