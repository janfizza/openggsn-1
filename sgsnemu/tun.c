/* 
 * TUN interface functions.
 * Copyright (C) 2002, 2003 Mondru AB.
 * 
 * The contents of this file may be used under the terms of the GNU
 * General Public License Version 2, provided that the above copyright
 * notice and this permission notice is included in all copies or
 * substantial portions of the software.
 * 
 * The initial developer of the original code is
 * Jens Jakobsen <jj@openggsn.org>
 * 
 * Contributor(s):
 * 
 */

/*
 * tun.c: Contains all TUN functionality. Is able to handle multiple
 * tunnels in the same program. Each tunnel is identified by the struct,
 * which is passed to functions.
 *
 */


#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <errno.h>
#include <net/route.h>

#ifdef __linux__
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#elif defined (__sun__)
#include <stropts.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <net/if_tun.h>
/*#include "sun_if_tun.h"*/
#endif


#include "tun.h"
#include "syserr.h"


#ifdef __linux__
int tun_nlattr(struct nlmsghdr *n, int nsize, int type, void *d, int dlen)
{
  int len = RTA_LENGTH(dlen);
  int alen = NLMSG_ALIGN(n->nlmsg_len);
  struct rtattr *rta = (struct rtattr*) (((void*)n) + alen);
  if (alen + len > nsize)
    return -1;
  rta->rta_len = len;
  rta->rta_type = type;
  memcpy(RTA_DATA(rta), d, dlen);
  n->nlmsg_len = alen + len;
  return 0;
}

int tun_gifindex(struct tun_t *this, int *index) {
  struct ifreq ifr;
  int fd;

  memset (&ifr, '\0', sizeof (ifr));
  ifr.ifr_addr.sa_family = AF_INET;
  ifr.ifr_dstaddr.sa_family = AF_INET;
  ifr.ifr_netmask.sa_family = AF_INET;
  strncpy(ifr.ifr_name, this->devname, IFNAMSIZ);
  ifr.ifr_name[IFNAMSIZ-1] = 0; /* Make sure to terminate */
  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    sys_err(LOG_ERR, __FILE__, __LINE__, errno,
	    "socket() failed");
  }
  if (ioctl(fd, SIOCGIFINDEX, &ifr)) {
    sys_err(LOG_ERR, __FILE__, __LINE__, errno,
	    "ioctl() failed");
    close(fd);
    return -1;
  }
  close(fd);
  *index = ifr.ifr_ifindex;
  return 0;
}
#endif

int tun_sifflags(struct tun_t *this, int flags) {
  struct ifreq ifr;
  int fd;

  memset (&ifr, '\0', sizeof (ifr));
  ifr.ifr_flags = flags;
  strncpy(ifr.ifr_name, this->devname, IFNAMSIZ);
  ifr.ifr_name[IFNAMSIZ-1] = 0; /* Make sure to terminate */
  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    sys_err(LOG_ERR, __FILE__, __LINE__, errno,
	    "socket() failed");
  }
  if (ioctl(fd, SIOCSIFFLAGS, &ifr)) {
    sys_err(LOG_ERR, __FILE__, __LINE__, errno,
	    "ioctl(SIOCSIFFLAGS) failed");
    close(fd);
    return -1;
  }
  close(fd);
  return 0;
}


/* Currently unused 
int tun_addroute2(struct tun_t *this,
		  struct in_addr *dst,
		  struct in_addr *gateway,
		  struct in_addr *mask) {
  
  struct {
    struct nlmsghdr 	n;
    struct rtmsg 	r;
    char buf[TUN_NLBUFSIZE];
  } req;
  
  struct sockaddr_nl local;
  int addr_len;
  int fd;
  int status;
  struct sockaddr_nl nladdr;
  struct iovec iov;
  struct msghdr msg;

  memset(&req, 0, sizeof(req));
  req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
  req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE;
  req.n.nlmsg_type = RTM_NEWROUTE;
  req.r.rtm_family = AF_INET;
  req.r.rtm_table  = RT_TABLE_MAIN;
  req.r.rtm_protocol = RTPROT_BOOT;
  req.r.rtm_scope  = RT_SCOPE_UNIVERSE;
  req.r.rtm_type  = RTN_UNICAST;
  tun_nlattr(&req.n, sizeof(req), RTA_DST, dst, 4);
  tun_nlattr(&req.n, sizeof(req), RTA_GATEWAY, gateway, 4);
  
  if ((fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) < 0) {
    sys_err(LOG_ERR, __FILE__, __LINE__, errno,
	    "socket() failed");
    return -1;
  }

  memset(&local, 0, sizeof(local));
  local.nl_family = AF_NETLINK;
  local.nl_groups = 0;
  
  if (bind(fd, (struct sockaddr*)&local, sizeof(local)) < 0) {
    sys_err(LOG_ERR, __FILE__, __LINE__, errno,
	    "bind() failed");
    close(fd);
    return -1;
  }

  addr_len = sizeof(local);
  if (getsockname(fd, (struct sockaddr*)&local, &addr_len) < 0) {
    sys_err(LOG_ERR, __FILE__, __LINE__, errno,
	    "getsockname() failed");
    close(fd);
    return -1;
  }

  if (addr_len != sizeof(local)) {
    sys_err(LOG_ERR, __FILE__, __LINE__, 0,
	    "Wrong address length %d", addr_len);
    close(fd);
    return -1;
  }

  if (local.nl_family != AF_NETLINK) {
    sys_err(LOG_ERR, __FILE__, __LINE__, 0,
	    "Wrong address family %d", local.nl_family);
    close(fd);
    return -1;
  }
  
  iov.iov_base = (void*)&req.n;
  iov.iov_len = req.n.nlmsg_len;

  msg.msg_name = (void*)&nladdr;
  msg.msg_namelen = sizeof(nladdr),
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = NULL;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;

  memset(&nladdr, 0, sizeof(nladdr));
  nladdr.nl_family = AF_NETLINK;
  nladdr.nl_pid = 0;
  nladdr.nl_groups = 0;

  req.n.nlmsg_seq = 0;
  req.n.nlmsg_flags |= NLM_F_ACK;

  status = sendmsg(fd, &msg, 0);  * TODO: Error check *
  close(fd);
  return 0;
}
*/

int tun_addaddr(struct tun_t *this,
		struct in_addr *addr,
		struct in_addr *dstaddr,
		struct in_addr *netmask) {

#ifdef __linux__
  struct {
    struct nlmsghdr 	n;
    struct ifaddrmsg 	i;
    char buf[TUN_NLBUFSIZE];
  } req;
  
  struct sockaddr_nl local;
  int addr_len;
  int fd;
  int status;

  struct sockaddr_nl nladdr;
  struct iovec iov;
  struct msghdr msg;
#endif

  if (!this->addrs) /* Use ioctl for first addr to make ping work */
    return tun_setaddr(this, addr, dstaddr, netmask);

#ifndef __linux__
  sys_err(LOG_ERR, __FILE__, __LINE__, errno,
	  "Setting multiple addresses only possible on linux");
  return -1;
#else
  memset(&req, 0, sizeof(req));
  req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
  req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE;
  req.n.nlmsg_type = RTM_NEWADDR;
  req.i.ifa_family = AF_INET;
  req.i.ifa_prefixlen = 32; /* 32 FOR IPv4 */
  req.i.ifa_flags = 0;
  req.i.ifa_scope = RT_SCOPE_HOST; /* TODO or 0 */
  if (tun_gifindex(this, &req.i.ifa_index)) {
    return -1;
  }

  tun_nlattr(&req.n, sizeof(req), IFA_ADDRESS, addr, sizeof(addr));
  tun_nlattr(&req.n, sizeof(req), IFA_LOCAL, dstaddr, sizeof(dstaddr));

  if ((fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) < 0) {
    sys_err(LOG_ERR, __FILE__, __LINE__, errno,
	    "socket() failed");
    return -1;
  }

  memset(&local, 0, sizeof(local));
  local.nl_family = AF_NETLINK;
  local.nl_groups = 0;
  
  if (bind(fd, (struct sockaddr*)&local, sizeof(local)) < 0) {
    sys_err(LOG_ERR, __FILE__, __LINE__, errno,
	    "bind() failed");
    close(fd);
    return -1;
  }

  addr_len = sizeof(local);
  if (getsockname(fd, (struct sockaddr*)&local, &addr_len) < 0) {
    sys_err(LOG_ERR, __FILE__, __LINE__, errno,
	    "getsockname() failed");
    close(fd);
    return -1;
  }

  if (addr_len != sizeof(local)) {
    sys_err(LOG_ERR, __FILE__, __LINE__, 0,
	    "Wrong address length %d", addr_len);
    close(fd);
    return -1;
  }

  if (local.nl_family != AF_NETLINK) {
    sys_err(LOG_ERR, __FILE__, __LINE__, 0,
	    "Wrong address family %d", local.nl_family);
    close(fd);
    return -1;
  }
  
  iov.iov_base = (void*)&req.n;
  iov.iov_len = req.n.nlmsg_len;

  msg.msg_name = (void*)&nladdr;
  msg.msg_namelen = sizeof(nladdr),
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = NULL;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;

  memset(&nladdr, 0, sizeof(nladdr));
  nladdr.nl_family = AF_NETLINK;
  nladdr.nl_pid = 0;
  nladdr.nl_groups = 0;

  req.n.nlmsg_seq = 0;
  req.n.nlmsg_flags |= NLM_F_ACK;

  status = sendmsg(fd, &msg, 0); /* TODO Error check */

  tun_sifflags(this, IFF_UP | IFF_RUNNING);
  close(fd);
  this->addrs++;
  return 0;
#endif
}


int tun_setaddr(struct tun_t *this,
		struct in_addr *addr,
		struct in_addr *dstaddr,
		struct in_addr *netmask)
{
  struct ifreq   ifr;
  int fd;

  memset (&ifr, '\0', sizeof (ifr));
  ifr.ifr_addr.sa_family = AF_INET;
  ifr.ifr_dstaddr.sa_family = AF_INET;
#ifndef __sun__
  ifr.ifr_netmask.sa_family = AF_INET;
#endif
  strncpy(ifr.ifr_name, this->devname, IFNAMSIZ);
  ifr.ifr_name[IFNAMSIZ-1] = 0; /* Make sure to terminate */

  /* Create a channel to the NET kernel. */
  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    sys_err(LOG_ERR, __FILE__, __LINE__, errno,
	    "socket() failed");
    return -1;
  }

  if (addr) { /* Set the interface address */
    this->addr.s_addr = addr->s_addr;
    ((struct sockaddr_in *) &ifr.ifr_addr)->sin_addr.s_addr = addr->s_addr;
    if (ioctl(fd, SIOCSIFADDR, (void *) &ifr) < 0) {
      if (errno != EEXIST) {
	sys_err(LOG_ERR, __FILE__, __LINE__, errno,
		"ioctl(SIOCSIFADDR) failed");
      }
      else {
	sys_err(LOG_WARNING, __FILE__, __LINE__, errno,
		"ioctl(SIOCSIFADDR): Address already exists");
      }
      close(fd);
      return -1;
    }
  }

  if (dstaddr) { /* Set the destination address */
    this->dstaddr.s_addr = dstaddr->s_addr;
    ((struct sockaddr_in *) &ifr.ifr_dstaddr)->sin_addr.s_addr = 
      dstaddr->s_addr;
    if (ioctl(fd, SIOCSIFDSTADDR, (caddr_t) &ifr) < 0) {
      sys_err(LOG_ERR, __FILE__, __LINE__, errno,
	      "ioctl(SIOCSIFDSTADDR) failed");
      close(fd);
      return -1; 
    }
  }

  if (netmask) { /* Set the netmask */
    this->netmask.s_addr = netmask->s_addr;
#ifdef __sun__
    ((struct sockaddr_in *) &ifr.ifr_dstaddr)->sin_addr.s_addr = 
      dstaddr->s_addr;
#else
    ((struct sockaddr_in *) &ifr.ifr_netmask)->sin_addr.s_addr = 
      netmask->s_addr;
#endif
    if (ioctl(fd, SIOCSIFNETMASK, (void *) &ifr) < 0) {
      sys_err(LOG_ERR, __FILE__, __LINE__, errno,
	      "ioctl(SIOCSIFNETMASK) failed");
      close(fd);
      return -1;
    }
  }

  close(fd);
  this->addrs++;
  return tun_sifflags(this, IFF_UP | IFF_RUNNING);
}

int tun_addroute(struct tun_t *this,
		 struct in_addr *dst,
		 struct in_addr *gateway,
		 struct in_addr *mask)
{

  /* TODO: Learn how to set routing table on sun */
#ifndef __sun__

  struct rtentry r;
  int fd;

  memset (&r, '\0', sizeof (r));
  r.rt_flags = RTF_UP | RTF_GATEWAY; /* RTF_HOST not set */

  /* Create a channel to the NET kernel. */
  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    sys_err(LOG_ERR, __FILE__, __LINE__, errno,
	    "socket() failed");
    return -1;
  }

  r.rt_dst.sa_family     = AF_INET;
  r.rt_gateway.sa_family = AF_INET;
  r.rt_genmask.sa_family = AF_INET;
  ((struct sockaddr_in *) &r.rt_dst)->sin_addr.s_addr = dst->s_addr;
  ((struct sockaddr_in *) &r.rt_gateway)->sin_addr.s_addr = gateway->s_addr;
  ((struct sockaddr_in *) &r.rt_genmask)->sin_addr.s_addr = mask->s_addr;

  if (ioctl(fd, SIOCADDRT, (void *) &r) < 0) {   /* SIOCDELRT */
    sys_err(LOG_ERR, __FILE__, __LINE__, errno,
	    "ioctl(SIOCADDRT) failed");
    close(fd);
    return -1;
  }
  close(fd);

#endif

  return 0;
}


int tun_new(struct tun_t **tun)
{

#ifndef __sun__
  struct ifreq ifr;
#else
  int if_fd, ppa = -1;
  static int ip_fd = 0;
#endif
  
  if (!(*tun = calloc(1, sizeof(struct tun_t)))) {
    sys_err(LOG_ERR, __FILE__, __LINE__, errno, "calloc() failed");
    return EOF;
  }
  
  (*tun)->cb_ind = NULL;
  (*tun)->addrs = 0;

#ifdef __linux__
  /* Open the actual tun device */
  if (((*tun)->fd  = open("/dev/net/tun", O_RDWR)) < 0) {
    sys_err(LOG_ERR, __FILE__, __LINE__, errno, "open() failed");
    return -1;
  }

  /* Set device flags. For some weird reason this is also the method
     used to obtain the network interface name */
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TUN | IFF_NO_PI; /* Tun device, no packet info */
  if (ioctl((*tun)->fd, TUNSETIFF, (void *) &ifr) < 0) {
    sys_err(LOG_ERR, __FILE__, __LINE__, errno, "ioctl() failed");
    close((*tun)->fd);
    return -1;
  } 

  strncpy((*tun)->devname, ifr.ifr_name, IFNAMSIZ);
  (*tun)->devname[IFNAMSIZ] = 0;

  ioctl((*tun)->fd, TUNSETNOCSUM, 1); /* Disable checksums */

#endif

#ifdef __sun__

  if( (ip_fd = open("/dev/ip", O_RDWR, 0)) < 0){
    sys_err(LOG_ERR, __FILE__, __LINE__, errno, "Can't open /dev/ip");
    return -1;
  }
  
  if( ((*tun)->fd = open("/dev/tun", O_RDWR, 0)) < 0){
    sys_err(LOG_ERR, __FILE__, __LINE__, errno, "Can't open /dev/tun");
    return -1;
  }
  
  /* Assign a new PPA and get its unit number. */
  if( (ppa = ioctl((*tun)->fd, TUNNEWPPA, -1)) < 0){
    syslog(LOG_ERR, "Can't assign new interface");
    return -1;
  }
  
  if( (if_fd = open("/dev/tun", O_RDWR, 0)) < 0){
    syslog(LOG_ERR, "Can't open /dev/tun (2)");
    return -1;
  }
  if(ioctl(if_fd, I_PUSH, "ip") < 0){
    syslog(LOG_ERR, "Can't push IP module");
    return -1;
  }
  
  /* Assign ppa according to the unit number returned by tun device */
  if(ioctl(if_fd, IF_UNITSEL, (char *)&ppa) < 0){
    syslog(LOG_ERR, "Can't set PPA %d", ppa);
    return -1;
  }

  /* Link the two streams */
  if(ioctl(ip_fd, I_LINK, if_fd) < 0){
    syslog(LOG_ERR, "Can't link TUN device to IP");
    return -1;
  }

  close (if_fd);
  
  sprintf((*tun)->devname, "tun%d", ppa);
  
#endif
  
  return 0;
}

int tun_free(struct tun_t *tun)
{
  if (close(tun->fd)) {
    sys_err(LOG_ERR, __FILE__, __LINE__, errno, "close() failed");
  }

  /* TODO: For solaris we need to unlink streams */

  free(tun);
  return 0;
}


int tun_set_cb_ind(struct tun_t *this, 
  int (*cb_ind) (struct tun_t *tun, void *pack, unsigned len)) {
  this->cb_ind = cb_ind;
  return 0;
}


int tun_decaps(struct tun_t *this)
{
  unsigned char buffer[PACKET_MAX];
  int status;
  
  if ((status = read(this->fd, buffer, sizeof(buffer))) <= 0) {
    sys_err(LOG_ERR, __FILE__, __LINE__, errno, "read() failed");
    return -1;
  }
  
  if (this->cb_ind)
    return this->cb_ind(this, buffer, status);

  return 0;
}

int tun_encaps(struct tun_t *tun, void *pack, unsigned len)
{
  return write(tun->fd, pack, len);
}

int tun_runscript(struct tun_t *tun, char* script) {
  
  char buf[TUN_SCRIPTSIZE];
  char snet[TUN_ADDRSIZE];
  char smask[TUN_ADDRSIZE];

  strncpy(snet, inet_ntoa(tun->addr), sizeof(snet));
  snet[sizeof(snet)-1] = 0;
  strncpy(smask, inet_ntoa(tun->netmask), sizeof(smask));
  smask[sizeof(smask)-1] = 0;
  
  /* system("ipup /dev/tun0 192.168.0.10 255.255.255.0"); */
  snprintf(buf, sizeof(buf), "%s %s %s %s",
	   script, tun->devname, snet, smask);
  buf[sizeof(buf)-1] = 0;
  system(buf);
  return 0;
}
