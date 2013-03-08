struct myvif
{
	u_char	metric;						//�ýӿڵ�Ȩֵ
	u_char	threshold;					//�ýӿ�ת������СTTL�����������ں�ѡ��
	u_int	rate_limit;					//�ӿڵ��������ƣ����������ں�ѡ��
	
	u_int32 address;					//�ӿڵ�IP��ַ�����������ں�ѡ��
	u_int32 remote_address;				//Զ�˵�IP��ַ���������ں�ѡ���Ѱ��·��ʱ��
	u_int32 subnet;						//������ַ
	u_int32 subnet_mask;				//��������
	u_int32 subnet_broadcast;			//�����Ĺ㲥��ַ
	char	name[IFNAMSIZ];				//���������֣�����IFNAMSIZ���ں˶��壬ֵΪ16
	struct igmp_group *groups;
	u_int32 querier;					//�洢32λ�Ĳ�ѯ����ַ�������0�Ļ�����ʾ���ӿھ��ǲ�ѯ��
	int igmp_querytimer_id;			//����IGMPЭ�����ڷ��Ͳ�ѯ���ĵļ�ʱ��
	
	int querier_retry_id;			//��һ��ʱ����û���յ�membership���ĺ����³�Ϊ��ѯ��
	int assert_preference;			//���ڶ��Ի��Ƶı���Э�����ȼ�
	int assert_metric;				//���ڶ��Ի��Ƶı��ؼ�¼Ȩֵ
	
	struct	pim_neighbor *neighbor;	//���ڼ�¼PIMЭ����ھ���Ϣ
	//int neighbor_number;				//PIM�ھӵ���Ŀ,���ã�
	int is_leaf_router;					//�Ƿ���leaf�ӿ�(�Ƿ���������PIM·����)���ǵĻ�Ϊ1�����ǵĻ�Ϊ0
	int is_active;						//�ýӿ��Ƿ񼤻�״̬��1��ʾ���0��ʾ������
	
	struct myvif *next; //��һ��
};

struct igmp_group
//������ݽṹ����igmp���Ĳ�ѯ��ʾ���ַ

{
	struct igmp_group *next;//ָ����һ��
	
	u_int32 address;//���ַ
	
	u_long leave_query;//��P157ҳ�뿪����ƣ����ڿ�ĳ���ض��鲥���Ƿ�ʱ

	int timeout;//��ʱ��¼��2�˻��ᣬ2��û�յ���Ӧ���棬��ɾ�����鲥��
};
struct pim_neighbor
{
	struct pim_neighbor *next;
	u_int32 address;
	int holdtime;
	int timeoutid;
};
