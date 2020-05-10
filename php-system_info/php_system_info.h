#ifndef PHP_SYSTEM_INFO_H
#define PHP_SYSTEM_INFO_H

#if HAVE_SYSTEM_INFO

extern zend_module_entry system_info_module_entry;
#define phpext_system_info_ptr &system_info_module_entry

#else
#define phpext_system_info_ptr NULL
#endif

#endif