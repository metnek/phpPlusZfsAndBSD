#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <xlsxwriter.h>
#include "php.h"
#include "php_xlsxwriter.h"

PHP_FUNCTION(xlsxwriter);

ZEND_BEGIN_ARG_INFO_EX(arginfo_xlsxwriter, 0, 0, 2)
	ZEND_ARG_INFO(0, file_path)
	ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

const zend_function_entry xlsxwriter_functions[] = {
	PHP_FE(xlsxwriter, arginfo_xlsxwriter)
	{NULL, NULL, NULL}
};

zend_module_entry xlsxwriter_module_entry = {
	STANDARD_MODULE_HEADER,
	"xlsxwriter",
	xlsxwriter_functions,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"0.0.1",
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_XLSXWRITER

ZEND_GET_MODULE(xlsxwriter)
#endif

PHP_FUNCTION(xlsxwriter)
{
	char *file_path;
	size_t file_path_len;
	zval *data = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa", &file_path, &file_path_len, &data) == FAILURE) {
		return;
	}

	int error = 0;

	if (data && Z_TYPE_P(data) == IS_ARRAY) {
		HashTable *data_hash;
		long num_row_key;
		zend_string *row_key;

		lxw_workbook  *workbook  = workbook_new(file_path);
		lxw_worksheet *worksheet = workbook_add_worksheet(workbook, NULL);
		zval *row = NULL;

		data_hash = Z_ARRVAL_P(data);
		ZEND_HASH_FOREACH_KEY_VAL(data_hash, num_row_key, row_key, row) {
			if(Z_TYPE_P(row) == IS_ARRAY) {
				long num_col_key;
				zend_string *col_key;
				zval *col;
				HashTable *row_hash = Z_ARRVAL_P(row);
				ZEND_HASH_FOREACH_KEY_VAL(row_hash, num_col_key, col_key, col) {
					if (Z_TYPE_P(col) == IS_STRING) {
						worksheet_write_string(worksheet, num_row_key, num_col_key, Z_STRVAL_P(col), NULL);
					} else if (Z_TYPE_P(col) == IS_LONG) {
						worksheet_write_number(worksheet, num_row_key, num_col_key, Z_LVAL_P(col), NULL);
					} else if (Z_TYPE_P(col) == IS_DOUBLE) {
						worksheet_write_number(worksheet, num_row_key, num_col_key, Z_DVAL_P(col), NULL);
					}
				} ZEND_HASH_FOREACH_END();
			} else {
				error = 1;
				break;
			}
		} ZEND_HASH_FOREACH_END();
		workbook_close(workbook);
	}
	if (error) {
		unlink(file_path);
		RETURN_FALSE;
	}
	RETURN_TRUE;
}

