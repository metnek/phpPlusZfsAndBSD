#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/mman.h>
#include "php.h"
#include "php_posix_shm.h"

#define PHP_POSIX_SHM_RSCR		"POSIX shared memory"

typedef struct php_posix_shm_rsrc {
	char *shm_name;
	char *sem_name;
	int size;
	int mode;
} php_posix_shm_rsrc_t;


PHP_MINIT_FUNCTION(posix_shm);
PHP_MSHUTDOWN_FUNCTION(posix_shm);
PHP_FUNCTION(posix_shm_attach);										// done
PHP_FUNCTION(posix_shm_write);										// done
PHP_FUNCTION(posix_shm_read);										// done
PHP_FUNCTION(posix_shm_close);										// done



ZEND_BEGIN_ARG_INFO_EX(arginfo_posix_shm_attach, 0, 0, 3)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, size)
	ZEND_ARG_INFO(0, mode)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_posix_shm_write, 0, 0, 2)
	ZEND_ARG_INFO(0, rsrc_shm)
	ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_posix_shm_read, 0, 0, 1)
	ZEND_ARG_INFO(0, rsrc_shm)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_posix_shm_close, 0, 0, 1)
	ZEND_ARG_INFO(0, rsrc_shm)
ZEND_END_ARG_INFO()


const zend_function_entry posix_shm_functions[] = {
	PHP_FE(posix_shm_attach, arginfo_posix_shm_attach)
	PHP_FE(posix_shm_write, arginfo_posix_shm_write)
	PHP_FE(posix_shm_read, arginfo_posix_shm_read)
	PHP_FE(posix_shm_close, arginfo_posix_shm_close)
	{NULL, NULL, NULL}
};

zend_module_entry posix_shm_module_entry = {
	STANDARD_MODULE_HEADER,
	"posix_shm",
	posix_shm_functions,
	PHP_MINIT(posix_shm),
	PHP_MSHUTDOWN(posix_shm),
	NULL,
	NULL,
	NULL,
	"0.0.1",
	STANDARD_MODULE_PROPERTIES
};
 
#ifdef COMPILE_DL_POSIX_SHM
ZEND_GET_MODULE(posix_shm)

#endif

static int le_posix_shm;

static void php_posix_shm_dtor(zend_resource *rsrc)
{
	php_posix_shm_rsrc_t *posix_shm_rsrc = (php_posix_shm_rsrc_t *)rsrc->ptr;
	if (posix_shm_rsrc->shm_name != NULL)
		free(posix_shm_rsrc->shm_name);
	if (posix_shm_rsrc->shm_name != NULL)
		free(posix_shm_rsrc->sem_name);
	free(posix_shm_rsrc);
}


PHP_MINIT_FUNCTION(posix_shm)
{
	le_posix_shm = zend_register_list_destructors_ex(php_posix_shm_dtor, NULL, PHP_POSIX_SHM_RSCR, module_number);
	// REGISTER_INI_ENTRIES();

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(posix_shm)
{
    // UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}

PHP_FUNCTION(posix_shm_attach)
{
	char *name;
	size_t name_len;
	zend_long size, mode = 0660;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "sl|l", &name, &name_len, &size, &mode) == FAILURE) {
		return;
	}

	php_posix_shm_rsrc_t *rsrc = malloc(sizeof(php_posix_shm_rsrc_t));
	if (rsrc == NULL)
		RETURN_FALSE;

	rsrc->shm_name = malloc(name_len + 5);
	if (rsrc->shm_name == NULL) {
		free(rsrc);
		RETURN_FALSE;
	}
	rsrc->sem_name = malloc(name_len + 5);
	if (rsrc->sem_name == NULL) {
		free(rsrc->shm_name);
		free(rsrc);
		RETURN_FALSE;
	}

	snprintf(rsrc->shm_name, name_len + 5, "/shm%s", name);
	snprintf(rsrc->sem_name, name_len + 5, "/sem%s", name);
	int ret = 0;
	sem_t *sem = NULL;
	int fd_shm = -1;
	char *ptr = NULL;
	int is_first = 0;
	if ((sem = sem_open(rsrc->sem_name, 0, 0, 0)) == SEM_FAILED) {
		if ((sem = sem_open(rsrc->sem_name, O_CREAT, 0660, 0)) == SEM_FAILED) {
#ifdef ZDEBUG
			printf("%s\n", "sem_open failed");
#endif
			ret = 1;
			goto out;
		}
		is_first = 1;
#ifdef ZDEBUG
		printf("first\n");
#endif
	}
#ifdef ZDEBUG
	int sem_ret = -1;
	sem_getvalue(sem, &sem_ret);
	printf("attach start sem state = %d\n", sem_ret);
#endif
	if ((fd_shm = shm_open (rsrc->shm_name, O_RDWR, rsrc->mode)) == -1) {
		if ((fd_shm = shm_open (rsrc->shm_name, O_RDWR | O_CREAT, mode)) == -1) {
#ifdef ZDEBUG
			printf("%s\n", "shm_open failed");
#endif
			ret = 1;
			goto out;
		}
		if (ftruncate (fd_shm, size) == -1) {
#ifdef ZDEBUG
			printf("%s\n", "ftruncate failed");
#endif
			ret = 1;
			goto out;
		}
		if ((ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
	            fd_shm, 0)) == MAP_FAILED) {
#ifdef ZDEBUG
				printf("%s\n", "mmap failed");
#endif
			ret = 1;
			goto out;
		}
	}

	if (is_first)
		if (sem_post (sem) == -1) {
#ifdef ZDEBUG
			printf("%s\n", "sem_post failed");
#endif
			munmap(ptr, rsrc->size);
			ret = 1;
			goto out;
		}


out:
	if (ret) {
		if (ptr != NULL)
			shm_unlink(rsrc->shm_name);
		if (sem != NULL)
			sem_close(sem);
		free(rsrc->shm_name);
		free(rsrc->sem_name);
		free(rsrc);
		close(fd_shm);
		RETURN_FALSE;
	}
	rsrc->size = size;
	rsrc->mode = mode;
	munmap(ptr, rsrc->size);
	close(fd_shm);
#ifdef ZDEBUG
	sem_getvalue(sem, &sem_ret);
	printf("attach end sem state = %d\n", sem_ret);
#endif
	if (sem != NULL)
		sem_close(sem);
	RETURN_RES(zend_register_resource(rsrc, le_posix_shm));
}

PHP_FUNCTION(posix_shm_write)
{
	zval *rsrc_shm;
	char *data;
	size_t data_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rs", &rsrc_shm, &data, &data_len) == FAILURE) {
		return;
	}

	char *dbname = NULL;
	php_posix_shm_rsrc_t *rsrc = NULL;
	if ((rsrc = (php_posix_shm_rsrc_t *)zend_fetch_resource(Z_RES_P(rsrc_shm), PHP_POSIX_SHM_RSCR, le_posix_shm)) == NULL) {
		RETURN_FALSE;
	}

	sem_t *sem = NULL;
	int val = 0;
	if ((sem = sem_open(rsrc->sem_name, 0, 0, 0)) == SEM_FAILED) {
#ifdef ZDEBUG
		printf("%s\n", "sem_open failed");
		printf("%s\n", strerror(errno));
#endif
		RETURN_FALSE;
	}
#ifdef ZDEBUG
	int sem_ret = -1;
	sem_getvalue(sem, &sem_ret);
	printf("write start sem state = %d\n", sem_ret);
#endif
	if (sem_wait(sem) == -1) {
#ifdef ZDEBUG
		printf("%s\n", "sem_wait failed");
		printf("%s\n", strerror(errno));
#endif
		sem_post(sem);
		sem_close(sem);
		RETURN_FALSE;
	}
	int fd_shm = -1;
	if ((fd_shm = shm_open (rsrc->shm_name, O_RDWR, rsrc->mode)) == -1) {
#ifdef ZDEBUG
		printf("%s\n", "shm_open failed");
		printf("%s\n", strerror(errno));
#endif
		sem_post(sem);
		sem_close(sem);
		RETURN_FALSE;
	}
	char *ptr = NULL;
	if ((ptr = mmap(NULL, rsrc->size, PROT_WRITE, MAP_SHARED,
            fd_shm, 0)) == MAP_FAILED) {
#ifdef ZDEBUG
		printf("%s size=%d, fd=%d\n", "mmap failed", rsrc->size, fd_shm);
		printf("%s\n", strerror(errno));
#endif

		close(fd_shm);
		sem_post(sem);
		sem_close(sem);
		RETURN_FALSE;
	}
	snprintf(ptr, rsrc->size, "%s", data);

	munmap(ptr, rsrc->size);

	close(fd_shm);
	sem_post(sem);

#ifdef ZDEBUG
	sem_getvalue(sem, &sem_ret);
	printf("write end sem state = %d\n", sem_ret);
#endif
	if (sem != NULL)
		sem_close(sem);
	RETURN_TRUE;

}

PHP_FUNCTION(posix_shm_read)
{
	zval *rsrc_shm;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &rsrc_shm) == FAILURE) {
		return;
	}

	php_posix_shm_rsrc_t *rsrc = NULL;
	if ((rsrc = (php_posix_shm_rsrc_t *)zend_fetch_resource(Z_RES_P(rsrc_shm), PHP_POSIX_SHM_RSCR, le_posix_shm)) == NULL) {
		RETURN_FALSE;
	}
	int fd_shm = -1;
	if ((fd_shm = shm_open (rsrc->shm_name, O_RDONLY, rsrc->mode)) == -1) {
		RETURN_FALSE;
	}
	char *ptr = NULL;
	if ((ptr = mmap(NULL, rsrc->size, PROT_READ, MAP_SHARED,
            fd_shm, 0)) == MAP_FAILED) {
		close(fd_shm);
		RETURN_FALSE;
	}
	char tmp[strlen(ptr) + 1];
	snprintf(tmp, sizeof(tmp), "%s", ptr);
	munmap(ptr, rsrc->size);
	close(fd_shm);
	RETURN_STRING(tmp);
}

PHP_FUNCTION(posix_shm_close)
{
	zval *rsrc_shm;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "r", &rsrc_shm) == FAILURE) {
		return;
	}

	php_posix_shm_rsrc_t *rsrc = NULL;
	if ((rsrc = (php_posix_shm_rsrc_t *)zend_fetch_resource(Z_RES_P(rsrc_shm), PHP_POSIX_SHM_RSCR, le_posix_shm)) == NULL) {
		RETURN_FALSE;
	}

	sem_t *sem = NULL;
	if ((sem = sem_open(rsrc->sem_name, 0, 0, 0)) == SEM_FAILED) {
		RETURN_FALSE;
	}
	if (sem_wait(sem) == -1) {
		sem_close(sem);
		RETURN_FALSE;
	}

	shm_unlink(rsrc->shm_name);
	sem_post (sem);
	// sem_close(sem);
	sem_unlink(rsrc->sem_name);
	zend_list_close(Z_RES_P(rsrc_shm));
}
