/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE.mrouted". Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * inet.c,v 3.8.4.1 1997/01/29 19:49:33 fenner Exp
 */
#define C(x)    ((x) & 0xff)

#include "defs.h"


/*
 * Exported variables.
 */
char s1[19];		/* buffers to hold the string representations  */
char s2[19];		/* of IP addresses, to be passed to inet_fmt() */
char s3[19];
char s4[19];


/*
 * Verify that a given IP address is credible as a host address.
 * (Without a mask, cannot detect addresses of the form {subnet,0} or
 * {subnet,-1}.)
 */
int
inet_valid_host(naddr)//�Ƿ�����Ч��������ַ
    u_int32 naddr;
{
    register u_int32 addr;
    
    addr = ntohl(naddr);
    
    return (!(IN_MULTICAST(addr) ||
	      IN_BADCLASS (addr) ||
	      (addr & 0xff000000) == 0));
}

/*
 * Verify that a given netmask is plausible;
 * make sure that it is a series of 1's followed by
 * a series of 0's with no discontiguous 1's.
 */
int
inet_valid_mask(mask)//�����Ƿ���Ч
    u_int32 mask;
{
    if (~(((mask & -mask) - 1) | mask) != 0) {
	/* Mask is not contiguous */
	return (0);
    }

    return (1);
}

/*
 * Verify that a given subnet number and mask pair are credible.
 *
 * With CIDR, almost any subnet and mask are credible.  mrouted still
 * can't handle aggregated class A's, so we still check that, but
 * otherwise the only requirements are that the subnet address is
 * within the [ABC] range and that the host bits of the subnet
 * are all 0.
 */
int
inet_valid_subnet(nsubnet, nmask)//�Ƿ�����Ч��������ַ
    u_int32 nsubnet, nmask;
{
    register u_int32 subnet, mask;

    subnet = ntohl(nsubnet);
    mask   = ntohl(nmask);

    if ((subnet & mask) != subnet)
	return (0);

    if (subnet == 0)
	return (mask == 0);

    if (IN_CLASSA(subnet)) {
	if (mask < 0xff000000 ||
	    (subnet & 0xff000000) == 0x7f000000 ||
	    (subnet & 0xff000000) == 0x00000000) return (0);
    }
    else if (IN_CLASSD(subnet) || IN_BADCLASS(subnet)) {
	/* Above Class C address space */
	return (0);
    }
    if (subnet & ~mask) {
	/* Host bits are set in the subnet */
	return (0);
    }
    if (!inet_valid_mask(mask)) {
	/* Netmask is not contiguous */
	return (0);
    }
    
    return (1);
}


/*
 * Convert an IP address in u_int32 (network) format into a printable string.
 */
char *
inet_fmt(addr, s)//��int�͵�IP��ַת�����ַ�����
    u_int32 addr;
    char *s;
{
    register u_char *a;
    
    a = (u_char *)&addr;
    sprintf(s, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
    return (s);
}


/*
 * Convert an IP subnet number in u_int32 (network) format into a printable
 * string including the netmask as a number of bits.
 */
#ifdef NOSUCHDEF	/* replaced by netname() */
char *
inet_fmts(addr, mask, s)//�ò���
    u_int32 addr, mask;
    char *s;
{
    register u_char *a, *m;
    int bits;

    if ((addr == 0) && (mask == 0)) {
	sprintf(s, "default");
	return (s);
    }
    a = (u_char *)&addr;
    m = (u_char *)&mask;
    bits = 33 - ffs(ntohl(mask));

    if      (m[3] != 0) sprintf(s, "%u.%u.%u.%u/%d", a[0], a[1], a[2], a[3],
				bits);
    else if (m[2] != 0) sprintf(s, "%u.%u.%u/%d",    a[0], a[1], a[2], bits);
    else if (m[1] != 0) sprintf(s, "%u.%u/%d",       a[0], a[1], bits);
    else                sprintf(s, "%u/%d",          a[0], bits);
    
    return (s);
}
#endif /* NOSUCHDEF */

/*
 * Convert the printable string representation of an IP address into the
 * u_int32 (network) format.  Return 0xffffffff on error.  (To detect the
 * legal address with that value, you must explicitly compare the string
 * with "255.255.255.255".)
 * The return value is in network order.
 */
u_int32
inet_parse(s, n)//�ò���
    char *s;
    int n;
{
    u_int32 a = 0;
    u_int a0 = 0, a1 = 0, a2 = 0, a3 = 0;
    int i;
    char c;

    i = sscanf(s, "%u.%u.%u.%u%c", &a0, &a1, &a2, &a3, &c);
    if (i < n || i > 4 || a0 > 255 || a1 > 255 || a2 > 255 || a3 > 255)
	return (0xffffffff);

    ((u_char *)&a)[0] = a0;
    ((u_char *)&a)[1] = a1;
    ((u_char *)&a)[2] = a2;
    ((u_char *)&a)[3] = a3;

    return (a);
}


/*
 * inet_cksum extracted from:
 *			P I N G . C
 *
 * Author -
 *	Mike Muuss
 *	U. S. Army Ballistic Research Laboratory
 *	December, 1983
 * Modified at Uc Berkeley
 *
 * (ping.c) Status -
 *	Public Domain.  Distribution Unlimited.
 *
 *			I N _ C K S U M
 *
 * Checksum routine for Internet Protocol family headers (C Version)
 *
 */
int
inet_cksum(addr, len)//����ַ�ͳ��ȣ�����У��ͣ�����ĺ������ò���
	u_int16 *addr;
	u_int len;
{
	register int nleft = (int)len;
	register u_int16 *w = addr;
	u_int16 answer = 0;
	register int sum = 0;

	/*
	 *  Our algorithm is simple, using a 32 bit accumulator (sum),
	 *  we add sequential 16 bit words to it, and at the end, fold
	 *  back all the carry bits from the top 16 bits into the lower
	 *  16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1) {
		*(u_char *) (&answer) = *(u_char *)w ;
		sum += answer;
	}

	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return (answer);
}

/*
 * Called by following netname() to create a mask specified network address.
 */
void
trimdomain(cp)
    char *cp;
{
    static char domain[MAXHOSTNAMELEN + 1];
    static int first = 1;
    char *s;
    
    if (first) {
	first = 0;
	if (gethostname(domain, MAXHOSTNAMELEN) == 0 &&
	    (s = strchr(domain, '.')))
	    (void) strcpy(domain, s + 1);
	else
	    domain[0] = 0;
    }

    if (domain[0]) {
	while ((cp = strchr(cp, '.'))) {
	    if (!strcasecmp(cp + 1, domain)) {
		*cp = 0;        /* hit it */
		break;
	    } else {
		cp++;
	    }
	}
    }
}

static u_long
forgemask(a)
    u_long a;
{
    u_long m;

    if (IN_CLASSA(a))
	m = IN_CLASSA_NET;
    else if (IN_CLASSB(a))
	m = IN_CLASSB_NET;
    else
	m = IN_CLASSC_NET;
    return (m);
}

static void
domask(dst, addr, mask)
    char *dst;
    u_long addr, mask;
{
    int b, i;
	
    if (!mask || (forgemask(addr) == mask)) {
	*dst = '\0';
	return;         
    }
    i = 0;
    for (b = 0; b < 32; b++)
	if (mask & (1 << b)) {
	    int bb;
	    
	    i = b;
	    for (bb = b+1; bb < 32; bb++)
		if (!(mask & (1 << bb))) {
		    i = -1; /* noncontig */
		    break;
		}
	    break;
	}
    if (i == -1) 
	sprintf(dst, "&0x%lx", mask);
    else
	sprintf(dst, "/%d", 32 - i);
}

/*
 * Return the name of the network whose address is given.
 * The address is assumed to be that of a net or subnet, not a host.
 */
char *
netname(addr, mask)       
    u_int32 addr, mask;
{
    static char line[MAXHOSTNAMELEN + 4];
    u_int32 omask;
    u_int32 i;
    
    i = ntohl(addr);
    omask = mask = ntohl(mask);
    if ((i & 0xffffff) == 0)
	sprintf(line, "%u", C(i >> 24));
    else if ((i & 0xffff) == 0)
	sprintf(line, "%u.%u", C(i >> 24) , C(i >> 16));
    else if ((i & 0xff) == 0)
	sprintf(line, "%u.%u.%u", C(i >> 24), C(i >> 16), C(i >> 8));
    else
	sprintf(line, "%u.%u.%u.%u", C(i >> 24),
		C(i >> 16), C(i >> 8), C(i));
    domask(line+strlen(line), i, omask);
    return (line);          
}
