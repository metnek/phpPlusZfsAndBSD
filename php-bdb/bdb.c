#include <errno.h>
#include <bdb.h>
#include <kenv.h>
#include "php.h"
#include "php_bdb.h"

#define PHP_DBP_RSCR		"BDB entry"

typedef struct php_bdb_rsrc {
	char *dbname;
	char *dir;
	long cur_key;
	DB *dbp;
} php_bdb_rsrc_t;

static int php_bdb_obj_to_str(zval *obj, bdb_data_t *data);
static int php_bdb_str_to_obj(bdb_data_t *data, zval *obj);
static void test(zval *data);


PHP_MINIT_FUNCTION(bdb);
PHP_MSHUTDOWN_FUNCTION(bdb);
PHP_FUNCTION(bdb_open);						// wait
PHP_FUNCTION(bdb_close);					// wait
PHP_FUNCTION(bdb_get);						// wait
PHP_FUNCTION(bdb_get_last);					// wait
PHP_FUNCTION(bdb_set);						// wait
PHP_FUNCTION(bdb_del);						// wait
PHP_FUNCTION(bdb_flush);					// wait
PHP_FUNCTION(bdb_get_next);					// wait
PHP_FUNCTION(bdb_get_prev);					// wait
PHP_FUNCTION(bdb_get_cursor);				// wait
PHP_FUNCTION(test123);						// wait

ZEND_BEGIN_ARG_INFO_EX(arginfo_bdb_open, 0, 0, 1)
	ZEND_ARG_INFO(0, dbname)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_bdb_close, 0, 0, 1)
	ZEND_ARG_INFO(0, dbr)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_bdb_get, 0, 0, 2)
	ZEND_ARG_INFO(0, dbr)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_bdb_set, 0, 0, 3)
	ZEND_ARG_INFO(0, dbr)
	ZEND_ARG_INFO(0, key)
	ZEND_ARG_INFO(0, obj)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_bdb_del, 0, 0, 2)
	ZEND_ARG_INFO(0, dbr)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_bdb_get_next, 0, 0, 1)
	ZEND_ARG_INFO(0, dbr)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_bdb_get_prev, 0, 0, 1)
	ZEND_ARG_INFO(0, dbr)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_bdb_get_cursor, 0, 0, 2)
	ZEND_ARG_INFO(0, dbr)
	ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_bdb_flush, 0, 0, 1)
	ZEND_ARG_INFO(0, dbr)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_bdb_get_last, 0, 0, 1)
	ZEND_ARG_INFO(0, dbr)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_test123, 0, 0, 1)
	ZEND_ARG_INFO(0, zzz)
ZEND_END_ARG_INFO()


const zend_function_entry bdb_functions[] = {
	PHP_FE(bdb_open, arginfo_bdb_open)
	PHP_FE(bdb_close, arginfo_bdb_close)
	PHP_FE(bdb_get, arginfo_bdb_get)
	PHP_FE(bdb_set, arginfo_bdb_set)
	PHP_FE(bdb_del, arginfo_bdb_del)
	PHP_FE(bdb_get_next, arginfo_bdb_get_next)
	PHP_FE(bdb_get_prev, arginfo_bdb_get_prev)
	PHP_FE(bdb_get_cursor, arginfo_bdb_get_cursor)
	PHP_FE(bdb_get_last, arginfo_bdb_get_last)
	PHP_FE(bdb_flush, arginfo_bdb_flush)
	PHP_FE(test123, arginfo_test123)
	{NULL, NULL, NULL}
};

zend_module_entry bdb_module_entry = {
	STANDARD_MODULE_HEADER,
	"bdb",
	bdb_functions,
	PHP_MINIT(bdb),
	PHP_MSHUTDOWN(bdb),
	NULL,
	NULL,
	NULL,
	"0.0.1",
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_BDB
ZEND_GET_MODULE(bdb)
#endif

PHP_INI_BEGIN()
    PHP_INI_ENTRY("kcs...", "none", PHP_INI_ALL, NULL)    
PHP_INI_END()

static int le_bdb;

static void php_bdb_dtor(zend_resource *rsrc)
{
	php_bdb_rsrc_t *bdb_rsrc = (php_bdb_rsrc_t *)rsrc->ptr;
	char *tmp = bdb_rsrc->dbname;
	free(tmp);
	if (bdb_rsrc->dbp != NULL)
		bdb_close(bdb_rsrc->dbp);
	if (bdb_rsrc->dir != NULL)
		free(bdb_rsrc->dir);
	free(bdb_rsrc);
}

PHP_MINIT_FUNCTION(bdb)
{
	le_bdb = zend_register_list_destructors_ex(php_bdb_dtor, NULL, PHP_DBP_RSCR, module_number);
	REGISTER_INI_ENTRIES();

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(bdb)
{
    UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}

PHP_FUNCTION(bdb_open)
{
	char *dbname;
	size_t dbname_len;
	char *dir = "";
	size_t dir_len = sizeof("") - 1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s", &dbname, &dbname_len, &dir, &dir_len) == FAILURE) { 
	   return;
	}
	char *info = malloc(strlen(dbname) + 1);
	snprintf(info, strlen(dbname) + 1, "%s", dbname);
	php_bdb_rsrc_t *bdb_rsrc = malloc(sizeof(php_bdb_rsrc_t));
	bdb_rsrc->dbname = info;
	bdb_rsrc->cur_key = -1;
	bdb_rsrc->dbp = NULL;
	if (dir[0]) {
		bdb_rsrc->dir = malloc(dir_len + 1);
		snprintf(bdb_rsrc->dir, dir_len + 1, "%s", dir);
	} else {
		bdb_rsrc->dir = NULL;
	}

	RETURN_RES(zend_register_resource(bdb_rsrc, le_bdb));
}

PHP_FUNCTION(bdb_close)
{
	zval *dbr;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &dbr) == FAILURE) {
		return;
	}
	php_bdb_rsrc_t *bdb_rsrc = NULL;
	if ((bdb_rsrc = (php_bdb_rsrc_t *)zend_fetch_resource(Z_RES_P(dbr), PHP_DBP_RSCR, le_bdb)) == NULL) {
		RETURN_FALSE;
	}
	zend_list_close(Z_RES_P(dbr));
}

PHP_FUNCTION(bdb_get)
{
	zval *dbr;
	zend_long key;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rl", &dbr, &key) == FAILURE) {
		return;
	}

	char *dbname = NULL;
	php_bdb_rsrc_t *bdb_rsrc = NULL;
	if ((bdb_rsrc = (php_bdb_rsrc_t *)zend_fetch_resource(Z_RES_P(dbr), PHP_DBP_RSCR, le_bdb)) == NULL) {
		RETURN_FALSE;
	}
	dbname = bdb_rsrc->dbname;
	char path_kenv[KENV_MVALLEN + 1];
	char *path;
	if (bdb_rsrc->dir == NULL) {
		if(kenv(KENV_GET, BDB_KENV_PATH, path_kenv, sizeof(path_kenv)) < 0)
			RETURN_FALSE;
		path = path_kenv;
	} else {
		path = bdb_rsrc->dir;
	}
	DB *dbp = bdb_open(dbname, path);
	if (dbp == NULL)
		RETURN_FALSE;

	bdb_data_t *data = bdb_get(dbp, key);
	bdb_close(dbp);
	if (data == NULL) {
		RETURN_FALSE;
	}
	array_init(return_value);
	if (php_bdb_str_to_obj(data, return_value))
		RETURN_FALSE;

	while (data->bdb_dt_schema_len) {
		data->bdb_dt_schema_len = data->bdb_dt_schema_len - strlen(data->bdb_dt_schema) - 1;
		char *ch = strchr(data->bdb_dt_schema, ':');
		*ch = '\0';
		add_assoc_null(return_value, data->bdb_dt_schema);
		while (*data->bdb_dt_schema++)
				;
		while (*data->bdb_dt_schema++)
				;
	}
}

PHP_FUNCTION(bdb_set)
{
	zval *dbr;
	zend_long key;
	zval *obj;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rlz", &dbr, &key, &obj) == FAILURE) {
		return;
	}
	char *dbname = NULL;
	php_bdb_rsrc_t *bdb_rsrc = NULL;
	if ((bdb_rsrc = (php_bdb_rsrc_t *)zend_fetch_resource(Z_RES_P(dbr), PHP_DBP_RSCR, le_bdb)) == NULL) {
		RETURN_FALSE;
	}
	dbname = bdb_rsrc->dbname;
	char path_kenv[KENV_MVALLEN + 1];
	char *path;
	if (bdb_rsrc->dir == NULL) {
		if(kenv(KENV_GET, BDB_KENV_PATH, path_kenv, sizeof(path_kenv)) < 0)
			RETURN_FALSE;
		path = path_kenv;
	} else {
		path = bdb_rsrc->dir;
	}
	DB *dbp = bdb_open(dbname, path);
	if (dbp == NULL)
		RETURN_FALSE;
	bdb_data_t data;
	memset(&data, 0, sizeof(bdb_data_t));
	data.bdb_dt_key = key;
	data.bdb_dt_schema = bdb_get_schema(dbp, &data.bdb_dt_schema_len);
	if (data.bdb_dt_schema == NULL) {
		bdb_close(dbp);
		RETURN_FALSE;
	}
	int ret = 0;
	if ((ret = php_bdb_obj_to_str(obj, &data)) == 0) {
		// if (!data.bdb_dt_schema_len)
			bdb_set(dbp, &data);
		// else
		// 	ret = 1;
		free(data.bdb_dt_p);
	}
	bdb_close(dbp);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(bdb_del)
{
	zval *dbr;
	zend_long key;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rl", &dbr, &key) == FAILURE) {
		return;
	}

	char *dbname = NULL;
	php_bdb_rsrc_t *bdb_rsrc = NULL;
	if ((bdb_rsrc = (php_bdb_rsrc_t *)zend_fetch_resource(Z_RES_P(dbr), PHP_DBP_RSCR, le_bdb)) == NULL) {
		RETURN_FALSE;
	}
	dbname = bdb_rsrc->dbname;
	char path_kenv[KENV_MVALLEN + 1];
	char *path;
	if (bdb_rsrc->dir == NULL) {
		if(kenv(KENV_GET, BDB_KENV_PATH, path_kenv, sizeof(path_kenv)) < 0)
			RETURN_FALSE;
		path = path_kenv;
	} else {
		path = bdb_rsrc->dir;
	}
	DB *dbp = bdb_open(dbname, path);
	if (dbp == NULL)
		RETURN_FALSE;

	int ret = bdb_del(dbp, key);
	bdb_close(dbp);
	if (ret) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}

PHP_FUNCTION(bdb_get_next)
{
	zval *dbr;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &dbr) == FAILURE) {
		return;
	}

	php_bdb_rsrc_t *bdb_rsrc = NULL;
	if ((bdb_rsrc = (php_bdb_rsrc_t *)zend_fetch_resource(Z_RES_P(dbr), PHP_DBP_RSCR, le_bdb)) == NULL) {
		RETURN_FALSE;
	}
	if (bdb_rsrc->dbp == NULL) {
		char *dbname = NULL;
		dbname = bdb_rsrc->dbname;
		char path_kenv[KENV_MVALLEN + 1];
		char *path;
		if (bdb_rsrc->dir == NULL) {
			if(kenv(KENV_GET, BDB_KENV_PATH, path_kenv, sizeof(path_kenv)) < 0)
				RETURN_FALSE;
			path = path_kenv;
		} else {
			path = bdb_rsrc->dir;
		}
		bdb_rsrc->dbp = bdb_open(dbname, path);
		if (bdb_rsrc->dbp == NULL)
			RETURN_FALSE;
	}
	bdb_data_t *data = bdb_seq(bdb_rsrc->dbp, BDB_POS_NEXT, -1);
	if (data == NULL) {
		bdb_close(bdb_rsrc->dbp);
		bdb_rsrc->dbp = NULL;
		RETURN_FALSE;
	}
	array_init(return_value);
	zval subitem;
	array_init(&subitem);
	if (php_bdb_str_to_obj(data, &subitem))
		RETURN_FALSE;
	add_assoc_zval(return_value, "data", &subitem);
	add_assoc_long(return_value, "key", data->bdb_dt_key);
	free(data);
}

PHP_FUNCTION(bdb_get_prev)
{
	zval *dbr;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &dbr) == FAILURE) {
		return;
	}

	php_bdb_rsrc_t *bdb_rsrc = NULL;
	if ((bdb_rsrc = (php_bdb_rsrc_t *)zend_fetch_resource(Z_RES_P(dbr), PHP_DBP_RSCR, le_bdb)) == NULL) {
		RETURN_FALSE;
	}
	if (bdb_rsrc->dbp == NULL) {
		char *dbname = NULL;
		dbname = bdb_rsrc->dbname;
		char path_kenv[KENV_MVALLEN + 1];
		char *path;
		if (bdb_rsrc->dir == NULL) {
			if(kenv(KENV_GET, BDB_KENV_PATH, path_kenv, sizeof(path_kenv)) < 0)
				RETURN_FALSE;
			path = path_kenv;
		} else {
			path = bdb_rsrc->dir;
		}
		bdb_rsrc->dbp = bdb_open(dbname, path);
		if (bdb_rsrc->dbp == NULL)
			RETURN_FALSE;
	}
	bdb_data_t *data = bdb_seq(bdb_rsrc->dbp, BDB_POS_PREV, -1);
	if (data == NULL) {
		bdb_close(bdb_rsrc->dbp);
		bdb_rsrc->dbp = NULL;
		RETURN_FALSE;
	}
	array_init(return_value);
	zval subitem;
	array_init(&subitem);
	if (php_bdb_str_to_obj(data, &subitem))
		RETURN_FALSE;
	add_assoc_zval(return_value, "data", &subitem);
	add_assoc_long(return_value, "key", data->bdb_dt_key);
	free(data);
}

PHP_FUNCTION(bdb_flush)
{
	zval *dbr;
	zend_long key;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &dbr) == FAILURE) {
		return;
	}

	char *dbname = NULL;
	php_bdb_rsrc_t *bdb_rsrc = NULL;
	if ((bdb_rsrc = (php_bdb_rsrc_t *)zend_fetch_resource(Z_RES_P(dbr), PHP_DBP_RSCR, le_bdb)) == NULL) {
		RETURN_FALSE;
	}
	dbname = bdb_rsrc->dbname;

	char path_kenv[KENV_MVALLEN + 1];
	char *path;
	if (bdb_rsrc->dir == NULL) {
		if(kenv(KENV_GET, BDB_KENV_PATH, path_kenv, sizeof(path_kenv)) < 0)
			RETURN_FALSE;
		path = path_kenv;
	} else {
		path = bdb_rsrc->dir;
	}
	DB *dbp = bdb_open(dbname, path);
	if (dbp == NULL)
		RETURN_FALSE;

	bdb_trunc(dbp);
	bdb_close(dbp);
	RETURN_TRUE;
}

PHP_FUNCTION(bdb_get_last)
{
	zval *dbr;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &dbr) == FAILURE) {
		return;
	}

	char *dbname = NULL;
	php_bdb_rsrc_t *bdb_rsrc = NULL;
	if ((bdb_rsrc = (php_bdb_rsrc_t *)zend_fetch_resource(Z_RES_P(dbr), PHP_DBP_RSCR, le_bdb)) == NULL) {
		RETURN_FALSE;
	}
	dbname = bdb_rsrc->dbname;
	char path_kenv[KENV_MVALLEN + 1];
	char *path;
	if (bdb_rsrc->dir == NULL) {
		if(kenv(KENV_GET, BDB_KENV_PATH, path_kenv, sizeof(path_kenv)) < 0)
			RETURN_FALSE;
		path = path_kenv;
	} else {
		path = bdb_rsrc->dir;
	}
	DB *dbp = bdb_open(dbname, path);
	if (dbp == NULL) {
		RETURN_FALSE;
	}

	bdb_data_t *data = bdb_seq(dbp, BDB_POS_LAST, -1);
	if (data == NULL) {
		bdb_close(dbp);
		RETURN_FALSE;
	}
	array_init(return_value);
	zval subitem;
	array_init(&subitem);
	if (php_bdb_str_to_obj(data, &subitem)) {
		bdb_close(dbp);
		RETURN_FALSE;
	}
	add_assoc_zval(return_value, "data", &subitem);
	add_assoc_long(return_value, "key", data->bdb_dt_key);
	free(data);
	bdb_close(dbp);
}

PHP_FUNCTION(bdb_get_cursor)
{
	zval *dbr;
	zend_long key;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rl", &dbr, &key) == FAILURE) {
		return;
	}

	php_bdb_rsrc_t *bdb_rsrc = NULL;
	if ((bdb_rsrc = (php_bdb_rsrc_t *)zend_fetch_resource(Z_RES_P(dbr), PHP_DBP_RSCR, le_bdb)) == NULL) {
		RETURN_FALSE;
	}
	if (bdb_rsrc->dbp == NULL) {
		char *dbname = NULL;
		dbname = bdb_rsrc->dbname;
		char path_kenv[KENV_MVALLEN + 1];
		char *path;
		if (bdb_rsrc->dir == NULL) {
			if(kenv(KENV_GET, BDB_KENV_PATH, path_kenv, sizeof(path_kenv)) < 0)
				RETURN_FALSE;
			path = path_kenv;
		} else {
			path = bdb_rsrc->dir;
		}
		bdb_rsrc->dbp = bdb_open(dbname, path);
		if (bdb_rsrc->dbp == NULL)
			RETURN_FALSE;
	}
	bdb_data_t *data = bdb_seq(bdb_rsrc->dbp, BDB_POS_CURSOR, key);
	if (data == NULL) {
		bdb_close(bdb_rsrc->dbp);
		bdb_rsrc->dbp = NULL;
		RETURN_FALSE;
	}
	array_init(return_value);
	zval subitem;
	array_init(&subitem);
	if (php_bdb_str_to_obj(data, &subitem))
		RETURN_FALSE;
	add_assoc_zval(return_value, "data", &subitem);
	add_assoc_long(return_value, "key", data->bdb_dt_key);
	free(data);

}

static int
php_bdb_str_to_obj(bdb_data_t *data, zval *obj)
{
	int ret = 0;
	bdb_value_t value;
	memset(&value, 0, sizeof(bdb_value_t));

	while ((ret = bdb_parse_next(data, &value)) == 0) {
		if(BDB_COL_TYPE(value) == BDB_COL_STRING) {
			add_assoc_string(obj, value.bdb_vl_colname, value.bdb_vl_s);
		} else if (BDB_COL_TYPE(value) == BDB_COL_LONG) {
			add_assoc_long(obj, value.bdb_vl_colname, value.bdb_vl_l);
		} else if (BDB_COL_TYPE(value) == BDB_COL_DOUBLE) {
			add_assoc_double(obj, value.bdb_vl_colname, value.bdb_vl_d);
		} else {
			zval subitem;
			array_init(&subitem);
			char del[] = BDB_ARRAY_DELIM;
			 if (BDB_COL_TYPE(value) == BDB_COL_ASTRING) {
			 	char *tmp;
			 	char *cur = value.bdb_vl_s;
			 	int is_last_empty = 0;
			 	while ((tmp = strchr(cur, *del)) != NULL) {
			 		*tmp = '\0';
	                if (tmp == cur) {
	                    add_next_index_null(&subitem);
	                } else {
						add_next_index_string(&subitem, cur);
	                }
	                cur = tmp + 1;
	                if (!(*cur)) {
	                    add_next_index_null(&subitem);
	                    is_last_empty = 1;
	                }
			 	}
			 	if (!is_last_empty) {
			 		if (strlen(cur) != 0) {
				 		add_next_index_string(&subitem, cur);
			 		}
			 	}

			} else if (BDB_COL_TYPE(value) == BDB_COL_ADOUBLE) {
				char *tmp = strtok(value.bdb_vl_s, BDB_ARRAY_DELIM);
				while (tmp != NULL) {
					errno = 0;
					double dvl = strtod(tmp, NULL);
					if (errno) {
						return -1;
					}
					add_next_index_double(&subitem, dvl);
					tmp = strtok(NULL, BDB_ARRAY_DELIM);
				}
			} else if (BDB_COL_TYPE(value) == BDB_COL_ALONG) {
				char *tmp = strtok(value.bdb_vl_s, BDB_ARRAY_DELIM);
				while (tmp != NULL) {
					errno = 0;
					long lvl = strtol(tmp, NULL, 10);
					if (errno) {
						return -1;
					}
					add_next_index_long(&subitem, lvl);
					tmp = strtok(NULL, BDB_ARRAY_DELIM);
				}
			}
			if (zend_hash_num_elements(Z_ARRVAL_P(&subitem)) > 0) {
				add_assoc_zval(obj, value.bdb_vl_colname, &subitem);
			} else {
				add_assoc_null(obj, value.bdb_vl_colname);
			}
		}
		if (ret) {
			return -1;
		}
	}
	return 0;
}


static int
php_bdb_obj_to_str(zval *obj, bdb_data_t *data)
{
	HashTable *hash;
	zval *item;
	HashPosition pointer;

	hash = Z_ARRVAL_P(obj);
	// for(
 //        zend_hash_internal_pointer_reset_ex(hash, &pointer);
 //        (item = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
 //        zend_hash_move_forward_ex(hash, &pointer)
 //    ) {
 	bdb_column_t *col = NULL;
 	while ((col = bdb_get_next_col(data)) != NULL) {
		if ((item = zend_hash_str_find(hash, col->col_name, strlen(col->col_name))) == NULL) {
			free(col);
			return -1;
		}
		// zend_ulong num_index;
		// zend_string *str_index;
		// zend_hash_get_current_key_ex(hash, &str_index, &num_index, &pointer);
		// if (strlen(ZSTR_VAL(str_index)) <= 0)
		// 	return -1;
		bdb_value_t value;
		value.bdb_vl_colname = col->col_name;
		char *buf = NULL;
    	if (Z_TYPE_P(item) == IS_STRING) {
    		if (*col->type != BDB_COL_STRING) {
    			free(col);
    			return -1;
    		}
    		value.bdb_vl_type = BDB_SCOL_STRING;
    		value.bdb_vl_s = Z_STRVAL_P(item);
		} else if (Z_TYPE_P(item) == IS_LONG) {
    		if (*col->type != BDB_COL_LONG) {
    			free(col);
    			return -1;
    		}
    		value.bdb_vl_type = BDB_SCOL_LONG;
    		value.bdb_vl_l = Z_LVAL_P(item);
		} else if (Z_TYPE_P(item) == IS_DOUBLE) {
    		if (*col->type != BDB_COL_DOUBLE) {
    			free(col);
    			return -1;
    		}
    		value.bdb_vl_type = BDB_SCOL_DOUBLE;
    		value.bdb_vl_d = Z_DVAL_P(item);
		} else if (Z_TYPE_P(item) == IS_ARRAY) {
			zval *inner;
			if ((inner = zend_hash_str_find(Z_ARRVAL_P(item), "type", sizeof("type")-1)) == NULL || Z_TYPE_P(inner) != IS_STRING) {
				free(col);
				return -1;
			}
			HashTable *col_hash;
			HashPosition col_ptr;
			char *type = NULL;
			size_t len = 0;
			type = Z_STRVAL_P(inner);
			col_hash = Z_ARRVAL_P(item);
			for(
		        zend_hash_internal_pointer_reset_ex(col_hash, &col_ptr);
		        (inner = zend_hash_get_current_data_ex(col_hash, &col_ptr)) != NULL;
		        zend_hash_move_forward_ex(col_hash, &col_ptr)
		    ) {
				zend_long col_nindex = -1;
				zend_string *col_sindex = NULL;

				if (zend_hash_get_current_key_ex(col_hash, &col_sindex, (zend_ulong *)&col_nindex, &col_ptr) == HASH_KEY_IS_STRING)
					continue;

				if (strcmp(type, "D") == 0) {
		    		if (*col->type != BDB_COL_ADOUBLE) {
		    			free(col);
						if (buf != NULL)
							free(buf);
		    			return -1;
		    		}
		    		if (Z_TYPE_P(inner) != IS_LONG || Z_TYPE_P(inner) != IS_DOUBLE) {
		    			continue;
		    		}
			    	zval dval;
			    	ZVAL_DOUBLE(&dval, Z_TYPE_P(inner) == IS_LONG ? (double)Z_LVAL_P(inner) : Z_DVAL_P(inner));
			    	buf = bdb_mk_double_array(Z_DVAL_P(&dval), buf, &len);
				} else if (strcmp(type, "L") == 0) {
		    		if (*col->type != BDB_COL_ALONG) {
		    			free(col);
						if (buf != NULL)
							free(buf);
		    			return -1;
		    		}
		    		if (Z_TYPE_P(inner) != IS_LONG) {
		    			continue;
		    		}
			    	buf = bdb_mk_long_array(Z_LVAL_P(inner), buf, &len);
				} else if (strcmp(type, "S") == 0) {
		    		if (*col->type != BDB_COL_ASTRING) {
		    			free(col);
						if (buf != NULL)
							free(buf);
		    			return -1;
		    		}
		    		if (Z_TYPE_P(inner) != IS_STRING) {
		    			continue;
		    		}
			    	buf = bdb_mk_string_array(Z_STRVAL_P(inner), buf, &len);
				} else {
					free(col);
					if (buf != NULL)
						free(buf);
					return -1;
				}
		    	if (buf == NULL) {
					free(col);
		    		return -1;
		    	}
			}
    		value.bdb_vl_type = type;
    		if (len)
	    		value.bdb_vl_s = buf;
	    	else 
	    		value.bdb_vl_s = "";
		} else {
			free(col);
			return -1;
		}
		int ret = bdb_add_next(data, &value);
		if (buf != NULL) {
			free(buf);
		}
		if (ret) {
			free(col);
			return -1;
		}
		free(col);
	}
	return 0;
}
PHP_FUNCTION(test123)
{
	zval *zzz;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "a", &zzz) == FAILURE) {
		return;
	}
	test(zzz);
}
static void
test(zval *data)
{
	zval *inner;
	HashTable *col_hash;
	HashPosition col_ptr;
	col_hash = Z_ARRVAL_P(data);
	char *type = NULL;
	char *buf = NULL;
	size_t len = 0;
	for(
        zend_hash_internal_pointer_reset_ex(col_hash, &col_ptr);
        (inner = zend_hash_get_current_data_ex(col_hash, &col_ptr)) != NULL;
        zend_hash_move_forward_ex(col_hash, &col_ptr)
    ) {
		zend_ulong col_nindex;
		zend_string *col_sindex;
		zend_hash_get_current_key_ex(col_hash, &col_sindex, &col_nindex, &col_ptr);
    	zval dval;
    	ZVAL_DOUBLE(&dval, Z_TYPE_P(inner) == IS_LONG ? (double)Z_LVAL_P(inner) : Z_DVAL_P(inner));
    	buf = bdb_mk_double_array(Z_DVAL_P(&dval), buf, &len);

	}
}