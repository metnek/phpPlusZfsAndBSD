#ifdef NEED_SOLARIS_BOOLEAN
#undef NEED_SOLARIS_BOOLEAN
#endif
#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/ioctl.h>
#include <sys/sbuf.h>
#include <sys/socket.h>
#include <sys/tty.h>
#include <sys/types.h>
#include <sys/resource.h>

#include <machine/cpu.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_media.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libutil.h>
#include <netdb.h>
#include <fcntl.h>
#include <paths.h>
#include <kvm.h>
#include <utmpx.h>
#include <vis.h>
#include "php.h"
#include "php_system_info.h"

#define	debugproc(p) *(&((struct kinfo_proc *)p)->ki_udata)
#define	W_DISPUSERSIZE	10
#define	W_DISPLINESIZE	8
#define	W_DISPHOSTSIZE	40

#define	ISRUN(p)	(((p)->ki_stat == SRUN) || ((p)->ki_stat == SIDL))
#define	TESTAB(a, b)    ((a)<<1 | (b))
#define	ONLYA   2
#define	ONLYB   1
#define	BOTH    3

struct entry {
	struct	entry *next;
	struct	utmpx utmp;
	dev_t	tdev;
	time_t	idle;
	struct	kinfo_proc *kp;
	char	*args;
	struct	kinfo_proc *dkp;
};


static int proc_compare(struct kinfo_proc *p1, struct kinfo_proc *p2);
static char *cmdpart(char *);
static char *shquote(char **);
static char *fmt_argv(char **argv, char *cmd, char *thread, size_t maxlen);
static struct stat *ttystat(char *line);
static int pr_idle(time_t idle, zval *info);
static long mem_rounded(long mem_size);

PHP_FUNCTION(system_info_uptime);
PHP_FUNCTION(system_info_users);
PHP_FUNCTION(system_info_ram);
PHP_FUNCTION(system_info_swap);
PHP_FUNCTION(system_info_cpu_use);
PHP_FUNCTION(system_info_ifaces);

ZEND_BEGIN_ARG_INFO(arginfo_system_info_uptime, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_system_info_users, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_system_info_ram, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_system_info_swap, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_system_info_cpu_use, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_system_info_ifaces, 0, 0, 1)
	ZEND_ARG_INFO(0, ifaces)
ZEND_END_ARG_INFO()

const zend_function_entry system_info_functions[] = {
	PHP_FE(system_info_uptime, NULL)
	PHP_FE(system_info_users, NULL)
	PHP_FE(system_info_ram, NULL)
	PHP_FE(system_info_swap, NULL)
	PHP_FE(system_info_cpu_use, NULL)
	PHP_FE(system_info_ifaces, arginfo_system_info_ifaces)
	{NULL, NULL, NULL}
};

zend_module_entry system_info_module_entry = {
	STANDARD_MODULE_HEADER,
	"system_info",
	system_info_functions,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"0.0.1",
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_SYSTEM_INFO

ZEND_GET_MODULE(system_info)
#endif

PHP_FUNCTION(system_info_uptime)
{
	double avenrun[3];
	struct timespec tp;
	time_t uptime;
	int days, hrs, i, mins, secs;

	array_init(return_value);

	if (clock_gettime(CLOCK_UPTIME, &tp) != -1) {
		uptime = tp.tv_sec;
		zval subitem;
		array_init(&subitem);
		if (pr_idle(uptime, &subitem)) {
			zend_array_destroy(Z_ARRVAL_P(&subitem));
		} else {
			add_assoc_zval(return_value, "time", &subitem);
		}
	}
	zval subitem;
	if (getloadavg(avenrun, nitems(avenrun)) == -1) {
		RETURN_TRUE;
	} else {
		array_init(&subitem);
		
		for (i = 0; i < (int)(nitems(avenrun)); i++) {
			add_index_double(&subitem, i, avenrun[i]);
		}
		add_assoc_zval(return_value, "load_avg", &subitem);
	}
}
PHP_FUNCTION(system_info_users)
{
	struct entry *ep, *ehead = NULL, **nextp = &ehead;
	kvm_t   *kd;
	char   **sel_users;
	int ch, i, nentries, nusers, longidle, longattime;
	struct kinfo_proc *kp;
	struct kinfo_proc *dkp;
	const char *memf, *nlistf, *p, *save_p;
	char fn[MAXHOSTNAMELEN];
	char *x_suffix;
	char buf[MAXHOSTNAMELEN], errbuf[_POSIX2_LINE_MAX];
	char *dot;
	time_t touched;
	time_t	now;
	(void)time(&now);
	struct stat *stp;
	struct utmpx *utmp;
	int	nflag = 0;
	array_init(return_value);
	memf = _PATH_DEVNULL;
	if ((kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, errbuf)) == NULL)
		RETURN_FALSE;
	setutxent();
	for (nusers = 0; (utmp = getutxent()) != NULL;) {
		if (utmp->ut_type != USER_PROCESS)
			continue;
		if (!(stp = ttystat(utmp->ut_line)))
			continue;
		++nusers;
		if (sel_users) {
			int usermatch;
			char **user;

			usermatch = 0;
			for (user = sel_users; !usermatch && *user; user++)
				if (!strcmp(utmp->ut_user, *user))
					usermatch = 1;
			if (!usermatch)
				continue;
		}
		if ((ep = calloc(1, sizeof(struct entry))) == NULL)
			break;
		*nextp = ep;
		nextp = &ep->next;
		memmove(&ep->utmp, utmp, sizeof *utmp);
		ep->tdev = stp->st_rdev;
		if (ep->tdev == 0) {
			size_t size;

			size = sizeof(dev_t);
			(void)sysctlbyname("machdep.consdev", &ep->tdev, &size, NULL, 0);
		}
		touched = stp->st_atime;
		if (touched < ep->utmp.ut_tv.tv_sec) {
			touched = ep->utmp.ut_tv.tv_sec;
		}
		if ((ep->idle = now - touched) < 0)
			ep->idle = 0;
	}
	endutxent();
	if ((kp = kvm_getprocs(kd, KERN_PROC_ALL, 0, &nentries)) == NULL)
		goto out;
	for (i = 0; i < nentries; i++, kp++) {
		if (kp->ki_stat == SIDL || kp->ki_stat == SZOMB ||
		    kp->ki_tdev == NODEV)
			continue;
		for (ep = ehead; ep != NULL; ep = ep->next) {
			if (ep->tdev == kp->ki_tdev) {
				if (ep->kp == NULL && kp->ki_pgid == kp->ki_tpgid) {
					if (proc_compare(ep->kp, kp))
						ep->kp = kp;
				}
				dkp = ep->dkp;
				ep->dkp = kp;
				debugproc(kp) = dkp;
			}
		}
	}
	for (ep = ehead; ep != NULL; ep = ep->next) {
		if (ep->kp == NULL) {
			ep->args = strdup("-");
			continue;
		}
		ep->args = fmt_argv(kvm_getargv(kd, ep->kp, 0),
		    ep->kp->ki_comm, NULL, MAXCOMLEN);
		if (ep->args == NULL) {
			goto out;
		}
	}

	for (ep = ehead; ep != NULL; ep = ep->next) {
		zval subitem;
		array_init(&subitem);
		struct addrinfo hints, *res;
		struct sockaddr_storage ss;
		struct sockaddr *sa = (struct sockaddr *)&ss;
		struct sockaddr_in *lsin = (struct sockaddr_in *)&ss;
		struct sockaddr_in6 *lsin6 = (struct sockaddr_in6 *)&ss;
		time_t t;
		int isaddr;

		save_p = p = *ep->utmp.ut_host ? ep->utmp.ut_host : "-";
		if ((x_suffix = strrchr(p, ':')) != NULL) {
			if ((dot = strchr(x_suffix, '.')) != NULL &&
			    strchr(dot+1, '.') == NULL)
				*x_suffix++ = '\0';
			else
				x_suffix = NULL;
		}

		isaddr = 0;
		memset(&ss, '\0', sizeof(ss));
		if (inet_pton(AF_INET6, p, &lsin6->sin6_addr) == 1) {
			lsin6->sin6_len = sizeof(*lsin6);
			lsin6->sin6_family = AF_INET6;
			isaddr = 1;
		} else if (inet_pton(AF_INET, p, &lsin->sin_addr) == 1) {
			lsin->sin_len = sizeof(*lsin);
			lsin->sin_family = AF_INET;
			isaddr = 1;
		}
		if (!isaddr) {
			memset(&hints, 0, sizeof(hints));
			hints.ai_flags = AI_PASSIVE;
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = SOCK_STREAM;
			if (getaddrinfo(p, NULL, &hints, &res) == 0) {
				if (res->ai_next == NULL &&
				    getnameinfo(res->ai_addr, res->ai_addrlen,
					fn, sizeof(fn), NULL, 0,
					NI_NUMERICHOST) == 0)
					p = fn;
				freeaddrinfo(res);
			}
		}

		if (x_suffix) {
			(void)snprintf(buf, sizeof(buf), "%s:%s", p, x_suffix);
			p = buf;
		}
		add_assoc_string(&subitem, "user", ep->utmp.ut_user);
		add_assoc_string(&subitem, "tty", *ep->utmp.ut_line ? (strncmp(ep->utmp.ut_line, "tty", 3) && strncmp(ep->utmp.ut_line, "cua", 3) ?
			 ep->utmp.ut_line : ep->utmp.ut_line + 3) : "-");
		add_assoc_string(&subitem, "ip", *p ? p : "-");
		t = ep->utmp.ut_tv.tv_sec;
		add_assoc_long(&subitem, "login_date", t);
		zval subsub;
		array_init(&subsub);
		if (!pr_idle(ep->idle, &subsub))
			add_assoc_zval(&subitem, "time", &subsub);
		else 
			zend_array_destroy(Z_ARRVAL_P(&subsub));
		add_assoc_string(&subitem, "cmd", ep->args);
		add_next_index_zval(return_value, &subitem);
	}
out:
	for (ep = ehead; ep != NULL; ep = ep->next) {
		free(ep);
	}
	(void)kvm_close(kd);
	if (zend_hash_num_elements(Z_ARRVAL_P(return_value)) < 0)
		RETURN_FALSE;
}

PHP_FUNCTION(system_info_ram)
{
	size_t size;
    long mem_phys, mem_hw;
    int pagesize;
    unsigned int mem_wire, mem_inactive, mem_cache, mem_free, mem_used;
    unsigned int mem_active, mem_gap_vm, mem_gap_sys, mem_gap_hw, mem_all;
    unsigned int total_used, mem_avail;

    size = sizeof mem_phys;
    sysctlbyname("hw.physmem", &mem_phys, &size, NULL, 0);
    mem_hw = mem_rounded(mem_phys);

    size = sizeof pagesize;
    sysctlbyname("hw.pagesize", &pagesize, &size, NULL, 0);
    size = sizeof mem_wire;
    sysctlbyname("vm.stats.vm.v_wire_count", &mem_wire, &size, NULL, 0);
    mem_wire = mem_wire * pagesize;
    sysctlbyname("vm.stats.vm.v_inactive_count", &mem_inactive, &size, NULL, 0);
    mem_inactive = mem_inactive * pagesize;
    sysctlbyname("vm.stats.vm.v_cache_count", &mem_cache, &size, NULL, 0);
    mem_cache = mem_cache * pagesize;
    sysctlbyname("vm.stats.vm.v_free_count", &mem_free, &size, NULL, 0);
    mem_free = mem_free * pagesize;
    sysctlbyname("vm.stats.vm.v_active_count", &mem_active, &size, NULL, 0);
    mem_active = mem_active * pagesize;
    sysctlbyname("vm.stats.vm.v_page_count", &mem_all, &size, NULL, 0);
    mem_all = mem_all * pagesize;

    mem_gap_vm = mem_all - (mem_wire + mem_active + mem_inactive + mem_cache + mem_free);
    mem_gap_sys = mem_phys - mem_all;
    mem_gap_hw = mem_hw - mem_phys;
    mem_avail = mem_inactive + mem_cache + mem_free;
    mem_used = mem_hw - mem_avail;
    total_used = mem_used - mem_wire;

    array_init(return_value);
    add_assoc_long(return_value, "mem_wire", mem_wire/(1024));
    add_assoc_long(return_value, "mem_active", mem_active/(1024));
    add_assoc_long(return_value, "mem_inactive", mem_inactive/(1024));
    add_assoc_long(return_value, "mem_cache", mem_cache/(1024));
    add_assoc_long(return_value, "mem_free", mem_free/(1024));
    add_assoc_long(return_value, "mem_gap_vm", mem_gap_vm/(1024));
    add_assoc_long(return_value, "mem_all", mem_all/(1024));
    add_assoc_long(return_value, "mem_gap_sys", mem_gap_sys/(1024));
    add_assoc_long(return_value, "mem_phys", mem_phys/(1024));
    add_assoc_long(return_value, "mem_gap_hw", mem_gap_hw/(1024));
    add_assoc_long(return_value, "mem_hw", mem_hw/(1024));
    add_assoc_long(return_value, "mem_used", mem_used/(1024));
    add_assoc_long(return_value, "mem_avail", mem_avail/(1024));
    add_assoc_long(return_value, "mem_total", mem_hw/(1024));
    add_assoc_long(return_value, "total_used", total_used/(1024));

}

PHP_FUNCTION(system_info_swap)
{
	int mib[16], n;
	struct xswdev xsw;
	size_t mibsize, size;
	int hlen, pagesize;
	long blocksize;
	pagesize = getpagesize();
	getbsize(&hlen, &blocksize);


	mibsize = sizeof mib / sizeof mib[0];
	sysctlnametomib("vm.swap_info", mib, &mibsize);
	array_init(return_value);
	for (n = 0; ; ++n) {
		mib[mibsize] = n;
		size = sizeof xsw;
		if (sysctl(mib, mibsize + 1, &xsw, &size, NULL, 0) == -1)
			break;
		if (xsw.xsw_version != XSWDEV_VERSION) {
			zend_array_destroy(Z_ARRVAL_P(return_value));
			RETURN_FALSE;
		}
		zval subitem;
		array_init(&subitem);
		if (xsw.xsw_dev == NODEV)
			add_assoc_string(&subitem, "dev", "<NFSfile>");
		else
			add_assoc_string(&subitem, "dev", devname(xsw.xsw_dev, S_IFCHR));
		add_assoc_long(&subitem, "total", (xsw.xsw_nblks * pagesize) / blocksize);
		add_assoc_long(&subitem, "used", (xsw.xsw_used * pagesize) / blocksize);
		add_assoc_long(&subitem, "avail", ((xsw.xsw_nblks - xsw.xsw_used) * pagesize) / blocksize);
		add_next_index_zval(return_value, &subitem);
	}
	if (zend_hash_num_elements(Z_ARRVAL_P(return_value)) <= 0) {
		zend_array_destroy(Z_ARRVAL_P(return_value));
		RETURN_FALSE;
	}

}

PHP_FUNCTION(system_info_cpu_use)
{
	size_t size_first, size_last;
	long cpu_first[5];
	long cpu_last[5];
	long sum_fisrt=0, sum_last=0;

    size_first = sizeof cpu_first;
    size_last = sizeof cpu_last;

	sysctlbyname("kern.cp_time", &cpu_first, &size_first, NULL, 0);
    usleep(200000);
	sysctlbyname("kern.cp_time", &cpu_last, &size_last, NULL, 0);

    for (int i = 0; i < 5; ++i) {
        sum_fisrt = sum_fisrt + cpu_first[i];
    }

    for (int i = 0; i < 5; ++i) {
        sum_last = sum_last + cpu_last[i];
    }

    long double result;
    result = ((long double)cpu_last[4] - (long double)cpu_first[4])/((long double)sum_last - (long double)sum_fisrt);
    array_init(return_value);
    add_assoc_double(return_value, "cpu_use", 100 - result * 100);

}


PHP_FUNCTION(system_info_ifaces)
{
	zval *ifaces;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &ifaces) == FAILURE) { 
	   return;
	}

	HashTable *hash;
	zval *data;
	HashPosition pointer;
	int present = 0;
	hash = Z_ARRVAL_P(ifaces);
    array_init(return_value);
	for(
        zend_hash_internal_pointer_reset_ex(hash, &pointer);
        (data = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
        zend_hash_move_forward_ex(hash, &pointer)
    ) {
    	if (Z_TYPE_P(data) != IS_STRING)
    		continue;
		struct ifaddrs *ifap, *ifa;
		present = 0;

	    getifaddrs (&ifap);
	    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
			if ((ifa->ifa_flags & IFF_CANTCONFIG) != 0)
				continue;

			if (ifa->ifa_addr->sa_family != AF_LINK)
				continue;
	    	if (strcmp(Z_STRVAL_P(data), ifa->ifa_name) == 0 && !present) {
				int s, ret;
				if ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
					if  ((s = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) 
						continue;
				}
				int xmedia = 1;
				struct ifmediareq ifmr;
				(void) memset(&ifmr, 0, sizeof(ifmr));
				(void) strlcpy(ifmr.ifm_name, ifa->ifa_name, sizeof(ifmr.ifm_name));
				if (ioctl(s, SIOCGIFXMEDIA, (caddr_t)&ifmr) < 0)
					xmedia = 0;
				if (xmedia == 0 && ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0) {
					close(s);
					continue;
				}
				if (ifmr.ifm_status & IFM_AVALID) {
					switch (IFM_TYPE(ifmr.ifm_active)) {
					case IFM_ETHER:
					case IFM_ATM:
						if (ifmr.ifm_status & IFM_ACTIVE && (ifa->ifa_flags & IFF_UP) != 0){
							add_assoc_long(return_value, Z_STRVAL_P(data), 1);
						} else{
							add_assoc_long(return_value, Z_STRVAL_P(data), 0);
						}
						break;
					}
				}
				close(s);
	    		present = 1;
	    	}
	    }
	    if (!present) {
	    	add_assoc_long(return_value, Z_STRVAL_P(data), 2);
	    }
	    freeifaddrs(ifap);
	}

    if (zend_hash_num_elements(Z_ARRVAL_P(return_value)) <= 0) {
    	zend_array_destroy(Z_ARRVAL_P(return_value));
    	RETURN_FALSE
    }

}

static char *
fmt_argv(char **argv, char *cmd, char *thread, size_t maxlen)
{
	size_t len;
	char *ap, *cp;

	if (argv == NULL || argv[0] == NULL) {
		if (cmd == NULL)
			return ("");
		ap = NULL;
		len = maxlen + 3;
	} else {
		ap = shquote(argv);
		len = strlen(ap) + maxlen + 4;
	}
	cp = malloc(len);
	if (cp == NULL)
		return NULL;
	if (ap == NULL) {
		if (thread != NULL) {
			asprintf(&ap, "%s/%s", cmd, thread);
			sprintf(cp, "[%.*s]", (int)maxlen, ap);
			free(ap);
		} else
			sprintf(cp, "[%.*s]", (int)maxlen, cmd);
	} else if (strncmp(cmdpart(argv[0]), cmd, maxlen) != 0)
		sprintf(cp, "%s (%.*s)", ap, (int)maxlen, cmd);
	else
		strcpy(cp, ap);
	return (cp);
}

static char *
shquote(char **argv)
{
	long arg_max;
	static size_t buf_size;
	size_t len;
	char **p, *dst, *src;
	static char *buf = NULL;

	if (buf == NULL) {
		if ((arg_max = sysconf(_SC_ARG_MAX)) == -1)
			return NULL;
		if (arg_max >= LONG_MAX / 4 || arg_max >= (long)(SIZE_MAX / 4))
			return NULL;
		buf_size = 4 * arg_max + 1;
		if ((buf = malloc(buf_size)) == NULL)
			return NULL;
	}

	if (*argv == NULL) {
		buf[0] = '\0';
		return (buf);
	}
	dst = buf;
	for (p = argv; (src = *p++) != NULL; ) {
		if (*src == '\0')
			continue;
		len = (buf_size - 1 - (dst - buf)) / 4;
		strvisx(dst, src, strlen(src) < len ? strlen(src) : len,
		    VIS_NL | VIS_CSTYLE);
		while (*dst != '\0')
			dst++;
		if ((buf_size - 1 - (dst - buf)) / 4 > 0)
			*dst++ = ' ';
	}
	/* Chop off trailing space */
	if (dst != buf && dst[-1] == ' ')
		dst--;
	*dst = '\0';
	return (buf);
}

static char *
cmdpart(char *arg0)
{
	char *cp;

	return ((cp = strrchr(arg0, '/')) != NULL ? cp + 1 : arg0);
}

static struct stat *
ttystat(char *line)
{
	static struct stat sb;
	char ttybuf[MAXPATHLEN];

	(void)snprintf(ttybuf, sizeof(ttybuf), "%s%s", _PATH_DEV, line);
	if (stat(ttybuf, &sb) == 0 && S_ISCHR(sb.st_mode)) {
		return (&sb);
	} else
		return (NULL);
}

static int
pr_idle(time_t idle, zval *info)
{
	int days, hrs, i, mins, secs;

	if (idle / 60 == 0) {
		return 1;
	}
	if (idle > 60)
		idle += 30;
	days = idle / 86400;
	idle %= 86400;
	hrs = idle / 3600;
	idle %= 3600;
	mins = idle / 60;
	secs = idle % 60;

	add_assoc_long(info, "days", days);
	add_assoc_long(info, "hours", hrs);
	add_assoc_long(info, "mins", mins);
	add_assoc_long(info, "secs", secs);
	return 0;
}

static long
mem_rounded(long mem_size)
{
	long chip_size = 1;
	long chip_guess = mem_size / 8 - 1;
	while (chip_guess != 0) {
		chip_guess = chip_guess / 2;
		chip_size = chip_size * 2;
	}
	return ((mem_size / chip_size + 1) * chip_size);
}

static int
proc_compare(struct kinfo_proc *p1, struct kinfo_proc *p2)
{

	if (p1 == NULL)
		return (1);
	switch (TESTAB(ISRUN(p1), ISRUN(p2))) {
	case ONLYA:
		return (0);
	case ONLYB:
		return (1);
	case BOTH:
		if (p2->ki_estcpu > p1->ki_estcpu)
			return (1);
		if (p1->ki_estcpu > p2->ki_estcpu)
			return (0);
		return (p2->ki_pid > p1->ki_pid);
	}

	switch (TESTAB(p1->ki_stat == SZOMB, p2->ki_stat == SZOMB)) {
	case ONLYA:
		return (1);
	case ONLYB:
		return (0);
	case BOTH:
		return (p2->ki_pid > p1->ki_pid);
	}

	if (p2->ki_slptime > p1->ki_slptime)
		return (0);
	if (p1->ki_slptime > p2->ki_slptime)
		return (1);
	
	if (p1->ki_tdflags & TDF_SINTR && (p2->ki_tdflags & TDF_SINTR) == 0)
		return (1);
	if (p2->ki_tdflags & TDF_SINTR && (p1->ki_tdflags & TDF_SINTR) == 0)
		return (0);
	return (p2->ki_pid > p1->ki_pid);
}
