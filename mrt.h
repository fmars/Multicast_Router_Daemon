struct mrt
{
	struct mrt *next;	//ָ����һ���鲥��
	u_int32 source;		//�鲥��Դ��ַ
	u_int32 group;		//�鲥�����ַ
	u_int32 upstream;	//���νӿڵ�ַ
	vifi_t incoming;	//�鲥����Դ�ӿ�
	u_int32 preference;	//���ؽӿڵ����ȼ�
	u_int32 metric;		//���νӿڵ�Ȩֵ

	int join_delay_timer_id;	//���鲥����ӳ�Join��ʱ��id
	int outvif[32];		//���鲥��ĳ��ڽӿ�,ע�⣬��vif=1��ʼ������ÿ���ӿڣ��ɱ�ʾ��outvif[�ӿں�],1��ʾת����0��ʾPrune����֦״̬��
	int outnumber;
	int prune_timer_id[32];
	int prune_delay_timer_id[32];	//��֦��ʱ��ʱ��
	int graft_timer_id;	//�޽Ӽ�ʱ��
	int timeout_id;
	int check_timerid;
};
struct mrt_timeout_data
{
	int source;
	int group;
};
