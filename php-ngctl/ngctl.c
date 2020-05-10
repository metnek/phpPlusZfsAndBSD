#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <fcntl.h>
#include <kenv.h>
#include <ngraph.h>

#define ZDEBUG			1

#define BDB_ENABLE		1
#ifdef BDB_ENABLE
#include <bdb.h>
#endif

#include "php.h"
#include "php_ngctl.h"

static void php_free_node_info(ngraph_hooks_info_t *);



PHP_MINIT_FUNCTION(ngctl);
PHP_MSHUTDOWN_FUNCTION(ngctl);
PHP_FUNCTION(ngctl_make_peer);					// wait
PHP_FUNCTION(ngctl_make_peer_rc);				// wait
PHP_FUNCTION(ngctl_connect);					// wait
PHP_FUNCTION(ngctl_node_name);					// wait
PHP_FUNCTION(ngctl_node_shutdown);				// wait
PHP_FUNCTION(ngctl_node_msg);					// wait
PHP_FUNCTION(ngctl_hook_del);					// wait
PHP_FUNCTION(ngctl_list);						// wait
PHP_FUNCTION(ngctl_node_info);					// wait
PHP_FUNCTION(ngctl_save);						// wait
PHP_FUNCTION(ngctl_rollback);					// wait
PHP_FUNCTION(test1234);

ZEND_BEGIN_ARG_INFO(arginfo_test1234, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_ngctl_list, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_ngctl_rollback, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ngctl_make_peer, 0, 0, 4)
	ZEND_ARG_INFO(0, peertype)
	ZEND_ARG_INFO(0, peerhook)
	ZEND_ARG_INFO(0, path)
	ZEND_ARG_INFO(0, hook)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ngctl_make_peer_rc, 0, 0, 4)
	ZEND_ARG_INFO(0, peertype)
	ZEND_ARG_INFO(0, peerhook)
	ZEND_ARG_INFO(0, path)
	ZEND_ARG_INFO(0, hook)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ngctl_connect, 0, 0, 4)
	ZEND_ARG_INFO(0, peerpath)
	ZEND_ARG_INFO(0, peerhook)
	ZEND_ARG_INFO(0, path)
	ZEND_ARG_INFO(0, hook)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ngctl_node_name, 0, 0, 2)
	ZEND_ARG_INFO(0, path)
	ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ngctl_node_shutdown, 0, 0, 1)
	ZEND_ARG_INFO(0, path)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ngctl_node_info, 0, 0, 1)
	ZEND_ARG_INFO(0, path)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ngctl_node_msg, 0, 0, 3)
	ZEND_ARG_INFO(0, path)
	ZEND_ARG_INFO(0, msg)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ngctl_hook_del, 0, 0, 2)
	ZEND_ARG_INFO(0, path)
	ZEND_ARG_INFO(0, hook)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_ngctl_save, 0, 0, 1)
	ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

const zend_function_entry ngctl_functions[] = {
	PHP_FE(ngctl_make_peer, arginfo_ngctl_make_peer)
	PHP_FE(ngctl_make_peer_rc, arginfo_ngctl_make_peer_rc)
	PHP_FE(ngctl_connect, arginfo_ngctl_connect)
	PHP_FE(ngctl_node_name, arginfo_ngctl_node_name)
	PHP_FE(ngctl_node_shutdown, arginfo_ngctl_node_shutdown)
	PHP_FE(ngctl_node_msg, arginfo_ngctl_node_msg)
	PHP_FE(ngctl_hook_del, arginfo_ngctl_hook_del)
	PHP_FE(ngctl_list, NULL)
	PHP_FE(ngctl_node_info, arginfo_ngctl_node_info)
	PHP_FE(ngctl_save, arginfo_ngctl_save)
	PHP_FE(ngctl_rollback, NULL)
	PHP_FE(test1234, NULL)
	{NULL, NULL, NULL}
};

zend_module_entry ngctl_module_entry = {
	STANDARD_MODULE_HEADER,
	"ngctl",
	ngctl_functions,
	PHP_MINIT(ngctl),
	PHP_MSHUTDOWN(ngctl),
	NULL,
	NULL,
	NULL,
	"0.0.1",
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_NGCTL
ZEND_GET_MODULE(ngctl)

#endif

PHP_INI_BEGIN()
    PHP_INI_ENTRY("kcs....", "none", PHP_INI_ALL, NULL)    
PHP_INI_END()

PHP_MINIT_FUNCTION(ngctl)
{
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(ngctl)
{
	return SUCCESS;
}

PHP_FUNCTION(test1234)
{
	char *dbname = "lol";
	char *dir = "/data/db";
	DB *dbp = bdb_open(dbname, dir);
	if (dbp != NULL) {
		bdb_trunc(dbp);
		bdb_close(dbp);
	} 
}

PHP_FUNCTION(ngctl_make_peer)
{
	char *peertype, *hook, *peerhook;
	zval *path;
	size_t peertype_len, path_len, hook_len, peerhook_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sszs", &peertype, &peertype_len, &peerhook, &peerhook_len, &path, &hook, &hook_len) == FAILURE) { 
	   return;
	}

	if (Z_TYPE_P(path) != IS_STRING && Z_TYPE_P(path) != IS_NULL)
		RETURN_FALSE;
	int ret = ngraph_mkpeer_node(Z_TYPE_P(path) == IS_NULL ? NULL : Z_STRVAL_P(path), peertype, hook, peerhook);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(ngctl_make_peer_rc)
{
	char *peertype, *hook, *peerhook;
	zval *path;
	size_t peertype_len, path_len, hook_len, peerhook_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sszs", &peertype, &peertype_len, &peerhook, &peerhook_len, &path, &hook, &hook_len) == FAILURE) { 
	   return;
	}

	if (Z_TYPE_P(path) != IS_STRING && Z_TYPE_P(path) != IS_NULL)
		RETURN_FALSE;
	char buf[NG_PATHSIZ];
	int ret = ngraph_mkpeer_node_rc(Z_TYPE_P(path) == IS_NULL ? NULL : Z_STRVAL_P(path), peertype, hook, peerhook, buf);
	if (ret)
		RETURN_FALSE;
	RETURN_STRING(buf);
}

PHP_FUNCTION(ngctl_connect)
{
	char *peertype, *hook, *peerhook, *path;
	size_t peertype_len, path_len, hook_len, peerhook_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ssss", &peertype, &peertype_len, &peerhook, &peerhook_len, &path, &path_len, &hook, &hook_len) == FAILURE) { 
	   return;
	}
	int ret = ngraph_connect_node(path, peertype, hook, peerhook);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(ngctl_node_name)
{
	char *path, *name;
	size_t name_len, path_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &path, &path_len, &name, &name_len) == FAILURE) { 
	   return;
	}
	int ret = ngraph_name_node(path, name);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(ngctl_node_shutdown)
{
	char *path;
	size_t path_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &path, &path_len) == FAILURE) { 
	   return;
	}
	int ret = ngraph_shutdown_node(path, NGRAPH_SHUTDOWN_BY_NAME);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(ngctl_node_msg)
{
	char *path, *msg, *params;
	size_t path_len, msg_len, params_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss", &path, &path_len, &msg, &msg_len, &params, &params_len) == FAILURE) { 
	   return;
	}
	int ret = ngraph_msg_node(path, msg, params);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(ngctl_hook_del)
{
	char *path, *hook;
	size_t path_len, hook_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &path, &path_len, &hook, &hook_len) == FAILURE) { 
	   return;
	}
	int ret = ngraph_rmhook(path, hook);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(ngctl_list)
{
	ngraph_info_t **info = NULL;
	int info_len = 0;
	if ((info = ngraph_list(&info_len)) == NULL)
		RETURN_FALSE;
	if (info_len == 0) {
		free(info);
		RETURN_FALSE;
	}
	array_init(return_value);
	for (int c = 0; c < info_len; c++) {
		if (strcmp(info[c]->type, NG_VLAN_NODE_TYPE) != 0 &&
			strcmp(info[c]->type, "eiface") != 0 &&
			// strcmp(info[c]->type, "bridge") != 0 &&
			strcmp(info[c]->type, "ether") != 0)
			continue;
		zval item;
		array_init(&item);
		zval subitem;
		array_init(&item);
		array_init(&subitem);
		struct ng_vlan_table *filter = NULL;
		add_assoc_string(&item, "name", info[c]->name);
		add_assoc_string(&item, "type", info[c]->type);
		if (strcmp(info[c]->type, "vlan") == 0) {
			char tmp[strlen(info[c]->name) + 2];
			snprintf(tmp, sizeof(tmp), "%s:", info[c]->name);
			filter = ngraph_msg_vlan(tmp, "gettable");
		}
		for (int k = 0; k < info[c]->hooks_count; k++) {
			zval subsub;
			array_init(&subsub);
			add_assoc_string(&subsub, "ourhook", info[c]->hooks[k]->ourhook);
			add_assoc_string(&subsub, "peertype", info[c]->hooks[k]->peertype);
			add_assoc_string(&subsub, "peerhook", info[c]->hooks[k]->peerhook);
			add_assoc_string(&subsub, "peername", info[c]->hooks[k]->peername);
			if (strcmp(info[c]->type, NG_VLAN_NODE_TYPE) == 0 && 
				strcmp(info[c]->hooks[k]->ourhook, NG_VLAN_HOOK_DOWNSTREAM) != 0 &&
				strcmp(info[c]->hooks[k]->ourhook, NG_VLAN_HOOK_NOMATCH) != 0) {
				if (filter == NULL) {
					add_assoc_null(&subsub, "filter");
				} else {
					int flag = 0;
					zval filters;
					array_init(&filters);
					if (strcmp(info[c]->hooks[k]->ourhook, NGRAPH_TRUNK_NF) == 0) {
			            for (int j = 0; j < EVL_VLID_MASK + 1; j++)
			                if (filter->trunk[j])
			                    add_next_index_long(&filters, j);
					} else {
						for (int i = 0; i < filter->n; i++) {
							if (strcmp(info[c]->hooks[k]->ourhook, filter->filter[i].hook_name) == 0) {
								flag = 1;
					            add_next_index_long(&filters, filter->filter[i].vid);
						        break;
							}
						}
					}
					if (zend_hash_num_elements(Z_ARRVAL_P(&filters)) > 0){
						add_assoc_zval(&subsub, "filter", &filters);
					} else {
						zend_array_destroy(Z_ARRVAL_P(&filters));
						add_assoc_null(&subsub, "filter");
					}
				}
			}
			add_next_index_zval(&subitem, &subsub);
		}
		if (filter != NULL)
			free(filter);
		if (zend_hash_num_elements(Z_ARRVAL_P(&subitem)) > 0)
			add_assoc_zval(&item, "hooks", &subitem);
		else
			add_assoc_null(&item, "hooks");
		add_next_index_zval(return_value, &item);
		ngraph_info_node_free(info[c]);
	}
	free(info);
}

PHP_FUNCTION(ngctl_node_info)
{
	char *path;
	size_t path_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &path, &path_len) == FAILURE) { 
	   return;
	}
	ngraph_info_t *info = ngraph_info_node(path);
	if (info == NULL) {
		RETURN_FALSE;
	}
	array_init(return_value);
	zval subitem;
	array_init(&subitem);
	add_assoc_string(return_value, "name", info->name);
	add_assoc_string(return_value, "type", info->type);
	struct ng_vlan_table *filter = NULL;
	if (strcmp(info->type, NG_VLAN_NODE_TYPE) == 0) {
		char tmp[strlen(info->name) + 2];
		snprintf(tmp, sizeof(tmp), "%s:", info->name);
		filter = ngraph_msg_vlan(tmp, "gettable");
	}
	for (int k = 0; k < info->hooks_count; k++) {
		zval subsub;
		array_init(&subsub);
		add_assoc_string(&subsub, "ourhook", info->hooks[k]->ourhook);
		add_assoc_string(&subsub, "peertype", info->hooks[k]->peertype);
		add_assoc_string(&subsub, "peerhook", info->hooks[k]->peerhook);
		add_assoc_string(&subsub, "peername", info->hooks[k]->peername);
		if (strcmp(info->type, NG_VLAN_NODE_TYPE) == 0 && 
			strcmp(info->hooks[k]->ourhook, NG_VLAN_HOOK_DOWNSTREAM) != 0 &&
			strcmp(info->hooks[k]->ourhook, NG_VLAN_HOOK_NOMATCH) != 0) {
			if (filter == NULL) {
				add_assoc_null(&subsub, "filter");
			} else {
				int flag = 0;
				zval filters;
				array_init(&filters);
				if (strcmp(info->hooks[k]->ourhook, NGRAPH_TRUNK_NF) == 0) {
		            for (int j = 0; j < EVL_VLID_MASK + 1; j++)
		                if (filter->trunk[j])
		                    add_next_index_long(&filters, j);
				} else {
					for (int i = 0; i < filter->n; i++) {
						if (strcmp(info->hooks[k]->ourhook, filter->filter[i].hook_name) == 0) {
							flag = 1;
				            add_next_index_long(&filters, filter->filter[i].vid);
					        break;
						}
					}
				}
				if (zend_hash_num_elements(Z_ARRVAL_P(&filters)) > 0){
					add_assoc_zval(&subsub, "filter", &filters);
				} else {
					zend_array_destroy(Z_ARRVAL_P(&filters));
					add_assoc_null(&subsub, "filter");
				}
			}
		}
		add_next_index_zval(&subitem, &subsub);
	}
	if (filter != NULL)
		free(filter);
	if (zend_hash_num_elements(Z_ARRVAL_P(&subitem)) > 0)
		add_assoc_zval(return_value, "hooks", &subitem);
	else
		add_assoc_null(return_value, "hooks");
	ngraph_info_node_free(info);
}

PHP_FUNCTION(ngctl_save)
{
	zval *data;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &data) == FAILURE) { 
	   return;
	}

#ifdef BDB_ENABLE
	char dbname[] = "ngraph";
	char tmp_dbname[] = "ngraph.tmp";

	char path[KENV_MVALLEN + 1];
	if(kenv(KENV_GET, BDB_KENV_PATH, path, sizeof(path)) < 0)
		RETURN_FALSE;
	DB *dbp = bdb_open(tmp_dbname, path);
	if (dbp == NULL) {
#ifdef ZDEBUG
		printf("dbopen failed %s/%s\n", path, tmp_dbname);
#endif
		RETURN_FALSE;
	}
	bdb_trunc(dbp);
	zval *item;
	HashTable *hash = Z_ARRVAL_P(data);
	HashPosition pointer;

	bdb_data_t dt;
	dt.bdb_dt_key = 1;
	int ret = 0;
	ngraph_hooks_info_t nf;
	memset(&nf, 0, sizeof(ngraph_hooks_info_t));
	long key = 0;
	for(
        zend_hash_internal_pointer_reset_ex(hash, &pointer);
        (item = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
        zend_hash_move_forward_ex(hash, &pointer)
    ) {
    	char *type;
    	zval *subitem;
		if ((subitem = zend_hash_str_find(Z_ARRVAL_P(item), "type", sizeof("type")-1)) == NULL || Z_TYPE_P(subitem) != IS_STRING) {
			ret = 1;
#ifdef ZDEBUG
			printf("node type not found\n");
#endif
			goto out;
		}
		if (strcmp(Z_STRVAL_P(subitem), "eiface") == 0) {
			continue;
		}
		type = Z_STRVAL_P(subitem);
		memset(&dt, 0, sizeof(bdb_data_t));
		dt.bdb_dt_schema = bdb_get_schema(dbp, &dt.bdb_dt_schema_len);
		bdb_value_t value;
		value.bdb_vl_colname = "type";
		value.bdb_vl_type = BDB_SCOL_STRING;
		value.bdb_vl_s = type;
		ret = bdb_add_next(&dt, &value);
		if (ret) {
#ifdef ZDEBUG
			printf("bdb add node type failed\n");
#endif
			goto out;
		}
		if ((subitem = zend_hash_str_find(Z_ARRVAL_P(item), "name", sizeof("name")-1)) != NULL && Z_TYPE_P(subitem) == IS_STRING) {
			value.bdb_vl_colname = "name";
			value.bdb_vl_type = BDB_SCOL_STRING;
			value.bdb_vl_s = Z_STRVAL_P(subitem);
			ret = bdb_add_next(&dt, &value);
			if (ret) {
#ifdef ZDEBUG
				printf("bdb add node name failed\n");
#endif
				goto out;
			}
		} else {
			ret = 1;
#ifdef ZDEBUG
			printf("node name not found\n");
#endif
			goto out;
		}
		if ((subitem = zend_hash_str_find(Z_ARRVAL_P(item), "hooks", sizeof("hooks")-1)) == NULL) {
			RETURN_FALSE;
		}
		if (Z_TYPE_P(subitem) == IS_NULL) {
			value.bdb_vl_type = BDB_SCOL_ASTRING;
			value.bdb_vl_s = "0";

#define PHP_NGCTL_ADD_EMPTY(v)	value.bdb_vl_colname = #v; \
			ret = bdb_add_next(&dt, &value); \
			if (ret) {\
				goto out; \
			}

			PHP_NGCTL_ADD_EMPTY(ourhook);
			PHP_NGCTL_ADD_EMPTY(peertype);
			PHP_NGCTL_ADD_EMPTY(peerhook);
			PHP_NGCTL_ADD_EMPTY(peername);
			PHP_NGCTL_ADD_EMPTY(filter);

		} else if (Z_TYPE_P(subitem) == IS_ARRAY) {
			zval *hookinfo;
			HashTable *hook_hash = Z_ARRVAL_P(subitem);
			HashPosition hook_ptr;
	    	memset(&nf, 0, sizeof(ngraph_hooks_info_t));
			for(
		        zend_hash_internal_pointer_reset_ex(hook_hash, &hook_ptr);
		        (hookinfo = zend_hash_get_current_data_ex(hook_hash, &hook_ptr)) != NULL;
		        zend_hash_move_forward_ex(hook_hash, &hook_ptr)
		    ) {
				zval *subsub;
#define PHP_NGCTL_NODE_HOOK_INFO(v)	 if ((subsub = zend_hash_str_find(Z_ARRVAL_P(hookinfo), #v, sizeof(#v)-1)) != NULL && Z_TYPE_P(subsub) == IS_STRING) { \
					nf.v##len += strlen(Z_STRVAL_P(subsub)) + 1; \
					nf.v = realloc(nf.v, nf.v##len); \
					if (nf.v == NULL) { \
						nf.v##len = 0; \
						ret = 1; \
						goto out; \
					} \
					if ((nf.v##len - strlen(Z_STRVAL_P(subsub)) - 1)) \
						snprintf(nf.v, nf.v##len, "%s,%s", nf.v, Z_STRVAL_P(subsub)); \
					else \
						snprintf(nf.v, nf.v##len, "%s", Z_STRVAL_P(subsub)); \
				} else { \
					ret = 1; \
					goto out; \
				}


				PHP_NGCTL_NODE_HOOK_INFO(ourhook);
				PHP_NGCTL_NODE_HOOK_INFO(peertype);
				PHP_NGCTL_NODE_HOOK_INFO(peerhook);
				PHP_NGCTL_NODE_HOOK_INFO(peername);

				if (strcmp(type, NG_VLAN_NODE_TYPE) == 0) {
					if ((subsub = zend_hash_str_find(Z_ARRVAL_P(hookinfo), "filter", sizeof("filter")-1)) != NULL) {
						if (Z_TYPE_P(subsub) != IS_ARRAY) {
							ret = 1;
#ifdef ZDEBUG
							printf("type vlan, filter found but not array\n");
#endif
							goto out;
						}
						zval *filter;
						HashTable *filter_hash = Z_ARRVAL_P(subsub);
						HashPosition filter_ptr;
						char *fl = NULL;
						size_t flen = 0;
						for(
					        zend_hash_internal_pointer_reset_ex(filter_hash, &filter_ptr);
					        (filter = zend_hash_get_current_data_ex(filter_hash, &filter_ptr)) != NULL;
					        zend_hash_move_forward_ex(filter_hash, &filter_ptr)
					    ) {
					    	if (Z_TYPE_P(filter) != IS_LONG) {
					    		if (flen)
					    			free(fl);
#ifdef ZDEBUG
								printf("filter not long\n");
#endif
								ret = 1;
								goto out;
					    	}
					    	int n = snprintf(NULL, 0, "%ld", Z_LVAL_P(filter)) + 1;
					    	flen += n;
					    	fl = realloc(fl, flen);
					    	if (fl == NULL) {
								ret = 1;
#ifdef ZDEBUG
								printf("filter realloc failed\n");
#endif
								goto out;
					    	}
					    	if ((flen - n))
						    	snprintf(fl, flen, "%s.%ld", fl, Z_LVAL_P(filter));
						    else
								snprintf(fl, flen, "%ld", Z_LVAL_P(filter));
						}
						nf.filterlen += flen + 1;
						nf.filter = realloc(nf.filter, nf.filterlen);
						if (nf.filter == NULL) {
							if (flen)
								free(fl);
							ret = 1;
#ifdef ZDEBUG
							printf("filters list realloc failed\n");
#endif
							goto out;
						}
						if ((nf.filterlen - flen - 1))
							snprintf(nf.filter, nf.filterlen, "%s,%s", nf.filter, fl);
						else
							snprintf(nf.filter, nf.filterlen, "%s", fl);
						free(fl);
					} else {
						nf.filterlen += 2;
						nf.filter = realloc(nf.filter, nf.filterlen);
						if (nf.filter == NULL) {
							ret = 1;
#ifdef ZDEBUG
							printf("filters list realloc failed\n");
#endif
							goto out;
						}
						if ((nf.filterlen - 2))
							snprintf(nf.filter, nf.filterlen, "%s,0", nf.filter);
						else
							snprintf(nf.filter, nf.filterlen, "%s", "0");
					}
				}
			}
#define PHP_NGCTL_ADD_NODE_BDB(v) value.bdb_vl_type = BDB_SCOL_ASTRING; \
			value.bdb_vl_colname = #v; \
			if (nf.v##len) { \
				value.bdb_vl_s = nf.v; \
			} else { \
				value.bdb_vl_s = "0"; \
			} \
			ret = bdb_add_next(&dt, &value); \
			if (ret) {\
				goto out;\
			}
		PHP_NGCTL_ADD_NODE_BDB(ourhook);
		PHP_NGCTL_ADD_NODE_BDB(peertype);
		PHP_NGCTL_ADD_NODE_BDB(peerhook);
		PHP_NGCTL_ADD_NODE_BDB(peername);
		PHP_NGCTL_ADD_NODE_BDB(filter);
		}
		dt.bdb_dt_key = key++;
		bdb_set(dbp, &dt);
		free(dt.bdb_dt_p);
	}

out:
	php_free_node_info(&nf);
	bdb_close(dbp);
	if (ret)
		RETURN_FALSE;
	if (bdb_copy(path, tmp_dbname, dbname)) {
#ifdef ZDEBUG
		printf("ngraph db copy failed\n");
#endif
		RETURN_FALSE;
	}
	RETURN_TRUE;
#else
	RETURN_FALSE;
#endif
}

PHP_FUNCTION(ngctl_rollback)
{
	int ret = ngraph_load(NULL, NULL);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

static void
php_free_node_info(ngraph_hooks_info_t *nf)
{
	if (nf->ourhooklen)
		free(nf->ourhook);
	if (nf->peertypelen)
		free(nf->peertype);
	if (nf->peerhooklen)
		free(nf->peerhook);
	if (nf->peernamelen)
		free(nf->peername);
	if (nf->filterlen)
		free(nf->filter);
}