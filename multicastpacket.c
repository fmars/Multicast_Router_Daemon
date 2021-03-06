#include "defs.h"
struct multicast_error_data
{//IGMP控制信息，不需理解
	u_long temp1;
	u_long temp2;
	u_char message;	/*为1表示没有MFC，2表示MFC错误*/
	u_char temp3;
	u_char vif;/*收到这条组播报文的接口号*/
	u_char temp4;
	struct in_addr source;/*收到的源，即组播的源*/
	struct in_addr destination;/*组播地址*/
};
int check_incoming(vifi_t vif,u_int32 address)
{//确认某个接口是否靠近组播源，如果是的话返回1，不是的话返回0
	u_int32 source;
	vifi_t rightvif;

	source=address;
	rightvif=get_incoming_vif(source);
	if(rightvif==vif)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}
void multicast_packet_error()
{//当收到一个组播报文的时候，如果内核没有相应的MFC或者是MFC错误，则返回一条特殊的IGMP消息，称为IGMP控制消息
	struct multicast_error_data *mydata;
	u_int32 source,group;
	vifi_t vif;
	struct mrt *mymrt;
	mydata=(struct multicast_error_data *)igmp_receive_buf;
	if(mydata->message==1)
	{//MFC里没有这个组播报文的转发表项，则建立
		if(check_incoming(mydata->vif,mydata->source.s_addr)==1)
		{//如果这是从靠近组播源的接口收到的话
			source=mydata->source.s_addr;
			group=mydata->destination.s_addr;
		
			log(LOG_INFO,"We Receive a new multicast packet from %s group %s,create the (S,G) entry and forward this packet!",inet_fmt(source,s1),inet_fmt(group,s1));
			add_sg(source,group,mydata->vif);/*建立这个组播表项目*/
//log(LOG_INFO,"Receive multicast packet from %s",myvifs[mydata->vif].name);
			change_mfc(igmpsocket,source,group,mydata->vif);
		}
		else
		{
			log(LOG_INFO,"This new multicast packet is from wrong interface!Ignore it");
		}
	}
	if(mydata->message==2)
	{
		source=mydata->source.s_addr;
		group=mydata->destination.s_addr;
		vif=mydata->vif;

		mymrt=find_sg(source,group);
		if(mymrt==NULL)
		{
			return;
		}
		log(LOG_INFO,"Receive multicast packet from %s with source %s",myvifs[vif].name,inet_fmt(source,s1));
		print_sg();
		if(mymrt->outnumber!=0)
		{
			log(LOG_INFO,"Send Assert packet!");
			pim_assert_output(source,group,vif,mymrt->preference,mymrt->metric);
		}
	}
}
