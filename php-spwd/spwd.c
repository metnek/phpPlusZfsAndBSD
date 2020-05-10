#include <sys/param.h>
#include <sys/endian.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pwd.h>
#include <db.h>
#include "php.h"
#include "php_spwd.h"


#define SPWD_ADD_ENTRY				0
#define SPWD_SET_ENTRY				1

#define SPWD_GET_CH					0
#define SPWD_GET_ALL				1


#define SPWD_PWD_LEN				106
#define SPWD_RAND_STR_LEN			64
#define PHP_SPWD_CLASSNAME_MAX		64

#define CURRENT_VERSION(x) 			_PW_VERSIONED(x, 4)
#define EXPAND(e)					e=lp; while( (*lp++ = *dp++) );
#define SCALAR(v)					memmove(&(v), dp, sizeof(v)); dp += sizeof(v); v = ntohl(v);
#define	COMPACT(e)					lp = e; while ((*dp++ = *lp++));
#define SCALAR1(e)					store = htonl((uint32_t)(e));      \
									memmove(dp, &store, sizeof(store)); \
									dp += sizeof(store);
#define PATH_CMP_DB					"/etc/cookie.db"

#define SWPD_SET_STR(e, v, c) \
	if (e == NULL || Z_TYPE_P(e) == IS_NULL) { \
		if ((e = zend_hash_str_find(Z_ARRVAL_P(&item), #v, sizeof(#v) - 1)) != NULL && Z_TYPE_P(e) == IS_STRING) { \
			v = Z_STRVAL_P(e); \
		} else { \
			v = c; \
		} \
	} else { \
		v = Z_STRVAL_P(e); \
	}

#define SWPD_SET_NUM(e, v) \
	if (e == -1) { \
		if ((old_item = zend_hash_str_find(Z_ARRVAL_P(&item), #v, sizeof(#v) - 1)) != NULL && Z_TYPE_P(old_item) == IS_LONG) { \
			v = Z_LVAL_P(old_item); \
		} else { \
			ret = 1; \
		} \
	} else { \
		v = e; \
	}


static int spwd_get(DB *dbp, zval *find, int is_num, zval *ret, int flag);
static int spwd_get_by_hash(char *hash, zval *ret);
static int spwd_check_pw_name(DB *dbp, const char *pw_name, int *uid, int *num, int cmd);
static int spwd_write(DB *dbp, unsigned int uid, int num, char *pw_name, char *pw_pwd, zend_ulong pw_gid, char *pw_class,
	char *pw_desc, char *pw_dir, char *pw_shell, zend_ulong pw_change, zend_ulong pw_expire);
static int spwd_del(DB *dbp, const char *pw_name, int  pw_uid, int num);
static void spwd_gen_rand_string(char *str, size_t len);
static int spwd_check_hash(char *hash);
static void spwd_clear_hash(int uid);
static int php_spwd_class_parse(char **list, char *class_str);
static int php_spwd_get_user_count(DB *dbp);



PHP_MINIT_FUNCTION(spwd);
PHP_MSHUTDOWN_FUNCTION(spwd);
PHP_FUNCTION(spwd_get_entry);						// done
PHP_FUNCTION(spwd_get_all);							// done
PHP_FUNCTION(spwd_add_entry_full);					// done
PHP_FUNCTION(spwd_add_entry);						// done
PHP_FUNCTION(spwd_set_entry);						// done
PHP_FUNCTION(spwd_del_entry);						// done
PHP_FUNCTION(spwd_verify);							// done
PHP_FUNCTION(spwd_get_entry_by_hash);				// done

PHP_FUNCTION(spwd_class_list);						// wait
PHP_FUNCTION(spwd_class_del_entry);					// wait
PHP_FUNCTION(spwd_class_set_entry);					// wait
PHP_FUNCTION(spwd_class_get);						// wait
PHP_FUNCTION(spwd_srand);							// done


ZEND_BEGIN_ARG_INFO_EX(arginfo_spwd_get_entry, 0, 0, 1)
	ZEND_ARG_INFO(0, hash)
	ZEND_ARG_INFO(0, uid)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_spwd_get_all, 0, 0, 1)
	ZEND_ARG_INFO(0, hash)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_spwd_add_entry, 0, 0, 3)
	ZEND_ARG_INFO(0, hash)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, pwd)
	ZEND_ARG_INFO(0, gid)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_spwd_add_entry_full, 0, 0, 3)
	ZEND_ARG_INFO(0, hash)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, pwd)
	ZEND_ARG_INFO(0, gid)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_spwd_set_entry, 0, 0, 1)
	ZEND_ARG_INFO(0, hash)
	ZEND_ARG_INFO(0, uid)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_spwd_del_entry, 0, 0, 1)
	ZEND_ARG_INFO(0, hash)
	ZEND_ARG_INFO(0, uid)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_spwd_verify, 0, 0, 2)
	ZEND_ARG_INFO(0, username)
	ZEND_ARG_INFO(0, pwd)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_spwd_get_entry_by_hash, 0, 0, 1)
	ZEND_ARG_INFO(0, hash)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_spwd_class_list, 0, 0, 1)
	ZEND_ARG_INFO(0, hash)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_spwd_class_del_entry, 0, 0, 3)
	ZEND_ARG_INFO(0, hash)
	ZEND_ARG_INFO(0, class_name)
	ZEND_ARG_INFO(0, entry)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_spwd_class_set_entry, 0, 0, 3)
	ZEND_ARG_INFO(0, hash)
	ZEND_ARG_INFO(0, class_name)
	ZEND_ARG_INFO(0, entry)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_spwd_class_get, 0, 0, 2)
	ZEND_ARG_INFO(0, hash)
	ZEND_ARG_INFO(0, class_name)
ZEND_END_ARG_INFO()


const zend_function_entry spwd_functions[] = {
	PHP_FE(spwd_get_entry, arginfo_spwd_get_entry)
	PHP_FE(spwd_get_all, NULL)
	PHP_FE(spwd_add_entry, arginfo_spwd_add_entry)
	PHP_FE(spwd_add_entry_full, arginfo_spwd_add_entry_full)
	PHP_FE(spwd_set_entry, arginfo_spwd_set_entry)
	PHP_FE(spwd_del_entry, arginfo_spwd_del_entry)
	PHP_FE(spwd_verify, arginfo_spwd_verify)
	PHP_FE(spwd_get_entry_by_hash, arginfo_spwd_get_entry_by_hash)
	PHP_FE(spwd_class_list, arginfo_spwd_class_list)
	PHP_FE(spwd_class_del_entry, arginfo_spwd_class_del_entry)
	PHP_FE(spwd_class_set_entry, arginfo_spwd_class_set_entry)
	PHP_FE(spwd_class_get, arginfo_spwd_class_get)
	PHP_FE(spwd_srand, NULL)
	{NULL, NULL, NULL}
};

zend_module_entry spwd_module_entry = {
	STANDARD_MODULE_HEADER,
	"spwd",
	spwd_functions,
	PHP_MINIT(spwd),
	PHP_MSHUTDOWN(spwd),
	NULL,
	NULL,
	NULL,
	"0.0.1",
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_SPWD
ZEND_GET_MODULE(spwd)

#endif

PHP_INI_BEGIN()
    PHP_INI_ENTRY("kcs.spwd.start_position", "1000", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("kcs.spwd.class_prefix", NULL, PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("kcs.spwd.class_max_params", "32", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("kcs.spwd.class_file", "/etc/login.conf.db", PHP_INI_ALL, NULL)
PHP_INI_END()

PHP_MINIT_FUNCTION(spwd)
{
	REGISTER_INI_ENTRIES();
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(spwd)
{
    UNREGISTER_INI_ENTRIES();
    return SUCCESS;
}

PHP_FUNCTION(spwd_get_entry)
{
	zval *find;
	char *hash;
	size_t hash_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz", &hash, &hash_len, &find) == FAILURE) { 
	   return;
	}

	int uid = spwd_check_hash(hash);
	int start = INI_INT("kcs.spwd.start_position");
	if (uid < start)
		RETURN_FALSE;

	DB *dbp = dbopen(_PATH_SMP_DB, O_RDWR, S_IRUSR|S_IWUSR, DB_HASH, NULL);
	if (dbp == NULL)
		RETURN_FALSE;
	int ret = 0;
	array_init(return_value);
	ret = spwd_get(dbp, find, 0, return_value, SPWD_GET_CH);
	dbp->close(dbp);

	if (ret) {
		zend_array_destroy(Z_ARR_P(return_value));
		RETURN_FALSE;
	}

}

PHP_FUNCTION(spwd_get_all)
{
	char *hash;
	size_t hash_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &hash, &hash_len) == FAILURE) { 
	   return;
	}

	int start = INI_INT("kcs.spwd.start_position");
	int uid = spwd_check_hash(hash);
	if (uid < start)
		RETURN_FALSE;


	DB *dbp = dbopen(_PATH_SMP_DB, O_RDWR, S_IRUSR|S_IWUSR, DB_HASH, NULL);
	if (dbp == NULL)
		RETURN_FALSE;

	zval subitem;
	int ucount = php_spwd_get_user_count(dbp);
	if (!ucount) {
		dbp->close(dbp);
		RETURN_FALSE;
	}
	int ret = 0, c = 0;
	array_init(return_value);
	int i = 0;
	while (c <= ucount) {
		array_init(&subitem);
		zval find;
		c++;
		ZVAL_LONG(&find, c);
		ret = spwd_get(dbp, &find, 1, &subitem, SPWD_GET_CH);
		if (ret == 0) {
			zval *tmp;
			if ((tmp = zend_hash_str_find(Z_ARRVAL_P(&subitem), "uid", sizeof("uid")-1)) != NULL && Z_TYPE_P(tmp) == IS_LONG) {
				if (Z_LVAL_P(tmp) >= start && Z_LVAL_P(tmp) < 65533)
					add_index_zval(return_value, i++, &subitem);
				else
					zend_array_destroy(Z_ARRVAL_P(&subitem));
			}
		} else {
			break;
		}
	}
	dbp->close(dbp);
	if (ret && c == 1) {
		zend_array_destroy(Z_ARR_P(return_value));
		RETURN_FALSE;
	}
}

PHP_FUNCTION(spwd_add_entry_full)
{
	char *hash;
	char *pw_name;
	char *pw_pwd;
	zend_ulong pw_gid;
	char *pw_class = "";
	char *pw_desc = "";
	zend_ulong pw_uid = -1;
	char *pw_dir = "/nonexistent";
	char *pw_shell = "/usr/sbin/nologin";
	zend_ulong pw_change = 0;
	zend_ulong pw_expire = 0;

	size_t hash_len;
	size_t pw_name_len, pw_pwd_len;
	size_t pw_class_len = sizeof("") - 1;
	size_t pw_desc_len = sizeof("") - 1;
	size_t pw_dir_len = sizeof("") - 1;
	size_t pw_shell_len = sizeof("") - 1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sssl|sslssll", &hash, &hash_len, &pw_name, &pw_name_len,
			&pw_pwd, &pw_pwd_len, &pw_gid, &pw_class, &pw_class_len, &pw_desc, &pw_desc_len, &pw_uid, &pw_dir,
			&pw_dir_len, &pw_shell, &pw_shell_len, &pw_change, &pw_expire) == FAILURE) { 
	   return;
	}

	int start = INI_INT("kcs.spwd.start_position");
	int chuid = spwd_check_hash(hash);
	if (chuid < start)
		RETURN_FALSE;

	int uid = -1;
	int num = -1;
	if (pw_uid < start) {
		RETURN_FALSE;
	} else {
		uid = pw_uid;
	}

	DB *dbp = dbopen(_PATH_SMP_DB, O_RDWR, S_IRUSR|S_IWUSR, DB_HASH, NULL);
	if (dbp == NULL)
		RETURN_FALSE;

	int ret = spwd_check_pw_name(dbp, pw_name, &uid, &num, SPWD_ADD_ENTRY);

	if (ret != 0 || uid < start) {
		dbp->close(dbp);
		RETURN_FALSE;
	}
	ret = spwd_write(dbp, uid, num, pw_name, pw_pwd, pw_gid, pw_class,
	pw_desc, pw_dir, pw_shell, pw_change, pw_expire);

	dbp->close(dbp);

#ifdef NEED_MASTER_SPWD
	if (!ret) {
		FILE *file = fopen(_PATH_MASTERPASSWD, "a");
		if (file != NULL) {
			fprintf(file, "%s:%s:%d:%ld:%s:%ld:%ld:%s:%s:%s\n", pw_name, pw_pwd, uid, pw_gid, pw_class, pw_change, pw_expire, pw_desc, pw_dir, pw_shell);
			fclose(file);
		}
	}
#endif

	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}


PHP_FUNCTION(spwd_add_entry)
{
	char *hash;
	char *pw_name;
	char *pw_pwd;
	zend_ulong pw_gid;
	char *pw_class = "";
	char *pw_desc = "";
	zend_ulong pw_uid = -1;

	size_t hash_len;
	size_t pw_name_len, pw_pwd_len;
	size_t pw_class_len = sizeof("") - 1;
	size_t pw_desc_len = sizeof("") - 1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sssl|ssl", &hash, &hash_len, &pw_name, &pw_name_len,
			&pw_pwd, &pw_pwd_len, &pw_gid, &pw_class, &pw_class_len, &pw_desc, &pw_desc_len, &pw_uid) == FAILURE) { 
	   return;
	}
	int start = INI_INT("kcs.spwd.start_position");
	int chuid = spwd_check_hash(hash);
	if (chuid < start)
		RETURN_FALSE;
	int uid = -1;
	int num = -1;
	char *pw_dir = "/nonexistent";
	char *pw_shell = "/usr/sbin/nologin";
	zend_ulong pw_change = 0;
	zend_ulong pw_expire = 0;
	if (pw_uid < start) {
		RETURN_FALSE;
	} else {
		uid = pw_uid;
	}
	DB *dbp = dbopen(_PATH_SMP_DB, O_RDWR, S_IRUSR|S_IWUSR, DB_HASH, NULL);
	if (dbp == NULL)
		RETURN_FALSE;

	int ret = spwd_check_pw_name(dbp, pw_name, &uid, &num, SPWD_ADD_ENTRY);

	if (ret != 0 || uid < start) {
		dbp->close(dbp);
		RETURN_FALSE;
	}

	ret = spwd_write(dbp, uid, num, pw_name, pw_pwd, pw_gid, pw_class,
	pw_desc, pw_dir, pw_shell, pw_change, pw_expire);
	dbp->close(dbp);

#ifdef NEED_MASTER_SPWD
	if (!ret) {
		FILE *file = fopen(_PATH_MASTERPASSWD, "a");
		if (file != NULL) {
			fprintf(file, "%s:%s:%d:%ld:%s:%ld:%ld:%s:%s:%s\n", pw_name, pw_pwd, uid, pw_gid, pw_class, pw_change, pw_expire, pw_desc, pw_dir, pw_shell);
			fclose(file);
		}
	}
#endif

	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(spwd_set_entry)
{
	char *hash;
	zend_ulong pw_uid;
	zval *pw_name = NULL;
	zval *pw_pwd = NULL;
	zend_long pw_gid = -1;
	zval *pw_class = NULL;
	zval *pw_desc = NULL;
	zval *pw_dir = NULL;
	zval *pw_shell = NULL;
	zend_long pw_change = -1;
	zend_long pw_expire = -1;

	size_t hash_len;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl|zzlzzzzll", &hash, &hash_len, &pw_uid, &pw_name, &pw_pwd, &pw_gid, &pw_class,
			&pw_desc, &pw_dir, &pw_shell, &pw_change, &pw_expire) == FAILURE) { 
	   return;
	}

	int start = INI_INT("kcs.spwd.start_position");
	int chuid = spwd_check_hash(hash);
	if (chuid < start)
		RETURN_FALSE;
	DB *dbp = dbopen(_PATH_SMP_DB, O_RDWR, S_IRUSR|S_IWUSR, DB_HASH, NULL);
	if (dbp == NULL)
		RETURN_FALSE;

	zval item;
	array_init(&item);
	zval find;
	ZVAL_LONG(&find, pw_uid);

	int ret = spwd_get(dbp, &find, 0, &item, SPWD_GET_ALL);
	int uid = -1;
	int num = -1;
	char *old_name = NULL;
	zval *old_item;
	if ((old_item = zend_hash_str_find(Z_ARRVAL_P(&item), "name", sizeof("name")-1)) != NULL && Z_TYPE_P(old_item) == IS_STRING) {
		old_name = Z_STRVAL_P(old_item);
		ret = spwd_check_pw_name(dbp, old_name, &uid, &num, SPWD_SET_ENTRY);
		if (ret || uid < start) {
			ret = 1;
			goto out;
		}
	}
  	int uid_tm = 0, num_tm = 0;
  
  	if (Z_TYPE_P(pw_name) == IS_STRING &&
        strcmp(old_name, Z_STRVAL_P(pw_name)) != 0 && strlen(Z_STRVAL_P(pw_name)) > 0) {
          if (spwd_check_pw_name(dbp, Z_STRVAL_P(pw_name), &uid_tm, &num_tm, SPWD_GET_ALL) == 0) {
              ret = 1;
              goto out;
          }
    }
	ret = spwd_del(dbp, old_name, -1, -1);
	if (ret)
		goto out;

	ret = 0;
	char *name, *pwd, *class, *desc;
	char *dir, *shell;
	long gid, change, expire;

	name = pwd = class = desc = dir = shell = NULL;

	SWPD_SET_STR(pw_name, name, "");
	SWPD_SET_STR(pw_pwd, pwd, "*");
	SWPD_SET_STR(pw_desc, desc, "");
	SWPD_SET_STR(pw_class, class, "");
	SWPD_SET_STR(pw_dir, dir, "/nonexistent");
	SWPD_SET_STR(pw_shell, shell, "/usr/sbin/nologin");
	SWPD_SET_NUM(pw_gid, gid);
	SWPD_SET_NUM(pw_change, change);
	SWPD_SET_NUM(pw_expire, expire);

	if (ret)
		goto out;
	ret = spwd_write(dbp, pw_uid, num, name, pwd, gid, class,
	desc, dir, shell, change, expire);

#ifdef NEED_MASTER_SPWD
	if (!ret) {
		FILE *file = fopen(_PATH_MASTERPASSWD, "r");
		if (file != NULL) {
			char str[1024];
			char tmp_file[strlen(_PATH_MASTERPASSWD) + 6];
			char name_match[strlen(name) + 2];
			snprintf(name_match, sizeof(name_match), "%s:", name);
			snprintf(tmp_file, sizeof(tmp_file), "%s.tmp", _PATH_MASTERPASSWD);
			FILE *t_file = fopen(tmp_file, "w");
		    while(fscanf(file, "%[^\n]\n", str) != -1) {
		    	if (strncmp(str, name_match, strlen(name_match)) != 0) {
		    		fprintf(t_file, "%s\n", str);
		    	}
		    }
		    fclose(t_file);
		    fclose(file);
		    rename(tmp_file, _PATH_MASTERPASSWD);
		}file = fopen(_PATH_MASTERPASSWD, "a");
		if (file != NULL) {
			fprintf(file, "%s:%s:%lu:%ld:%s:%ld:%ld:%s:%s:%s\n", name, pwd, pw_uid, gid, class, change, expire, desc, dir, shell);
			fclose(file);
		}
	}
#endif

out:
	dbp->close(dbp);

	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(spwd_del_entry)
{
	char *hash;
	zend_ulong pw_uid;
	size_t hash_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &hash, &hash_len, &pw_uid) == FAILURE) { 
	   return;
	}

	int start = INI_INT("kcs.spwd.start_position");
	int chuid = spwd_check_hash(hash);
	if (chuid < start)
		RETURN_FALSE;

	DB *dbp = dbopen(_PATH_SMP_DB, O_RDWR, S_IRUSR|S_IWUSR, DB_HASH, NULL);
	if (dbp == NULL)
		RETURN_FALSE;

	zval item;
	array_init(&item);
	zval find;
	ZVAL_LONG(&find, pw_uid);

	int ret = spwd_get(dbp, &find, 0, &item, SPWD_GET_CH);

	int uid = -1;
	int num = -1;
	char *name = NULL;
	zval *subitem;
	if ((subitem = zend_hash_str_find(Z_ARRVAL_P(&item), "name", sizeof("name")-1)) != NULL && Z_TYPE_P(subitem) == IS_STRING) {
		name = Z_STRVAL_P(subitem);
		ret = spwd_check_pw_name(dbp, name, &uid, &num, SPWD_SET_ENTRY);
		if (ret || uid < start) {
			ret = 1;
			goto out;
		}
	}
	if ((unsigned long)uid != pw_uid) {
		ret = 1;
		goto out;
	}

	ret = spwd_del(dbp, name, pw_uid, num);
	if (!ret)
		spwd_clear_hash(uid);

#ifdef NEED_MASTER_SPWD
	if (!ret) {
		FILE *file = fopen(_PATH_MASTERPASSWD, "r");
		if (file != NULL) {
			char str[1024];
			char tmp_file[strlen(_PATH_MASTERPASSWD) + 6];
			char name_match[strlen(name) + 2];
			snprintf(name_match, sizeof(name_match), "%s:", name);
			snprintf(tmp_file, sizeof(tmp_file), "%s.tmp", _PATH_MASTERPASSWD);
			FILE *t_file = fopen(tmp_file, "w");
		    while(fscanf(file, "%[^\n]\n", str) != -1) {
		    	if (strncmp(str, name_match, strlen(name_match)) != 0) {
		    		fprintf(t_file, "%s\n", str);
		    	}
		    }
		    fclose(t_file);
		    fclose(file);
		    rename(tmp_file, _PATH_MASTERPASSWD);
		}
	}
#endif

out:
	dbp->close(dbp);

	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(spwd_verify)
{
	char *username, *pwd;
	size_t username_len, pwd_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &username, &username_len, &pwd, &pwd_len) == FAILURE) { 
	   return;
	}

	int d = 0;
	DB *dbp = dbopen(_PATH_SMP_DB, O_RDWR, S_IRUSR|S_IWUSR, DB_HASH, NULL);
	if (dbp == NULL) {
		RETURN_FALSE;
	}

	zval find;
	zend_string *tmp;
	tmp = zend_string_init(username, strlen(username), 1);

	ZVAL_STR(&find, tmp);

	zval item;
	array_init(&item);
	int ret = spwd_get(dbp, &find, 0, &item, SPWD_GET_ALL);
	if (ret) {
		dbp->close(dbp);
		RETURN_FALSE;
	}
	dbp->close(dbp);

	char *pw_pwd = NULL;
	zval *subitem;
	if ((subitem = zend_hash_str_find(Z_ARRVAL_P(&item), "pwd", sizeof("pwd")-1)) != NULL && Z_TYPE_P(subitem) == IS_STRING) {
		pw_pwd = Z_STRVAL_P(subitem);
	}
	if (pw_pwd == NULL) {
		RETURN_FALSE;
	}
	if (strcmp(pw_pwd, crypt(pwd, pw_pwd)) != 0) {
		RETURN_FALSE;
	}

	long uid = -1;
	if ((subitem = zend_hash_str_find(Z_ARRVAL_P(&item), "uid", sizeof("uid")-1)) != NULL && Z_TYPE_P(subitem) == IS_LONG) {
		uid = Z_LVAL_P(subitem);
	}
	int start = INI_INT("kcs.spwd.start_position");
	if (uid < start) {
		RETURN_FALSE;
	}

	dbp = dbopen(PATH_CMP_DB, O_RDWR, S_IRUSR|S_IWUSR, DB_HASH, NULL);
	if (dbp == NULL) {
		dbp = dbopen(PATH_CMP_DB, O_CREAT | O_RDWR, S_IRUSR|S_IWUSR, DB_HASH, NULL);
		if (dbp == NULL) {
			RETURN_FALSE;
        }
	}
	ret = 0;
	DBT key, data;
	uint32_t store;
	char *dp;
	spwd_clear_hash(uid);

	char rand_str[SPWD_RAND_STR_LEN + 1];
  	spwd_gen_rand_string(rand_str, SPWD_RAND_STR_LEN);
	char buf[SPWD_RAND_STR_LEN + 1];
	data.data = buf;
	dp = data.data;
	key.data = rand_str;
	key.size = SPWD_RAND_STR_LEN;
	SCALAR1(uid);
	data.size = dp - buf;
	ret = dbp->put(dbp, &key, &data, 0);
	dbp->close(dbp);
	if (ret) {
		RETURN_FALSE;
	}
	RETURN_STRINGL(rand_str, SPWD_RAND_STR_LEN);
}

PHP_FUNCTION(spwd_get_entry_by_hash)
{
	char *hash;
	size_t hash_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &hash, &hash_len) == FAILURE) { 
	   return;
	}

	if (strlen(hash) != SPWD_RAND_STR_LEN)
		RETURN_FALSE;

	int uid = 0, ret = 0;

	uid = spwd_check_hash(hash);
	int start = INI_INT("kcs.spwd.start_position");
	if (uid < start)
		RETURN_FALSE;

	DB *dbp = dbopen(_PATH_SMP_DB, O_RDWR, S_IRUSR|S_IWUSR, DB_HASH, NULL);
	if (dbp == NULL)
		RETURN_FALSE;
	zval find;

	ZVAL_LONG(&find, uid);
	array_init(return_value);
	ret = spwd_get(dbp, &find, 0, return_value, SPWD_GET_CH);
	dbp->close(dbp);

	if (ret) {
		zend_array_destroy(Z_ARR_P(return_value));
		RETURN_FALSE;
	}

}

static int
spwd_get(DB *dbp, zval *find, int is_num, zval *info, int flag)
{
	char verskey[] = _PWD_VERSION_KEY;
	DBT key, data;
	int ret = 0;
	int use_version;
	char *buf = NULL;
	size_t bufsz=0;
	char *dp, *lp;
	int32_t pw_change, pw_expire;
	char keystr[MAX(MAXPATHLEN, LINE_MAX * 2)];

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	key.data = verskey;
	key.size = sizeof(verskey)-1;
	int min = INI_INT("kcs.spwd.start_position");

	if ((dbp->get)(dbp, &key, &data, 0) == 0)
		use_version = *(unsigned char *)data.data;
	else
		use_version = 3;

	if (Z_TYPE_P(find) == IS_LONG) {
		uint32_t store;
		if (is_num) {
			// if (Z_LVAL_P(find) < min + 1) {
			// 	return -1;
			// }
			keystr[0] = _PW_VERSIONED(_PW_KEYBYNUM, use_version);
		} else {
			if (Z_LVAL_P(find) < min || Z_LVAL_P(find) > 65533) {
				return -1;
			}
			keystr[0] = _PW_VERSIONED(_PW_KEYBYUID, use_version);
		}
		store = htonl(Z_LVAL_P(find));
		memmove(keystr + 1, &store, sizeof(store));
		key.size = sizeof(int)+1;
	} else if (Z_TYPE_P(find) == IS_STRING) {
		keystr[0] = _PW_VERSIONED(_PW_KEYBYNAME, use_version);
		size_t len = strlen(Z_STRVAL_P(find));
		memmove(keystr + 1, Z_STRVAL_P(find), MIN(len, sizeof(keystr) - 1));
		key.size = len + 1;
	} else {
		return -1;
	}
	key.data = keystr;

	ret = (dbp->get)(dbp, &key, &data, 0);
	if (ret != 0) {
		return -1;
	}

	dp = data.data;

	if(data.size > bufsz)
	{
		bufsz = data.size + 1024;
		buf = reallocf(buf, bufsz);
		if(buf==NULL) {
			return -1;
		}
	}
	bzero(buf, bufsz);

	lp = buf;

	struct passwd pstore;
	EXPAND(pstore.pw_name);
	EXPAND(pstore.pw_passwd);
	SCALAR(pstore.pw_uid);
	if (pstore.pw_uid < min || pstore.pw_uid > 65533) {
		free(buf);
		return 0;
	}
	SCALAR(pstore.pw_gid);
	SCALAR(pw_change);
	EXPAND(pstore.pw_class);
	EXPAND(pstore.pw_gecos);
	EXPAND(pstore.pw_dir);
	EXPAND(pstore.pw_shell);
	SCALAR(pw_expire);

	bcopy(dp, (char *)&pstore.pw_fields, sizeof(pstore.pw_fields));
	dp += sizeof(pstore.pw_fields);
	add_assoc_string(info, "name", pstore.pw_name);
	if (flag == SPWD_GET_ALL)
		add_assoc_string(info, "pwd", pstore.pw_passwd);
	add_assoc_long(info, "uid", pstore.pw_uid);
	add_assoc_long(info, "gid", pstore.pw_gid);
	add_assoc_long(info, "change", pw_change);
	add_assoc_string(info, "class", pstore.pw_class);
	add_assoc_string(info, "desc", pstore.pw_gecos);
	add_assoc_string(info, "dir", pstore.pw_dir);
	add_assoc_string(info, "shell", pstore.pw_shell);
	add_assoc_long(info, "uid", pstore.pw_uid);
	add_assoc_long(info, "expire", pw_expire);

	free(buf);
	return 0;
}

static int
spwd_get_by_hash(char *hash, zval *ret)
{
	DB *sdbp;
	DB *cdbp;

	return 0;

}

static int
spwd_check_pw_name(DB *dbp, const char *pw_name, int *uid, int *num, int cmd)
{
	zval subitem;
	zval *data;
	int c = 0;
	int ret = 0;
	int f_uid = *uid;
	int start = INI_INT("kcs.spwd.start_position");
	if (cmd != SPWD_ADD_ENTRY)
		*uid = start;

	int ucount = php_spwd_get_user_count(dbp);
	if (!ucount) {
		return 0;
	}
	while (c <= ucount) {
		array_init(&subitem);
		zval find;
		c++;
		ZVAL_LONG(&find, c);
		ret = spwd_get(dbp, &find, 1, &subitem, SPWD_GET_CH);
		if ((data = zend_hash_str_find(Z_ARRVAL_P(&subitem), "name", sizeof("name")-1)) != NULL && Z_TYPE_P(data) == IS_STRING) {
			if (strcmp(pw_name, Z_STRVAL_P(data)) == 0) {
				if (cmd == SPWD_ADD_ENTRY) {
					return -1;
				} else {
					if ((data = zend_hash_str_find(Z_ARRVAL_P(&subitem), "uid", sizeof("uid")-1)) != NULL && Z_TYPE_P(data) == IS_LONG) {
						*uid = Z_LVAL_P(data);
					}
					*num = c;
					return 0;
				}
			}
		}
		if ((data = zend_hash_str_find(Z_ARRVAL_P(&subitem), "uid", sizeof("uid")-1)) != NULL && Z_TYPE_P(data) == IS_LONG) {
			*uid = Z_LVAL_P(data) + 1;
		}
	}
	*num = c;
	if (cmd == SPWD_ADD_ENTRY) {
		char tbuf[32];
		uint32_t store;
		DBT k, d;
		if (f_uid >= start) {
			memset(tbuf, 0, sizeof(tbuf));
			tbuf[0] = CURRENT_VERSION(_PW_KEYBYUID);
			store = htonl(f_uid);
			memmove(tbuf + 1, &store, sizeof(store));
			k.size = sizeof(store) + 1;
			k.data = (u_char *)tbuf;
			if ((dbp->get)(dbp, &k, &d, 0)) {
				*uid = f_uid;
				return 0;
			}
		}

		c = start;

		while (1) {
			memset(tbuf, 0, sizeof(tbuf));
			tbuf[0] = CURRENT_VERSION(_PW_KEYBYUID);
			store = htonl(c++);
			memmove(tbuf + 1, &store, sizeof(store));
			k.size = sizeof(store) + 1;
			k.data = (u_char *)tbuf;
			if ((dbp->get)(dbp, &k, &d, 0)) {
				break;
			}
		}
		*uid = c - 1;
	}

	return 0;
}

static int
spwd_write(DB* dbp, unsigned int uid, int num, char *pw_name, char *pw_pwd, zend_ulong pw_gid, char *pw_class,
	char *pw_desc, char *pw_dir, char *pw_shell, zend_ulong pw_change, zend_ulong pw_expire)
{
	uint32_t store;
	const char *lp;
	char *dp;
	DBT key, data;
	char verskey[] = _PWD_VERSION_KEY;
	int use_version;
	char buf[MAX(MAXPATHLEN, LINE_MAX * 2)];
	char tbuf[1024];
	unsigned int len;

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	key.data = verskey;
	key.size = sizeof(verskey)-1;

	if ((dbp->get)(dbp, &key, &data, 0) == 0)
		use_version = *(unsigned char *)data.data;
	else
		use_version = 3;

	int pw_fields = _PWF_NAME | _PWF_PASSWD | _PWF_UID | _PWF_GID | _PWF_EXPIRE | _PWF_CHANGE;
	if (pw_class[0])
		pw_fields |= _PWF_CLASS;
	if (pw_desc[0])
		pw_fields |= _PWF_GECOS;
	if (pw_dir[0])
		pw_fields |= _PWF_DIR;
	if (pw_shell[0])
		pw_fields |= _PWF_SHELL;
	if (pw_class[0])
		pw_fields |= _PWF_CLASS;


	dp = buf;
	COMPACT(pw_name);
	COMPACT(pw_pwd);
	SCALAR1(uid);
	SCALAR1(pw_gid);
	SCALAR1(pw_change);
	COMPACT(pw_class);
	COMPACT(pw_desc);
	COMPACT(pw_dir);
	COMPACT(pw_shell);
	SCALAR1(pw_expire);
	SCALAR1(pw_fields);

	data.size = dp - buf;
	data.data = (u_char *)buf;
	key.data = (u_char *)tbuf;

	tbuf[0] = CURRENT_VERSION(_PW_KEYBYNAME);
	len = strlen(pw_name);
	memmove(tbuf + 1, pw_name, len);
	key.size = len + 1;
	if ((dbp->put)(dbp, &key, &data, 0) == -1) {
		return -1;
	}

	tbuf[0] = CURRENT_VERSION(_PW_KEYBYUID);
	store = htonl(uid);
	memmove(tbuf + 1, &store, sizeof(store));
	key.size = sizeof(store) + 1;
	if ((dbp->put)(dbp, &key, &data, 0) == -1) {
		return -1;
	}

	tbuf[0] = CURRENT_VERSION(_PW_KEYBYNUM);
	store = htonl(num);
	memmove(tbuf + 1, &store, sizeof(store));
	key.size = sizeof(store) + 1;
	if ((dbp->put)(dbp, &key, &data, 0) == -1) {
		return -1;
	}

	return 0;
}

static int
spwd_del(DB *dbp, const char *pw_name, int pw_uid, int num)
{
	uint32_t store;
	char tbuf[1024];
	DBT key;
	unsigned int len;

	key.data = (u_char *)tbuf;

	tbuf[0] = CURRENT_VERSION(_PW_KEYBYNAME);
	len = strlen(pw_name);
	memmove(tbuf + 1, pw_name, len);
	key.size = len + 1;
	int ret = 0;
	if ((ret = (dbp->del)(dbp, &key, 0)) == -1) {
		return -1;
	}
	if (pw_uid != -1) {
		memset(tbuf, 0, sizeof(tbuf));
		tbuf[0] = CURRENT_VERSION(_PW_KEYBYUID);
		store = htonl(pw_uid);
		memmove(tbuf + 1, &store, sizeof(store));
		key.size = sizeof(store) + 1;
		if ((dbp->del)(dbp, &key, 0) == -1) {
			return -1;
		}
	}
	if (num != -1) {
		memset(tbuf, 0, sizeof(tbuf));
		tbuf[0] = CURRENT_VERSION(_PW_KEYBYNUM);
		store = htonl(num);
		memmove(tbuf + 1, &store, sizeof(store));
		key.size = sizeof(store) + 1;
		if ((dbp->del)(dbp, &key, 0) == -1) {
			return -1;
		}
	}
	DBT k, d;
	memset(tbuf, 0, sizeof(tbuf));
	tbuf[0] = CURRENT_VERSION(_PW_KEYBYNUM);
	store = htonl(++num);
	memmove(tbuf + 1, &store, sizeof(store));
	k.size = sizeof(store) + 1;
	k.data = (u_char *)tbuf;

	while (dbp->get(dbp, &k, &d, 0) == 0) {
		memset(tbuf, 0, sizeof(tbuf));
		tbuf[0] = CURRENT_VERSION(_PW_KEYBYNUM);
		store = htonl(num-1);
		memmove(tbuf + 1, &store, sizeof(store));
		k.size = sizeof(store) + 1;
		dbp->put(dbp, &k, &d, 0);
		memset(tbuf, 0, sizeof(tbuf));
		tbuf[0] = CURRENT_VERSION(_PW_KEYBYNUM);
		store = htonl(++num);
		memmove(tbuf + 1, &store, sizeof(store));
		k.size = sizeof(store) + 1;
		k.data = (u_char *)tbuf;
	}
	memset(tbuf, 0, sizeof(tbuf));
	tbuf[0] = CURRENT_VERSION(_PW_KEYBYNUM);
	store = htonl(num-1);
	memmove(tbuf + 1, &store, sizeof(store));
	k.size = sizeof(store) + 1;
	k.data = (u_char *)tbuf;
	(dbp->del)(dbp, &k, 0);
	return 0;
}

static void
spwd_gen_rand_string(char *str, size_t len)
{
	char charset[] = "0123456789"
            "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int f = open("/dev/urandom", O_RDONLY);            
	unsigned int randomNum;
  
	while (len-- > 0) {
        read(f, &randomNum, sizeof(randomNum));
        size_t index = randomNum % strlen(charset);
        *str++ = charset[index];
    }
	close(f);
    *str = '\0';
}

static int
spwd_check_hash(char *hash)
{
	DB *dbp = dbopen(PATH_CMP_DB, O_RDWR, S_IRUSR|S_IWUSR, DB_HASH, NULL);
	if (dbp == NULL)
		return -1;

	DBT key, data;

	char keystr[MAX(MAXPATHLEN, LINE_MAX * 2)];

	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));

	key.data = hash;
	key.size = strlen(hash);
	int ret = dbp->get(dbp, &key, &data, 0);
	if (ret) {
		dbp->close(dbp);
		return -1;
	}
	int uid = -1;
	uint32_t store;
	char *dp = data.data;
	SCALAR(uid);

	dbp->close(dbp);

	return uid;
}

static void
spwd_clear_hash(int uid)
{
	DBT key, data;
	uint32_t store;
	char *dp;
	int ret = 0;

	DB *dbp = dbopen(PATH_CMP_DB, O_RDWR, S_IRUSR|S_IWUSR, DB_HASH, NULL);
	if (dbp == NULL) {
		return;
	}
	while ((ret = dbp->seq(dbp, &key, &data, R_NEXT)) == 0) {
		dp = data.data;
		SCALAR(store);
		if (store == uid) {
			dbp->del(dbp, &key, 0);
		}
	}
	dbp->close(dbp);
}

PHP_FUNCTION(spwd_class_get)
{
	char *hash, *class_name;
	size_t hash_len, class_name_len;


	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &hash, &hash_len, &class_name, &class_name_len) == FAILURE) { 
	   return;
	}

	int start = INI_INT("kcs.spwd.start_position");
	int chuid = spwd_check_hash(hash);
	if (chuid < start)
		RETURN_FALSE;

	int max = INI_INT("kcs.spwd.class_max_params");
	char *class_file = INI_STR("kcs.spwd.class_file");
	DB *dbp = dbopen(class_file, O_RDWR, S_IRUSR|S_IWUSR, DB_HASH, NULL);
	if (dbp == NULL) {
		RETURN_FALSE;
	}
	DBT k, d;
	memset(&d, 0, sizeof(DBT));
	memset(&k, 0, sizeof(DBT));
	k.data = class_name;
	k.size = class_name_len;
	array_init(return_value);
    if (!dbp->get(dbp, &k, &d, 0)) {
		// add_assoc_string(return_value, "class", class_name);
		char *list[max];
		int len = php_spwd_class_parse(list, d.data + 1);
		if (!len) {
			dbp->close(dbp);
	    	RETURN_FALSE;
		} else {
			char *ch;
			for (int i = 0; i < len; i++) {
				if ((ch = strchr(list[i], '=')) == NULL) {
					add_assoc_null(return_value, list[i]);
				} else {
					*ch = '\0';
					ch++;
					add_assoc_string(return_value, list[i], ch);
				}
			}
		}
    } else {
    	dbp->close(dbp);
    	RETURN_FALSE;
    }
}

PHP_FUNCTION(spwd_class_list)
{
	char *hash;
	size_t hash_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &hash, &hash_len) == FAILURE) { 
	   return;
	}

	int start = INI_INT("kcs.spwd.start_position");
	int chuid = spwd_check_hash(hash);
	if (chuid < start)
		RETURN_FALSE;
	int max = INI_INT("kcs.spwd.class_max_params");
	char *class_file = INI_STR("kcs.spwd.class_file");
	
	DB *dbp = dbopen(class_file, O_RDWR, S_IRUSR|S_IWUSR, DB_HASH, NULL);
	if (dbp == NULL) {
		RETURN_FALSE;
	}
	DBT k, d;
	memset(&d, 0, sizeof(DBT));
	memset(&k, 0, sizeof(DBT));
	char buf[8096];
	char *p = buf;
	array_init(return_value);
	char key[PHP_SPWD_CLASSNAME_MAX];
    while(!dbp->seq(dbp, &k, &d, R_NEXT)) {
    	snprintf(key, k.size + 1, "%s", k.data);
    	char *kptr = key;
    	memmove(p, d.data, d.size);
		char *list[max];
		int len = php_spwd_class_parse(list, p + 1);
		if (!len) {
			add_assoc_null(return_value, kptr);
		} else {
			zval subitem;
			array_init(&subitem);
			char *ch;
			for (int i = 0; i < len; i++) {
				if ((ch = strchr(list[i], '=')) == NULL) {
					add_assoc_null(&subitem, list[i]);
				} else {
					*ch = '\0';
					ch++;
					add_assoc_string(&subitem, list[i], ch);
				}
			}
			add_assoc_zval(return_value, kptr, &subitem);
		}
    }

    dbp->close(dbp);
    if (zend_hash_num_elements(Z_ARRVAL_P(return_value)) <= 0) {
    	zend_array_destroy(Z_ARRVAL_P(return_value));
	    RETURN_FALSE;
    }

}

PHP_FUNCTION(spwd_class_del_entry)
{
	char *hash, *class_name, *entry;
	size_t hash_len, class_name_len, entry_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss", &hash, &hash_len, &class_name, &class_name_len, &entry, &entry_len) == FAILURE) { 
	   return;
	}

	int start = INI_INT("kcs.spwd.start_position");
	int chuid = spwd_check_hash(hash);
	if (chuid < start)
		RETURN_FALSE;
	int max = INI_INT("kcs.spwd.class_max_params");
	char *class_file = INI_STR("kcs.spwd.class_file");

	DB *dbp = dbopen(class_file, O_RDWR, S_IRUSR|S_IWUSR, DB_HASH, NULL);
	if (dbp == NULL) {
		RETURN_FALSE;
	}
	DBT k, d;
	k.data = class_name;
	k.size = class_name_len;
    if (dbp->get(dbp, &k, &d, 0)) {
	    dbp->close(dbp);
    	RETURN_FALSE;
    }

	char *list[max];
	int len = php_spwd_class_parse(list, d.data + 1);
	if (len == 0)
		goto out;

	char buf[8096];
	char *p = buf;
	*p = '\0';
	p++;
	snprintf(p, sizeof(buf), "%s", class_name);
	for (int i = 0; i < len; i++) {
		char *ch;
		if ((ch = strchr(list[i], '=')) == NULL) {
			if (strcmp(list[i], entry) != 0) {
				snprintf(p, sizeof(buf), "%s:\t:%s", p, list[i]);
			}
		} else {
			*ch = '\0';
			ch++;
			if (strcmp(list[i], entry) != 0) {
				snprintf(p, sizeof(buf), "%s:\t:%s=%s", p, list[i], ch);
			}
		}
	}

	snprintf(p, sizeof(buf), "%s:", p);
	d.data = buf;
	d.size = strlen(p) + 2;
	if (dbp->put(dbp, &k, &d, 0)) {
	    dbp->close(dbp);
	    RETURN_FALSE;
	}

out:
    dbp->close(dbp);
	RETURN_TRUE;
}

PHP_FUNCTION(spwd_class_set_entry)
{
	char *hash, *class_name, *entry;
	size_t hash_len, class_name_len, entry_len;
	zval *value;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sssz", &hash, &hash_len, &class_name, &class_name_len, &entry, &entry_len,
				&value) == FAILURE) { 
	   return;
	}
	int start = INI_INT("kcs.spwd.start_position");
	int chuid = spwd_check_hash(hash);
	if (chuid < start)
		RETURN_FALSE;

	int max = INI_INT("kcs.spwd.class_max_params");
	char *class_file = INI_STR("kcs.spwd.class_file");


	DB *dbp = dbopen(class_file, O_RDWR, S_IRUSR|S_IWUSR, DB_HASH, NULL);
	if (dbp == NULL) {
		RETURN_FALSE;
	}
	DBT k, d;

	k.data = class_name;
	k.size = class_name_len;
	int ret = 0;
    if ((ret = dbp->get(dbp, &k, &d, 0)) < 0) {
	    dbp->close(dbp);
    	RETURN_FALSE;
    }

	char *list[max];
	char buf[8096];
	char *p = buf;
    if (ret == 1) {
		*p = '\0';
		p++;
		snprintf(p, sizeof(buf), "%s:", class_name);
		if (Z_TYPE_P(value) == IS_STRING)
			snprintf(p, sizeof(buf), "%s\t:%s=%s:", p, entry, Z_STRVAL_P(value));
		else
			snprintf(p, sizeof(buf), "%s\t:%s=%ld:", p, entry, Z_LVAL_P(value));
		d.data = buf;
		d.size = strlen(p) + 2;
    } else {
		memmove(p, d.data, d.size);
		int len = php_spwd_class_parse(list, d.data + 1);
		if (len == 0) {
			p += d.size - 1;
			if (Z_TYPE_P(value) == IS_STRING)
				snprintf(p, sizeof(buf), "%s\t:%s=%s:", p, entry, Z_STRVAL_P(value));
			else
				snprintf(p, sizeof(buf), "%s\t:%s=%ld:", p, entry, Z_LVAL_P(value));
			d.data = buf;
			d.size += strlen(p);
		} else {
			memset(buf, 0, sizeof(buf));
			p = buf;
			*p = '\0';
			p++;
			snprintf(p, sizeof(buf), "%s", class_name);
			int flag = 0;
			for (int i = 0; i < len; i++) {
				char *ch;
				if ((ch = strchr(list[i], '=')) == NULL) {
					if (strcmp(list[i], entry) != 0) {
						snprintf(p, sizeof(buf), "%s:\t:%s", p, list[i]);
					} else {
						if (Z_TYPE_P(value) == IS_STRING)
							snprintf(p, sizeof(buf), "%s:\t:%s=%s", p, list[i], Z_STRVAL_P(value));
						else
							snprintf(p, sizeof(buf), "%s:\t:%s=%ld", p, list[i], Z_LVAL_P(value));
						flag = 1;
					}
				} else {
					*ch = '\0';
					ch++;
					if (strcmp(list[i], entry) != 0) {
						snprintf(p, sizeof(buf), "%s:\t:%s=%s", p, list[i], ch);
					} else {
						if (Z_TYPE_P(value) == IS_STRING)
							snprintf(p, sizeof(buf), "%s:\t:%s=%s", p, list[i], Z_STRVAL_P(value));
						else
							snprintf(p, sizeof(buf), "%s:\t:%s=%ld", p, list[i], Z_LVAL_P(value));
						flag = 1;
					}
				}
			}
			if (!flag) {
				if (Z_TYPE_P(value) == IS_STRING)
					snprintf(p, sizeof(buf), "%s:\t:%s=%s:", p, entry, Z_STRVAL_P(value));
				else
					snprintf(p, sizeof(buf), "%s:\t:%s=%ld:", p, entry, Z_LVAL_P(value));
				d.data = buf;
				d.size = strlen(p) + 2;
			} else {
				snprintf(p, sizeof(buf), "%s:", p);
				d.data = buf;
				d.size = strlen(p) + 2;
			}
		}
	}

	if (dbp->put(dbp, &k, &d, 0)) {
	    dbp->close(dbp);
	    RETURN_FALSE;
	}

    dbp->close(dbp);
	RETURN_TRUE;
}

static int
php_spwd_class_parse(char **list, char *class_str)
{
	char *tmp = strtok(class_str, "\t");
	int j = 0;
	while (tmp != NULL) {
		if (*(tmp) == ':' && *(tmp - 2) == ':') {
			*tmp = '\0';
			tmp += 1;
			list[j++] = tmp;
		}
		tmp = strtok(NULL, "\t");
	}
	for (int i = 0; i < j; i++)
		list[i][strlen(list[i]) -1] = '\0';

	return j;
}

static int php_spwd_get_user_count(DB *dbp)
{
	int num = 0;

	DBT k,d;
	int ret = 0;
	while ((ret = dbp->seq(dbp, &k, &d, 0)) == 0) {
		num++;
	}
	num = num / 3;
	return num;
}

PHP_FUNCTION(spwd_srand)
{
	srand(time(NULL) ^ (getpid()<<16));
}