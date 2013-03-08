#define PIM_HELLO_PERIOD 30 //Hello报文的发送周期为30秒

#define PIM_HELLO_HOLDTIME 3.5*PIM_HELLO_PERIOD
#define PIM_JOIN_DELAY 2 //延迟发送Join消息的时间为0到2.5秒
#define PIM_PRUNE_DELAY 3 //
#define PIM_JOIN 1
#define PIM_PRUNE 2
#define PIM_ASSERT_TIMER 180	//断言失败的一方在180秒的时间内都不会再发送报文
#define PIM_GRAFT_TIMER 3	//发送Graft报文后，如果在3秒内没有收到Graft-ack的话，将重新发送该报文
#define PIM_PRUNE_HOLD_TIME 30
#define PIM_SG_TIMEOUT 298
#define PIM_SG_CHECK   30
#define ADD_LEAF 0
#define DEL_LEAF 1
#define ADD_NEIGHBOR 2
#define DEL_NEIGHBOR 3
struct encode_unicast_option
{
	u_char address_family;
	u_char encode_type;
};
struct encode_group_address
{
	char address_family;
	char encode_type;
	char reserved;
	char mask_len;
	u_int32 group_address;
};
struct encode_source_address
{
	char address_family;
	char encode_type;
	char reserved;
	char mask_len;
	u_int32 source_address;
};
struct pim_hello_header
{
	short option_type;
	short option_length;
};
struct pim_join_prune_header
{
	u_int32 upstream;
	char reserved;
	char num_groups;
	short holdtime;
};
struct pim_join_prune_group
{
	struct encode_group_address group_address;
	short join_number;
	short prune_number;
};
struct vif_data
{
	vifi_t vif;
	u_int32 group;	
};

struct pim_hello_output_data
{
	vifi_t vif;
	int holdtime;
	int regular;
};
struct pim_join_data
{
	u_int32 source;
	u_int32 group;
	u_int32 destination;
};
struct pim_prune_data
{
	u_int32 source;
	u_int32 group;
	u_int32 destination;
	vifi_t vif;
	int holdtime;
};
struct pim_graft_data
{
	vifi_t vif;
	u_int32 destination;
	u_int32 group;
};
