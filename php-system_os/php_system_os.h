#ifndef PHP_SYSTEM_OS_H
#define PHP_SYSTEM_OS_H

#if HAVE_SYSTEM_OS

extern zend_module_entry system_os_module_entry;
#define phpext_system_os_ptr &system_os_module_entry

#else
#define phpext_system_os_ptr NULL
#endif

#endif