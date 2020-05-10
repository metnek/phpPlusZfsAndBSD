#ifndef PHP_XLS2ARRAY_H
#define PHP_XLS2ARRAY_H

#if HAVE_XLS2ARRAY

extern zend_module_entry xls2array_module_entry;
#define phpext_xls2array_ptr &xls2array_module_entry

#else
#define phpext_xls2array_ptr NULL
#endif

#endif