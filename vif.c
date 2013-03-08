#include "defs.h"
struct myvif myvifs[32];
vifi_t maxvif;
int udp_socket;
void active_vifs();
void init_vifs()
//��������õĽӿڣ�ִ�г�ʼ�����񣬴��ں��л�ȡ������ӿ���Ϣ���ڸ����ӿ��Ϸ���PIM���ĺ�IGMP�����Լ���PIM-DMЭ��
{
	vifi_t vif;
	struct myvif *myv;
	maxvif=0;
	int enable=0;//���õĽӿڵ���Ŀ
	int n;//���ڿ��ƶ�ȡ����Ҫ���ƫ�Ƶ�λ��
	struct ifreq *ifstart,*ifend;
	u_int32 address,netmask,subnet;
	short flags;	//���ں˶�ȡ��־λ�����ӿ��Ƿ񼤻�״̬
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
		//����IPv4Э����Ĳ���
		{
			continue;
		}

		address=((struct sockaddr_in *)&ifstart->ifr_addr)->sin_addr.s_addr;
	
		bcopy(ifstart->ifr_name,iftemp.ifr_name,sizeof(iftemp.ifr_name));

		if(ioctl(udp_socket,SIOCGIFFLAGS,(char *)&iftemp)<0)
		//��ýӿڵ�һЩ��Ϣ��־
		{
			log(LOG_INFO,"iotcl error!");
		}
		flags=iftemp.ifr_flags;
		if ((flags&(IFF_LOOPBACK|IFF_MULTICAST))!=IFF_MULTICAST)
		//����ǲ���loopback�����鲥�ӿ�
		{
			continue;
		}

		if (ioctl(udp_socket,SIOCGIFNETMASK,(char *)&iftemp) < 0)
		{
			log(LOG_INFO,"ioctl error!");
		}
		netmask=((struct sockaddr_in *)&iftemp.ifr_addr)->sin_addr.s_addr;
		subnet=address&netmask;
		
		//ʡȥ��ַ���������Ĺ����Լ�����ֵ����Ч�ԣ��Ƿ������ȣ��ļ������
		if(maxvif==32)
		//�ں˶������ֻ��32������֧�֣��������ֻ����32������ӿ�
		{
			log(LOG_INFO,"Two many interface��");
			break;
		}
	
		myv++;

		myv->metric=1;
		myv->threshold=1;
		myv->rate_limit=0;
		myv->address=address;
		myv->remote_address=(u_int32)0x00000000;//����
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
			//��������PIM·�����鲥��
			k_join(igmpsocket, allpimrouters,myv->address);
			//��������·�����鲥��
			k_join(igmpsocket, allrouters,myv->address);
			//send_pim_hello();//����PIM��Hello
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
//���������IP��ַ�ͱ���ĳ���ӿڵĵ�ַ��ͬһ�����εģ����ظýӿں�,���򷵻�0
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
			//Ҫ�ҵ���ͬһ�����εģ������Ǳ�����ַ
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
//�鿴������IP��ַ�Ƿ��Ǳ��صĵ�ַ���ǵĻ����ؽӿ����кţ����ǵĻ�����0
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
//���ú�find_vif_direct���ƣ���ͬ��������Ǳ�����ַ��Ҳ���ؽӿں�
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
