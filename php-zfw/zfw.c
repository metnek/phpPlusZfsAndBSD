#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <zfw.h>
#include "php.h"
#include "php_zfw.h"


PHP_MINIT_FUNCTION(zfw);
PHP_MSHUTDOWN_FUNCTION(zfw);

// rules
PHP_FUNCTION(zfw_rule_add);						// done
PHP_FUNCTION(zfw_rule_delete);					// done
PHP_FUNCTION(zfw_rule_move);					// done
PHP_FUNCTION(zfw_rules_list);					// done
PHP_FUNCTION(zfw_rules_flush);					// done
PHP_FUNCTION(zfw_rules_rollback);				// wait

// tables
PHP_FUNCTION(zfw_table_create);					// done
PHP_FUNCTION(zfw_table_destroy);				// done
PHP_FUNCTION(zfw_table_destroy_all);			// done
PHP_FUNCTION(zfw_table_entry_add);				// done
PHP_FUNCTION(zfw_table_entry_delete);			// done
PHP_FUNCTION(zfw_table_flush);					// done
PHP_FUNCTION(zfw_table_list);					// done
PHP_FUNCTION(zfw_table_get);					// done

// nat
PHP_FUNCTION(zfw_nat_delete);					// done
PHP_FUNCTION(zfw_nat_config);					// done

// sets
PHP_FUNCTION(zfw_set_delete);					// done
PHP_FUNCTION(zfw_set_list);						// done
PHP_FUNCTION(zfw_set_enable);					// done
PHP_FUNCTION(zfw_set_disable);					// done
PHP_FUNCTION(zfw_set_move);						// done

// pipe/queue
PHP_FUNCTION(zfw_pipe_delete);					// done
PHP_FUNCTION(zfw_queue_delete);					// done
PHP_FUNCTION(zfw_pipe_config);					// done
PHP_FUNCTION(zfw_queue_config);					// done


// rules
ZEND_BEGIN_ARG_INFO_EX(arginfo_zfw_rule_add, 0, 0, 1)
	ZEND_ARG_INFO(0, rules)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfw_rule_delete, 0, 0, 3)
	ZEND_ARG_INFO(0, set)
	ZEND_ARG_INFO(0, num)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfw_rule_move, 0, 0, 2)
	ZEND_ARG_INFO(0, set)
	ZEND_ARG_INFO(0, num)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfw_rules_list, 0, 0, 2)
	ZEND_ARG_INFO(0, set)
	ZEND_ARG_INFO(0, show_stats)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfw_rules_flush, 0, 0, 1)
	ZEND_ARG_INFO(0, set)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_zfw_rules_rollback, 0)
ZEND_END_ARG_INFO()


// tables
ZEND_BEGIN_ARG_INFO_EX(arginfo_zfw_table_create, 0, 0, 1)
	ZEND_ARG_INFO(0, tablename)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfw_table_destroy, 0, 0, 1)
	ZEND_ARG_INFO(0, tablename)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_zfw_table_destroy_all, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfw_table_entry_add, 0, 0, 2)
	ZEND_ARG_INFO(0, tablename)
	ZEND_ARG_INFO(0, entry)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfw_table_entry_delete, 0, 0, 2)
	ZEND_ARG_INFO(0, tablename)
	ZEND_ARG_INFO(0, entry)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfw_table_flush, 0, 0, 1)
	ZEND_ARG_INFO(0, tablename)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_zfw_table_list, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfw_table_get, 0, 0, 1)
	ZEND_ARG_INFO(0, tablename)
ZEND_END_ARG_INFO()


// nat
ZEND_BEGIN_ARG_INFO_EX(arginfo_zfw_nat_delete, 0, 0, 1)
	ZEND_ARG_INFO(0, num)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfw_nat_config, 0, 0, 1)
	ZEND_ARG_INFO(0, num)
ZEND_END_ARG_INFO()


// sets
ZEND_BEGIN_ARG_INFO_EX(arginfo_zfw_set_delete, 0, 0, 1)
	ZEND_ARG_INFO(0, num)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_zfw_set_list, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfw_set_enable, 0, 0, 1)
	ZEND_ARG_INFO(0, num)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfw_set_disable, 0, 0, 1)
	ZEND_ARG_INFO(0, num)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfw_set_move, 0, 0, 2)
	ZEND_ARG_INFO(0, from)
	ZEND_ARG_INFO(0, to)
ZEND_END_ARG_INFO()


// pipe/queue
ZEND_BEGIN_ARG_INFO_EX(arginfo_zfw_pipe_delete, 0, 0, 1)
	ZEND_ARG_INFO(0, num)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfw_queue_delete, 0, 0, 1)
	ZEND_ARG_INFO(0, num)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfw_pipe_config, 0, 0, 1)
	ZEND_ARG_INFO(0, num)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfw_queue_config, 0, 0, 1)
	ZEND_ARG_INFO(0, num)
ZEND_END_ARG_INFO()


const zend_function_entry zfw_functions[] = {
	// rules
	PHP_FE(zfw_rule_add, arginfo_zfw_rule_add)
	PHP_FE(zfw_rule_delete, arginfo_zfw_rule_delete)
	PHP_FE(zfw_rule_move, arginfo_zfw_rule_move)
	PHP_FE(zfw_rules_list, arginfo_zfw_rules_list)
	PHP_FE(zfw_rules_flush, arginfo_zfw_rules_flush)
	PHP_FE(zfw_rules_rollback, NULL)

	// tables
	PHP_FE(zfw_table_create, arginfo_zfw_table_create)
	PHP_FE(zfw_table_destroy, arginfo_zfw_table_destroy)
	PHP_FE(zfw_table_destroy_all, NULL)
	PHP_FE(zfw_table_entry_add, arginfo_zfw_table_entry_add)
	PHP_FE(zfw_table_entry_delete, arginfo_zfw_table_entry_delete)
	PHP_FE(zfw_table_flush, arginfo_zfw_table_flush)
	PHP_FE(zfw_table_list, NULL)
	PHP_FE(zfw_table_get, arginfo_zfw_table_get)

	// nat
	PHP_FE(zfw_nat_delete, arginfo_zfw_nat_delete)
	PHP_FE(zfw_nat_config, arginfo_zfw_nat_config)

	// sets
	PHP_FE(zfw_set_delete, arginfo_zfw_set_delete)
	PHP_FE(zfw_set_list, NULL)
	PHP_FE(zfw_set_enable, arginfo_zfw_set_enable)
	PHP_FE(zfw_set_disable, arginfo_zfw_set_disable)
	PHP_FE(zfw_set_move, arginfo_zfw_set_move)

	// pipe/queue
	PHP_FE(zfw_pipe_delete, arginfo_zfw_pipe_delete)
	PHP_FE(zfw_queue_delete, arginfo_zfw_queue_delete)
	PHP_FE(zfw_pipe_config, arginfo_zfw_pipe_config)
	PHP_FE(zfw_queue_config, arginfo_zfw_queue_config)

	{NULL, NULL, NULL}
};


zend_module_entry zfw_module_entry = {
	STANDARD_MODULE_HEADER,
	"zfw",
	zfw_functions,
	PHP_MINIT(zfw),
	PHP_MSHUTDOWN(zfw),
	NULL,
	NULL,
	NULL,
	"0.0.1",
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_ZFW

ZEND_GET_MODULE(zfw)
#endif

PHP_INI_BEGIN()
    PHP_INI_ENTRY("kcs.zfw.conf", "/etc/kinit.conf", PHP_INI_ALL, NULL)
PHP_INI_END()

PHP_MINIT_FUNCTION(zfw)
{
	REGISTER_INI_ENTRIES();

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(zfw)
{
    UNREGISTER_INI_ENTRIES();

    return SUCCESS;
}

// rules
PHP_FUNCTION(zfw_rule_add)
{
	zval *rules;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &rules) == FAILURE) { 
	   return;
	}
	int ret = 0;
	if (Z_TYPE_P(rules) == IS_ARRAY) {
		HashTable *hash = Z_ARRVAL_P(rules);
		zval *data;
		HashPosition pointer;
		for(
	        zend_hash_internal_pointer_reset_ex(hash, &pointer);
	        (data = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
	        zend_hash_move_forward_ex(hash, &pointer)
	    ) {
			ret = zfw_rule_add(Z_STRVAL_P(data));
			if (ret)
				break;
		}
	} else if (Z_TYPE_P(rules) == IS_STRING) {
		ret = zfw_rule_add(Z_STRVAL_P(rules));
	} else {
		RETURN_FALSE;
	}
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfw_rule_delete)
{
	zend_long set, num;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ll", &set, &num) == FAILURE) { 
	   return;
	}

	if (set < 0 || set >= ZFW_SET_MAX_NUM || num <= 0 || num > ZFW_RULE_MAX_NUM)
		RETURN_FALSE;

	int ret = zfw_rule_delete(set, num, num);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfw_rule_move)
{
	zend_long set, num;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ll", &num, &set) == FAILURE) { 
	   return;
	}

	if (set < 0 || set >= ZFW_SET_MAX_NUM || num <= 0 || num > ZFW_RULE_MAX_NUM)
		RETURN_FALSE;

	int ret = zfw_rule_move_to_set(num, set);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfw_rules_list)
{
	zend_long set;
	zend_bool show_stats = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l|b", &set, &show_stats) == FAILURE) { 
	   return;
	}
	zfw_rule_list_t *list = zfw_rule_list(set, show_stats);
	if (list == NULL)
		RETURN_FALSE;
	if (list->count == 0) {
		zfw_rule_list_free(list);
		RETURN_FALSE;
	}
	array_init(return_value);
	for (int i = 0; i < list->count; i++) {
		zval subitem;
		array_init(&subitem);
		add_assoc_long(&subitem, "set", list->rules[i]->set);
		add_assoc_long(&subitem, "rule_num", list->rules[i]->rule_num);
		add_assoc_bool(&subitem, "disabled", list->rules[i]->is_disabled);
		add_assoc_string(&subitem, "rule", list->rules[i]->body);
		add_next_index_zval(return_value, &subitem);
	}

	zfw_rule_list_free(list);
	if (!zend_hash_num_elements(Z_ARRVAL_P(return_value))) {
		zend_array_destroy(Z_ARRVAL_P(return_value));
		RETURN_FALSE;
	}
}

PHP_FUNCTION(zfw_rules_flush)
{
	zend_long set;
	zend_bool is_all = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l|b", &set, &is_all) == FAILURE) { 
	   return;
	}

	if (!is_all && (set < 0 || set >= ZFW_SET_MAX_NUM))
		RETURN_FALSE;

	int ret = zfw_rule_delete(is_all ? -1 : set, 2, ZFW_RULE_MAX_NUM);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfw_rules_rollback)
{
	int ret = zfw_rules_load_db();
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}


// tables
PHP_FUNCTION(zfw_table_create)
{
	char *tablename;
	size_t tablename_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &tablename, &tablename_len) == FAILURE) { 
	   return;
	}

	int ret = zfw_table_create(tablename, NULL, 0);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfw_table_destroy)
{
	char *tablename;
	size_t tablename_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &tablename, &tablename_len) == FAILURE) { 
	   return;
	}

	int ret = zfw_table_destroy(tablename);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfw_table_destroy_all)
{
	int ret = zfw_table_destroy_all();
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfw_table_entry_add)
{
	char *tablename, *entry;
	size_t tablename_len, entry_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &tablename, &tablename_len, &entry, &entry_len) == FAILURE) { 
	   return;
	}
	zend_string *copy = zend_string_alloc(entry_len + 1, 0);
	snprintf(ZSTR_VAL(copy), entry_len + 1, "%s", entry);
	int ret = zfw_table_entry_add(tablename, ZSTR_VAL(copy));
	zend_string_free(copy);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfw_table_entry_delete)
{
	char *tablename, *entry;
	size_t tablename_len, entry_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &tablename, &tablename_len, &entry, &entry_len) == FAILURE) { 
	   return;
	}

	zend_string *copy = zend_string_alloc(entry_len + 1, 0);
	snprintf(ZSTR_VAL(copy), entry_len + 1, "%s", entry);
	int ret = zfw_table_entry_delete(tablename, ZSTR_VAL(copy));
	zend_string_free(copy);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfw_table_flush)
{
	char *tablename;
	size_t tablename_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &tablename, &tablename_len) == FAILURE) { 
	   return;
	}

	int ret = zfw_table_flush(tablename);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfw_table_list)
{
	zfw_table_list_t *table_list = zfw_table_list();
	if (table_list == NULL)
		RETURN_FALSE;
	array_init(return_value);
	for (int i = 0; i < table_list->count; i++) {
		if (table_list->tables[i]->entries_size) {
			zval subitem;
			array_init(&subitem);
			char *tmp = strtok(table_list->tables[i]->table_entries,  ",");
			while (tmp != NULL) {
				add_next_index_string(&subitem, tmp);
				tmp = strtok(NULL,  ",");
			}
			if (zend_hash_num_elements(Z_ARRVAL_P(&subitem))) {
				add_assoc_zval(return_value, table_list->tables[i]->table_name, &subitem);
			} else {
				zend_array_destroy(Z_ARRVAL_P(&subitem));
				add_assoc_null(return_value, table_list->tables[i]->table_name);
			}
		} else {
			add_assoc_null(return_value, table_list->tables[i]->table_name);
		}
	}
	zfw_table_list_free(table_list);
	if (!zend_hash_num_elements(Z_ARRVAL_P(return_value))) {
		zend_array_destroy(Z_ARRVAL_P(return_value));
		RETURN_FALSE;
	}
}

PHP_FUNCTION(zfw_table_get)
{
	char *tablename;
	size_t tablename_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &tablename, &tablename_len) == FAILURE) { 
	   return;
	}

	zfw_table_t *table = zfw_table_get(tablename);
	if (table == NULL)
		RETURN_FALSE;
	array_init(return_value);
	if (table->entries_size) {
		zval subitem;
		array_init(&subitem);
		char *tmp = strtok(table->table_entries,  ",");
		while (tmp != NULL) {
			add_next_index_string(&subitem, tmp);
			tmp = strtok(NULL,  ",");
		}
		if (zend_hash_num_elements(Z_ARRVAL_P(&subitem))) {
			add_assoc_zval(return_value, table->table_name, &subitem);
		} else {
			zend_array_destroy(Z_ARRVAL_P(&subitem));
			add_assoc_null(return_value, table->table_name);
		}
	} else {
		add_assoc_null(return_value, table->table_name);
	}
	zfw_table_free(table);
}

// nat
PHP_FUNCTION(zfw_nat_delete)
{
	zend_long num;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &num) == FAILURE) { 
	   return;
	}

	int ret = zfw_nat_delete(num);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfw_nat_config)
{
	zend_long num;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &num) == FAILURE) { 
	   return;
	}

	int ret = zfw_nat_config_db(num);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}


// sets
PHP_FUNCTION(zfw_set_delete)
{
	zend_long num;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &num) == FAILURE) { 
	   return;
	}

	if (num < 0 || num >= ZFW_SET_MAX_NUM)
		RETURN_FALSE;

	int ret = zfw_set_delete(num);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfw_set_list)
{
	unsigned char *list = zfw_set_list();
	if (list == NULL)
		RETURN_FALSE;
	array_init(return_value);
    for (int i = 0; i < ZFW_SET_MAX_NUM; i++) {
		add_index_bool(return_value, i, list[i]);
    }
}

PHP_FUNCTION(zfw_set_enable)
{
	zend_long num;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &num) == FAILURE) { 
	   return;
	}

	if (num < 0 || num >= ZFW_SET_MAX_NUM)
		RETURN_FALSE;

	int ret = zfw_set_enable(num);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfw_set_disable)
{
	zend_long num;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &num) == FAILURE) { 
	   return;
	}

	if (num < 0 || num >= ZFW_SET_MAX_NUM)
		RETURN_FALSE;

	int ret = zfw_set_disable(num);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfw_set_move)
{
	zend_long from, to;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ll", &from, &to) == FAILURE) { 
	   return;
	}
	
	if (from < 0 || from >= ZFW_SET_MAX_NUM)
		RETURN_FALSE;
	if (to < 0 || to >= ZFW_SET_MAX_NUM)
		RETURN_FALSE;

	int ret = zfw_set_move(from, to);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}


// pipe/queue
PHP_FUNCTION(zfw_pipe_delete)
{
	zend_long num;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &num) == FAILURE) { 
	   return;
	}

	int ret = zfw_pipe_delete(ZFW_PIPE, num);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfw_queue_delete)
{
	zend_long num;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &num) == FAILURE) { 
	   return;
	}

	int ret = zfw_pipe_delete(ZFW_QUEUE, num);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}


PHP_FUNCTION(zfw_pipe_config)
{
	zend_long num;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &num) == FAILURE) { 
	   return;
	}

	int ret = zfw_pipe_config_db(ZFW_PIPE, num);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfw_queue_config)
{
	zend_long num;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &num) == FAILURE) { 
	   return;
	}

	int ret = zfw_pipe_config_db(ZFW_QUEUE, num);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}
