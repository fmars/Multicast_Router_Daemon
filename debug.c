//暂时只支持log函数功能 2010.9.8
#include "defs.h"
#include <stdarg.h>//提供不定数目参数的支持
char *
packet_kind(proto, type, code)
    u_int proto, type, code;
{//返回IGMP报文的类型，直接是字符串
    static char unknown[60];

    switch (proto) {
    case IPPROTO_IGMP:
	switch (type) {
	case IGMP_MEMBERSHIP_QUERY:    return "IGMP Membership Query    ";
	case IGMP_V1_MEMBERSHIP_REPORT:return "IGMP v1 Member Report    ";
	case IGMP_V2_MEMBERSHIP_REPORT:return "IGMP v2 Member Report    ";
	case IGMP_V2_LEAVE_GROUP:      return "IGMP Leave message       ";
	    default:
		sprintf(unknown,   "UNKNOWN DVMRP message code = %3d ", code);
		return unknown;
	    }
    default:
	sprintf(unknown,          "UNKNOWN proto =%3d               ", proto);
	return unknown;
    }
}
void log(int severity,char *format, ...)
{//显示信息，使用的时候，log(LOG_INFO,...),...表示的内容和printf用到的一样，第一个参数如果是LOG_INFO，表示正常，如果是LOG_ERR，则是严重错误，直接退出程序
    va_list ap;
    char fmt[200];
    char *msg;
    struct timeval now;
    time_t now_sec;
    struct tm *thyme;
    
    va_start(ap, format);

    vsprintf(&fmt, format, ap);
    va_end(ap);
    msg = fmt;
    
	gettimeofday(&now,NULL);
	now_sec = now.tv_sec;
	thyme = localtime(&now_sec);
	if (severity == LOG_WARNING)
	{
		fprintf(stderr,"%02d:%02d:%02d.%03ld Warning!!--%s\n",thyme->tm_hour,thyme->tm_min,thyme->tm_sec,now.tv_usec/1000,msg);
	}
	else
	{
		fprintf(stderr,"%02d:%02d:%02d.%03ld %s\n",thyme->tm_hour,thyme->tm_min,thyme->tm_sec,now.tv_usec/1000,msg);    
    }
	if (severity == LOG_ERR) exit(-1);
}
