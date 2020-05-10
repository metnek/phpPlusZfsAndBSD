#ifndef PHP_ZFW_H
#define PHP_ZFW_H

#if HAVE_ZFW

extern zend_module_entry zfw_module_entry;
#define phpext_zfw_ptr &zfw_module_entry

#else
#define phpext_zfw_ptr NULL
#endif

#endif