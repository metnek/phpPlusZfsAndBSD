#ifndef PHP_GPART_H
#define PHP_GPART_H

#if HAVE_GPART

extern zend_module_entry gpart_module_entry;
#define phpext_gpart_ptr &gpart_module_entry

#else
#define phpext_gpart_ptr NULL
#endif

#endif