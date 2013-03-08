#include "defs.h"
struct myvif myvifs[32];
vifi_t maxvif;
int udp_socket;
void active_vifs();
void init_vifs()
//主程序调用的接口，执行初始化任务，从内核中获取了网络接口信息后，在各个接口上发送PIM报文和IGMP报文以激活PIM-DM协议
{
	vifi_t vif;
	struct myvif *myv;
	maxvif=0;
	int enable=0;//能用的接口的数目
	int n;//用于控制读取网卡要向后偏移的位数
	struct ifreq *ifstart,*ifend;
	u_int32 address,netmask,subnet;
	short flags;	//从内核读取标志位，检查接口是否激活状态
	int vif_max_number=32;
	struct ifconf ifc;
	ifc.ifc_len=vif_max_number*sizeof(struct ifreq);
    	ifc.ifc_buf=calloc(ifc.ifc_len,sizeof(char));

	myv=myvifs;

	if((udp_socket=socket(AF_INET,SOCK_DGRAM,0))<0)
	{
		log(LOG_INFO,"socket create failed");
	}

	log(LOG_INFO,"Get and active interfaces:");

    	while (ifc.ifc_buf) 
    	{
		if (ioctl(udp_socket,SIOCGIFCONF, (char *)&ifc) < 0)
	    		log(LOG_INFO,"ioctl SIOCGIFCONF");
	
		if ((vif_max_number* sizeof(struct ifreq))>=ifc.ifc_len + sizeof(struct ifreq))
	    		break;
	
		vif_max_number*= 2;
		ifc.ifc_len =vif_max_number * sizeof(struct ifreq);
		ifc.ifc_buf = realloc(ifc.ifc_buf, ifc.ifc_len);
    	}

	if(ifc.ifc_buf==NULL)
	{
		log(LOG_INFO,"Get interface information Error!!");
	}

	ifstart=(struct ifreq *)ifc.ifc_buf;
	ifend=(struct ifreq *)(ifc.ifc_buf+ifc.ifc_len);

	for(;ifstart<ifend;ifstart=(struct ifreq *)((char *)ifstart+n))
	{
		struct ifreq iftemp;
		n=ifstart->ifr_addr.sa_len+sizeof(ifstart->ifr_name);
		if(n<sizeof(*ifstart))
		{
			n=sizeof(*ifstart);
		}

		if(ifstart->ifr_addr.sa_family!=AF_INET)
		//不是IPv4协议族的不管
		{
			continue;
		}

		address=((struct sockaddr_in *)&ifstart->ifr_addr)->sin_addr.s_addr;
	
		bcopy(ifstart->ifr_name,iftemp.ifr_name,sizeof(iftemp.ifr_name));

		if(ioctl(udp_socket,SIOCGIFFLAGS,(char *)&iftemp)<0)
		//获得接口的一些信息标志
		{
			log(LOG_INFO,"iotcl error!");
		}
		flags=iftemp.ifr_flags;
		if ((flags&(IFF_LOOPBACK|IFF_MULTICAST))!=IFF_MULTICAST)
		//检查是不是loopback或不是组播接口
		{
			continue;
		}

		if (ioctl(udp_socket,SIOCGIFNETMASK,(char *)&iftemp) < 0)
		{
			log(LOG_INFO,"ioctl error!");
		}
		netmask=((struct sockaddr_in *)&iftemp.ifr_addr)->sin_addr.s_addr;
		subnet=address&netmask;
		
		//省去地址、子网检查的过程以及读出值的有效性（是否重名等）的检查问题
		if(maxvif==32)
		//内核定义最大只有32个网口支持，所以最多只能有32个网络接口
		{
			log(LOG_INFO,"Two many interface？");
			break;
		}
	
		myv++;

		myv->metric=1;
		myv->threshold=1;
		myv->rate_limit=0;
		myv->address=address;
		myv->remote_address=(u_int32)0x00000000;//所有
		myv->subnet=subnet;
		myv->subnet_mask=netmask;
		myv->subnet_broadcast=subnet|~netmask;
		strncpy(myv->name,iftemp.ifr_name,IFNAMSIZ);
		myv->groups=NULL;
		myv->querier=0;
		myv->igmp_querytimer_id=0;
		myv->querier_retry_id=0;
		myv->assert_preference=101;
		myv->assert_metric=1024;
		myv->neighbor=NULL;
		myv->is_leaf_router=1;
		//myv->neighbor_number=0;

		if (!(flags&IFF_UP)) 
		{
			myv->is_active=0;
		}
		else
		{
			myv->is_active=1;
			enable++;
		}
		
		maxvif++;
	}
	if(enable<2)
	{
		log(LOG_INFO,"Only %d interface is enable,we need at leas 2 interface enable",enable);
	}
	
	k_init_pim(igmpsocket);
	
	active_vifs();
}
void active_vifs()
{
	vifi_t vif,*temp;
	struct myvif *myv;
	myv=myvifs;myv++;
	struct vif_data *mydata[maxvif];
	struct pim_hello_output_data *hellodata[maxvif];
	struct mrt *mymrt;
	for(vif=1;vif<=maxvif;myv++,vif++)
	{
		if(myv->is_active==0)
		{
			log(LOG_INFO,"Interface %s is Down",myv->name);
		}
		else
		{
			k_add_vif(igmpsocket,vif,&myvifs[vif]);
			log(LOG_INFO,"Add interface %s(%s)",myv->name,inet_fmt(myv->address,s1));
			//加入所有PIM路由器组播组
			k_join(igmpsocket, allpimrouters,myv->address);
			//加入所有路由器组播组
			k_join(igmpsocket, allrouters,myv->address);
			//send_pim_hello();//发送PIM的Hello
			mydata[vif]=(struct vif_data *)malloc(sizeof(struct vif_data));
			mydata[vif]->vif=vif;
			mydata[vif]->group=0;
			send_query(mydata[vif]);
			
			hellodata[vif]=(struct pim_hello_output_data *)malloc(sizeof(struct pim_hello_output_data));
			hellodata[vif]->vif=vif;
			hellodata[vif]->holdtime=PIM_HELLO_HOLDTIME;
			hellodata[vif]->regular=1;
			pim_hello_output(hellodata[vif]);
		}
	}
}
vifi_t find_vif_direct(u_int32 address)
//如果给定的IP地址和本机某个接口的地址是同一个网段的，返回该接口号,否则返回0
{
	vifi_t vif;
	struct myvif *myv;
	myv=myvifs;
	myv++;
	for(vif=1;vif<=maxvif;myv++,vif++)
	{
		if(myv->is_active==0)
		{
			continue;
		}
		if(myv->address==address)
		{
			//要找的是同一个网段的，但不是本机地址
			return 0;
		}
		if((address&myv->subnet_mask)==myv->subnet&&((myv->subnet_mask==0xffffffff)|(address!=myv->subnet_broadcast)))
		{
			return vif;
		}
	}
	return 0;
}
vifi_t is_local_address(u_int32 address)
//查看给定的IP地址是否是本地的地址，是的话返回接口序列号，不是的话返回0
{
	vifi_t vif;
	struct myvif *myv;
	myv=myvifs;
	myv++;
	for(vif=1;vif<=maxvif;myv++,vif++)
	{
		if(myv->is_active==0)
		{
			continue;
		}
		if(myv->address==address)
		{
			return vif;
		}
	}
	return 0;
}
vifi_t find_vif_direct_local(u_int32 address)
//作用和find_vif_direct类似，不同的是如果是本机地址，也返回接口号
{
	vifi_t vif;
	struct myvif *myv;
	myv=myvifs;
	myv++;
	for(vif=1;vif<=maxvif;myv++,vif++)
	{
		if(myv->is_active==0)
		{
			continue;
		}
		if(myv->address==address)
		{
			return vif;
		}
		if((address&myv->subnet_mask)==myv->subnet&&((myv->subnet_mask==0xffffffff)|(address!=myv->subnet_broadcast)))
		{
			return vif;
		}
	}
	return 0;
}
void test_vif()
{
	struct myvif *myv;
	vifi_t vif;
	struct igmp_group *g;
	myv=myvifs;
	myv++;
	for(vif=1;vif<=maxvif;myv++,vif++)
	{
		//log(LOG_INFO,"Interface number:%d",vif);
		log(LOG_INFO,"Name:%s",myv->name);
		g=myv->groups;
		while(g!=NULL)
		{
			log(LOG_INFO,"\tGroup:%s",inet_fmt(g->address,s1));
			g=g->next;
		}
	}
}
