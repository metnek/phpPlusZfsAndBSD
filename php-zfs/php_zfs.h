#ifndef PHP_ZFS_H
#define PHP_ZFS_H

#if HAVE_ZFS

extern zend_module_entry zfs_module_entry;
#define phpext_zfs_ptr &zfs_module_entry

#else
#define phpext_zfs_ptr NULL
#endif

#endif