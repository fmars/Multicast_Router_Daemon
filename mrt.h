struct mrt
{
	struct mrt *next;	//指向下一个组播组
	u_int32 source;		//组播的源地址
	u_int32 group;		//组播的组地址
	u_int32 upstream;	//上游接口地址
	vifi_t incoming;	//组播的来源接口
	u_int32 preference;	//本地接口的优先级
	u_int32 metric;		//上游接口的权值

	int join_delay_timer_id;	//该组播组的延迟Join计时器id
	int outvif[32];		//该组播组的出口接口,注意，从vif=1开始，对于每个接口，可表示成outvif[接口号],1表示转发，0表示Prune（剪枝状态）
	int outnumber;
	int prune_timer_id[32];
	int prune_delay_timer_id[32];	//剪枝延时计时器
	int graft_timer_id;	//嫁接计时器
	int timeout_id;
	int check_timerid;
};
struct mrt_timeout_data
{
	int source;
	int group;
};
