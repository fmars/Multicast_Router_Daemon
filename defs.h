#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <ctype.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h> 
#include <string.h> 
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/igmp.h>
#include <osreldate.h>
#define rtentry kernel_rtentry
#include <net/route.h>
#include <netinet/ip_mroute.h>
#include <strings.h>
#include <netinet/pim.h>
#include<sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include<netinet/in.h>
#include<stdio.h>
#include<unistd.h>

typedef u_int   u_int32;
typedef u_short u_int16;
typedef u_char  u_int8;

#include "vif.h"
#include "pimdm.h"
#include "mrt.h"

#ifndef __P
#ifdef __STDC__
#define __P(x)  x
#else
#define __P(x)  ()
#endif
#endif
#define INADDR_ANY_N 0x00000000
typedef void (*cfunc_t) __P((void *));
typedef void (*ihfunc_t) __P((int, fd_set *));//往fd_set中注册函数需要的提前声明

#define IGMP_MEMBERSHIP_QUERY 0x11
#define IGMP_V1_MEMBERSHIP_REPORT 0x12
#define IGMP_V2_MEMBERSHIP_REPORT 0x16
#define IGMP_V2_LEAVE_GROUP 0x17

extern char		*igmp_receive_buf;
extern char		*igmp_send_buf;
extern char		*pim_receive_buf;
extern char		*pim_send_buf;
extern int		igmpsocket;
extern int		pimsocket;
extern u_int32		allhosts;
extern u_int32		allrouters;
extern u_int32 		allpimrouters;
extern struct myvif	myvifs[32];
extern vifi_t maxvif;
extern struct mrt *themrt;

extern int		udp_socket;
extern int		routesocket;

extern char		s1[];
extern char		s2[];
extern char		s3[];
extern char		s4[];

//IGMP协议所用的一些常数
#define IGMP_Query_Time 10	//IGMP查询器的查询周期是125秒
#define IGMP_Robustness 2 //IGMP特定查询报文的发送次数是2
#define IGMP_ResponseTime 100 //IGMP查询报文中携带的最大响应时间，规定了主机需发送报告报文的最短时间，默认值是100，表示100/10=10秒
#define IGMP_Querier_Timeout 310 //当过去了310秒后，如果仍然没有组查询报文，则本机重新成为查询器


/* callout.c */
extern void     callout_init      __P((void));
extern void     free_all_callouts __P((void));
extern void     age_callout_queue __P((int));
extern int      timer_nextTimer   __P((void));
extern int      timer_setTimer    __P((int, cfunc_t, void *));
extern void     timer_clearTimer  __P((int));
extern int      timer_leftTimer   __P((int));

/* debug.c */
extern char	*packet_kind  __P((u_int proto,u_int type,u_int code));
extern void     log          __P((int, char *, ...));
//extern void     dump_vifs    __P((void));
//extern void     dump_pim_mrt __P((void));

/*mrt.c*/
extern void init_mrt();
extern struct mrt *find_sg(u_int32 source,u_int32 group);
extern int delete_sg(u_int32 source,u_int32 group);
extern void add_sg(u_int32 source,u_int32 group,vifi_t incoming);
extern void print_sg();
extern void add_leaf(vifi_t vif,u_int32 group);
extern void del_leaf(vifi_t vif,u_int32 group);
extern void refresh_mrt(vifi_t vif,int i);

/* igmp.c */
extern void     init_igmp     __P(());
extern void     send_igmp     __P((char *buf, u_int32 src, u_int32 dst,
				   int type, int code, u_int32 group,
				   int datalen));
extern void     send_query(void *v);
extern void     accept_membership_query __P((u_int32 src, u_int32 dst,u_int32 group));
extern void     accept_group_report     __P((u_int32 src,u_int32 group));
extern void     accept_leave_message    __P((u_int32 src,u_int32 group));
extern int      check_grp_membership    __P((struct myvif *v, u_int32 group));
extern void     test_vif	__P(());

/* inet.c */
extern int      inet_cksum        __P((u_int16 *addr, u_int len));
extern int      inet_valid_host   __P((u_int32 naddr));
extern int      inet_valid_mask   __P((u_int32 mask));
extern int      inet_valid_subnet __P((u_int32 nsubnet, u_int32 nmask));
extern char    *inet_fmt          __P((u_int32, char *s));
extern char    *netname           __P((u_int32 addr, u_int32 mask));
extern u_int32  inet_parse        __P((char *s, int n));

/* kern.c */
extern void     k_set_rcvbuf    __P((int socket, int bufsize, int minsize));
extern void     k_hdr_include   __P((int socket, int bool));
extern void     k_set_ttl       __P((int socket, int t));
extern void     k_set_loop      __P((int socket, int l));
extern void     k_set_if        __P((int socket, u_int32 ifa));
extern void     k_join          __P((int socket, u_int32 grp, u_int32 ifa));
extern void     k_leave         __P((int socket, u_int32 grp, u_int32 ifa));
extern void     k_init_pim      __P(());
extern int      k_del_mfc       __P((int socket, u_int32 source,
				     u_int32 group));
extern int      k_chg_mfc       __P((int socket, u_int32 source,
				     u_int32 group, vifi_t iif,
				     vifbitmap_t oifs));
extern void     k_add_vif       __P((int socket, vifi_t vifi, struct myvif *v));
extern void     k_del_vif       __P((int socket, vifi_t vifi));
extern int k_get_sg_cnt(int socket,u_int32 source,u_int32 group);
/* main.c */
extern int      register_input_handler __P((int fd, ihfunc_t func));

/* vif.c */
extern void    init_vifs               __P(());
extern vifi_t  local_address           __P((u_int32 src));
extern vifi_t  find_vif_direct         __P((u_int32 src));
extern vifi_t  find_vif_direct_local   __P((u_int32 src));

/*multicastpacket.c*/
extern int check_incoming(vifi_t vif,u_int32 address);
extern void multicast_packet_error();

/*route.c*/
void init_route();
int get_upstream(int source);
int get_incoming_vif(int source);
