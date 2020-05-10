#ifndef PHP_BDB_H
#define PHP_BDB_H

#if HAVE_BDB

extern zend_module_entry bdb_module_entry;
#define phpext_bdb_ptr &bdb_module_entry

#else
#define phpext_bdb_ptr NULL
#endif

#endif