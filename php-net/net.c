#include <limits.h>
#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/ethernet.h>
#include <net/route.h>
#include <netinet6/nd6.h>
#include <sys/pciio.h>
#include <dev/pci/pcireg.h>
#include <fcntl.h>
#include <string.h>
#include <kenv.h>
#include <netdb.h>
#include <net/if_dl.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include <signal.h>
#include <resolv.h>
#include <sys/wait.h>
#ifdef ZFS_ENABLE
#include <kzfs.h>
#include <base.h>
#endif
#include "php.h"
#include "php_net.h"


#define NET_CMD_MAX_LEN					128
#define NET_CONF_LINE_MAX_LEN			128
#define NET_DHCP_PID_PATH_LEN			128
#define	NET_ALL_STRLEN					64
#define NET_ROUTE_NAME_MAX_LEN			32

struct fibl {
	TAILQ_ENTRY(fibl)	fl_next;

	int	fl_num;
	int	fl_error;
	int	fl_errno;
};
TAILQ_HEAD(fibl_head_t, fibl) fibl_head;
struct sockaddr_storage so[RTAX_MAX];
extern char **environ;

static void up_deleted_ip(long deleted_num, char *ifname, char *type);
static void remove_all_ip_conf(char *ifname);
static void remove_all_ip(char *ifname);
static void kill_dhcp(char *ifname);
static void get_ip_conf(char *ifname, long alias_num, char *type, char *ip);
static int add_new_ip4(char *ifname, long alias_num, char *ip, char *netmask, zend_bool enabled, zend_long settype);
static int add_new_ip6(char *ifname, long alias_num, char *ip, long prefix, zend_bool enabled, zend_long settype);
static void is_dns_dhcp(char *res);
static void get_route_conf(char *res, char *what, char *name);
static int php_net_check_route(char *routename);

static int fill_sockaddr_storage(int id, char *addr);
static int fill_fibs(struct fibl_head_t *flh);
static int fiboptlist_range(const char *arg, struct fibl_head_t *flh, int numfibs);
static int fiboptlist_csv(const char *arg, struct fibl_head_t *flh, int numfibs, int defaultfib);
static int rtmsg(int type, int flags, int rtm_addrs, int *seq, int fib);
static int check_route(char *type, char *dest);
static int get_def_route(char *def, size_t len);



PHP_MINIT_FUNCTION(net);
PHP_MSHUTDOWN_FUNCTION(net);
PHP_FUNCTION(zcfg_save); 				// done
PHP_FUNCTION(test01); 				// done
PHP_FUNCTION(get_hostname);				// done
PHP_FUNCTION(set_hostname);				// done
PHP_FUNCTION(ip4_add);					// done
PHP_FUNCTION(ip4_del);					// done
PHP_FUNCTION(ip6_add);					// done
PHP_FUNCTION(ip6_del);					// done
PHP_FUNCTION(ips_get);					// done
PHP_FUNCTION(ips_set);					// done
PHP_FUNCTION(dhcp_set_ip); 				// done
PHP_FUNCTION(dhcp_del_ip); 				// done
PHP_FUNCTION(dns_unset);				// done
PHP_FUNCTION(dns_get);					// done
PHP_FUNCTION(dns_set);					// done
PHP_FUNCTION(dns_set_dhcp);				// done
PHP_FUNCTION(route_add); 				// done
PHP_FUNCTION(route_del); 				// done
PHP_FUNCTION(route_set_default); 		// done
PHP_FUNCTION(route_del_default); 		// done
PHP_FUNCTION(route_get);				// wait
PHP_FUNCTION(routes_get); 				// done
PHP_FUNCTION(netif_clear); 				// done
PHP_FUNCTION(netif_mac);				// done
PHP_FUNCTION(netif_up);					// done


const char *path = "/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin:/data/bin";


ZEND_BEGIN_ARG_INFO_EX(arginfo_zcfg_save, 0, 0, 1)
	ZEND_ARG_INFO(0, script_path)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_get_hostname, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_dns_unset, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_set_hostname, 0, 0, 1)
	ZEND_ARG_INFO(0, new_hostname)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_netif_up, 0, 0, 1)
	ZEND_ARG_INFO(0, ifname)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_netif_mac, 0, 0, 2)
	ZEND_ARG_INFO(0, ifname)
	ZEND_ARG_INFO(0, mac)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_netif_clear, 0, 0, 1)
	ZEND_ARG_INFO(0, ifname)
ZEND_END_ARG_INFO()
  
ZEND_BEGIN_ARG_INFO_EX(arginfo_ip4_add, 0, 0, 6)
	ZEND_ARG_INFO(0, ifname)
	ZEND_ARG_INFO(0, ip)
	ZEND_ARG_INFO(0, netmask)
	ZEND_ARG_INFO(0, alias_num)
	ZEND_ARG_INFO(0, enabled)
	ZEND_ARG_INFO(0, settype)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ip4_del, 0, 0, 3)
	ZEND_ARG_INFO(0, ifname)
	ZEND_ARG_INFO(0, alias_num)
	ZEND_ARG_INFO(0, settype)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ip6_add, 0, 0, 6)
	ZEND_ARG_INFO(0, ifname)
	ZEND_ARG_INFO(0, ip6)
	ZEND_ARG_INFO(0, prefix)
	ZEND_ARG_INFO(0, alias_num)
	ZEND_ARG_INFO(0, enabled)
	ZEND_ARG_INFO(0, settype)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ip6_del, 0, 0, 3)
	ZEND_ARG_INFO(0, ifname)
	ZEND_ARG_INFO(0, alias_num)
	ZEND_ARG_INFO(0, settype)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ips_get, 0, 0, 0)
	ZEND_ARG_INFO(0, ifname)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ips_set, 0, 0, 4)
	ZEND_ARG_INFO(0, ifname)
	ZEND_ARG_INFO(0, ips)
	ZEND_ARG_INFO(0, enabled)
	ZEND_ARG_INFO(0, settype)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_dhcp_set_ip, 0, 0, 3)
	ZEND_ARG_INFO(0, ifname)
	ZEND_ARG_INFO(0, enabled)
	ZEND_ARG_INFO(0, settype)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_dhcp_del_ip, 0, 0, 2)
	ZEND_ARG_INFO(0, ifname)
	ZEND_ARG_INFO(0, settype)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_dns_get, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_dns_set, 0, 0, 2)
	ZEND_ARG_INFO(0, domain)
	ZEND_ARG_INFO(0, dns1)
	ZEND_ARG_INFO(0, dns2)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_dns_set_dhcp, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_route_add, 0, 0, 5)
	ZEND_ARG_INFO(0, type)		// net/host - 0/1
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, dest)
	ZEND_ARG_INFO(0, gtw)
	ZEND_ARG_INFO(0, status)	// 0/1 disable/enable
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_route_del, 0, 0, 1)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_route_set_default, 0, 0, 1)
	ZEND_ARG_INFO(0, gtw)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_route_del_default, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_route_get, 0, 0, 1)
	ZEND_ARG_INFO(0, route_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_routes_get, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_test01, 0, 0, 1)
	ZEND_ARG_INFO(0, test)
ZEND_END_ARG_INFO()


const zend_function_entry net_functions[] = {
	PHP_FE(test01, arginfo_test01)
	PHP_FE(zcfg_save, arginfo_zcfg_save)
	PHP_FE(get_hostname, NULL)
	PHP_FE(set_hostname, arginfo_set_hostname)
	PHP_FE(netif_up, arginfo_netif_up)
	PHP_FE(netif_mac, arginfo_netif_mac)
	PHP_FE(netif_clear, arginfo_netif_clear)
	PHP_FE(ip4_add, arginfo_ip4_add)
	PHP_FE(ip4_del, arginfo_ip4_del)
	PHP_FE(ip6_add, arginfo_ip6_add)
	PHP_FE(ip6_del, arginfo_ip6_del)
	PHP_FE(ips_get, arginfo_ips_get)
	PHP_FE(ips_set, arginfo_ips_set)
	PHP_FE(dns_get, NULL)
	PHP_FE(dns_set, arginfo_dns_set)
	PHP_FE(dns_unset, NULL)
	PHP_FE(dns_set_dhcp, NULL)
	PHP_FE(dhcp_set_ip, arginfo_dhcp_set_ip)
	PHP_FE(dhcp_del_ip, arginfo_dhcp_del_ip)
	PHP_FE(route_add, arginfo_route_add)
	PHP_FE(route_del, arginfo_route_del)
	PHP_FE(route_set_default, arginfo_route_set_default)
	PHP_FE(route_del_default, NULL)
	PHP_FE(route_get, arginfo_route_get)
	PHP_FE(routes_get, NULL)
	{NULL, NULL, NULL}
};

zend_module_entry net_module_entry = {
	STANDARD_MODULE_HEADER,
	"net",
	net_functions,
	PHP_MINIT(net),
	PHP_MSHUTDOWN(net),
	NULL,
	NULL,
	NULL,
	"0.0.1",
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_NET
ZEND_GET_MODULE(net)
#endif

PHP_INI_BEGIN()
    PHP_INI_ENTRY("kcs.net.conf", "/etc/kinit.conf", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("kcs.dhcp.pid_path", "/var/run", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("kcs.dhcp.pid_format", "%s/dhcpcd-%s.pid", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("kcs.net.ifaces", NULL, PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("kcs.net.prefix.enable", "0", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("kcs.dhcpd.cmd", "/sbin/dhclient", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("kcs.dhcpd.flags", NULL, PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("kcs.dhcpd.name", "dhclient", PHP_INI_ALL, NULL)
PHP_INI_END()

enum php_net_ip_set_type {
	PHP_NET_IP_CONF,
	PHP_NET_IP_SYSTEM,
	PHP_NET_IP_AUTO
};

PHP_MINIT_FUNCTION(net)
{
	REGISTER_LONG_CONSTANT("PHP_NET_IP_CONF", PHP_NET_IP_CONF, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PHP_NET_IP_SYSTEM", PHP_NET_IP_SYSTEM, CONST_CS|CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PHP_NET_IP_AUTO", PHP_NET_IP_AUTO, CONST_CS|CONST_PERSISTENT);
	REGISTER_INI_ENTRIES();
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(net)
{
    UNREGISTER_INI_ENTRIES();
    return SUCCESS;
}


PHP_FUNCTION(test01)
{
	zval *test;
	zend_bool t;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &t) == FAILURE) { 
	   return;
	}
	printf("%d\n", t);
	RETURN_TRUE;
}

PHP_FUNCTION(zcfg_save)
{
	char *script_path = "";
	size_t script_path_len = sizeof("")-1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &script_path, &script_path_len) == FAILURE) { 
	   return;
	}

	setenv("PATH", path, 1);
	char cmd[NET_CMD_MAX_LEN];
	snprintf(cmd, sizeof(cmd), "%s save", script_path);
	system(cmd);
	RETURN_TRUE;
}

PHP_FUNCTION(netif_up)
{
	char *iface = "";
	size_t ifname_len = sizeof("")-1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &iface, &ifname_len) == FAILURE) { 
	   return;
	}

    int s;
    if ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
        if  ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
			RETURN_FALSE;
        }
    }
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(struct ifreq));
	strcpy(ifr.ifr_name, iface);
 	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
 		close(s);
		RETURN_FALSE;
 	}

	int all_flags = 0;
	all_flags = (ifr.ifr_flags & 0xffff) | (ifr.ifr_flagshigh << 16);

	all_flags |= IFF_UP;

	ifr.ifr_flags = all_flags & 0xffff;
	ifr.ifr_flagshigh = all_flags >> 16;
	int ret = ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifr);

    close(s);
    if (ret)
    	RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(netif_clear)
{
	char *iface = "";
	size_t ifname_len = sizeof("")-1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &iface, &ifname_len) == FAILURE) { 
	   return;
	}
  	remove_all_ip_conf(iface);
    remove_all_ip(iface);
  	RETURN_TRUE;
}

PHP_FUNCTION(netif_mac)
{
	char *iface = "";
	size_t ifname_len = sizeof("")-1;
	char *mac = "";
	size_t mac_len = sizeof("")-1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &iface, &ifname_len, &mac, &mac_len) == FAILURE) { 
	   return;
	}

    int s;
    if ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
        if  ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
            RETURN_FALSE;
        }
    }
    struct ifreq link_ridreq;
    memset(&link_ridreq, 0, sizeof(struct ifreq));
    struct sockaddr_dl sdl;
    memset(&sdl, 0, sizeof(struct sockaddr_dl));
    struct sockaddr *sa = &link_ridreq.ifr_addr;
    char *temp;

    if ((temp = malloc(strlen(mac) + 2)) == NULL) {
        close(s);
        RETURN_FALSE;
    }
    temp[0] = ':';
    strcpy(temp + 1, mac);
    sdl.sdl_len = sizeof(sdl);
    link_addr(temp, &sdl);
    free(temp);
    sa->sa_family = AF_LINK;
    sa->sa_len = sdl.sdl_alen;
    bcopy(LLADDR(&sdl), sa->sa_data, sdl.sdl_alen);
    strlcpy(link_ridreq.ifr_name, iface, sizeof(link_ridreq.ifr_name));
    if (ioctl(s, SIOCSIFLLADDR, (caddr_t)&link_ridreq) < 0) {
        close(s);
        RETURN_FALSE;
    }
    close(s);

  	RETURN_TRUE;
}


PHP_FUNCTION(get_hostname)
{
	char buf[_POSIX_HOST_NAME_MAX + 1];
	if (gethostname(buf, sizeof(buf)) != -1) {
		RETURN_STRING(buf);
	}
	RETURN_FALSE;
}


PHP_FUNCTION(set_hostname)
{
	char *new_hostname = "";
	size_t new_hostname_len = sizeof("")-1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &new_hostname, &new_hostname_len) == FAILURE) { 
	   return;
	}
	if (!new_hostname[0]) {
		RETURN_FALSE;
	}
	// uid_t ruid, euid, suid;
	// getresuid(&ruid, &euid, &suid);
	// if (setreuid(suid, -1) == -1) {
	// 	RETURN_FALSE;
	// }
	char buf[_POSIX_HOST_NAME_MAX + 1];
	if (gethostname(buf, sizeof(buf)) != -1) {
		if (strcmp(new_hostname, buf) != 0) {
			if (sethostname(new_hostname, new_hostname_len) != -1) {
#ifdef ZFS_ENABLE
				char kzfs_val[KENV_MVALLEN + 1];
				if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
					snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
				kzfs_set_uprop(kzfs_val, "net:hostname", new_hostname);
#else
				char *conf_file = INI_STR("kcs.net.conf");
				setenv("PATH", path, 1);
				char cmd[NET_CMD_MAX_LEN];
				snprintf(cmd, sizeof(cmd), "sysrc -f %s hostname=\"%s\" 1>/dev/null", conf_file, new_hostname);
				// printf("%s\n", cmd);
				system(cmd);
#endif
				// setreuid(ruid, -1);
				RETURN_TRUE;
			}
		}
	}
	// setreuid(ruid, -1);
	RETURN_FALSE;
}

PHP_FUNCTION(ip4_add)
{
	char *ifname = "";
	char *ip = "";
	char *netmask = "";
	zend_long alias_num;
	zend_bool enabled = 0;
	zend_long settype;

	size_t ifname_len = sizeof("")-1;
	size_t ip_len = sizeof("")-1;
	size_t netmask_len = sizeof("")-1;


	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssslbl", &ifname, &ifname_len, &ip, &ip_len, &netmask, &netmask_len, &alias_num, &enabled, &settype) == FAILURE) { 
	   return;
	}

	if (settype == PHP_NET_IP_SYSTEM || settype == PHP_NET_IP_AUTO)
		kill_dhcp(ifname);
	if (add_new_ip4(ifname, (long)alias_num, ip, netmask, enabled, settype) == -1)
		RETURN_FALSE;

	RETURN_TRUE;
}

PHP_FUNCTION(ip4_del)
{
	char *ifname = "";
	size_t ifname_len = sizeof("")-1;
	zval *data;
	zend_long settype;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "szl", &ifname, &ifname_len, &data, &settype) == FAILURE) { 
	   return;
	}

	if (settype == PHP_NET_IP_SYSTEM && Z_TYPE_P(data) != IS_STRING)
		RETURN_FALSE;

	if ((settype == PHP_NET_IP_AUTO || settype == PHP_NET_IP_CONF) && Z_TYPE_P(data) != IS_LONG)
		RETURN_FALSE;
	
	zend_long alias_num;
	char ip[20];
	memset(ip, 0, sizeof(ip));
	setenv("PATH", path, 1);

	if (settype == PHP_NET_IP_AUTO || settype == PHP_NET_IP_CONF) {
		alias_num = Z_LVAL_P(data);
		get_ip_conf(ifname, alias_num, "ip4", ip);
	} else if (settype == PHP_NET_IP_SYSTEM) {
		snprintf(ip, sizeof(ip), "%s", Z_STRVAL_P(data));
	}
	if (settype == PHP_NET_IP_SYSTEM || settype == PHP_NET_IP_AUTO) {
		if (!ip[0] || strcmp(ip, "DHCP") == 0)
			RETURN_FALSE;
		int s;
		if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			RETURN_FALSE;
		}
		int ret = 0;

		struct ifreq ifr;
		memset(&ifr, 0, sizeof(struct ifreq));
		strcpy(ifr.ifr_name, ifname);
		struct sockaddr_in *sai = (struct sockaddr_in *)&ifr.ifr_addr;
		memset(sai, 0, sizeof(struct sockaddr));
	    sai->sin_family = AF_INET;
	    sai->sin_port = 0;
	    sai->sin_addr.s_addr = inet_addr(ip);

		ret = ioctl(s, SIOCDIFADDR, (caddr_t)&ifr);

		close(s);

		if (ret < 0) {
			RETURN_FALSE;
		}
	}
	if (settype == PHP_NET_IP_AUTO || settype == PHP_NET_IP_CONF) {
#ifdef ZFS_ENABLE
		char kzfs_val[KENV_MVALLEN + 1];
		if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
			snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
		char param_name[KZFS_PARAM_MAX_LEN];
		snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip4_alias_%ld", ifname, alias_num);
		kzfs_del_uprop(kzfs_val, param_name);
		snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip4_netmask_%ld", ifname, alias_num);
		kzfs_del_uprop(kzfs_val, param_name);
#else
		char *conf_file = INI_STR("kcs.net.conf");
		char cmd[NET_CMD_MAX_LEN];
		snprintf(cmd, sizeof(cmd), "sysrc -f %s -x ifconfig_%s_ip4_alias_%ld", conf_file, ifname, alias_num);
		system(cmd);
		snprintf(cmd, sizeof(cmd), "sysrc -f %s -x ifconfig_%s_ip4_netmask_%ld", conf_file, ifname, alias_num);
		system(cmd);
#endif
		up_deleted_ip(alias_num, ifname, "ip4");
	}
	RETURN_TRUE;
}

PHP_FUNCTION(ip6_add)
{
	char *ifname = "";
	char *ip6 = "";
	zend_long prefix;
	zend_long alias_num;
	zend_bool enabled;
	zend_long settype;

	size_t ifname_len = sizeof("")-1;
	size_t ip6_len = sizeof("")-1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssllbl", &ifname, &ifname_len, &ip6, &ip6_len, &prefix, &alias_num, &enabled, &settype) == FAILURE) { 
	   return;
	}
	if (settype == PHP_NET_IP_SYSTEM || settype == PHP_NET_IP_AUTO)
		kill_dhcp(ifname);

	if (add_new_ip6(ifname, alias_num, ip6, prefix, enabled, settype) == -1)
		RETURN_FALSE;

	RETURN_TRUE;
}

PHP_FUNCTION(ip6_del)
{
	char *ifname = "";
	size_t ifname_len = sizeof("")-1;
	zval *data;
	zend_long settype;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "szl", &ifname, &ifname_len, &data, &settype) == FAILURE) { 
	   return;
	}

	if (settype == PHP_NET_IP_SYSTEM && Z_TYPE_P(data) != IS_STRING)
		RETURN_FALSE;

	if ((settype == PHP_NET_IP_AUTO || settype == PHP_NET_IP_CONF) && Z_TYPE_P(data) != IS_LONG)
		RETURN_FALSE;

	zend_long alias_num;
	setenv("PATH", path, 1);
	char ip[40];
	memset(ip, 0, sizeof(ip));

	if (settype == PHP_NET_IP_AUTO || settype == PHP_NET_IP_CONF) {
		alias_num = Z_LVAL_P(data);
		get_ip_conf(ifname, alias_num, "ip6", ip);
	} else if (settype == PHP_NET_IP_SYSTEM) {
		snprintf(ip, sizeof(ip), "%s", Z_STRVAL_P(data));
	}
	if (settype == PHP_NET_IP_SYSTEM || settype == PHP_NET_IP_AUTO) {
		if (!ip[0])
			RETURN_FALSE
		int s;
		if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
			RETURN_FALSE;
		}

		int ret = 0;
		struct in6_aliasreq in6_addreq = 
		  { .ifra_flags = 0,
		    .ifra_lifetime = { 0, 0, ND6_INFINITE_LIFETIME, ND6_INFINITE_LIFETIME } };
		strlcpy(((struct ifreq *)&in6_addreq)->ifr_name, ifname,
				IFNAMSIZ);
		struct addrinfo hints, *res;
		bzero(&hints, sizeof(struct addrinfo));
		hints.ai_family = AF_INET6;
		getaddrinfo(ip, NULL, &hints, &res);
		bcopy(res->ai_addr, (struct sockaddr_in6 *) &(in6_addreq.ifra_addr), res->ai_addrlen);
		freeaddrinfo(res);
		ret = ioctl(s, SIOCDIFADDR_IN6, (caddr_t)&in6_addreq);
		close(s);
		if (ret < 0)
			RETURN_FALSE;
	}
	if (settype == PHP_NET_IP_AUTO || settype == PHP_NET_IP_CONF) {
#ifdef ZFS_ENABLE
		char kzfs_val[KENV_MVALLEN + 1];
		if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
			snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
		char param_name[KZFS_PARAM_MAX_LEN];
		snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip6_alias_%ld", ifname, alias_num);
		kzfs_del_uprop(kzfs_val, param_name);
		snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip6_prefix_%ld", ifname, alias_num);
		kzfs_del_uprop(kzfs_val, param_name);
#else
		char cmd[NET_CMD_MAX_LEN];
		char *conf_file = INI_STR("kcs.net.conf");
		snprintf(cmd, sizeof(cmd), "sysrc -f %s -x ifconfig_%s_ip6_alias_%ld", conf_file, ifname, alias_num);
		system(cmd);
		snprintf(cmd, sizeof(cmd), "sysrc -f %s -x ifconfig_%s_ip6_prefix_%ld", conf_file, ifname, alias_num);
		system(cmd);
#endif
		up_deleted_ip(alias_num, ifname, "ip6");
	}
	RETURN_TRUE;
}


PHP_FUNCTION(ips_get)
{
	char *ifname = "";
	size_t ifname_len = sizeof("")-1;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &ifname, &ifname_len) == FAILURE) { 
	   return;
	}

	array_init(return_value);
	struct ifaddrs *ifap, *ifa;

	int ip4_alias_num;
	int ip6_alias_num;
	setenv("PATH", path, 1);
	char cmd[NET_CMD_MAX_LEN];
	char ip[20];
	int flag = 0;


	char *conf_file = INI_STR("kcs.net.conf");
	char *ifaces_list = INI_STR("kcs.net.ifaces");
	if (!ifaces_list) {
		int fd;
		struct pci_conf_io pc;
		struct pci_conf conf[255], *p;
		fd = open("/dev/pci", O_RDONLY);
		if (fd < 0) {
		    freeifaddrs(ifap);
			RETURN_FALSE;
		}

		bzero(&pc, sizeof(struct pci_conf_io));
		pc.match_buf_len = sizeof(conf);
		pc.matches = conf;
		int none_count = 0;
		do {
			if (ioctl(fd, PCIOCGETCONF, &pc) == -1) {
				close(fd);
			    freeifaddrs(ifap);
				RETURN_FALSE;
			}

			if (pc.status == PCI_GETCONF_LIST_CHANGED) {
				close(fd);
			    freeifaddrs(ifap);
				RETURN_FALSE;
			}  else if (pc.status ==  PCI_GETCONF_ERROR) {
				close(fd);
			    freeifaddrs(ifap);
				RETURN_FALSE;
			}
			for (p = conf; p < &conf[pc.num_matches]; p++) {
				if ((p->pc_class ^ PCIC_NETWORK) == 0 && (!(p->pc_subclass & PCIS_NETWORK_ETHERNET))) {
					if (!(*p->pd_name)) {

					} else {
			        	char cur_ifname[IFNAMSIZ + 1];
			        	// if (strcmp(p->pd_name, "virtio_pci") == 0) {
			        	// 	snprintf(cur_ifname, sizeof(cur_ifname), "vtnet%d", none_count++);
			        	// } else {
			        		snprintf(cur_ifname, sizeof(cur_ifname), "%s%d", p->pd_name, (int)p->pd_unit);
			        	// }

			        	if (ifname[0] && strcmp(ifname, cur_ifname) != 0)
			        		continue;

						memset(ip, 0, sizeof(ip));
#ifdef ZFS_ENABLE
						char kzfs_val[KENV_MVALLEN + 1];
						if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
							snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
#ifdef DEBUG
						printf("ZFS:%s\n", kzfs_val);
						printf("KENV:%s\n", KZFS_CONF_KENV_KEY);
#endif
						char param_name[KZFS_PARAM_MAX_LEN];
						snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip4_alias_0", cur_ifname);
						char *param_val = kzfs_get_uprop(kzfs_val, param_name);
						if (param_val != NULL)
							snprintf(ip, sizeof(ip), "%s", param_val);
#else
#ifdef DEBUG
						printf("%s\n", "NOZFS");
#endif
						snprintf(cmd, sizeof(cmd), "sysrc -f %s -in ifconfig_%s_ip4_alias_0", conf_file, cur_ifname);
						FILE *file = popen(cmd, "r");
						if (file != NULL) {
							fscanf(file, "%s", ip);
							pclose(file);
						}
#endif
						zval subitem;
						array_init(&subitem);
						if (ip[0] && strncmp(ip, "DHCP", 4) == 0) {
							zval subsub;
							array_init(&subsub);
					        add_assoc_null(&subsub, "netmask");
					        add_assoc_null(&subsub, "ip");
					        add_assoc_null(&subsub, "alias");
					        add_assoc_long(&subsub, "type", 2);
							add_next_index_zval(&subitem, &subsub);
							add_assoc_zval(return_value, cur_ifname, &subitem);
							continue;
						}
						ip4_alias_num = 0;
						ip6_alias_num = 0;
					    getifaddrs (&ifap);
					    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
					    	if (ifa->ifa_addr->sa_family != AF_INET && ifa->ifa_addr->sa_family != AF_INET6 && ifa->ifa_addr->sa_family != AF_LINK)
					    		continue;
					        if (ifa->ifa_addr->sa_family == AF_INET) {
					        	if (strcmp(cur_ifname, ifa->ifa_name) == 0) {
									zval subsub;
									array_init(&subsub);
								    struct sockaddr_in *sa;
								    char addr[16];
							        sa = (struct sockaddr_in *) ifa->ifa_addr;
							        inet_ntop(ifa->ifa_addr->sa_family, &sa->sin_addr, addr, sizeof(addr));
							        add_assoc_string(&subsub, "ip", addr);
							        sa = (struct sockaddr_in *) ifa->ifa_netmask;
							        inet_ntop(ifa->ifa_addr->sa_family, &sa->sin_addr, addr, sizeof(addr));
							        add_assoc_string(&subsub, "netmask", addr);
							        add_assoc_long(&subsub, "alias", ip4_alias_num++);
							        add_assoc_long(&subsub, "type", 0);
									add_next_index_zval(&subitem, &subsub);
							    }
						    } else if (ifa->ifa_addr->sa_family == AF_INET6) {
					        	if (strcmp(cur_ifname, ifa->ifa_name) == 0 && !(IN6_IS_ADDR_LINKLOCAL(&((struct sockaddr_in6 *) ifa->ifa_addr)->sin6_addr))) {
									zval subsub;
									array_init(&subsub);
								    struct sockaddr_in6 *sa;
								    char addr[39];
							        sa = (struct sockaddr_in6 *) ifa->ifa_addr;
							        inet_ntop(ifa->ifa_addr->sa_family, &sa->sin6_addr, addr, sizeof(addr));
							        add_assoc_string(&subsub, "ip6", addr);
							        unsigned char *c = ((struct sockaddr_in6 *)ifa->ifa_netmask)->sin6_addr.s6_addr;
							        int i = 0, j = 0;
					                unsigned char n = 0;
					                while (i < 16) {
					                    n = c[i];
					                    while (n > 0) {
					                        if (n & 1) j++;
					                        n = n/2;
					                    }
					                    i++;
					                }
					                add_assoc_long(&subsub, "prefix", j);
							        add_assoc_long(&subsub, "alias", ip6_alias_num++);
							        add_assoc_long(&subsub, "type", 1);
									add_next_index_zval(&subitem, &subsub);
							    }
					        } else if (ifa->ifa_addr->sa_family == AF_LINK) {
					        	if (strcmp(cur_ifname, ifa->ifa_name) == 0) {
					        		unsigned char *ptr;
					        		ptr = (unsigned char *)LLADDR((struct sockaddr_dl *)(ifa)->ifa_addr);
					        		char mac[45];
					        		snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x", *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4), *(ptr+5));
					        		add_assoc_string(&subitem, "mac", mac);
					        	}
					        }
						}
						add_assoc_zval(return_value, cur_ifname, &subitem);
					    freeifaddrs(ifap);
					}
				}
			}
		} while (pc.status == PCI_GETCONF_MORE_DEVS);
		close(fd);
	} else {
		char tmp_buf[strlen(ifaces_list) + 1];
		snprintf(tmp_buf, sizeof(tmp_buf), "%s", ifaces_list);
		int pref_check = INI_INT("kcs.net.prefix.enable");
		if (!pref_check) {
			char *cur_ifname_ini = strtok(tmp_buf, " ");
			while (cur_ifname_ini != NULL) {
	        	if (ifname[0] && strncmp(ifname, cur_ifname_ini, strlen(cur_ifname_ini)) != 0) {
					cur_ifname_ini = strtok(NULL, " ");
	        		continue;
	        	}
				zval subitem;
				array_init(&subitem);
				ip4_alias_num = 0;
				ip6_alias_num = 0;
			    getifaddrs (&ifap);
			    char cur_ifa_name[32];
			    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
			    	if (ifa->ifa_addr->sa_family != AF_INET && ifa->ifa_addr->sa_family != AF_INET6 && ifa->ifa_addr->sa_family != AF_LINK) 
			    		continue;
			        if (ifa->ifa_addr->sa_family == AF_INET) {
			        	if (strncmp(cur_ifname_ini, ifa->ifa_name, strlen(cur_ifname_ini)) == 0) {
					    	snprintf(cur_ifa_name, sizeof(cur_ifa_name), "%s", ifa->ifa_name);
							zval subsub;
							array_init(&subsub);
						    struct sockaddr_in *sa;
						    char addr[16];
					        sa = (struct sockaddr_in *) ifa->ifa_addr;
					        inet_ntop(ifa->ifa_addr->sa_family, &sa->sin_addr, addr, sizeof(addr));
					        add_assoc_string(&subsub, "ip", addr);
					        sa = (struct sockaddr_in *) ifa->ifa_netmask;
					        inet_ntop(ifa->ifa_addr->sa_family, &sa->sin_addr, addr, sizeof(addr));
					        add_assoc_string(&subsub, "netmask", addr);
					        add_assoc_long(&subsub, "alias", ip4_alias_num++);
					        add_assoc_long(&subsub, "type", 0);
							add_next_index_zval(&subitem, &subsub);
					    }
				    } else if (ifa->ifa_addr->sa_family == AF_INET6) {
			        	if (strncmp(cur_ifname_ini, ifa->ifa_name, strlen(cur_ifname_ini)) == 0 && !(IN6_IS_ADDR_LINKLOCAL(&((struct sockaddr_in6 *) ifa->ifa_addr)->sin6_addr))) {
					    	snprintf(cur_ifa_name, sizeof(cur_ifa_name), "%s", ifa->ifa_name);
							zval subsub;
							array_init(&subsub);
						    struct sockaddr_in6 *sa;
						    char addr[39];
					        sa = (struct sockaddr_in6 *) ifa->ifa_addr;
					        inet_ntop(ifa->ifa_addr->sa_family, &sa->sin6_addr, addr, sizeof(addr));
					        add_assoc_string(&subsub, "ip6", addr);
					        unsigned char *c = ((struct sockaddr_in6 *)ifa->ifa_netmask)->sin6_addr.s6_addr;
					        int i = 0, j = 0;
			                unsigned char n = 0;
			                while (i < 16) {
			                    n = c[i];
			                    while (n > 0) {
			                        if (n & 1) j++;
			                        n = n/2;
			                    }
			                    i++;
			                }
			                add_assoc_long(&subsub, "prefix", j);
					        add_assoc_long(&subsub, "alias", ip6_alias_num++);
					        add_assoc_long(&subsub, "type", 1);
							add_next_index_zval(&subitem, &subsub);
					    }
			        } else if (ifa->ifa_addr->sa_family == AF_LINK) {
			        	if (strncmp(cur_ifname_ini, ifa->ifa_name, strlen(cur_ifname_ini)) == 0) {
					    	snprintf(cur_ifa_name, sizeof(cur_ifa_name), "%s", ifa->ifa_name);
			        		unsigned char *ptr;
			        		ptr = (unsigned char *)LLADDR((struct sockaddr_dl *)(ifa)->ifa_addr);
			        		char mac[45];
			        		snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x", *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4), *(ptr+5));
			        		add_assoc_string(&subitem, "mac", mac);
							memset(ip, 0, sizeof(ip));
#ifdef ZFS_ENABLE
						char kzfs_val[KENV_MVALLEN + 1];
						if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
							snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
#ifdef DEBUG
						printf("ZFS:%s\n", kzfs_val);
						printf("KENV:%s\n", KZFS_CONF_KENV_KEY);
#endif
						char param_name[KZFS_PARAM_MAX_LEN];
						snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip4_alias_0", ifa->ifa_name);
						char *param_val = kzfs_get_uprop(kzfs_val, param_name);
						if (param_val != NULL)
							snprintf(ip, sizeof(ip), "%s", param_val);
#else
#ifdef DEBUG
						printf("%s\n", "NOZFS");
#endif
							snprintf(cmd, sizeof(cmd), "sysrc -f %s -in ifconfig_%s_ip4_alias_0", conf_file, ifa->ifa_name);
							FILE *file = popen(cmd, "r");
							if (file != NULL) {
								fscanf(file, "%s", ip);
								pclose(file);
							}
#endif
							if (ip[0] && strncmp(ip, "DHCP", 4) == 0) {
								zval subsub;
								array_init(&subsub);
						        add_assoc_null(&subsub, "netmask");
						        add_assoc_null(&subsub, "ip");
						        add_assoc_null(&subsub, "alias");
						        add_assoc_long(&subsub, "type", 2);
								add_next_index_zval(&subitem, &subsub);
							}
			        	}
			        }
				}
				if (!zend_hash_num_elements(Z_ARRVAL_P(&subitem))) {
					add_assoc_null(return_value, cur_ifname_ini);
				} else {
					add_assoc_zval(return_value, cur_ifa_name, &subitem);
				}
			    freeifaddrs(ifap);
				cur_ifname_ini = strtok(NULL, " ");
			}
		} else {
		    getifaddrs (&ifap);
			char *cur_ifname_ini = strtok(tmp_buf, " ");
			zval subitem;
			array_init(&subitem);
		    char cur_ifa_name[32];
		    memset(cur_ifa_name, 0, sizeof(cur_ifa_name));
			ip4_alias_num = 0;
			ip6_alias_num = 0;
		    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		    	if (ifa->ifa_addr->sa_family != AF_INET && ifa->ifa_addr->sa_family != AF_INET6 && ifa->ifa_addr->sa_family != AF_LINK) 
		    		continue;
		    	if (cur_ifa_name[0] && strcmp(cur_ifa_name, ifa->ifa_name) != 0) {
					char tmp_buf[strlen(ifaces_list) + 1];
					snprintf(tmp_buf, sizeof(tmp_buf), "%s", ifaces_list);
					char *cur_ifname_ini = strtok(tmp_buf, " ");
					while (cur_ifname_ini != NULL) {
						if (strncmp(cur_ifa_name, cur_ifname_ini, strlen(cur_ifname_ini)) == 0)
							flag = 1;
						cur_ifname_ini = strtok(NULL, " ");
					}
					if (flag)
						add_assoc_zval(return_value, cur_ifa_name, &subitem);
					flag = 0;
					ip4_alias_num = 0;
					ip6_alias_num = 0;
					array_init(&subitem);
		    	}
		    	snprintf(cur_ifa_name, sizeof(cur_ifa_name), "%s", ifa->ifa_name);
		        if (ifa->ifa_addr->sa_family == AF_INET) {
					zval subsub;
					array_init(&subsub);
				    struct sockaddr_in *sa;
				    char addr[16];
			        sa = (struct sockaddr_in *) ifa->ifa_addr;
			        inet_ntop(ifa->ifa_addr->sa_family, &sa->sin_addr, addr, sizeof(addr));
			        add_assoc_string(&subsub, "ip", addr);
			        sa = (struct sockaddr_in *) ifa->ifa_netmask;
			        inet_ntop(ifa->ifa_addr->sa_family, &sa->sin_addr, addr, sizeof(addr));
			        add_assoc_string(&subsub, "netmask", addr);
			        add_assoc_long(&subsub, "alias", ip4_alias_num++);
			        add_assoc_long(&subsub, "type", 0);
					add_next_index_zval(&subitem, &subsub);
			    } else if (ifa->ifa_addr->sa_family == AF_INET6) {
					zval subsub;
					array_init(&subsub);
				    struct sockaddr_in6 *sa;
				    char addr[39];
			        sa = (struct sockaddr_in6 *) ifa->ifa_addr;
			        inet_ntop(ifa->ifa_addr->sa_family, &sa->sin6_addr, addr, sizeof(addr));
			        add_assoc_string(&subsub, "ip6", addr);
			        unsigned char *c = ((struct sockaddr_in6 *)ifa->ifa_netmask)->sin6_addr.s6_addr;
			        int i = 0, j = 0;
	                unsigned char n = 0;
	                while (i < 16) {
	                    n = c[i];
	                    while (n > 0) {
	                        if (n & 1) j++;
	                        n = n/2;
	                    }
	                    i++;
	                }
	                add_assoc_long(&subsub, "prefix", j);
			        add_assoc_long(&subsub, "alias", ip6_alias_num++);
			        add_assoc_long(&subsub, "type", 1);
					add_next_index_zval(&subitem, &subsub);
		        } else if (ifa->ifa_addr->sa_family == AF_LINK) {
			    	snprintf(cur_ifa_name, sizeof(cur_ifa_name), "%s", ifa->ifa_name);
	        		unsigned char *ptr;
	        		ptr = (unsigned char *)LLADDR((struct sockaddr_dl *)(ifa)->ifa_addr);
	        		char mac[45];
	        		snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x", *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4), *(ptr+5));
	        		add_assoc_string(&subitem, "mac", mac);
					memset(ip, 0, sizeof(ip));
#ifdef ZFS_ENABLE
						char kzfs_val[KENV_MVALLEN + 1];
						if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
							snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
#ifdef DEBUG
						printf("ZFS:%s\n", kzfs_val);
						printf("KENV:%s\n", KZFS_CONF_KENV_KEY);
#endif
						char param_name[KZFS_PARAM_MAX_LEN];
						snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip4_alias_0", ifa->ifa_name);
						char *param_val = kzfs_get_uprop(kzfs_val, param_name);
						if (param_val != NULL)
							snprintf(ip, sizeof(ip), "%s", param_val);
#else
#ifdef DEBUG
						printf("%s\n", "NOZFS");
#endif
					snprintf(cmd, sizeof(cmd), "sysrc -f %s -in ifconfig_%s_ip4_alias_0", conf_file, ifa->ifa_name);
					FILE *file = popen(cmd, "r");
					if (file != NULL) {
						fscanf(file, "%s", ip);
						pclose(file);
					}
#endif
					if (ip[0] && strncmp(ip, "DHCP", 4) == 0) {
						zval subsub;
						array_init(&subsub);
				        add_assoc_null(&subsub, "netmask");
				        add_assoc_null(&subsub, "ip");
				        add_assoc_null(&subsub, "alias");
				        add_assoc_long(&subsub, "type", 2);
						add_next_index_zval(&subitem, &subsub);
					}
		        }
		    }
	    	if (cur_ifa_name[0]) {
				char tmp_buf[strlen(ifaces_list) + 1];
				snprintf(tmp_buf, sizeof(tmp_buf), "%s", ifaces_list);
				char *cur_ifname_ini = strtok(tmp_buf, " ");
				while (cur_ifname_ini != NULL) {
					if (strncmp(cur_ifa_name, cur_ifname_ini, strlen(cur_ifname_ini)) == 0)
						flag = 1;
					cur_ifname_ini = strtok(NULL, " ");
				}
				if (flag)
					add_assoc_zval(return_value, cur_ifa_name, &subitem);
				flag = 0;
				ip4_alias_num = 0;
				ip6_alias_num = 0;
				array_init(&subitem);
	    	}
		    freeifaddrs(ifap);
		}
	}
}

PHP_FUNCTION(ips_set)
{
	char *ifname = "";
	size_t ifname_len = sizeof("")-1;
	zval *ips;
	zend_bool enabled;
	zend_long settype;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sabl", &ifname, &ifname_len, &ips, &enabled, &settype) == FAILURE) { 
	   return;
	}
	if (settype == PHP_NET_IP_SYSTEM || settype == PHP_NET_IP_AUTO)
		kill_dhcp(ifname);
	zval *inner;

	if (ips && Z_TYPE_P(ips) == IS_ARRAY) {
		HashTable *ht = Z_ARRVAL_P(ips);
		long i = 0;
		if (settype == PHP_NET_IP_CONF || settype == PHP_NET_IP_AUTO)
			remove_all_ip_conf(ifname);
		if (settype == PHP_NET_IP_SYSTEM || settype == PHP_NET_IP_AUTO)
			remove_all_ip(ifname);
		while (zend_hash_index_exists(ht, i) && (inner = zend_hash_index_find(ht, i)) != NULL) {
			if (Z_TYPE_P(inner) == IS_ARRAY) {
				zval *item;
				long type;
				if ((item = zend_hash_str_find(Z_ARRVAL_P(inner), "type", sizeof("type")-1)) != NULL && Z_TYPE_P(item) == IS_LONG)
				{
					type = Z_LVAL_P(item);
					if (type == 0) {
						char *ip, *mask;
						long alias_num;
						if ((item = zend_hash_str_find(Z_ARRVAL_P(inner), "ip", sizeof("ip")-1)) != NULL && Z_TYPE_P(item) == IS_STRING)
						{
							ip = Z_STRVAL_P(item);
						} else {
							i++;
#ifdef ZWARN
							php_error_docref(NULL, E_WARNING, "'ip' key not found");
#endif
							continue;
						}
						if ((item = zend_hash_str_find(Z_ARRVAL_P(inner), "netmask", sizeof("netmask")-1)) != NULL && Z_TYPE_P(item) == IS_STRING)
						{
							mask = Z_STRVAL_P(item);
						} else {
							i++;
#ifdef ZWARN
							php_error_docref(NULL, E_WARNING, "'netmask' key not found");
#endif
							continue;
						}
						if ((item = zend_hash_str_find(Z_ARRVAL_P(inner), "alias", sizeof("alias")-1)) != NULL && Z_TYPE_P(item) == IS_LONG)
						{
							alias_num = Z_LVAL_P(item);
						} else {
							i++;
#ifdef ZWARN
							php_error_docref(NULL, E_WARNING, "'alias' key not found");
#endif
							continue;
						}
						add_new_ip4(ifname, alias_num, ip, mask, enabled, settype);
					} else if (type == 1) {
						char *ip;
						long alias_num, prefix;
						if ((item = zend_hash_str_find(Z_ARRVAL_P(inner), "ip", sizeof("ip")-1)) != NULL && Z_TYPE_P(item) == IS_STRING)
						{
							ip = Z_STRVAL_P(item);
						} else {
							i++;
#ifdef ZWARN
							php_error_docref(NULL, E_WARNING, "'ip' key not found");
#endif
							continue;
						}
						if ((item = zend_hash_str_find(Z_ARRVAL_P(inner), "prefix", sizeof("prefix")-1)) != NULL && Z_TYPE_P(item) == IS_LONG)
						{
							prefix = Z_LVAL_P(item);
						} else {
							i++;
#ifdef ZWARN
							php_error_docref(NULL, E_WARNING, "'prefix' key not found");
#endif
							continue;
						}
						if ((item = zend_hash_str_find(Z_ARRVAL_P(inner), "alias", sizeof("alias")-1)) != NULL && Z_TYPE_P(item) == IS_LONG)
						{
							alias_num = Z_LVAL_P(item);
						} else {
							i++;
#ifdef ZWARN
							php_error_docref(NULL, E_WARNING, "'alias' key not found");
#endif
							continue;
						}
						add_new_ip6(ifname, alias_num, ip, prefix, enabled, settype);
					}
				}
			}
			i++;
		}
	} else {
		RETURN_FALSE;
	}

	RETURN_TRUE;
}

PHP_FUNCTION(dhcp_set_ip)
{
	char *ifname = "";
	size_t ifname_len = sizeof("")-1;
	zend_long settype;
	zend_bool enabled;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sbl", &ifname, &ifname_len, &enabled, &settype) == FAILURE) { 
	   return;
	}

	if (enabled && (settype == PHP_NET_IP_SYSTEM || settype == PHP_NET_IP_AUTO)) {
		kill_dhcp(ifname);
		remove_all_ip(ifname);
	}
	if (settype == PHP_NET_IP_CONF || settype == PHP_NET_IP_AUTO)
		remove_all_ip_conf(ifname);
#ifdef ZFS_ENABLE
	char *main_cmd = INI_STR("kcs.dhcpd.cmd");
	char *cmd_flags = INI_STR("kcs.dhcpd.flags");
	char *cmd_name = INI_STR("kcs.dhcpd.name");
	if (settype == PHP_NET_IP_CONF || settype == PHP_NET_IP_AUTO) {
		char kzfs_val[KENV_MVALLEN + 1];
		if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
			snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
		char param_name[KZFS_PARAM_MAX_LEN];
		snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip4_alias_0", ifname);
		kzfs_set_uprop(kzfs_val, param_name, "DHCP");
		snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip4_aliases", ifname);
		kzfs_set_uprop(kzfs_val, param_name, "0");
		snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip4_netmask_0", ifname);
		kzfs_set_uprop(kzfs_val, param_name, "0");
		snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_enable", ifname);
		kzfs_set_uprop(kzfs_val, param_name, enabled ? "true" : "false");

		if (settype == PHP_NET_IP_AUTO) {
			snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_flags", ifname);
			kzfs_set_uprop(kzfs_val, param_name, "up");
		}
	}

	if (enabled && (settype == PHP_NET_IP_SYSTEM || settype == PHP_NET_IP_AUTO)) {
		if (cmd_flags == NULL) {
			if (base_execl(main_cmd, cmd_name, ifname, NULL))
				RETURN_FALSE;
		} else {
			if (base_execl(main_cmd, cmd_name, cmd_flags, ifname, NULL))
				RETURN_FALSE;
		}
	}
#else
	setenv("PATH", path, 1);
	char cmd[NET_CMD_MAX_LEN];
	if (enabled && (settype == PHP_NET_IP_SYSTEM || settype == PHP_NET_IP_AUTO)) {
		snprintf(cmd, sizeof(cmd), "dhclient -b %s 2>/dev/null", ifname);
		system(cmd);
	}
	if (settype == PHP_NET_IP_CONF || settype == PHP_NET_IP_AUTO) {
		char *conf_file = INI_STR("kcs.net.conf");
		snprintf(cmd, sizeof(cmd), "sysrc -f %s ifconfig_%s_ip4_alias_0=\"DHCP\" 1>/dev/null", conf_file, ifname);
		system(cmd);
		snprintf(cmd, sizeof(cmd), "sysrc -f %s ifconfig_%s_ip4_aliases=\"0\" 1>/dev/null", conf_file, ifname);
		system(cmd);
		snprintf(cmd, sizeof(cmd), "sysrc -f %s ifconfig_%s_ip4_netmask_0=\"0\" 1>/dev/null", conf_file, ifname);
		system(cmd);
		if (settype == PHP_NET_IP_AUTO) {
			snprintf(cmd, sizeof(cmd), "sysrc -f %s ifconfig_%s_flags=\"up\" 1>/dev/null", conf_file, ifname);
			system(cmd);
		}
	}

#endif
	RETURN_TRUE;
}

PHP_FUNCTION(dhcp_del_ip)
{
	char *ifname = "";
	size_t ifname_len = sizeof("")-1;
	zend_long settype;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &ifname, &ifname_len, &settype) == FAILURE) { 
	   return;
	}

	char *dhcp_pid_path = INI_STR("kcs.dhcp.pid_path");
	char *dhcp_pid_format = INI_STR("kcs.dhcp.pid_format");
	char pid_file[NET_DHCP_PID_PATH_LEN];
	snprintf(pid_file, sizeof(pid_file), dhcp_pid_format, dhcp_pid_path, ifname);

	if (settype == PHP_NET_IP_SYSTEM || settype == PHP_NET_IP_AUTO) {
		if (access(pid_file, F_OK) == 0) {
			kill_dhcp(ifname);
			remove_all_ip(ifname);
		} else {
			RETURN_FALSE;
		}
	}
	if (settype == PHP_NET_IP_CONF || settype == PHP_NET_IP_AUTO) {
		remove_all_ip_conf(ifname);
	}

	RETURN_TRUE;
}

PHP_FUNCTION(dns_unset)
{
#ifdef ZFS_ENABLE
	char kzfs_val[KENV_MVALLEN + 1];
	if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
		snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
	char param_name[KZFS_PARAM_MAX_LEN];
	snprintf(param_name, sizeof(param_name), "net:%s", "dns");
	kzfs_del_uprop(kzfs_val, param_name);
#else
	char *conf_file = INI_STR("kcs.net.conf");
	setenv("PATH", path, 1);
	char cmd[NET_CMD_MAX_LEN];
	snprintf(cmd, sizeof(cmd), "sysrc -f %s -ix dns", conf_file);
	system(cmd);
#endif
	FILE *file = fopen(_PATH_RESCONF, "w");
	if (file != NULL)
		fclose(file);
	RETURN_TRUE;
}

PHP_FUNCTION(dns_get)
{
	setenv("PATH", path, 1);
	char res[10];
	memset(res, 0, sizeof(res));
	is_dns_dhcp(res);
	if (res[0] && strcmp(res, "DHCP") == 0)
		RETURN_STRING(res);

	FILE *file = fopen(_PATH_RESCONF, "r");
	if (file == NULL) {
		RETURN_FALSE;
	}

	array_init(return_value);
    char *key;
    char *value;
    char str[128];
    int i = 1;
    int check = 1;
    while(fscanf(file, "%[^\n]\n", str) != -1)
        if (str[0] && str[0] != '#') {
        	check = 0;
            key = str;
            if ((value = strchr(str, ' ')) != NULL) {
                *value = '\0';
                value += 1;
                if (strcmp(key, "nameserver") == 0) {
                	char tmp[5];
                	snprintf(tmp, 5, "dns%d", i++);
                	add_assoc_string(return_value, tmp, value);
                } else {
                	add_assoc_string(return_value, "domain", value);
                }
            }
        }
    if (i == 2)
    	add_assoc_null(return_value, "dns2");

    fclose(file);

    if (check)
    	RETURN_FALSE;

}


PHP_FUNCTION(dns_set)
{
	char *domain = "";
	size_t domain_len = sizeof("")-1;
	char *dns1 = "";
	size_t dns1_len = sizeof("")-1;
	char *dns2 = "";
	size_t dns2_len = sizeof("")-1;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|s", &domain, &domain_len, &dns1, &dns1_len, &dns2, &dns2_len) == FAILURE) { 
	   return;
	}

#ifdef ZFS_ENABLE
	char kzfs_val[KENV_MVALLEN + 1];
	if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
		snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
	char param_name[KZFS_PARAM_MAX_LEN];
	snprintf(param_name, sizeof(param_name), "net:%s", "dns");
	kzfs_del_uprop(kzfs_val, param_name);
#else
	char *conf_file = INI_STR("kcs.net.conf");
	setenv("PATH", path, 1);
	char cmd[NET_CMD_MAX_LEN];
	snprintf(cmd, sizeof(cmd), "sysrc -f %s -ix dns", conf_file);
	system(cmd);
#endif
	FILE *file = fopen(_PATH_RESCONF, "w");
	if (file == NULL) {
		RETURN_FALSE;
	}

	fprintf(file, "search %s\n", domain);
	fprintf(file, "nameserver %s\n", dns1);
	if (dns2[0])
		fprintf(file, "nameserver %s", dns2);

	fclose(file);
	RETURN_TRUE;
}


PHP_FUNCTION(dns_set_dhcp)
{
#ifdef ZFS_ENABLE
	char kzfs_val[KENV_MVALLEN + 1];
	if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
		snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
	char param_name[KZFS_PARAM_MAX_LEN];
	snprintf(param_name, sizeof(param_name), "net:%s", "dns");
	kzfs_set_uprop(kzfs_val, param_name, "DHCP");
#else
	char *conf_file = INI_STR("kcs.net.conf");
	setenv("PATH", path, 1);
	char cmd[NET_CMD_MAX_LEN];
	snprintf(cmd, sizeof(cmd), "sysrc -f %s -i dns=\"DHCP\" 1>/dev/null", conf_file);
	system(cmd);
#endif
	FILE *file = fopen(_PATH_RESCONF, "w");
	if (file != NULL)
		fclose(file);
	RETURN_TRUE;
}

PHP_FUNCTION(route_add)
{
	zend_long type;			// net = 0, host = 1
	char *name = "";
	char *dest = "";
	char *gtw = "";
	zend_long status;

	size_t name_len = sizeof("")-1;
	size_t dest_len = sizeof("")-1;
	size_t gtw_len = sizeof("")-1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lsssl", &type, &name, &name_len, &dest, &dest_len, &gtw, &gtw_len, &status) == FAILURE) { 
	   return;
	}

	if (type != 0 && type != 1)
		RETURN_FALSE;
	if (type == 0 && strchr(dest, '/') == NULL)
		RETURN_FALSE;
	if (!php_net_check_route(name)) {
		RETURN_FALSE;
	}
  
#ifdef ZFS_ENABLE
	char kzfs_val[KENV_MVALLEN + 1];
	if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
		snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
	char param_int[11];
	snprintf(param_int, sizeof(param_int), "%ld", status);
	char param_name[KZFS_PARAM_MAX_LEN];
	snprintf(param_name, sizeof(param_name), "net:route_static_%s_status", name);
	if (kzfs_set_uprop(kzfs_val, param_name, param_int) < 0)
		RETURN_FALSE;
	snprintf(param_name, sizeof(param_name), "net:route_static_%s_dest", name);
	kzfs_set_uprop(kzfs_val, param_name, dest);
	snprintf(param_name, sizeof(param_name), "net:route_static_%s_gtw", name);
	kzfs_set_uprop(kzfs_val, param_name, gtw);
	snprintf(param_int, sizeof(param_int), "%ld", type);
	snprintf(param_name, sizeof(param_name), "net:route_static_%s_type", name);
	kzfs_set_uprop(kzfs_val, param_name, param_int);
	char *route_list = kzfs_get_uprop(kzfs_val, "net:route_list");
	char new_route_list[(route_list ? strlen(route_list) : 0) + strlen(name) + 2];
	snprintf(new_route_list, sizeof(new_route_list), "%s%s%s", route_list ? route_list : "", route_list ? "," : "", name);

	kzfs_set_uprop(kzfs_val, "net:route_list", new_route_list);
#else
	char *conf_file = INI_STR("kcs.net.conf");
	setenv("PATH", path, 1);
	char cmd[NET_CMD_MAX_LEN];
	snprintf(cmd, sizeof(cmd), "sysrc -f %s route_static_%s_status=\"%ld\" 1>/dev/null", conf_file, name, status);
	system(cmd);
	snprintf(cmd, sizeof(cmd), "sysrc -f %s route_static_%s_dest=\"%s\" 1>/dev/null", conf_file, name, dest);
	system(cmd);
	snprintf(cmd, sizeof(cmd), "sysrc -f %s route_static_%s_gtw=\"%s\" 1>/dev/null", conf_file, name, gtw);
	system(cmd);
	snprintf(cmd, sizeof(cmd), "sysrc -f %s route_static_%s_type=\"%ld\" 1>/dev/null", conf_file, name, type);
	system(cmd);
#endif
	int error = 0;
	if (status == 1) {
		TAILQ_INIT(&fibl_head);
		if (fill_fibs(&fibl_head) != 0)
			RETURN_FALSE;
		struct fibl *fl;
		int flags = 0, seq = 0, rtm_addrs = 0;

		flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;
		rtm_addrs = RTA_DST | RTA_GATEWAY;
		fill_sockaddr_storage(RTAX_DST, dest);
		if (type == 1) {
			flags |= RTF_HOST;
		} else {
			rtm_addrs |= RTA_NETMASK;
		}
		fill_sockaddr_storage(RTAX_GATEWAY, gtw);
		TAILQ_FOREACH(fl, &fibl_head, fl_next) {
			error += rtmsg(RTM_ADD, flags, rtm_addrs, &seq, fl->fl_num);
		}
	}
	if (error)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(route_del)
{
	char *name = "";
	size_t name_len = sizeof("")-1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &name, &name_len) == FAILURE) { 
	   return;
	}

	char dest[20];
	char gtw[20];
	char type[2];
	memset(dest, 0, sizeof(dest));
	memset(gtw, 0, sizeof(gtw));
	memset(type, 0, sizeof(type));


	get_route_conf(dest, "dest", name);
	get_route_conf(gtw, "gtw", name);
	get_route_conf(type, "type", name);

	if (!dest[0] || !gtw[0] || !type[0])
		RETURN_FALSE;

	if (*type != '0' && *type != '1')
		RETURN_FALSE;

	TAILQ_INIT(&fibl_head);
	if (fill_fibs(&fibl_head) != 0)
		RETURN_FALSE;

	struct fibl *fl;
	int flags = 0, seq = 0, rtm_addrs = 0;

	flags = RTF_UP | RTF_GATEWAY | RTF_STATIC | RTF_PINNED;
	rtm_addrs = RTA_DST | RTA_GATEWAY;
	fill_sockaddr_storage(RTAX_DST, dest);
	if (*type == '1') {
		flags |= RTF_HOST;
	} else {
		rtm_addrs |= RTA_NETMASK;
	}
	fill_sockaddr_storage(RTAX_GATEWAY, gtw);

	int error = 0;

	TAILQ_FOREACH(fl, &fibl_head, fl_next) {
		error += rtmsg(RTM_DELETE, flags, rtm_addrs, &seq, fl->fl_num);
	}
	if (error)
		RETURN_FALSE;

#ifdef ZFS_ENABLE
	char kzfs_val[KENV_MVALLEN + 1];
	if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
		snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
	char param_name[KZFS_PARAM_MAX_LEN];
	snprintf(param_name, sizeof(param_name), "net:route_static_%s_status", name);
	kzfs_del_uprop(kzfs_val, param_name);
	snprintf(param_name, sizeof(param_name), "net:route_static_%s_dest", name);
	kzfs_del_uprop(kzfs_val, param_name);
	snprintf(param_name, sizeof(param_name), "net:route_static_%s_gtw", name);
	kzfs_del_uprop(kzfs_val, param_name);
	snprintf(param_name, sizeof(param_name), "net:route_static_%s_type", name);
	kzfs_del_uprop(kzfs_val, param_name);
	char *route_list = kzfs_get_uprop(kzfs_val, "net:route_list");
	char new_route_list[strlen(route_list) + 1];
	memset(new_route_list, 0, sizeof(new_route_list));
	char old_route_list[strlen(route_list) + 1];
	snprintf(old_route_list, sizeof(old_route_list), "%s", route_list);
	char *tmp = strtok(old_route_list, ",");
	int counter = 0;
	while (tmp != NULL) {
		if (strcmp(tmp, name) != 0) {
			snprintf(new_route_list, sizeof(new_route_list), "%s%s%s", new_route_list[0] ? new_route_list : "", new_route_list[0] ? "," : "", tmp);
			counter++;
		}
		tmp = strtok(NULL, ",");
	}
	kzfs_set_uprop(kzfs_val, "net:route_list", new_route_list);
#else
	char *conf_file = INI_STR("kcs.net.conf");
	setenv("PATH", path, 1);
	char cmd[NET_CMD_MAX_LEN];
	snprintf(cmd, sizeof(cmd), "sysrc -f %s -ix route_static_%s_status", conf_file, name);
	system(cmd);
	snprintf(cmd, sizeof(cmd), "sysrc -f %s -ix route_static_%s_dest", conf_file, name);
	system(cmd);
	snprintf(cmd, sizeof(cmd), "sysrc -f %s -ix route_static_%s_gtw", conf_file, name);
	system(cmd);
	snprintf(cmd, sizeof(cmd), "sysrc -f %s -ix route_static_%s_type", conf_file, name);
	system(cmd);
#endif


	RETURN_TRUE;
}

PHP_FUNCTION(route_set_default)
{
	char *gtw = "";
	size_t gtw_len = sizeof("")-1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &gtw, &gtw_len) == FAILURE) { 
	   return;
	}

#ifdef ZFS_ENABLE
	char kzfs_val[KENV_MVALLEN + 1];
	if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
		snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
	char param_name[KZFS_PARAM_MAX_LEN];
	snprintf(param_name, sizeof(param_name), "net:%s", "route_default");
	kzfs_set_uprop(kzfs_val, param_name, gtw);
#else
	char *conf_file = INI_STR("kcs.net.conf");
	setenv("PATH", path, 1);
	char cmd[NET_CMD_MAX_LEN];
	snprintf(cmd, sizeof(cmd), "sysrc -f %s route_default=\"%s\" 1>/dev/null", conf_file, gtw);
	system(cmd);
#endif
	char dest[] = "0.0.0.0/0";

	TAILQ_INIT(&fibl_head);
	if (fill_fibs(&fibl_head) != 0)
		RETURN_FALSE;

	struct fibl *fl;
	int flags = 0, seq = 0, rtm_addrs = 0;

	flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;
	rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
	fill_sockaddr_storage(RTAX_DST, dest);
	fill_sockaddr_storage(RTAX_GATEWAY, gtw);

	int error = 0;

	TAILQ_FOREACH(fl, &fibl_head, fl_next) {
		error += rtmsg(RTM_ADD, flags, rtm_addrs, &seq, fl->fl_num);
	}

	if (error)
		RETURN_FALSE;

	RETURN_TRUE;
}


PHP_FUNCTION(route_del_default)
{
	char dest[] = "0.0.0.0/0";

	TAILQ_INIT(&fibl_head);
	if (fill_fibs(&fibl_head) != 0)
		RETURN_FALSE;

	struct fibl *fl;
	int flags = 0, seq = 0, rtm_addrs = 0;

	flags = RTF_UP | RTF_GATEWAY | RTF_STATIC | RTF_PINNED;
	rtm_addrs = RTA_DST | RTA_NETMASK;
	fill_sockaddr_storage(RTAX_DST, dest);

	int error = 0;

	TAILQ_FOREACH(fl, &fibl_head, fl_next) {
		error += rtmsg(RTM_DELETE, flags, rtm_addrs, &seq, fl->fl_num);
	}

	if (error)
		RETURN_FALSE;

#ifdef ZFS_ENABLE
	char kzfs_val[KENV_MVALLEN + 1];
	if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
		snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
	char param_name[KZFS_PARAM_MAX_LEN];
	snprintf(param_name, sizeof(param_name), "net:%s", "route_default");
	kzfs_del_uprop(kzfs_val, param_name);
#else
	char *conf_file = INI_STR("kcs.net.conf");
	setenv("PATH", path, 1);
	char cmd[NET_CMD_MAX_LEN];
	snprintf(cmd, sizeof(cmd), "sysrc -f %s -ix route_default", conf_file);
	system(cmd);
#endif

	RETURN_TRUE;
}

PHP_FUNCTION(routes_get)
{
    char status[2];
    char dest[20];
    char gtw[20];
    char type[2];
	char def[20];
	memset(def, 0, sizeof(def));
	array_init(return_value);

	int flag = 0;
	zval subitem;

#ifdef ZFS_ENABLE
	char kzfs_val[KENV_MVALLEN + 1];
	if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
		snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);

	char *route_list = kzfs_get_uprop(kzfs_val, "net:route_list");
	char param_name[KZFS_PARAM_MAX_LEN];
	if (route_list != NULL) {
		char *route_list_st = malloc(strlen(route_list) + 1);
		if (route_list_st == NULL)
			RETURN_FALSE;
		snprintf(route_list_st, strlen(route_list) + 1, "%s", route_list);
		char *route_name = strtok(route_list_st, ",");
		while (route_name != NULL) {
			flag = 1;
			array_init(&subitem);
			snprintf(param_name, sizeof(param_name), "net:route_static_%s_dest", route_name);
			char *param_val = kzfs_get_uprop(kzfs_val, param_name);
			if (param_val != NULL) {
				snprintf(dest, sizeof(dest), "%s", param_val);
                add_assoc_string(&subitem, "dest", param_val);
			} else {
				memset(dest, 0, sizeof(dest));
                add_assoc_null(&subitem, "dest");
			}
			snprintf(param_name, sizeof(param_name), "net:route_static_%s_gtw", route_name);
			param_val = kzfs_get_uprop(kzfs_val, param_name);
			if (param_val != NULL) {
                add_assoc_string(&subitem, "gtw", param_val);
			} else {
                add_assoc_null(&subitem, "gtw");
			}
			snprintf(param_name, sizeof(param_name), "net:route_static_%s_type", route_name);
			param_val = kzfs_get_uprop(kzfs_val, param_name);
			if (param_val != NULL) {
				snprintf(type, sizeof(type), "%s", param_val);
                add_assoc_long(&subitem, "type", strtol(param_val, NULL, 10));
			} else {
				memset(type, 0, sizeof(type));
                add_assoc_null(&subitem, "type");
			}
			snprintf(param_name, sizeof(param_name), "net:route_static_%s_status", route_name);
			param_val = kzfs_get_uprop(kzfs_val, param_name);
            int error = check_route(type, dest);
			if (param_val != NULL) {
	            if (error && *param_val == '1')
	            	add_assoc_long(&subitem, "status", 2);
	            else
	            	add_assoc_long(&subitem, "status", strtol(param_val, NULL, 10));
			} else {
                add_assoc_null(&subitem, "status");
			}
            add_assoc_zval(return_value, route_name, &subitem);
			route_name = strtok(NULL, ",");
		}
		free(route_list_st);
	}
	snprintf(param_name, sizeof(param_name), "net:%s", "route_default");
	char *param_val = kzfs_get_uprop(kzfs_val, param_name);
	if (param_val != NULL)
		snprintf(def, sizeof(def), "%s", param_val);
#else
	char *conf_file = INI_STR("kcs.net.conf");
	FILE *file = fopen(conf_file, "r");
    if (file != NULL) {
        char str[NET_CONF_LINE_MAX_LEN];
        char name[NET_ROUTE_NAME_MAX_LEN];
        while(fgets(str, sizeof(str), file)) {
            if (strncmp(str, "route_static_", 13) == 0) {
            	str[strlen(str)-2] = '\0';
                char *key = str + 13;
                char *val = strchr(str, '=');
                *val = '\0';
                val += 2;
                if (*(val - 3) == 's') {
                    *(val - 9) = '\0';

					array_init(&subitem);
					flag = 1;
                    snprintf(name, sizeof(name), "%s", key);
                    snprintf(status, sizeof(status), "%s", val);
                } else if (*(val - 3) == 't') {
                    snprintf(dest, sizeof(dest), "%s", val);
                    add_assoc_string(&subitem, "dest", val);

                } else if (*(val - 3) == 'w') {
                    snprintf(gtw, sizeof(gtw), "%s", val);
                    add_assoc_string(&subitem, "gtw", val);
                } else {
                    snprintf(type, sizeof(type), "%s", val);
                    add_assoc_long(&subitem, "type", strtol(val, NULL, 10));
                    int error = check_route(type, dest);
                    if (error && *status == '1')
                    	add_assoc_long(&subitem, "status", 2);
                    else
                    	add_assoc_long(&subitem, "status", strtol(status, NULL, 10));
                    add_assoc_zval(return_value, name, &subitem);
                }
            }
        }
        fclose(file);
		setenv("PATH", path, 1);
		char cmd[NET_CMD_MAX_LEN];
		snprintf(cmd, sizeof(cmd), "sysrc -f %s -in route_default", conf_file);
		file = popen(cmd, "r");
		if (file != NULL) {
			fscanf(file, "%s", def);
			pclose(file);
		}
	}
#endif
	array_init(&subitem);
	add_assoc_null(&subitem, "type");
	add_assoc_null(&subitem, "dest");
	if (def[0]) {
		flag = 1;
		add_assoc_string(&subitem, "gtw", def);
		char def_sys[20];
		if (get_def_route(def_sys, 20)) {
			add_assoc_long(&subitem, "status", 2);
		} else {
			if (strcmp(def, def_sys)) {
				add_assoc_long(&subitem, "status", 2);
			} else {
				add_assoc_long(&subitem, "status", 1);
			}
		}
	} else {
		add_assoc_null(&subitem, "gtw");
		add_assoc_long(&subitem, "status", 0);
	}
	add_assoc_zval(return_value, "default", &subitem);
    if (!flag) {
    	RETURN_FALSE;
	}
}

PHP_FUNCTION(route_get)
{
	char *route_name;
	size_t route_name_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &route_name, &route_name_len) == FAILURE) { 
	   return;
	}

	if (php_net_check_route(route_name) == -1)
		RETURN_FALSE;

    char status[2];
    char dest[20];
    char gtw[20];
    char type[2];
	array_init(return_value);

	int flag = 0;
	zval subitem;
	array_init(&subitem);

#ifdef ZFS_ENABLE
	char kzfs_val[KENV_MVALLEN + 1];
	if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
		snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
	char param_name[KENV_MVALLEN + 1];
	snprintf(param_name, sizeof(param_name), "net:route_static_%s_dest", route_name);
	char *param_val = kzfs_get_uprop(kzfs_val, param_name);
	if (param_val != NULL) {
		snprintf(dest, sizeof(dest), "%s", param_val);
        add_assoc_string(&subitem, "dest", param_val);
	} else {
		memset(dest, 0, sizeof(dest));
        add_assoc_null(&subitem, "dest");
	}
	snprintf(param_name, sizeof(param_name), "net:route_static_%s_gtw", route_name);
	param_val = kzfs_get_uprop(kzfs_val, param_name);
	if (param_val != NULL) {
        add_assoc_string(&subitem, "gtw", param_val);
	} else {
        add_assoc_null(&subitem, "gtw");
	}
	snprintf(param_name, sizeof(param_name), "net:route_static_%s_type", route_name);
	param_val = kzfs_get_uprop(kzfs_val, param_name);
	if (param_val != NULL) {
		snprintf(type, sizeof(type), "%s", param_val);
        add_assoc_long(&subitem, "type", strtol(param_val, NULL, 10));
	} else {
		memset(type, 0, sizeof(type));
        add_assoc_null(&subitem, "type");
	}
	snprintf(param_name, sizeof(param_name), "net:route_static_%s_status", route_name);
	param_val = kzfs_get_uprop(kzfs_val, param_name);
    int error = check_route(type, dest);
	if (param_val != NULL) {
        if (error && *param_val == '1')
        	add_assoc_long(&subitem, "status", 2);
        else
        	add_assoc_long(&subitem, "status", strtol(param_val, NULL, 10));
	} else {
        add_assoc_null(&subitem, "status");
	}
    add_assoc_zval(return_value, route_name, &subitem);
#else
	char *conf_file = INI_STR("kcs.net.conf");
	int error = 0;
	setenv("PATH", path, 1);
	char cmd[NET_CMD_MAX_LEN];
	snprintf(cmd, sizeof(cmd), "sysrc -f %s -in route_static_%s_dest", conf_file, route_name);
	FILE *file = popen(cmd, "r");
	if (file != NULL) {
		fscanf(file, "%s", dest);
	    add_assoc_string(&subitem, "dest", dest);
		pclose(file);
	} else {
		add_assoc_null(&subitem, "dest");
		error++;
	}
	snprintf(cmd, sizeof(cmd), "sysrc -f %s -in route_static_%s_gtw", conf_file, route_name);
	file = popen(cmd, "r");
	if (file != NULL) {
		fscanf(file, "%s", gtw);
	    add_assoc_string(&subitem, "gtw", gtw);
		pclose(file);
	} else {
		add_assoc_null(&subitem, "gtw");
	}
	snprintf(cmd, sizeof(cmd), "sysrc -f %s -in route_static_%s_type", conf_file, route_name);
	file = popen(cmd, "r");
	if (file != NULL) {
		fscanf(file, "%s", type);
	    add_assoc_long(&subitem, "type", strtol(type, NULL, 10));
		pclose(file);
	} else {
		add_assoc_null(&subitem, "type");
		error++;
	}
	snprintf(cmd, sizeof(cmd), "sysrc -f %s -in route_static_%s_status", conf_file, route_name);
	file = popen(cmd, "r");
	if (file != NULL) {
		fscanf(file, "%s", status);
		if (!error) {
			error = check_route(type, dest);
            if (error && *status == '1')
            	add_assoc_long(&subitem, "status", 2);
            else
            	add_assoc_long(&subitem, "status", strtol(status, NULL, 10));
		}
		pclose(file);
	} else {
		add_assoc_null(&subitem, "status");
	}
    add_assoc_zval(return_value, route_name, &subitem);
#endif
}

static void
up_deleted_ip(long deleted_num, char *ifname, char *type)
{
#ifdef ZFS_ENABLE
	char kzfs_val[KENV_MVALLEN + 1];
	if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
		snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
	char param_name[KZFS_PARAM_MAX_LEN];
	char *tmp_newval = NULL;
	size_t tmp_size = 0;
	snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_%s_aliases", ifname, type);
	char *param_val = kzfs_get_uprop(kzfs_val, param_name);
	if (param_val != NULL && strcmp(param_val, "0") == 0) {
		kzfs_del_uprop(kzfs_val, param_name);
	} else {
		if (deleted_num) {
			for (long i = 0; i < deleted_num; i++) {
				tmp_size += 4;
				tmp_newval = realloc(tmp_newval, tmp_size);
				if (!(tmp_size - 4))
					snprintf(tmp_newval, tmp_size, "%ld", i);
				else
					snprintf(tmp_newval, tmp_size, "%s,%ld", tmp_newval, i);
			}
		}
	}
	snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_%s_alias_%lu", ifname, type, ++deleted_num);
	param_val = kzfs_get_uprop(kzfs_val, param_name);
	int flag = 0;
	while (param_val != NULL) {
		char *tmp_val = malloc(strlen(param_val) + 1);
		snprintf(tmp_val, strlen(param_val) + 1, "%s", param_val);
		snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_%s_alias_%lu", ifname, type, deleted_num-1);
		kzfs_set_uprop(kzfs_val, param_name, tmp_val);
		free(tmp_val);
		if (strcmp(type, "ip4") == 0) {
			snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_%s_netmask_%lu", ifname, type, deleted_num);
		} else {
			snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_%s_prefix_%lu", ifname, type, deleted_num);
		}
		param_val = kzfs_get_uprop(kzfs_val, param_name);
		tmp_val = malloc(strlen(param_val) + 1);
		snprintf(tmp_val, strlen(param_val) + 1, "%s", param_val);
		if (strcmp(type, "ip4") == 0) {
			snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_%s_netmask_%lu", ifname, type, deleted_num-1);
		} else {
			snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_%s_prefix_%lu", ifname, type, deleted_num-1);
		}
		kzfs_set_uprop(kzfs_val, param_name, tmp_val);
		tmp_size += 4;
		tmp_newval = realloc(tmp_newval, tmp_size);
		if (!(tmp_size - 4))
			snprintf(tmp_newval, tmp_size, "%ld", deleted_num-1);
		else
			snprintf(tmp_newval, tmp_size, "%s,%ld", tmp_newval, deleted_num-1);
		snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_%s_alias_%lu", ifname, type, ++deleted_num);
		param_val = kzfs_get_uprop(kzfs_val, param_name);
		free(tmp_val);
		flag = 1;
	}
	if (flag) {
		if (strcmp(type, "ip4") == 0) {
			snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_%s_netmask_%lu", ifname, type, deleted_num-1);
		} else {
			snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_%s_prefix_%lu", ifname, type, deleted_num-1);
		}
		kzfs_del_uprop(kzfs_val, param_name);
		snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_%s_alias_%lu", ifname, type, deleted_num-1);
		kzfs_del_uprop(kzfs_val, param_name);
	}
	snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_%s_aliases", ifname, type);
	kzfs_set_uprop(kzfs_val, param_name, tmp_newval);
	free(tmp_newval);
#else
	char *conf_file = INI_STR("kcs.net.conf");
	char num_t[2];
	snprintf(num_t, 2, "%ld", deleted_num);
	char find[64];
	snprintf(find, sizeof(find), "ifconfig_%s_%s_", ifname, type);
	FILE *file = fopen(conf_file, "r+");
	if (file != NULL) {
		long cur = ftell(file);
		long prev = cur;
		char c = fgetc(file);
		char tmp[NET_CONF_LINE_MAX_LEN];
		char *p = tmp;
		while (c != -1) {
			if (c == '\n') {
				cur = ftell(file);
				if (strncmp(tmp, find, strlen(find)) == 0) {
                    char *t = strchr(tmp, '=');
                    t--;
	                if (*t > *num_t) {
    	                (*t)--;
    	      	        fseek(file, prev, SEEK_SET);
                      	fprintf(file, "%s\n", tmp);
                      	fseek(file, cur, SEEK_SET);
                    }
				}
				prev = cur;
				memset(tmp, 0, sizeof(tmp));
				p = tmp;
			} else {
				*p = c;
				p++;
			}
			c = fgetc(file);
		}
		fclose(file);
	}
#endif
}

static void
remove_all_ip_conf(char *ifname)
{
#ifdef ZFS_ENABLE
	char kzfs_val[KENV_MVALLEN + 1];
	if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
		snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
	char param_name[KZFS_PARAM_MAX_LEN];
	snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip4_aliases", ifname);
	char *param_val = kzfs_get_uprop(kzfs_val, param_name);
	if (param_val != NULL) {
		char *tmp_val = malloc(strlen(param_val) + 1);
		if (tmp_val != NULL) {
			snprintf(tmp_val, strlen(param_val) + 1, "%s", param_val);
			char *tmp = strtok(tmp_val, ",");
			while (tmp != NULL) {
				snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip4_alias_%s", ifname, tmp);
				kzfs_del_uprop(kzfs_val, param_name);
				snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip4_netmask_%s", ifname, tmp);
				kzfs_del_uprop(kzfs_val, param_name);
				tmp = strtok(NULL, ",");
			}
			free(tmp_val);
		}
		snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip4_aliases", ifname);
		kzfs_del_uprop(kzfs_val, param_name);
	}
	snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip6_aliases", ifname);
	param_val = kzfs_get_uprop(kzfs_val, param_name);
	if (param_val != NULL) {
		char *tmp_val = malloc(strlen(param_val) + 1);
		if (tmp_val != NULL) {
			snprintf(tmp_val, strlen(param_val) + 1, "%s", param_val);
			char *tmp = strtok(tmp_val, ",");
			while (tmp != NULL) {
				snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip6_alias_%s", ifname, tmp);
				kzfs_del_uprop(kzfs_val, param_name);
				snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip6_prefix_%s", ifname, tmp);
				kzfs_del_uprop(kzfs_val, param_name);
				tmp = strtok(NULL, ",");
			}
			free(tmp_val);
		}
		snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip6_aliases", ifname);
		kzfs_del_uprop(kzfs_val, param_name);
	}
#else
	char *conf_file = INI_STR("kcs.net.conf");
	char find1[NET_CMD_MAX_LEN];
    char find2[NET_CMD_MAX_LEN];
    int i = 0;
    int ret = 0;
    while (ret == 0) {
        snprintf(find1, sizeof(find1), "sysrc -f %s -ix ifconfig_%s_ip4_alias_%d", conf_file, ifname, i);
        snprintf(find2, sizeof(find2), "sysrc -f %s -ix ifconfig_%s_ip4_netmask_%d", conf_file, ifname, i);
        ret = system(find1);
        ret = system(find2);
        i++;
    }
    i = 0;
    ret = 0;
    while (ret == 0) {
        snprintf(find1, sizeof(find1), "sysrc -f %s -ix ifconfig_%s_ip6_alias_%d", conf_file, ifname, i);
        snprintf(find2, sizeof(find2), "sysrc -f %s -ix ifconfig_%s_ip6_prefix_%d", conf_file, ifname, i);
        ret = system(find1);
        ret = system(find2);
        i++;
    }
#endif
}

static void
remove_all_ip(char *ifname)
{
	struct ifaddrs *ifap, *ifa;
	getifaddrs (&ifap);

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (strcmp(ifname, ifa->ifa_name) != 0)
			continue;
		if (ifa->ifa_addr->sa_family != AF_INET && ifa->ifa_addr->sa_family != AF_INET6)
			continue;

		int s;
		if ((s = socket(ifa->ifa_addr->sa_family, SOCK_STREAM, 0)) < 0) {
			continue;
		}

		if (ifa->ifa_addr->sa_family == AF_INET) {
			struct ifreq ifr;
			memset(&ifr, 0, sizeof(struct ifreq));

			strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);

			struct sockaddr_in *sai = (struct sockaddr_in *)&ifr.ifr_addr;
			memset(sai, 0, sizeof(struct sockaddr));

			sai->sin_family = ifa->ifa_addr->sa_family;
			sai->sin_port = 0;
			sai->sin_addr.s_addr = ((struct sockaddr_in *) ifa->ifa_addr)->sin_addr.s_addr;

			ioctl(s, SIOCDIFADDR, (caddr_t)&ifr);
		} else {
			struct sockaddr_in6 *sin = (struct sockaddr_in6 *)ifa->ifa_addr;
			char addr_buf1[NI_MAXHOST];

			inet_ntop(AF_INET6, &sin->sin6_addr, addr_buf1, sizeof(addr_buf1));

			if(strchr(addr_buf1, '%') != NULL)
				continue;

			struct in6_aliasreq in6_addreq =
			  { .ifra_flags = 0,
			    .ifra_lifetime = { 0, 0, ND6_INFINITE_LIFETIME, ND6_INFINITE_LIFETIME } };

			strlcpy(((struct ifreq *)&in6_addreq)->ifr_name, ifname,
			IFNAMSIZ);

			struct addrinfo hints, *res;
			bzero(&hints, sizeof(struct addrinfo));
			hints.ai_family = AF_INET6;
			getaddrinfo(addr_buf1, NULL, &hints, &res);
			bcopy(res->ai_addr, (struct sockaddr_in6 *) &(in6_addreq.ifra_addr), res->ai_addrlen);
			freeaddrinfo(res);

			ioctl(s, SIOCDIFADDR_IN6, (caddr_t)&in6_addreq);
		}
		close(s);
	}

	freeifaddrs(ifap);
}

static void
kill_dhcp(char *ifname)
{
	char *dhcp_pid_path = INI_STR("kcs.dhcp.pid_path");
	char *dhcp_pid_format = INI_STR("kcs.dhcp.pid_format");
	char pid_file[NET_DHCP_PID_PATH_LEN];
	snprintf(pid_file, sizeof(pid_file), dhcp_pid_format, dhcp_pid_path, ifname);
	if (access(pid_file, F_OK) == 0) {
		FILE *file = fopen(pid_file, "r");
        if (file != NULL) {
            int pid;
            fscanf(file, "%d", &pid);
            fclose(file);
            kill(pid, SIGTERM);
        }
        // remove_all_ip(ifname);
	}

}

static void
get_ip_conf(char *ifname, long alias_num, char *type, char *ip)
{
#ifdef ZFS_ENABLE
	char kzfs_val[KENV_MVALLEN + 1];
	if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
		snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
	char param_name[KZFS_PARAM_MAX_LEN];
	snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_%s_alias_%ld", ifname, type, alias_num);
	char *param_val = kzfs_get_uprop(kzfs_val, param_name);
	if (param_val != NULL) {
		snprintf(ip, strlen(param_val) + 1, "%s", param_val);
	}
#else
	char *conf_file = INI_STR("kcs.net.conf");
	char cmd[NET_CMD_MAX_LEN];
	snprintf(cmd, sizeof(cmd), "sysrc -f %s -in ifconfig_%s_%s_alias_%ld", conf_file, ifname, type, alias_num);
	FILE *file = popen(cmd, "r");
	if (file != NULL) {
		fscanf(file, "%s", ip);
		pclose(file);
	}
#endif
}

static void
is_dns_dhcp(char *res)
{
#ifdef ZFS_ENABLE
	char kzfs_val[KENV_MVALLEN + 1];
	if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
		snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
	char param_name[KZFS_PARAM_MAX_LEN];
	snprintf(param_name, sizeof(param_name), "net:%s", "dns");
	char *param_val = kzfs_get_uprop(kzfs_val, param_name);
	if (param_val != NULL) {
		snprintf(res, strlen(param_val) + 1, "%s", param_val);
	}
#else
	char *conf_file = INI_STR("kcs.net.conf");
	char cmd[NET_CMD_MAX_LEN];
	snprintf(cmd, sizeof(cmd), "sysrc -f %s -in dns", conf_file);
	FILE *file = popen(cmd, "r");
	if (file != NULL) {
		fscanf(file, "%s", res);
		pclose(file);
	}
#endif
}

static void
get_route_conf(char *res, char *what, char *name)
{
#ifdef ZFS_ENABLE
	char kzfs_val[KENV_MVALLEN + 1];
	if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
		snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
	char param_name[KZFS_PARAM_MAX_LEN];
	snprintf(param_name, sizeof(param_name), "net:route_static_%s_%s", name, what);
	char *param_val = kzfs_get_uprop(kzfs_val, param_name);
	if (param_val != NULL)
		snprintf(res, strlen(param_val) + 1, "%s", param_val);
#else
	char *conf_file = INI_STR("kcs.net.conf");
	char cmd[NET_CMD_MAX_LEN];
	snprintf(cmd, sizeof(cmd), "sysrc -f %s -in route_static_%s_%s", conf_file, name, what);
	FILE *file = popen(cmd, "r");
	if (file != NULL) {
		fscanf(file, "%s", res);
		pclose(file);
	}
#endif
}


static int
add_new_ip4(char *ifname, long alias_num, char *ip, char *netmask, zend_bool enabled, zend_long settype)
{
	if (enabled && (settype == PHP_NET_IP_SYSTEM || settype == PHP_NET_IP_AUTO)) {
		int s;
		if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			return -1;
		}
		int ret = 0;

		struct in_aliasreq in_addreq;
		memset(&in_addreq, 0, sizeof(in_addreq));
		struct sockaddr_in *min = ((struct sockaddr_in *) &(in_addreq.ifra_mask));
		min->sin_family = AF_INET;
		min->sin_len = sizeof(*min);
		inet_aton(netmask, &min->sin_addr);

		struct sockaddr_in *sin = ((struct sockaddr_in *) &(in_addreq.ifra_addr));
		sin->sin_len = sizeof(*sin);
		sin->sin_family = AF_INET;
		inet_aton(ip, &sin->sin_addr);
		strlcpy(((struct ifreq *)&in_addreq)->ifr_name, ifname,
				IFNAMSIZ);
		ret = ioctl(s, SIOCAIFADDR, (char *)&in_addreq);
		if (ret < 0) {
			close(s);
			return -1;
	    }
		struct ifreq ifr;
		memset(&ifr, 0, sizeof(struct ifreq));
		strcpy(ifr.ifr_name, ifname);
	    ioctl(s, SIOCGIFFLAGS, &ifr);

	    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;

	    ioctl(s, SIOCSIFFLAGS, &ifr);
		close(s);
	}
	char flags[] = "up";
	if (settype == PHP_NET_IP_CONF || settype == PHP_NET_IP_AUTO) {
#ifdef ZFS_ENABLE
		char kzfs_val[KENV_MVALLEN + 1];
		if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
			snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
		char param_name[KZFS_PARAM_MAX_LEN];
		snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_enable", ifname);
		kzfs_set_uprop(kzfs_val, param_name, enabled ? "true" : "false");
		snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip4_alias_%ld", ifname, alias_num);
		kzfs_set_uprop(kzfs_val, param_name, ip);
		snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip4_netmask_%ld", ifname, alias_num);
		kzfs_set_uprop(kzfs_val, param_name, netmask);
		if (settype == PHP_NET_IP_AUTO) {
			snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_flags", ifname);
			kzfs_set_uprop(kzfs_val, param_name, flags);
		}
		snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip4_aliases", ifname);
		char *param_val = kzfs_get_uprop(kzfs_val, param_name);
		if (param_val == NULL) {
			char *str_alias_num = malloc(2);
			snprintf(str_alias_num, 2, "%ld", alias_num);
			kzfs_set_uprop(kzfs_val, param_name, str_alias_num);
			free(str_alias_num);
		} else {
			char *tmp_val = malloc(strlen(param_val) + 5);
			snprintf(tmp_val, strlen(param_val) + 1, "%s", param_val);
			char *str_alias_num = malloc(2);
			snprintf(str_alias_num, 2, "%ld", alias_num);
			char *tmp = strtok(tmp_val, ",");
			int flag = 0;
			while (tmp != NULL) {
				if (strcmp(str_alias_num, tmp) == 0) {
					flag = 1;
				}
				tmp = strtok(NULL, ",");
			}
			if (!flag) {
				snprintf(tmp_val, strlen(param_val) + 5, "%s,%s", param_val, str_alias_num);
				kzfs_set_uprop(kzfs_val, param_name, tmp_val);
			}
			free(tmp_val);
			free(str_alias_num);
		}
#else
		char *conf_file = INI_STR("kcs.net.conf");
		setenv("PATH", path, 1);
		char cmd[NET_CMD_MAX_LEN];
		snprintf(cmd, sizeof(cmd), "sysrc -f %s ifconfig_%s_ip4_alias_%ld=\"%s\" 1>/dev/null", conf_file, ifname, alias_num, ip);
		system(cmd);
		snprintf(cmd, sizeof(cmd), "sysrc -f %s ifconfig_%s_enable=\"%s\" 1>/dev/null", conf_file, ifname, enabled ? "true" : "false");
		system(cmd);
		snprintf(cmd, sizeof(cmd), "sysrc -f %s ifconfig_%s_ip4_netmask_%ld=\"%s\" 1>/dev/null", conf_file, ifname, alias_num, netmask);
		system(cmd);
		if (settype == PHP_NET_IP_AUTO) {
			snprintf(cmd, sizeof(cmd), "sysrc -f %s ifconfig_%s_flags=\"%s\" 1>/dev/null", conf_file, ifname, flags);
			system(cmd);
		}
#endif
	}
	return 0;
}


static int
add_new_ip6(char *ifname, long alias_num, char *ip, long prefix, zend_bool enabled, zend_long settype)
{
	long prefix_cmd = (long)prefix;
	if (enabled && (settype == PHP_NET_IP_SYSTEM || settype == PHP_NET_IP_AUTO)) {
		int s;
		if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
			return -1;
		}

		int ret = 0;
		struct in6_aliasreq in6_addreq = 
		  { .ifra_flags = 0,
		    .ifra_lifetime = { 0, 0, ND6_INFINITE_LIFETIME, ND6_INFINITE_LIFETIME } };

		struct sockaddr_in6 *pref = ((struct sockaddr_in6 *) &(in6_addreq.ifra_prefixmask));
		pref->sin6_family = AF_INET6;
		pref->sin6_len = sizeof(*pref);
		if ((prefix == 0) || (prefix == 128)) {
			memset(&pref->sin6_addr, 0xff, sizeof(struct in6_addr));
		} else {
			memset((void *)&pref->sin6_addr, 0x00, sizeof(pref->sin6_addr));
			u_char *cp;
			for (cp = (u_char *)&pref->sin6_addr; prefix > 7; prefix -= 8)
				*cp++ = 0xff;
			*cp = 0xff << (8 - (long)prefix);
		}
		struct sockaddr_in6 *sin = ((struct sockaddr_in6 *) &(in6_addreq.ifra_addr));
		sin->sin6_len = sizeof(*sin);
		sin->sin6_family = AF_INET6;
		if (inet_pton(AF_INET6, ip, &sin->sin6_addr) != 1) {
			close(s);
			return -1;
		}
		strlcpy(((struct ifreq *)&in6_addreq)->ifr_name, ifname,
				IFNAMSIZ);
		ret = ioctl(s, SIOCAIFADDR_IN6, (caddr_t)&in6_addreq);
		close(s);
		if (ret < 0) {
			return -1;
	    }
	}
	if (settype == PHP_NET_IP_CONF || settype == PHP_NET_IP_AUTO) {
#ifdef ZFS_ENABLE
		char kzfs_val[KENV_MVALLEN + 1];
		if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
			snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
		char param_name[KZFS_PARAM_MAX_LEN];
		snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_enable", ifname);
		kzfs_set_uprop(kzfs_val, param_name, enabled ? "true" : "false");
		snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip6_alias_%ld", ifname, alias_num);
		kzfs_set_uprop(kzfs_val, param_name, ip);
		snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip6_prefix_%ld", ifname, alias_num);
		if (settype == PHP_NET_IP_AUTO) {
			snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_flags", ifname);
			kzfs_set_uprop(kzfs_val, param_name, "up");
		}
		char *str_alias_num = malloc(5);
		snprintf(str_alias_num, 5, "%ld", prefix_cmd);
		kzfs_set_uprop(kzfs_val, param_name, str_alias_num);
		free(str_alias_num);
		snprintf(param_name, sizeof(param_name), "net:ifconfig_%s_ip6_aliases", ifname);
		char *param_val = kzfs_get_uprop(kzfs_val, param_name);
		if (param_val == NULL) {
			str_alias_num = malloc(2);
			snprintf(str_alias_num, 2, "%ld", alias_num);
			kzfs_set_uprop(kzfs_val, param_name, str_alias_num);
			free(str_alias_num);
		} else {
			char *tmp_val = malloc(strlen(param_val) + 5);
			snprintf(tmp_val, strlen(param_val) + 1, "%s", param_val);
			str_alias_num = malloc(2);
			snprintf(str_alias_num, 2, "%ld", alias_num);
			char *tmp = strtok(tmp_val, ",");
			int flag = 0;
			while (tmp != NULL) {
				if (strcmp(str_alias_num, tmp) == 0) {
					flag = 1;
				}
				tmp = strtok(NULL, ",");
			}
			if (!flag) {
				snprintf(tmp_val, strlen(param_val) + 5, "%s,%s", param_val, str_alias_num);
				kzfs_set_uprop(kzfs_val, param_name, tmp_val);
			}
			free(tmp_val);
			free(str_alias_num);
		}
#else
		char *conf_file = INI_STR("kcs.net.conf");
		setenv("PATH", path, 1);
		char cmd[NET_CMD_MAX_LEN];
		snprintf(cmd, sizeof(cmd), "sysrc -f %s ifconfig_%s_ip6_alias_%ld=\"%s\" 1>/dev/null", conf_file, ifname, alias_num, ip);
		system(cmd);
		snprintf(cmd, sizeof(cmd), "sysrc -f %s ifconfig_%s_enable=\"%s\" 1>/dev/null", conf_file, ifname, enabled ? "true" : "false");
		system(cmd);
		snprintf(cmd, sizeof(cmd), "sysrc -f %s ifconfig_%s_ip6_prefix_%ld=\"%ld\" 1>/dev/null", conf_file, ifname, alias_num, prefix_cmd);
		system(cmd);
		if (settype == PHP_NET_IP_AUTO) {
			snprintf(cmd, sizeof(cmd), "sysrc -f %s ifconfig_%s_flags=\"up\" 1>/dev/null", conf_file, ifname);
			system(cmd);
		}
#endif
	}
	return 0;
}

static int
fill_sockaddr_storage(int id, char *addr)
{
	struct sockaddr_in *sin;
	struct sockaddr *sa;
	u_long val;

	sa = (struct sockaddr *)&so[id];
	sa->sa_family = AF_INET;
	sa->sa_len = sizeof(struct sockaddr_in);
	sin = (struct sockaddr_in *)(void *)sa;
	if (strncmp(addr, "0.0.0.0", 7) == 0) 
		return 0;
	char *q;
	q = strchr(addr,'/');

	if (q != NULL && id == RTAX_DST) {				// net
		*q = '\0';
		val = inet_network(addr);
		u_long mask = strtoul(q+1, 0, 0);
		if (val > 0)
			while ((val & 0xff000000) == 0)
				val <<= 8;

		mask = 0xffffffff << (32 - mask);
		sin->sin_addr.s_addr = htonl(val);
		((struct sockaddr_in *)&so[RTAX_NETMASK])->sin_addr.s_addr = htonl(mask);
		((struct sockaddr_in *)&so[RTAX_NETMASK])->sin_len = sizeof(struct sockaddr_in);
		((struct sockaddr_in *)&so[RTAX_NETMASK])->sin_family = AF_INET;
		*q = '/';
	} else {										// gw/host
		inet_aton(addr, &sin->sin_addr);
		val = sin->sin_addr.s_addr;
		inet_lnaof(sin->sin_addr);
	}

	return 0;
}

static int
fill_fibs(struct fibl_head_t *flh)
{
	int	defaultfib, numfibs;
	int error;
	size_t len;

	len = sizeof(numfibs);
	if (sysctlbyname("net.fibs", (void *)&numfibs, &len, NULL, 0) == -1)
		numfibs = -1;
	len = sizeof(defaultfib);
	if (numfibs != -1 &&
	    sysctlbyname("net.my_fibnum", (void *)&defaultfib, &len, NULL,
		0) == -1)
		defaultfib = -1;

	if (TAILQ_EMPTY(flh)) {
		error = fiboptlist_csv("default", flh, numfibs, defaultfib);
		if (error)
			return -1;
	}

	return 0;
}

static int
fiboptlist_range(const char *arg, struct fibl_head_t *flh, int numfibs)
{
	struct fibl *fl;
	char *str0, *str, *token, *endptr;
	int fib[2], i, error;
	str0 = str = strdup(arg);
	error = 0;
	i = 0;
	while ((token = strsep(&str, "-")) != NULL) {
		switch (i) {
		case 0:
		case 1:
			errno = 0;
			fib[i] = strtol(token, &endptr, 0);
			if (errno == 0) {
				if (*endptr != '\0' ||
				    fib[i] < 0 ||
				    (numfibs != -1 && fib[i] > numfibs - 1))
					errno = EINVAL;
			}
			if (errno)
				error = 1;
			break;
		default:
			error = 1;
		}
		if (error)
			goto fiboptlist_range_ret;
		i++;
	}
	if (fib[0] >= fib[1]) {
		error = 1;
		goto fiboptlist_range_ret;
	}
	for (i = fib[0]; i <= fib[1]; i++) {
		fl = calloc(1, sizeof(*fl));
		if (fl == NULL) {
			error = 1;
			goto fiboptlist_range_ret;
		}
		fl->fl_num = i;
		TAILQ_INSERT_TAIL(flh, fl, fl_next);
	}
fiboptlist_range_ret:
	free(str0);
	return (error ? -1 : 0);
}

static int
fiboptlist_csv(const char *arg, struct fibl_head_t *flh, int numfibs, int defaultfib)
{
	struct fibl *fl;
	char *str0, *str, *token, *endptr;
	int fib, error;

	str0 = str = NULL;
	if (strcmp("all", arg) == 0) {
		str = calloc(1, NET_ALL_STRLEN);
		if (str == NULL) {
			error = 1;
			goto fiboptlist_csv_ret;
		}
		if (numfibs > 1)
			snprintf(str, NET_ALL_STRLEN - 1, "%d-%d", 0, numfibs - 1);
		else
			snprintf(str, NET_ALL_STRLEN - 1, "%d", 0);
	} else if (strcmp("default", arg) == 0) {
		str0 = str = calloc(1, NET_ALL_STRLEN);
		if (str == NULL) {
			error = 1;
			goto fiboptlist_csv_ret;
		}
		snprintf(str, NET_ALL_STRLEN - 1, "%d", defaultfib);
	} else
		str0 = str = strdup(arg);

	error = 0;
	while ((token = strsep(&str, ",")) != NULL) {
		if (*token != '-' && strchr(token, '-') != NULL) {
			error = fiboptlist_range(token, flh, numfibs);
			if (error)
				goto fiboptlist_csv_ret;
		} else {
			errno = 0;
			fib = strtol(token, &endptr, 0);
			if (errno == 0) {
				if (*endptr != '\0' ||
				    fib < 0 ||
				    (numfibs != -1 && fib > numfibs - 1))
					errno = EINVAL;
			}
			if (errno) {
				error = 1;
				goto fiboptlist_csv_ret;
			}
			fl = calloc(1, sizeof(*fl));
			if (fl == NULL) {
				error = 1;
				goto fiboptlist_csv_ret;
			}
			fl->fl_num = fib;
			TAILQ_INSERT_TAIL(flh, fl, fl_next);
		}
	}
fiboptlist_csv_ret:
	if (str0 != NULL)
		free(str0);
	return (error);
}



#define NEXTADDR(w, u)										\
	if (rtm_addrs & (w)) {									\
		l = (((struct sockaddr *)&(u))->sa_len == 0) ?		\
		    sizeof(long) :									\
		    1 + ((((struct sockaddr *)&(u))->sa_len - 1)	\
			| (sizeof(long) - 1));							\
		memmove(cp, (char *)&(u), l);						\
		cp += l;											\
	}

#define rtm m_rtmsg.m_rtm


static int
rtmsg(int type, int flags, int rtm_addrs, int *seq, int fib)
{

	int s;
	s = socket(PF_ROUTE, SOCK_RAW, 0);
	if (s < 0)
		return -1;
	shutdown(s, SHUT_RD);
	struct {
		struct	rt_msghdr m_rtm;
		char	m_space[512];
	} m_rtmsg;

	struct rt_metrics rt_metrics = {0};
	u_long  rtm_inits = 0;

	rtm.rtm_type = type;
	rtm.rtm_flags = flags;
	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_seq = ++(*seq);
	rtm.rtm_addrs = rtm_addrs;
	rtm.rtm_pid = 0;
	rtm.rtm_rmx = rt_metrics;
	rtm.rtm_inits = rtm_inits;
	rtm.rtm_errno = 0;

	char *cp = m_rtmsg.m_space;
	int l;

	NEXTADDR(RTA_DST, so[RTAX_DST]);
	NEXTADDR(RTA_GATEWAY, so[RTAX_GATEWAY]);
	NEXTADDR(RTA_NETMASK, so[RTAX_NETMASK]);
	rtm.rtm_msglen = l = cp - (char *)&m_rtmsg;

	if (setsockopt(s, SOL_SOCKET, SO_SETFIB, (void *)&fib, sizeof(fib)) != 0) {
		close(s);
	    return -1;
	}
	int rlen;
	if ((rlen = write(s, (char *)&m_rtmsg, l)) < 0) {
        if (type == RTM_DELETE && errno == ESRCH) {
          close(s);
          return 0;
        }
		close(s);
		return -1;
	}
	close(s);
	return 0;
}

static int
check_route(char *type, char *dest)
{
	TAILQ_INIT(&fibl_head);
	if (fill_fibs(&fibl_head) != 0)
		return -1;

	int s = socket(PF_ROUTE, SOCK_RAW, 0);
	if (s < 0)
		return -1;
	struct fibl *fl;
	int flags = 0, seq = 0, rtm_addrs = 0;

	if (so[RTAX_IFP].ss_family == 0) {
		so[RTAX_IFP].ss_family = AF_LINK;
		so[RTAX_IFP].ss_len = sizeof(struct sockaddr_dl);
		rtm_addrs |= RTA_IFP;
	}

	flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;
	rtm_addrs = RTA_DST | RTA_IFP;
	fill_sockaddr_storage(RTAX_DST, dest);
	if (*type == '1') {
		flags |= RTF_HOST;
	} else {
		rtm_addrs |= RTA_NETMASK;
	}

	struct {
		struct	rt_msghdr m_rtm;
		char	m_space[512];
	} m_rtmsg;
	int error = 0;
	int count = 0;
	TAILQ_FOREACH(fl, &fibl_head, fl_next) {
		count++;
		struct rt_metrics rt_metrics = {0};
		u_long  rtm_inits = 0;

		rtm.rtm_type = RTM_GET;
		rtm.rtm_flags = flags;
		rtm.rtm_version = RTM_VERSION;
		rtm.rtm_seq = ++seq;
		rtm.rtm_addrs = rtm_addrs;
		rtm.rtm_pid = 0;
		rtm.rtm_rmx = rt_metrics;
		rtm.rtm_inits = rtm_inits;
		rtm.rtm_errno = 0;

		char *cp = m_rtmsg.m_space;
		int l;

		NEXTADDR(RTA_DST, so[RTAX_DST]);
		NEXTADDR(RTA_NETMASK, so[RTAX_NETMASK]);
		NEXTADDR(RTA_IFP, so[RTAX_IFP]);
		rtm.rtm_msglen = l = cp - (char *)&m_rtmsg;

		if (setsockopt(s, SOL_SOCKET, SO_SETFIB, (void *)&fl->fl_num, sizeof(fl->fl_num)) == 0) {
			int rlen;
			if ((rlen = write(s, (char *)&m_rtmsg, l)) < 0) {
				error++;
			}
		}
	}
	return error == count ? 1 : 0;
}

static int
get_def_route(char *def, size_t len)
{
	int mib[7];
	char *buf, *next, *lim;
	int	af = AF_UNSPEC;
	int fibnum = 0;

	size_t needed;
	struct rt_msghdr *rtms;
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = af;
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;
	mib[6] = fibnum;

	if (sysctl(mib, nitems(mib), NULL, &needed, NULL, 0) < 0)
		return -1;

	if ((buf = malloc(needed)) == NULL)
		return -1;
	if (sysctl(mib, nitems(mib), buf, &needed, NULL, 0) < 0) {
		free(buf);
		return -1;
	}

	lim  = buf + needed;
	int f = 0;

	for (next = buf; next < lim; next += rtms->rtm_msglen) {
		rtms = (struct rt_msghdr *)next;
		if (rtms->rtm_version != RTM_VERSION)
			continue;
		struct sockaddr *sa, *addr[RTAX_MAX];
		sa = (struct sockaddr *)(rtms + 1);
		for (int i = 0; i < RTAX_MAX; i++) {
			if (rtms->rtm_addrs & (1 << i))
				addr[i] = sa;
			sa = (struct sockaddr *)((char *)sa + SA_SIZE(sa));
		}

		if (((struct sockaddr_in *)addr[RTAX_DST])->sin_family == AF_INET) {
			if (((struct sockaddr_in *)addr[RTAX_DST])->sin_addr.s_addr == INADDR_ANY ){
				char *ip = inet_ntoa(((struct sockaddr_in *)addr[RTAX_GATEWAY])->sin_addr);
				snprintf(def, len, "%s", ip);
				f = 1;
			}
		}
	}
	free(buf);
	if (!f)
		return -1;

	return 0;
}
/*check if route exists in conffile or zfs*/
static int
php_net_check_route(char *routename)
{
	char def[20];
	memset(def, 0, sizeof(def));

#ifdef ZFS_ENABLE
	char kzfs_val[KENV_MVALLEN + 1];
	if(kenv(KENV_GET, KZFS_CONF_KENV_KEY, kzfs_val, sizeof(kzfs_val)) < 0)
		snprintf(kzfs_val, sizeof(kzfs_val), "%s", KZFS_CONF_KENV_DEFAULT);
	char param_name[KZFS_PARAM_MAX_LEN];
	snprintf(param_name, sizeof(param_name), "net:route_static_%s_status", routename);
	char *param_val = kzfs_get_uprop(kzfs_val, param_name);
	if (param_val != NULL)
		snprintf(def, sizeof(def), "%s", param_val);
#else
	char *conf_file = INI_STR("kcs.net.conf");
	setenv("PATH", path, 1);
	char cmd[NET_CMD_MAX_LEN];
	snprintf(cmd, sizeof(cmd), "sysrc -f %s -in route_static_%s_status", conf_file, routename);
	FILE *file = popen(cmd, "r");
	if (file != NULL) {
		fscanf(file, "%s", def);
		pclose(file);
	}
#endif
	if (def[0]) {
		return 0;
	} else {
		return -1;
	}
}
