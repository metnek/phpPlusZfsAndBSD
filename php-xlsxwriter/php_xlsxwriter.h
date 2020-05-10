#ifndef PHP_XLSXWRITER_H
#define PHP_XLSXWRITER_H

#if HAVE_XLSXWRITER

extern zend_module_entry xlsxwriter_module_entry;
#define phpext_xlsxwriter_ptr &xlsxwriter_module_entry

#else
#define phpext_xlsxwriter_ptr NULL
#endif

#endif