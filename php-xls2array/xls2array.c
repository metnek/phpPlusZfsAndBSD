/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * Copyright 2004 Komarov Valery
 * Copyright 2006 Christophe Leitienne
 * Copyright 2008-2017 David Hoerl
 * Copyright 2013 Bob Colbert
 * Copyright 2013-2018 Evan Miller
 * Copyright 2019 Alia Akauova
 *
 * This file is part of libxls -- A multiplatform, C/C++ library for parsing
 * Excel(TM) files.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ''AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "php.h"
#include "php_xls2array.h"
#include "include/xls.h"
#define PHP_XLS_RSCR	"XLS File"
static char *encoding = "UTF-8";

typedef struct php_xls_file {
	xlsWorkBook* work_book;
	xlsWorkSheet* work_sheet;
	int j;
	int i;
} php_xls_file_t;

PHP_MINIT_FUNCTION(xls2array);
PHP_MSHUTDOWN_FUNCTION(xls2array);
PHP_FUNCTION(xls2array);
PHP_FUNCTION(xls2array_open);
PHP_FUNCTION(xls2array_close);
PHP_FUNCTION(xls2array_read);
PHP_FUNCTION(xls_get_sheets);

ZEND_BEGIN_ARG_INFO_EX(arginfo_xls2array, 0, 1, 1)
	ZEND_ARG_INFO(0, xls_path)
	ZEND_ARG_INFO(0, sheet_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_xls2array_open, 0, 0, 1)
	ZEND_ARG_INFO(0, xls_path)
	ZEND_ARG_INFO(0, sheet_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_xls2array_close, 0, 0, 1)
	ZEND_ARG_INFO(0, xls_rscr)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_xls2array_read, 0, 1, 1)
	ZEND_ARG_INFO(0, xls_rscr)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_xls_get_sheets, 0, 0, 1)
	ZEND_ARG_INFO(0, xls_path)
ZEND_END_ARG_INFO()

const zend_function_entry xls2array_functions[] = {
	PHP_FE(xls2array, arginfo_xls2array)
	PHP_FE(xls2array_open, arginfo_xls2array_open)
	PHP_FE(xls2array_close, arginfo_xls2array_close)
	PHP_FE(xls2array_read, arginfo_xls2array_read)
	PHP_FE(xls_get_sheets, arginfo_xls_get_sheets)
	{NULL, NULL, NULL}
};

zend_module_entry xls2array_module_entry = {
	STANDARD_MODULE_HEADER,
	"xls2array",
	xls2array_functions,
	PHP_MINIT(xls2array),
	PHP_MSHUTDOWN(xls2array),
	NULL,
	NULL,
	NULL,
	"0.1.2",
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_XLS2ARRAY

ZEND_GET_MODULE(xls2array)
#endif

static int le_xls;

static void php_xls_dtor(zend_resource *rsrc)
{
	php_xls_file_t *xls_rsrc = (php_xls_file_t *)rsrc->ptr;
	xls_close_WS(xls_rsrc->work_sheet);
	xls_close(xls_rsrc->work_book);
	free(xls_rsrc);
}

PHP_MINIT_FUNCTION(xls2array)
{
	le_xls = zend_register_list_destructors_ex(php_xls_dtor, NULL, PHP_XLS_RSCR, module_number);

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(xls2array)
{
	return SUCCESS;
}

PHP_FUNCTION(xls2array_open)
{
	char *xls_path;
	size_t xls_path_len;
    char *sheet_name = "";
    size_t sheet_name_len = sizeof("")-1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s", &xls_path, &xls_path_len, &sheet_name, &sheet_name_len) == FAILURE) { 
	   return;
	}
	xlsWorkBook* work_book = NULL;
	xlsWorkSheet* work_sheet = NULL;
    xls_error_t error = LIBXLS_OK;

	unsigned int i;
	php_xls_file_t *xls_rsrc = malloc(sizeof(php_xls_file_t));

	work_book = xls_open_file(xls_path, encoding, &error);
	if (!work_book) {
		free(xls_rsrc);
		RETURN_FALSE;
	}

	if (sheet_name[0]) {
		for (i = 0; i < work_book->sheets.count; i++) {
			if (strcmp(sheet_name, (char *)work_book->sheets.sheet[i].name) == 0) {
				break;
			}
		}

		if (i == work_book->sheets.count) {
			RETURN_FALSE;
		}
	}
	for (i = 0; i < work_book->sheets.count; i++) {
		if (sheet_name[0]) {
			xls_rsrc->i = -2;
			if (strcmp(sheet_name, (char *)work_book->sheets.sheet[i].name) != 0) {
				continue;
			}
		}
		if (xls_rsrc->i == -1)
			xls_rsrc->i = i;
		work_sheet = xls_getWorkSheet(work_book, i);
		xls_parseWorkSheet(work_sheet);
		break;
	}
	xls_rsrc->work_book = work_book;
	xls_rsrc->work_sheet = work_sheet;
	// xls_rsrc->i = -1;
	xls_rsrc->j = -1;

	RETURN_RES(zend_register_resource(xls_rsrc, le_xls));
}

PHP_FUNCTION(xls2array_close)
{
	zval *xls_rsrc;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &xls_rsrc) == FAILURE) {
		return;
	}
	php_xls_file_t *xls_r = NULL;
	if ((xls_r = (php_xls_file_t *)zend_fetch_resource(Z_RES_P(xls_rsrc), PHP_XLS_RSCR, le_xls)) == NULL) {
		RETURN_FALSE;
	}
	zend_list_close(Z_RES_P(xls_rsrc));
	RETURN_TRUE;
}

PHP_FUNCTION(xls2array_read)
{
	zval *xls_rsrc;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &xls_rsrc) == FAILURE) {
		return;
	}
	php_xls_file_t *xls_r = NULL;
	if ((xls_r = (php_xls_file_t *)zend_fetch_resource(Z_RES_P(xls_rsrc), PHP_XLS_RSCR, le_xls)) == NULL) {
		RETURN_FALSE;
	}

	if (xls_r->j == -1) {
		xls_r->j = 0;
	} else {
		xls_r->j++;
	}

	if (xls_r->j > (unsigned int)xls_r->work_sheet->rows.lastrow) {
		if (xls_r->i != -2) {
			xls_r->i++;
			if (xls_r->i < xls_r->work_book->sheets.count) {
				xls_r->j = 0;
				xls_close_WS(xls_r->work_sheet);
				xls_r->work_sheet = xls_getWorkSheet(xls_r->work_book, xls_r->i);
				xls_parseWorkSheet(xls_r->work_sheet);
			} else {
				RETURN_FALSE;
			}
		} else {
			RETURN_FALSE;
		}
	}

	array_init(return_value);

	WORD cell_row = (WORD)xls_r->j;
	WORD cell_col;
	for (cell_col = 0; cell_col <= xls_r->work_sheet->rows.lastcol; cell_col++) {

		xlsCell *cell = xls_cell(xls_r->work_sheet, cell_row, cell_col);

		if ((!cell) || (cell->isHidden)) {
			continue;
		}

		if (cell->id == XLS_RECORD_RK || cell->id == XLS_RECORD_MULRK || cell->id == XLS_RECORD_NUMBER) {
			add_next_index_double(return_value, cell->d);
		} else if (cell->id == XLS_RECORD_FORMULA || cell->id == XLS_RECORD_FORMULA_ALT) {
			if (cell->l == 0) {
				add_next_index_double(return_value, cell->d);
			} else {
				if (!strcmp((char *)cell->str, "bool")) {
					add_next_index_string(return_value, cell->d ? "true" : "false");
				} else if (!strcmp((char *)cell->str, "error")) {
					add_next_index_string(return_value, "*error*");
				} else {
					add_next_index_string(return_value, (char *)cell->str);
				}
			}
		} else if (cell->str != NULL) {
			add_next_index_string(return_value, (char *)cell->str);
		} else {
			add_next_index_null(return_value);
		}
	}
}

PHP_FUNCTION(xls2array) {

	char *xls_path;
	size_t xls_path_len;
    char *sheet_name = "";
    size_t sheet_name_len = sizeof("")-1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s", &xls_path, &xls_path_len, &sheet_name, &sheet_name_len) == FAILURE) { 
	   return;
	}

	xlsWorkBook* work_book;
	xlsWorkSheet* work_sheet;
    xls_error_t error = LIBXLS_OK;
	unsigned int i, j;

	work_book = xls_open_file(xls_path, encoding, &error);
	if (!work_book) {
		RETURN_FALSE;
	}

	if (sheet_name[0]) {
		for (i = 0; i < work_book->sheets.count; i++) {
			if (strcmp(sheet_name, (char *)work_book->sheets.sheet[i].name) == 0) {
				break;
			}
		}

		if (i == work_book->sheets.count) {
			RETURN_FALSE;
		}
	}

	array_init(return_value);
	for (i = 0; i < work_book->sheets.count; i++) {
		if (sheet_name[0]) {
			if (strcmp(sheet_name, (char *)work_book->sheets.sheet[i].name) != 0) {
				continue;
			}
		}

		work_sheet = xls_getWorkSheet(work_book, i);
		xls_parseWorkSheet(work_sheet);

		for (j = 0; j <= (unsigned int)work_sheet->rows.lastrow; ++j) {
			zval subitem;
			array_init(&subitem);
			WORD cell_row = (WORD)j;

			WORD cell_col;
			for (cell_col = 0; cell_col <= work_sheet->rows.lastcol; cell_col++) {

				xlsCell *cell = xls_cell(work_sheet, cell_row, cell_col);

				if ((!cell) || (cell->isHidden)) {
					continue;
				}

				if (cell->id == XLS_RECORD_RK || cell->id == XLS_RECORD_MULRK || cell->id == XLS_RECORD_NUMBER) {
					add_next_index_double(&subitem, cell->d);
				} else if (cell->id == XLS_RECORD_FORMULA || cell->id == XLS_RECORD_FORMULA_ALT) {
					if (cell->l == 0) {
						add_next_index_double(&subitem, cell->d);
					} else {
						if (!strcmp((char *)cell->str, "bool")) {
							add_next_index_string(&subitem, cell->d ? "true" : "false");
						} else if (!strcmp((char *)cell->str, "error")) {
							add_next_index_string(&subitem, "*error*");
						} else {
							add_next_index_string(&subitem, (char *)cell->str);
						}
					}
				} else if (cell->str != NULL) {
					add_next_index_string(&subitem, (char *)cell->str);
				} else {
					add_next_index_null(&subitem);
				}
			}
			add_next_index_zval(return_value, &subitem);
		}
		xls_close_WS(work_sheet);
	}

	xls_close(work_book);
	return;
}



PHP_FUNCTION(xls_get_sheets) {

	char *xls_path;
	size_t xls_path_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &xls_path, &xls_path_len) == FAILURE) { 
	   return;
	}

	array_init(return_value);

	xlsWorkBook* work_book;
	xlsWorkSheet* work_sheet;
	unsigned int i;

	struct st_row_data* row;
	WORD cell_row, cell_col;

	work_book = xls_open(xls_path, encoding);
	if (!work_book) {
		zend_array_destroy(Z_ARR_P(return_value));
		RETURN_FALSE;
	}

	for (i = 0; i < work_book->sheets.count; i++) {
        add_next_index_string(return_value, (char *)work_book->sheets.sheet[i].name);
	}
	return;
}
