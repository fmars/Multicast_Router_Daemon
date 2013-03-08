struct myvif
{
	u_char	metric;						//该接口的权值
	u_char	threshold;					//该接口转发的最小TTL，用于设置内核选项
	u_int	rate_limit;					//接口的速率限制，用于设置内核选项
	
	u_int32 address;					//接口的IP地址，用于设置内核选项
	u_int32 remote_address;				//远端的IP地址，在设置内核选项和寻找路由时用
	u_int32 subnet;						//子网地址
	u_int32 subnet_mask;				//子网掩码
	u_int32 subnet_broadcast;			//子网的广播地址
	char	name[IFNAMSIZ];				//网卡的名字，这里IFNAMSIZ由内核定义，值为16
	struct igmp_group *groups;
	u_int32 querier;					//存储32位的查询器地址，如果是0的话，表示本接口就是查询器
	int igmp_querytimer_id;			//用于IGMP协议周期发送查询报文的计时器
	
	int querier_retry_id;			//在一段时间内没有收到membership报文后，重新成为查询器
	int assert_preference;			//用于断言机制的本地协议优先级
	int assert_metric;				//用于断言机制的本地记录权值
	
	struct	pim_neighbor *neighbor;	//用于记录PIM协议的邻居信息
	//int neighbor_number;				//PIM邻居的数目,作用？
	int is_leaf_router;					//是否是leaf接口(是否连接其他PIM路由器)，是的话为1，不是的话为0
	int is_active;						//该接口是否激活状态？1表示激活，0表示不激活
	
	struct myvif *next; //下一个
};

struct igmp_group
//这个数据结构用于igmp报文查询表示组地址

{
	struct igmp_group *next;//指向下一个
	
	u_int32 address;//组地址
	
	u_long leave_query;//见P157页离开组机制，用于看某个特定组播组是否超时

	int timeout;//超时记录，2此机会，2此没收到相应报告，则删除该组播组
};
struct pim_neighbor
{
	struct pim_neighbor *next;
	u_int32 address;
	int holdtime;
	int timeoutid;
};
