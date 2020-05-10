#ifndef PHP_POSIX_SHM_H
#define PHP_POSIX_SHM_H

#if HAVE_POSIX_SHM

extern zend_module_entry posix_shm_module_entry;
#define phpext_posix_shm_ptr &posix_shm_module_entry

#else
#define phpext_posix_shm_ptr NULL
#endif

#endif