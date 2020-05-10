#ifdef NEED_SOLARIS_BOOLEAN
#undef NEED_SOLARIS_BOOLEAN
#endif
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/cpuset.h>
#include <sys/mount.h>
#include <sys/uio.h>
#include <sys/user.h>

#include <archive.h>
#include <archive_entry.h>
#include <kvm.h>
#include <kenv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "php.h"
#include "php_system_os.h"

#include <base.h>

#define SYSTEM_OS_TIME_INIT				1900
#define SYSTEM_OS_SERVICE_RUN_DIR		"/tmp/run"


static int system_os_copy_data(struct archive *ar, struct archive *aw);


PHP_MINIT_FUNCTION(system_os);
PHP_MSHUTDOWN_FUNCTION(system_os);

PHP_FUNCTION(system_os_datetime_set);
PHP_FUNCTION(system_os_reboot);
PHP_FUNCTION(system_os_extract);
PHP_FUNCTION(system_os_ncpu);
PHP_FUNCTION(system_os_cpu_set);
PHP_FUNCTION(system_os_mount);
PHP_FUNCTION(system_os_unmount);
PHP_FUNCTION(system_os_mount_list);
PHP_FUNCTION(system_os_chroot_cmd);
PHP_FUNCTION(system_os_kenv_get);
PHP_FUNCTION(system_os_service_pid);
PHP_FUNCTION(system_os_service_start);
PHP_FUNCTION(system_os_service_stop);
PHP_FUNCTION(system_os_process_info);

ZEND_BEGIN_ARG_INFO_EX(arginfo_system_os_process_info, 0, 0, 1)
	ZEND_ARG_INFO(0, pid)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_system_os_service_pid, 0, 0, 1)
	ZEND_ARG_INFO(0, service_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_system_os_service_start, 0, 0, 2)
	ZEND_ARG_INFO(0, service_cmd)
	ZEND_ARG_INFO(0, service_args)
	ZEND_ARG_INFO(0, service_nopid)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_system_os_service_stop, 0, 0, 2)
	ZEND_ARG_INFO(0, service_name)
	ZEND_ARG_INFO(0, service_args)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_system_os_datetime_set, 0, 0, 1)
	ZEND_ARG_INFO(0, datetime)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_system_os_extract, 0, 0, 2)
	ZEND_ARG_INFO(0, filename)
	ZEND_ARG_INFO(0, path)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_system_os_reboot, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_system_os_ncpu, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_system_os_cpu_set, 0, 0, 2)
	ZEND_ARG_INFO(0, pid)
	ZEND_ARG_INFO(0, proc)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_system_os_mount, 0, 0, 2)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, mountpoint)
	ZEND_ARG_INFO(0, source)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_system_os_unmount, 0, 0, 1)
	ZEND_ARG_INFO(0, mountpoint)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_system_os_mount_list, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_system_os_chroot_cmd, 0, 0, 2)
	ZEND_ARG_INFO(0, dir)
	ZEND_ARG_INFO(0, cmd)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_system_os_kenv_get, 0, 0, 1)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

const zend_function_entry system_os_functions[] = {
	PHP_FE(system_os_datetime_set, arginfo_system_os_datetime_set)
	PHP_FE(system_os_reboot, arginfo_system_os_reboot)
	PHP_FE(system_os_extract, arginfo_system_os_extract)
	PHP_FE(system_os_ncpu, NULL)
	PHP_FE(system_os_cpu_set, arginfo_system_os_cpu_set)
	PHP_FE(system_os_mount, arginfo_system_os_mount)
	PHP_FE(system_os_unmount, arginfo_system_os_unmount)
	PHP_FE(system_os_mount_list, NULL)
	PHP_FE(system_os_chroot_cmd, arginfo_system_os_chroot_cmd)
	PHP_FE(system_os_kenv_get, arginfo_system_os_kenv_get)
	PHP_FE(system_os_process_info, arginfo_system_os_process_info)
	PHP_FE(system_os_service_pid, arginfo_system_os_service_pid)
	PHP_FE(system_os_service_start, arginfo_system_os_service_start)
	PHP_FE(system_os_service_stop, arginfo_system_os_service_stop)
	{NULL, NULL, NULL}
};

zend_module_entry system_os_module_entry = {
	STANDARD_MODULE_HEADER,
	"system_os",
	system_os_functions,
	PHP_MINIT(system_os),
	PHP_MSHUTDOWN(system_os),
	NULL,
	NULL,
	NULL,
	"0.0.1",
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_SYSTEM_OS

ZEND_GET_MODULE(system_os)
#endif

PHP_INI_BEGIN()
    PHP_INI_ENTRY("kcs.reboot.cmd", "none", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("kcs.php.mount.enable", "0", PHP_INI_ALL, NULL)
PHP_INI_END()

PHP_MINIT_FUNCTION(system_os)
{
	REGISTER_INI_ENTRIES();

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(system_os)
{
    UNREGISTER_INI_ENTRIES();

    return SUCCESS;
}

PHP_FUNCTION(system_os_process_info)
{
	zend_long pid;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &pid) == FAILURE) { 
	   return;
	}

	char errbuf[_POSIX2_LINE_MAX];
    kvm_t *kernel = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf);
    int n = 0;
    struct kinfo_proc *kinfo = kvm_getprocs(kernel, KERN_PROC_PROC, 0, &n);

	array_init(return_value);
	int exists = 0;
    for (int i = 0; i < n; i++) {
    	if (pid != kinfo[i].ki_pid)
    		continue;

        char **args = kvm_getargv(kernel, &kinfo[i], 0);
		char arg_line[_POSIX2_LINE_MAX];
		snprintf(arg_line, sizeof(arg_line), "%s ", kinfo[i].ki_comm);
        if (args != NULL)
            while (*args) {
				snprintf(arg_line, sizeof(arg_line), "%s%s ", arg_line, (*args));
                args++;
            }
        add_assoc_string(return_value, "cmd", arg_line);
        add_assoc_long(return_value, "uid", kinfo[i].ki_uid);
        exists = 1;
    }
    kvm_close(kernel);
    if (!exists)
    	RETURN_FALSE;
}

PHP_FUNCTION(system_os_service_pid)
{
	char *service_name;
	size_t service_name_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &service_name, &service_name_len) == FAILURE) { 
	   return;
	}

	char pidpath[_POSIX_PATH_MAX];
	snprintf(pidpath, sizeof(pidpath), "%s/%s.pid", SYSTEM_OS_SERVICE_RUN_DIR, service_name);

	FILE *f = fopen(pidpath, "r");
	if (f == NULL)
		RETURN_FALSE;

	pid_t pid = -1;

	fscanf(f, "%d", &pid);

	fclose(f);
	if (pid < 1)
		RETURN_FALSE;
	RETURN_LONG(pid);
}

PHP_FUNCTION(system_os_service_start)
{
	char *service_name;
	size_t service_name_len;
	char *cmd, *args;
	size_t cmd_len, args_len;
	zend_bool nopid = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss|b", &service_name, &service_name_len, 
		&cmd, &cmd_len, &args, &args_len, &nopid) == FAILURE) { 
	   return;
	}

	pid_t pid = base_execl_path_nowait(cmd, args, NULL);
	if (pid > 0) {
		if (!nopid) {
			char pidpath[_POSIX_PATH_MAX];
			snprintf(pidpath, sizeof(pidpath), "%s/%s.pid", SYSTEM_OS_SERVICE_RUN_DIR, service_name);

			FILE *f = fopen(pidpath, "w");
			if (f != NULL) {
				fprintf(f, "%d", pid);
				fclose(f);
			}  // TODO false or true????
		}
	} else {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}

PHP_FUNCTION(system_os_service_stop)
{
	char *service_name;
	size_t service_name_len;
	zval *stop_cmd;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &service_name, &service_name_len, &stop_cmd) == FAILURE) { 
	   return;
	}

	if (Z_TYPE_P(stop_cmd) != IS_STRING) {
		char pidpath[_POSIX_PATH_MAX];
		snprintf(pidpath, sizeof(pidpath), "%s/%s.pid", SYSTEM_OS_SERVICE_RUN_DIR, service_name);

		FILE *f = fopen(pidpath, "r");
		if (f == NULL)
			RETURN_FALSE;

		pid_t pid = -1;

		fscanf(f, "%d", &pid);

		fclose(f);
		if (pid < 1)
			RETURN_FALSE;
		if (kill(pid, SIGTERM))
			RETURN_FALSE;
	} else {
		base_execs_out(Z_STRVAL_P(stop_cmd), NULL);
	}
	
	RETURN_TRUE;
}

PHP_FUNCTION(system_os_datetime_set)
{
	zval *datetime;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &datetime) == FAILURE) { 
	   return;
	}
	struct tm mtm;
	memset(&mtm, 0, sizeof(struct tm));
	zval *item;
	if ((item = zend_hash_str_find(Z_ARRVAL_P(datetime), "sec", sizeof("sec")-1)) != NULL && Z_TYPE_P(item) == IS_LONG) {
		if (Z_LVAL_P(item) >= 0 && Z_LVAL_P(item) <= 59)
			mtm.tm_sec = Z_LVAL_P(item);
	}
	if ((item = zend_hash_str_find(Z_ARRVAL_P(datetime), "min", sizeof("min")-1)) != NULL && Z_TYPE_P(item) == IS_LONG) {
		if (Z_LVAL_P(item) >= 0 && Z_LVAL_P(item) <=59)
			mtm.tm_min = Z_LVAL_P(item);
	}
	if ((item = zend_hash_str_find(Z_ARRVAL_P(datetime), "hour", sizeof("hour")-1)) != NULL && Z_TYPE_P(item) == IS_LONG) {
		if (Z_LVAL_P(item) >= 0 && Z_LVAL_P(item) <=23)
			mtm.tm_hour = Z_LVAL_P(item);
	}
	if ((item = zend_hash_str_find(Z_ARRVAL_P(datetime), "year", sizeof("year")-1)) != NULL && Z_TYPE_P(item) == IS_LONG) {
		if (Z_LVAL_P(item) > SYSTEM_OS_TIME_INIT)
			mtm.tm_year = Z_LVAL_P(item) - SYSTEM_OS_TIME_INIT;
		else
			mtm.tm_year = SYSTEM_OS_TIME_INIT;
	}
	if ((item = zend_hash_str_find(Z_ARRVAL_P(datetime), "mon", sizeof("mon")-1)) != NULL && Z_TYPE_P(item) == IS_LONG) {
		if (Z_LVAL_P(item) >= 0 && Z_LVAL_P(item) <=11)
			mtm.tm_mon = Z_LVAL_P(item);
	}
	if ((item = zend_hash_str_find(Z_ARRVAL_P(datetime), "mday", sizeof("mday")-1)) != NULL && Z_TYPE_P(item) == IS_LONG) {
		if (Z_LVAL_P(item) > 0 && Z_LVAL_P(item) <=31)
			mtm.tm_mday = Z_LVAL_P(item);
	}

	time_t mtime = mktime(&mtm);
	struct timespec tp;
	tp.tv_sec = mtime;
	tp.tv_nsec = 0;

	if (clock_settime(CLOCK_REALTIME, &tp))
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(system_os_reboot)
{
	char *cmd = INI_STR("kcs.reboot.cmd");
	if (cmd == NULL)
		RETURN_FALSE;
	if (strcmp(cmd, "none") != 0) {
		pid_t pid = fork();
		if (pid == 0) {

		} else if (pid == -1) {
			RETURN_FALSE;
		} else {
			if (waitpid(pid, NULL, 0) < 0) {
				RETURN_FALSE;
	        }
	    }
	}

	reboot(RB_AUTOBOOT);

}

PHP_FUNCTION(system_os_extract)
{
	char *filename, *path;
	size_t filename_len, path_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &filename, &filename_len, &path, &path_len) == FAILURE) { 
	   return;
	}

	char cwd[PATH_MAX];
	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		RETURN_FALSE;
	}

	if (chdir(path))
		RETURN_FALSE;

	int ret = 0;
	struct archive *a;
	struct archive *ext;
	struct archive_entry *entry;
	int flags;
	int r;

	flags = ARCHIVE_EXTRACT_OWNER | ARCHIVE_EXTRACT_PERM;

	a = archive_read_new();
	archive_read_support_format_all(a);
#if ARCHIVE_VERSION_NUMBER < 3000000
	archive_read_support_compression_all(a);
#else
	archive_read_support_filter_all(a);
#endif
	ext = archive_write_disk_new();
	archive_write_disk_set_options(ext, flags);
	archive_write_disk_set_standard_lookup(ext);
#if ARCHIVE_VERSION_NUMBER < 3000000
	if ((r = archive_read_open_file(a, filename, 10240))) {
		ret = 1;
		goto out;
	}
#else
	if ((r = archive_read_open_filename(a, filename, 10240))) {
		ret = 1;
		goto out;
	}
#endif
	for (;;) {
		r = archive_read_next_header(a, &entry);
		if (r == ARCHIVE_EOF)
			break;
		if (r < ARCHIVE_WARN) {
			ret = 1;
			goto out;
		}
		r = archive_write_header(ext, entry);
		if (r == ARCHIVE_OK && archive_entry_size(entry) > 0) {
			r = system_os_copy_data(a, ext);
			if (r < ARCHIVE_WARN) {
				ret = 1;
				goto out;
			}
		}
		r = archive_write_finish_entry(ext);
		if (r < ARCHIVE_WARN) {
			ret = 1;
			goto out;
		}
	}

out:
	archive_read_close(a);
	archive_read_free(a);
	archive_write_close(ext);
	archive_write_free(ext);

	if (chdir(cwd))
		RETURN_FALSE;
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(system_os_ncpu)
{
	int buf;
	size_t size = sizeof(buf);
	if (sysctlbyname("hw.ncpu", &buf, &size, NULL, 0))
		RETURN_FALSE;
	RETURN_LONG(buf);
}

PHP_FUNCTION(system_os_cpu_set)
{
	zval *proc;
	zend_long pid;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lz", &pid, &proc) == FAILURE) { 
	   return;
	}

	if ((Z_TYPE_P(proc) != IS_LONG && Z_TYPE_P(proc) != IS_ARRAY) || pid == 0)
		RETURN_FALSE;

	cpuset_t mask;
	cpulevel_t level;
	cpuwhich_t which;

	CPU_ZERO(&mask);
	level = CPU_LEVEL_WHICH;
	which = CPU_WHICH_PID;

	if (Z_TYPE_P(proc) == IS_LONG) {
		if (Z_LVAL_P(proc) == -1) {
			if (cpuset_getaffinity(CPU_LEVEL_ROOT, CPU_WHICH_PID, -1,
			    sizeof(mask), &mask) != 0)
				RETURN_FALSE;
		} else {
			CPU_SET(Z_LVAL_P(proc), &mask);
		}
	} else {
		HashTable *hash;
		zval *data;
		HashPosition pointer;
		hash = Z_ARRVAL_P(proc);
		for(
	        zend_hash_internal_pointer_reset_ex(hash, &pointer);
	        (data = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
	        zend_hash_move_forward_ex(hash, &pointer)
	    ) {
	    	if (Z_TYPE_P(data) != IS_LONG)
	    		RETURN_FALSE;
	    	CPU_SET(Z_LVAL_P(data), &mask);
		}
	}

	if (cpuset_setaffinity(level, which, pid, sizeof(mask),
	    &mask) != 0)
	    RETURN_FALSE;

	RETURN_TRUE;
}

PHP_FUNCTION(system_os_mount)
{
	char *type, *mountpoint;
	char *source = "";
	size_t type_len, mountpoint_len, source_len;
	source_len = sizeof("") - 1;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|s", &type, &type_len, &mountpoint, &mountpoint_len, &source, &source_len) == FAILURE) { 
	   return;
	}

	int allowed = INI_INT("kcs.php.mount.enable");
	if (!allowed)
		RETURN_FALSE;

	int f = 0;
	if (strcmp(type, "nullfs") == 0 || strcmp(type, "zfs") == 0) {
		if(!source[0]) {
			RETURN_FALSE;
		} else {
			f = 1;
		}
	}

	if (!f && source[0])
		RETURN_FALSE;

	char *names[] = {"fstype", "fspath", "from"};

	struct iovec iov[6];
	int i = 4;
	iov[0].iov_base = __DECONST(char *, names[0]);
	iov[0].iov_len = strlen(names[0]) + 1;
	iov[1].iov_base = __DECONST(char *, type);
	iov[1].iov_len = strlen(type) + 1;
	iov[2].iov_base = __DECONST(char *, names[1]);
	iov[2].iov_len = strlen(names[1]) + 1;
	iov[3].iov_base = __DECONST(char *, mountpoint);
	iov[3].iov_len = strlen(mountpoint) + 1;

	if (f) {
		i = 6;
		iov[4].iov_base = __DECONST(char *, names[2]);
		iov[4].iov_len = strlen(names[2]) + 1;
		iov[5].iov_base = __DECONST(char *, source);
		iov[5].iov_len = strlen(source) + 1;
	}

	int ret = nmount(iov, i, 0);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(system_os_unmount)
{
	char *mountpoint;
	size_t mountpoint_len;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &mountpoint, &mountpoint_len) == FAILURE) { 
	   return;
	}

	int allowed = INI_INT("kcs.php.mount.enable");
	if (!allowed)
		RETURN_FALSE;

	if (unmount(mountpoint, 0))
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(system_os_mount_list)
{
	int allowed = INI_INT("kcs.php.mount.enable");
	if (!allowed)
		RETURN_FALSE;

	int mntsize;
	struct statfs *mntbuf;
	if ((mntsize = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0)
		RETURN_FALSE;

	array_init(return_value);
	for (int i = 0; i < mntsize; i++) {
		zval subitem;
		array_init(&subitem);
		add_assoc_string(&subitem, "type", mntbuf[i].f_fstypename);
		add_assoc_string(&subitem, "mountpoint", mntbuf[i].f_mntonname);
		add_assoc_string(&subitem, "source", mntbuf[i].f_mntfromname);
		add_next_index_zval(return_value, &subitem);
	}
}

PHP_FUNCTION(system_os_chroot_cmd)
{
	char *dir/*, *cmd*/;
	zval *cmd;
	size_t dir_len, *cmd_len;
	zend_bool is_fork = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz|b", &dir, &dir_len, &cmd/*, &cmd_len*/, &is_fork) == FAILURE) { 
	   return;
	}

	int allowed = INI_INT("kcs.php.mount.enable");
	if (allowed == 0) {
		RETURN_FALSE;
	}

	if (is_fork) {
		pid_t child;
		child = fork();
		if (!child) {
			if (chdir(dir) == -1 || chroot(".") == -1)
				_exit(1);

			char *av[64];

			HashTable *hash;
			zval *data;
			HashPosition pointer;
			hash = Z_ARRVAL_P(cmd);
			int i = 0;
			for(
		        zend_hash_internal_pointer_reset_ex(hash, &pointer);
		        (data = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
		        zend_hash_move_forward_ex(hash, &pointer)
		    ) {
		    	av[i++] = Z_STRVAL_P(data);
			}
			// char *tmp = strtok(cmd, " ");
			// while (tmp != NULL) {
			// 	av[i++] = tmp;
			// 	tmp = strtok(NULL, " ");
			// }
			av[i] = NULL;

			execvp(av[0], av);
			RETURN_FALSE;
		} else {
			if (waitpid(child, NULL, 0) < 0) {
				RETURN_FALSE;
	        }
	        RETURN_TRUE;
		}
	} else {
		if (chdir(dir) == -1 || chroot(".") == -1)
			RETURN_FALSE;

		char *av[64];
			HashTable *hash;
			zval *data;
			HashPosition pointer;
			hash = Z_ARRVAL_P(cmd);
			int i = 0;
			for(
		        zend_hash_internal_pointer_reset_ex(hash, &pointer);
		        (data = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
		        zend_hash_move_forward_ex(hash, &pointer)
		    ) {
		    	av[i++] = Z_STRVAL_P(data);
			}
		// char *tmp = strtok(cmd, " ");
		// int i = 0;
		// while (tmp != NULL) {
		// 	av[i++] = tmp;
		// 	tmp = strtok(NULL, " ");
		// }
		av[i] = NULL;
		execvp(av[0], av);
	}
	RETURN_FALSE;
}

PHP_FUNCTION(system_os_kenv_get)
{
	char *key;
	size_t key_len;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len) == FAILURE) { 
	   return;
	}

	char val[KENV_MVALLEN + 1];
	if(kenv(KENV_GET, key, val, sizeof(val)) < 0)
		RETURN_FALSE;

	RETURN_STRING(val);
}

static int
system_os_copy_data(struct archive *ar, struct archive *aw)
{
	int r;
	const void *buff;
	size_t size;
	off_t offset;

	for (;;) {
		r = archive_read_data_block(ar, &buff, &size, &offset);
		if (r == ARCHIVE_EOF)
			return (ARCHIVE_OK);
		if (r != ARCHIVE_OK)
			return (r);
		r = archive_write_data_block(aw, buff, size, offset);
		if (r != ARCHIVE_OK) {
			return (r);
		}
	}
}
