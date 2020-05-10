#ifndef PHP_SPWD_H
#define PHP_SPWD_H

#if HAVE_SPWD

extern zend_module_entry spwd_module_entry;
#define phpext_spwd_ptr &spwd_module_entry

#else
#define phpext_spwd_ptr NULL
#endif

#endif