/*****************************************************************************
Copyright (C)  2016  Brecht Sanders  All Rights Reserved
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "xlsxio_read.h"
#include "xlsxio_version.h"
#include "php.h"
#include "php_xlsx2array.h"

#define PHP_XLSX_RSCR  "XLSX File"

typedef struct php_xlsx_file {
  xlsxioreader xlsxioread;
  xlsxioreadersheetlist sheetlisthandle;
  xlsxioreadersheet sheethandle;
  int flag;
} php_xlsx_file_t;

PHP_MINIT_FUNCTION(xlsx2array);
PHP_MSHUTDOWN_FUNCTION(xlsx2array);
PHP_FUNCTION(xlsx2array);
PHP_FUNCTION(xlsx2array_open);
PHP_FUNCTION(xlsx2array_read);
PHP_FUNCTION(xlsx2array_close);
PHP_FUNCTION(xlsx_get_sheets);

ZEND_BEGIN_ARG_INFO_EX(arginfo_xlsx2array, 0, 1, 1)
  ZEND_ARG_INFO(0, xlsx_path)
  ZEND_ARG_INFO(0, sheet_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_xlsx2array_open, 0, 0, 1)
  ZEND_ARG_INFO(0, xlsx_path)
  ZEND_ARG_INFO(0, sheet_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_xlsx2array_close, 0, 0, 1)
  ZEND_ARG_INFO(0, xlsx_rsrc)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_xlsx2array_read, 0, 0, 1)
  ZEND_ARG_INFO(0, xlsx_rsrc)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_xlsx_get_sheets, 0, 0, 1)
  ZEND_ARG_INFO(0, xlsx_path)
ZEND_END_ARG_INFO()

const zend_function_entry xlsx2array_functions[] = {
  PHP_FE(xlsx2array, arginfo_xlsx2array)
  PHP_FE(xlsx2array_open, arginfo_xlsx2array_open)
  PHP_FE(xlsx2array_close, arginfo_xlsx2array_close)
  PHP_FE(xlsx2array_read, arginfo_xlsx2array_read)
  PHP_FE(xlsx_get_sheets, arginfo_xlsx_get_sheets)
  {NULL, NULL, NULL}
};


zend_module_entry xlsx2array_module_entry = {
  STANDARD_MODULE_HEADER,
  "xlsx2array",
  xlsx2array_functions,
  PHP_MINIT(xlsx2array),
  PHP_MSHUTDOWN(xlsx2array),
  NULL,
  NULL,
  NULL,
  "0.1.0",
  STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_XLSX2ARRAY

ZEND_GET_MODULE(xlsx2array)
#endif

static int le_xlsx;

static void php_xlsx_dtor(zend_resource *rsrc)
{
  php_xlsx_file_t *xlsx_rsrc = (php_xlsx_file_t *)rsrc->ptr;
  if (xlsx_rsrc->sheethandle != NULL)
    xlsxioread_sheet_close(xlsx_rsrc->sheethandle);
  if (xlsx_rsrc->sheetlisthandle != NULL)
    xlsxioread_sheetlist_close(xlsx_rsrc->sheetlisthandle);
  if (xlsx_rsrc->xlsxioread != NULL)
    xlsxioread_close(xlsx_rsrc->xlsxioread);
  free(xlsx_rsrc);
}

PHP_MINIT_FUNCTION(xlsx2array)
{
  le_xlsx = zend_register_list_destructors_ex(php_xlsx_dtor, NULL, PHP_XLSX_RSCR, module_number);

  return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(xlsx2array)
{
  return SUCCESS;
}

PHP_FUNCTION(xlsx2array_open)
{
  char *xlsx_path;
  size_t xlsx_path_len;
  char *sheet_name = "";
  size_t sheet_name_len = sizeof("")-1;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s", &xlsx_path, &xlsx_path_len, &sheet_name, &sheet_name_len) == FAILURE) { 
     return;
  }

  php_xlsx_file_t *xlsx_rsrc = malloc(sizeof(php_xlsx_file_t));
  int sheet_found = 0;
  xlsx_rsrc->flag = 0;

  if ((xlsx_rsrc->xlsxioread = xlsxioread_open(xlsx_path)) != NULL) {
    xlsx_rsrc->sheetlisthandle = xlsxioread_sheetlist_open(xlsx_rsrc->xlsxioread);
    const char *cur_sheetname;
    while ((cur_sheetname = xlsxioread_sheetlist_next(xlsx_rsrc->sheetlisthandle)) != NULL) {
      if (sheet_name[0]) {
        xlsx_rsrc->flag = 1;
        if (strcmp(sheet_name, cur_sheetname) != 0) {
          continue;
        }
      }
      sheet_found = 1;
      if ((xlsx_rsrc->sheethandle = xlsxioread_sheet_open(xlsx_rsrc->xlsxioread, cur_sheetname, XLSXIOREAD_SKIP_EMPTY_ROWS)) != NULL) {
        break;
      } else {
        xlsxioread_sheetlist_close(xlsx_rsrc->sheetlisthandle);
        xlsxioread_close(xlsx_rsrc->xlsxioread);
        free(xlsx_rsrc);
        RETURN_FALSE;
      }
    }
  } else {
    free(xlsx_rsrc);
    RETURN_FALSE;
  }
  if (!sheet_found) {
    xlsxioread_sheetlist_close(xlsx_rsrc->sheetlisthandle);
    xlsxioread_close(xlsx_rsrc->xlsxioread);
    free(xlsx_rsrc);
    RETURN_FALSE;
  }
  RETURN_RES(zend_register_resource(xlsx_rsrc, le_xlsx));
}
PHP_FUNCTION(xlsx2array_read)
{
  zval *xlsx_rsrc;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &xlsx_rsrc) == FAILURE) {
    return;
  }
  php_xlsx_file_t *xlsx_r = NULL;
  if ((xlsx_r = (php_xlsx_file_t *)zend_fetch_resource(Z_RES_P(xlsx_rsrc), PHP_XLSX_RSCR, le_xlsx)) == NULL) {
    RETURN_FALSE;
  }


  while (1) {
    if(xlsxioread_sheet_next_row(xlsx_r->sheethandle) != 0) {
      array_init(return_value);
      char *value;
      while ((value = xlsxioread_sheet_next_cell(xlsx_r->sheethandle)) != NULL) {
        add_next_index_string(return_value, value);
        free(value);
      }
      break;
    } else {
      if (xlsx_r->flag) {
        RETURN_FALSE;
      } else {
        const char *cur_sheetname;
        if (((cur_sheetname = xlsxioread_sheetlist_next(xlsx_r->sheetlisthandle)) != NULL)) {
          xlsxioreadersheet sheethandle;
          if ((sheethandle = xlsxioread_sheet_open(xlsx_r->xlsxioread, cur_sheetname, XLSXIOREAD_SKIP_EMPTY_ROWS)) != NULL) {
            xlsxioread_sheet_close(xlsx_r->sheethandle);
            xlsx_r->sheethandle = sheethandle;
          } else {
            RETURN_FALSE;
          }
        } else {
          RETURN_FALSE;
        }
      }
    }
  }


}
PHP_FUNCTION(xlsx2array_close)
{
  zval *xlsx_rsrc;

  if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &xlsx_rsrc) == FAILURE) {
    return;
  }
  php_xlsx_file_t *xlsx_r = NULL;
  if ((xlsx_r = (php_xlsx_file_t *)zend_fetch_resource(Z_RES_P(xlsx_rsrc), PHP_XLSX_RSCR, le_xlsx)) == NULL) {
    RETURN_FALSE;
  }
  zend_list_close(Z_RES_P(xlsx_rsrc));
  RETURN_TRUE;
}

PHP_FUNCTION(xlsx2array)
{
  char *xlsx_path;
  size_t xlsx_path_len;
  char *sheet_name = "";
  size_t sheet_name_len = sizeof("")-1;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s", &xlsx_path, &xlsx_path_len, &sheet_name, &sheet_name_len) == FAILURE) { 
     return;
  }

  xlsxioreader xlsxioread;
  int sheet_found = 0;

  if ((xlsxioread = xlsxioread_open(xlsx_path)) != NULL) {
    xlsxioreadersheetlist sheetlisthandle = xlsxioread_sheetlist_open(xlsxioread);
    const char *cur_sheetname;
    array_init(return_value);
    while ((cur_sheetname = xlsxioread_sheetlist_next(sheetlisthandle)) != NULL) {
      if (sheet_name[0]) {
        if (strcmp(sheet_name, cur_sheetname) != 0) {
          continue;
        }
      }
      sheet_found = 1;
      xlsxioreadersheet sheethandle;
      if ((sheethandle = xlsxioread_sheet_open(xlsxioread, cur_sheetname, XLSXIOREAD_SKIP_EMPTY_ROWS)) != NULL) {
        while (xlsxioread_sheet_next_row(sheethandle) != 0) {
          zval subitem;
          array_init(&subitem);
          char *value;
          while ((value = xlsxioread_sheet_next_cell(sheethandle)) != NULL) {
            add_next_index_string(&subitem, value);
            free(value);
          }
          add_next_index_zval(return_value, &subitem);
        }
        xlsxioread_sheet_close(sheethandle);
      } else {
        continue;
      }
    }
    xlsxioread_sheetlist_close(sheetlisthandle);
    xlsxioread_close(xlsxioread);
  } else {
    RETURN_FALSE;
  }
  if (!sheet_found)
    RETURN_FALSE;
  return;
}


PHP_FUNCTION(xlsx_get_sheets) {
  char *xlsx_path;
  size_t xlsx_path_len;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &xlsx_path, &xlsx_path_len) == FAILURE) { 
     return;
  }
  xlsxioreader xlsxioread;
  if ((xlsxioread = xlsxioread_open(xlsx_path)) != NULL) {
    xlsxioreadersheetlist sheetlisthandle = xlsxioread_sheetlist_open(xlsxioread);
    const char *cur_sheetname;
    array_init(return_value);
    while ((cur_sheetname = xlsxioread_sheetlist_next(sheetlisthandle)) != NULL) {
      add_next_index_string(return_value, cur_sheetname);
    }
    xlsxioread_sheetlist_close(sheetlisthandle);
    xlsxioread_close(xlsxioread);
  } else {
    RETURN_FALSE;
  }
  return;
}
