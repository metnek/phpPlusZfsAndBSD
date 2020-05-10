#ifndef PHP_NGCTL_H
#define PHP_NGCTL_H

#if HAVE_NGCTL

extern zend_module_entry ngctl_module_entry;
#define phpext_ngctl_ptr &ngctl_module_entry

#else
#define phpext_ngctl_ptr NULL
#endif

#endif