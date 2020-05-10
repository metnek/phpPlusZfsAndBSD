#ifndef PHP_NET_H
#define PHP_NET_H

#if HAVE_NET

extern zend_module_entry net_module_entry;
#define phpext_net_ptr &net_module_entry

#else
#define phpext_net_ptr NULL
#endif

#endif