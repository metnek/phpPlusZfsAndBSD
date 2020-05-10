#ifndef PHP_XLSX2ARRAY_H
#define PHP_XLSX2ARRAY_H

#if HAVE_XLSX2ARRAY

extern zend_module_entry xlsx2array_module_entry;
#define phpext_xlsx2array_ptr &xlsx2array_module_entry

#else
#define phpext_xlsx2array_ptr NULL
#endif

#endif