//#include "php_need_solaris_boolean.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <libintl.h>
#include <libuutil.h>
#include <libnvpair.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <zone.h>
#include <grp.h>
#include <pwd.h>
#include <kenv.h>
#include <signal.h>
#include <sys/debug.h>
#include <sys/list.h>
#include <sys/mntent.h>
#include <sys/mnttab.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/fs/zfs.h>
#include <sys/types.h>
#include <time.h>
#include <err.h>
#include <jail.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <libzfs.h>
#include <libzfs_core.h>
#include <zfs_prop.h>
#include <zfs_deleg.h>
#include <libuutil.h>
#include <libgeom.h>
#include <kzfs.h>
#include "libzfs_impl.h"
#include "zfs_comutil.h"
#include "zfeature_common.h"
#include "php.h"
#include "php_ini.h"
#include "php_zfs.h"

#if !defined(BSD112) && !defined(BSD12) && !defined(BSD113)
#error "please, define BSD112|BSD12|BSD113"
#endif

libzfs_handle_t *g_zfs;

typedef struct destroy_cbdata {
	boolean_t	cb_first;
	boolean_t	cb_force;
	boolean_t	cb_recurse;
	boolean_t	cb_error;
	boolean_t	cb_doclones;
	zfs_handle_t	*cb_target;
	boolean_t	cb_defer_destroy;
	boolean_t	cb_verbose;
	boolean_t	cb_parsable;
	boolean_t	cb_dryrun;
	nvlist_t	*cb_nvl;
	nvlist_t	*cb_batchedsnaps;
	char		*cb_firstsnap;
	char		*cb_prevsnap;
	int64_t		cb_snapused;
	char		*cb_snapspec;
	char		*cb_bookmark;
} destroy_cbdata_t;


typedef struct zfs_node {
	zfs_handle_t	*zn_handle;
	uu_avl_node_t	zn_avlnode;
} zfs_node_t;

typedef struct snap_cbdata {
	nvlist_t *sd_nvl;
	boolean_t sd_recursive;
	const char *sd_snapname;
} snap_cbdata_t;

typedef struct callback_data {
	zfs_type_t		cb_types;
	zprop_list_t		*cb_proplist;
	uint8_t			cb_props_table[ZFS_NUM_PROPS];
	void *	tmp;
} callback_data_t;

typedef struct zpool_cb_data {
	uint8_t			cb_props_table[ZPOOL_NUM_PROPS];
	void *	tmp;
} zpool_cb_data_t;

typedef struct zpool_imp_cb {
	int cb_type;
	void *cb_ret;
} zpool_imp_cb_t;

typedef struct status_cbdata {
	int		cb_count;
	boolean_t	cb_allpools;
	boolean_t	cb_verbose;
	zval *cb_ret;
} status_cbdata_t;

typedef struct spare_cbdata {
	uint64_t	cb_guid;
	zpool_handle_t	*cb_zhp;
} spare_cbdata_t;

typedef struct scrub_cbdata {
	int	cb_type;
	pool_scrub_cmd_t cb_scrub_cmd;
} scrub_cbdata_t;

typedef struct replication_level {
	char *zprl_type;
	uint64_t zprl_children;
	uint64_t zprl_parity;
} replication_level_t;

typedef struct unshare_unmount_node {
	zfs_handle_t	*un_zhp;
	char		*un_mountp;
	uu_avl_node_t	un_avlnode;
} unshare_unmount_node_t;

#define PHP_ZPOOL_SCRUB_START	0
#define PHP_ZPOOL_SCRUB_PAUSE	1
#define PHP_ZPOOL_SCRUB_STOP	2

#define	ZPOOL_FUZZ						(16 * 1024 * 1024)
#define ZFS_CRON_RULE_LEN				512
#define ZFS_CRON_LINE_LEN				512

#if defined(BSD113) || defined(BSD12)

typedef enum { OP_SHARE, OP_MOUNT } share_mount_op_t;

typedef struct share_mount_state {
    share_mount_op_t    sm_op;
    boolean_t   sm_verbose;
    int sm_flags;
    char    *sm_options;
    char    *sm_proto; /* only valid for OP_SHARE */
    pthread_mutex_t sm_lock; /* protects the remaining fields */
    uint_t  sm_total; /* number of filesystems to process */
    uint_t  sm_done; /* number of filesystems processed */
    int sm_status; /* -1 if any of the share/mount operations failed */
} share_mount_state_t;
typedef struct get_all_state {
    boolean_t   ga_verbose;
    get_all_cb_t    *ga_cbp;
} get_all_state_t;


#else
#define OP_SHARE    0x1
#define OP_MOUNT    0x2
#endif


static boolean_t should_auto_mount(zfs_handle_t *zhp);
static int destroy_check_dependent(zfs_handle_t *zhp, void *data);
static int destroy_callback(zfs_handle_t *zhp, void *data);
static int zfs_callback(zfs_handle_t *zhp, void *data);
static boolean_t prop_list_contains_feature(nvlist_t *proplist);
static int add_prop_list(const char *propname, char *propval, nvlist_t **props, boolean_t poolprop);
static nvlist_t *make_root_vdev(zpool_handle_t *zhp, int force, int check_rep,
    boolean_t replacing, boolean_t dryrun, zpool_boot_label_t boot_type,
    uint64_t boot_size, zval *devs);
static nvlist_t *construct_spec(zval *devs);
static boolean_t is_device_in_use(nvlist_t *config, nvlist_t *nv, boolean_t force,
    boolean_t replacing, boolean_t isspare);
static const char *is_grouping(const char *type, int *mindev, int *maxdev);
static int check_device(const char *name, boolean_t force, boolean_t isspare);
static boolean_t is_whole_disk(const char *arg);
static nvlist_t *make_leaf_vdev(const char *arg, uint64_t is_log);
static int check_file(const char *file, boolean_t force, boolean_t isspare);
static void zpool_no_memory(void);
static int remove_from_file(char *filename, char *match_line, int line_num);
static int add_snap_rule(zval *sched, char *cron_text, size_t size, char *zname);
static int zfs_snapshot_cb(zfs_handle_t *zhp, void *arg);
static int snapshot_to_nvl_cb(zfs_handle_t *zhp, void *arg);
static int gather_snapshots(zfs_handle_t *zhp, void *arg);
static int zfs_snapshot_rb(const char *zname, char *snapname, long recursive);
static int zfs_dataset_rb(char *zname, long recursive);
static int zfs_delete_user_prop(char *zname, char *propname);
static int zpool_callback(zpool_handle_t *zhp, void *data);
static int do_import(nvlist_t *config, const char *newname, const char *mntopts,
    nvlist_t *props, int flags);
static int add_prop_list_default(const char *propname, char *propval, nvlist_t **props,
    boolean_t poolprop);
static int zpool_import_callback(char *zpool_name, zval *params, zpool_imp_cb_t *cb);
static void fill_import_info(nvlist_t *config, zval *subitem);
static void zpool_get_import_config(const char *name, nvlist_t *nv, int depth, zval *subitem);
static int zpool_status_callback(zpool_handle_t *zhp, void *data);
static void zpool_get_status_config(zpool_handle_t *zhp, const char *name, nvlist_t *nv, int depth, boolean_t isspare, zval *item);
static int check_replication(nvlist_t *config, nvlist_t *newroot);
static replication_level_t *get_replication(nvlist_t *nvroot, boolean_t fatal);
static boolean_t is_spare(nvlist_t *config, const char *path);
static int zpool_attach_or_replace(char *zpool_name, char *old_dev, char *new_dev, int force, int replacing);
static boolean_t zpool_has_checkpoint(zpool_handle_t *zhp);
static int zpool_scrub_callback(zpool_handle_t *zhp, void *data);
static nvlist_t *split_mirror_vdev(zpool_handle_t *zhp, char *newname, nvlist_t *props,
    splitflags_t flags, zval *devs);
static int share_mount_one(zfs_handle_t *zhp, int op, int flags, char *protocol,
    boolean_t explicit, const char *options);

#ifdef BSD112
static void get_all_datasets(libzfs_handle_t *g_zfs, zfs_handle_t ***dslist, size_t *count);
#elif defined(BSD12) || defined(BSD113)
static void get_all_datasets(libzfs_handle_t *g_zfs, get_all_cb_t *cbp, boolean_t verbose);
static int share_mount_one_cb(zfs_handle_t *zhp, void *arg);
#endif
// static void get_all_datasets(zfs_handle_t ***dslist, size_t *count);
static int get_one_dataset(zfs_handle_t *zhp, void *data);
static int unshare_unmount_path(char *path, int flags, boolean_t is_manual);
static int unshare_unmount_compare(const void *larg, const void *rarg, void *unused);
static int zfs_prop_cb(int prop, void *cb);
static int zpool_prop_cb(int prop, void *cb);
static void zpool_devs_list(zpool_handle_t *zhp, nvlist_t *nv, const char *name, int depth, zval *info);


static void zpool_get_scan_status(pool_scan_stat_t *ps, zval *item);
static void zpool_get_checkpoint_scan_warning(pool_scan_stat_t *ps, pool_checkpoint_stat_t *pcs, zval *item);
static void zpool_get_removal_status(zpool_handle_t *zhp, pool_removal_stat_t *prs, zval *item);
static void zpool_get_checkpoint_status(pool_checkpoint_stat_t *pcs, zval *item);
static void zpool_get_error_log(zpool_handle_t *zhp, zval *item);
static void zpool_get_spares(zpool_handle_t *zhp, nvlist_t **spares, uint_t nspares, zval *item);
static void zpool_get_l2cache(zpool_handle_t *zhp, nvlist_t **l2cache, uint_t nl2cache, zval *item);
static boolean_t find_vdev(nvlist_t *nv, uint64_t search);
static int find_spare(zpool_handle_t *zhp, void *data);
static void zpool_get_logs(zpool_handle_t *zhp, nvlist_t *nv, boolean_t verbose, zval *item);
static uint_t num_logs(nvlist_t *nv);
static inline int prop_cmp(const void *a, const void *b);


static int fake_sort(const void *larg, const void *rarg, void *data);



PHP_MINIT_FUNCTION(zfs);
PHP_MSHUTDOWN_FUNCTION(zfs);

PHP_FUNCTION(zfs_ds_create);				// done
PHP_FUNCTION(zfs_ds_destroy);				// done
PHP_FUNCTION(zfs_ds_list);					// done
PHP_FUNCTION(zfs_ds_update);				// done
PHP_FUNCTION(zfs_ds_props_remove);			// wait
PHP_FUNCTION(zfs_snap_create);				// done
PHP_FUNCTION(zfs_snap_destroy);				// done
PHP_FUNCTION(zfs_snap_update);				// done
PHP_FUNCTION(zfs_snap_rollback);			// done
PHP_FUNCTION(zfs_snap_list);				// done
// PHP_FUNCTION(zfs_snap_rule_create);			// done
PHP_FUNCTION(zfs_snap_rule_delete);			// done
PHP_FUNCTION(zfs_snap_rule_set);			// done
PHP_FUNCTION(zfs_snap_rule_get);			// done
PHP_FUNCTION(zfs_send);						// done
PHP_FUNCTION(zfs_recv);						// done
PHP_FUNCTION(zfs_ds_rename);				// done
PHP_FUNCTION(zfs_snap_rename);				// done
PHP_FUNCTION(zfs_ds_props);				// done
PHP_FUNCTION(zfs_mount);					// done
PHP_FUNCTION(zfs_mount_list);				// done
PHP_FUNCTION(zfs_mount_all);				// done
PHP_FUNCTION(zfs_unmount);					// done
PHP_FUNCTION(zfs_unmount_all);				// wait  (not tested)
PHP_FUNCTION(zfs_all_props);				// done
PHP_FUNCTION(zfs_settings_merge);			// wait

PHP_FUNCTION(zpool_create);					// wait (not tested)
PHP_FUNCTION(zpool_all_props);				// done
PHP_FUNCTION(zpool_destroy);				// done
PHP_FUNCTION(zpool_list);					// done
PHP_FUNCTION(zpool_update);					// done
PHP_FUNCTION(zpool_import);					// done
PHP_FUNCTION(zpool_import_all);				// done
PHP_FUNCTION(zpool_import_list);			// done
PHP_FUNCTION(zpool_export);					// done
PHP_FUNCTION(zpool_status);					// wait (not tested)
PHP_FUNCTION(zpool_attach);					// wait (not tested)
PHP_FUNCTION(zpool_detach);					// wait (not tested)
PHP_FUNCTION(zpool_online);					// wait (not tested)
PHP_FUNCTION(zpool_offline);				// wait (not tested)
PHP_FUNCTION(zpool_add);					// done
PHP_FUNCTION(zpool_remove);					// wait !!! DANGER !!!
PHP_FUNCTION(zpool_replace);				// done
PHP_FUNCTION(zpool_clear);					// done
PHP_FUNCTION(zpool_reguid);					// done
PHP_FUNCTION(zpool_reopen);					// done
PHP_FUNCTION(zpool_scrub);					// wait (not tested)
PHP_FUNCTION(zpool_labelclear);				// wait (not tested)
PHP_FUNCTION(zpool_split);					// done
PHP_FUNCTION(zpool_root_mount_from);		// done
PHP_FUNCTION(zpool_get_devs);				// wait

PHP_FUNCTION(test1);

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_ds_create, 0, 0, 2)
	ZEND_ARG_INFO(0, zname)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_zfs_all_props, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_zpool_root_mount_from, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_zpool_all_props, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_ds_destroy, 0, 0, 1)
	ZEND_ARG_INFO(0, zname)
	ZEND_ARG_INFO(0, recursive)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_ds_update, 0, 0, 2)
	ZEND_ARG_INFO(0, zname)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_ds_props_remove, 0, 0, 2)
	ZEND_ARG_INFO(0, zname)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_ds_rename, 0, 0, 2)
	ZEND_ARG_INFO(0, zname)
	ZEND_ARG_INFO(0, new_zname)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_snap_rename, 0, 0, 3)
	ZEND_ARG_INFO(0, zname)
	ZEND_ARG_INFO(0, snapname)
	ZEND_ARG_INFO(0, new_snapname)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_snap_create, 0, 0, 3)
	ZEND_ARG_INFO(0, zname)
	ZEND_ARG_INFO(0, snapname)
	ZEND_ARG_INFO(0, desc)
	ZEND_ARG_INFO(0, recursive)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_snap_destroy, 0, 0, 2)
	ZEND_ARG_INFO(0, zname)
	ZEND_ARG_INFO(0, snapname)
	ZEND_ARG_INFO(0, recursive)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_snap_update, 0, 0, 3)
	ZEND_ARG_INFO(0, zname)
	ZEND_ARG_INFO(0, snapname)
	ZEND_ARG_INFO(0, desc)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_snap_rollback, 0, 0, 2)
	ZEND_ARG_INFO(0, zname)
	ZEND_ARG_INFO(0, snapname)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_snap_list, 0, 0, 1)
	ZEND_ARG_INFO(0, zname)
ZEND_END_ARG_INFO()

// ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_snap_rule_create, 0, 0, 2)
// 	ZEND_ARG_INFO(0, zname)
// 	ZEND_ARG_INFO(0, sched)
// ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_snap_rule_set, 0, 0, 2)
	ZEND_ARG_INFO(0, zname)
	ZEND_ARG_INFO(0, sched)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_snap_rule_delete, 0, 0, 1)
	ZEND_ARG_INFO(0, zname)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_ds_props, 0, 0, 1)
	ZEND_ARG_INFO(0, zname)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_settings_merge, 0, 0, 2)
	ZEND_ARG_INFO(0, from)
	ZEND_ARG_INFO(0, to)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_snap_rule_get, 0, 0, 1)
	ZEND_ARG_INFO(0, zname)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_ds_list, 0, 0, 1) 
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_mount, 0, 0, 1) 
	ZEND_ARG_INFO(0, zname)
	ZEND_ARG_INFO(0, mntopts)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_unmount, 0, 0, 1)
	ZEND_ARG_INFO(0, entry)
	ZEND_ARG_INFO(0, force)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_unmount_all, 0, 0, 1)
	ZEND_ARG_INFO(0, force)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_zfs_mount_list, 0) 
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_mount_all, 0, 0, 1) 
	ZEND_ARG_INFO(0, mntopts)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_send, 0, 0, 2)
	ZEND_ARG_INFO(0, zname)
	ZEND_ARG_INFO(0, params)
	ZEND_ARG_INFO(0, move)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zfs_recv, 0, 0, 2)
	ZEND_ARG_INFO(0, zname)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_create, 0, 0, 2)
	ZEND_ARG_INFO(0, zpool_name)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_destroy, 0, 0, 1)
	ZEND_ARG_INFO(0, zpool_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_export, 0, 0, 1)
	ZEND_ARG_INFO(0, zpool_name)
	ZEND_ARG_INFO(0, force)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_online, 0, 0, 2)
	ZEND_ARG_INFO(0, zpool_name)
	ZEND_ARG_INFO(0, devs)
	ZEND_ARG_INFO(0, expand)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_offline, 0, 0, 2)
	ZEND_ARG_INFO(0, zpool_name)
	ZEND_ARG_INFO(0, devs)
	ZEND_ARG_INFO(0, temp)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_list, 0, 0, 1)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_update, 0, 0, 2)
	ZEND_ARG_INFO(0, zpool_name)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_import, 0, 0, 2)
	ZEND_ARG_INFO(0, name)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_import_list, 0, 0, 1)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_import_all, 0, 0, 1)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_attach, 0, 0, 3)
	ZEND_ARG_INFO(0, zpool_name)
	ZEND_ARG_INFO(0, dev)
	ZEND_ARG_INFO(0, new_dev)
	ZEND_ARG_INFO(0, force)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_detach, 0, 0, 2)
	ZEND_ARG_INFO(0, zpool_name)
	ZEND_ARG_INFO(0, dev)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_status, 0, 0, 1)
	ZEND_ARG_INFO(0, zpool_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_add, 0, 0, 2)
	ZEND_ARG_INFO(0, zpool_name)
	ZEND_ARG_INFO(0, devs)
	ZEND_ARG_INFO(0, force)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_remove, 0, 0, 2)
	ZEND_ARG_INFO(0, zpool_name)
	ZEND_ARG_INFO(0, devs)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_replace, 0, 0, 2)
	ZEND_ARG_INFO(0, zpool_name)
	ZEND_ARG_INFO(0, dev)
	ZEND_ARG_INFO(0, new_dev)
	ZEND_ARG_INFO(0, force)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_clear, 0, 0, 1)
	ZEND_ARG_INFO(0, zpool_name)
	ZEND_ARG_INFO(0, do_rewind)
	ZEND_ARG_INFO(0, dev)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_reguid, 0, 0, 1)
	ZEND_ARG_INFO(0, zpool_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_reopen, 0, 0, 1)
	ZEND_ARG_INFO(0, zpool_name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_scrub, 0, 0, 1)
	ZEND_ARG_INFO(0, zpool_list)
	ZEND_ARG_INFO(0, cmd)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_labelclear, 0, 0, 1)
	ZEND_ARG_INFO(0, dev)
	ZEND_ARG_INFO(0, force)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_split, 0, 0, 2)
	ZEND_ARG_INFO(0, zpool_name)
	ZEND_ARG_INFO(0, new_zpool_name)
	ZEND_ARG_INFO(0, params)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_test1, 0, 0, 1) 
	ZEND_ARG_INFO(0, ar)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_zpool_get_devs, 0, 0, 1)
	ZEND_ARG_INFO(0, zpool_name)
ZEND_END_ARG_INFO()

const zend_function_entry zfs_functions[] = {
	PHP_FE(zfs_ds_create, arginfo_zfs_ds_create)
	PHP_FE(zfs_ds_props, arginfo_zfs_ds_props)
	PHP_FE(zfs_all_props, NULL)
	PHP_FE(zfs_ds_destroy, arginfo_zfs_ds_destroy)
	PHP_FE(zfs_ds_update, arginfo_zfs_ds_update)
	PHP_FE(zfs_ds_props_remove, arginfo_zfs_ds_props_remove)
	PHP_FE(zfs_ds_rename, arginfo_zfs_ds_rename)
	PHP_FE(zfs_snap_rename, arginfo_zfs_snap_rename)
	PHP_FE(zfs_snap_create, arginfo_zfs_snap_create)
	PHP_FE(zfs_snap_destroy, arginfo_zfs_snap_destroy)
	PHP_FE(zfs_snap_update, arginfo_zfs_snap_update)
	PHP_FE(zfs_snap_rollback, arginfo_zfs_snap_rollback)
	PHP_FE(zfs_snap_list, arginfo_zfs_snap_list)
	// PHP_FE(zfs_snap_rule_create, arginfo_zfs_snap_rule_create)
	PHP_FE(zfs_snap_rule_delete, arginfo_zfs_snap_rule_delete)
	PHP_FE(zfs_snap_rule_set, arginfo_zfs_snap_rule_set)
	PHP_FE(zfs_snap_rule_get, arginfo_zfs_snap_rule_get)
	PHP_FE(zfs_send, arginfo_zfs_send)
	PHP_FE(zfs_recv, arginfo_zfs_recv)
	PHP_FE(zfs_ds_list, arginfo_zfs_ds_list)
	PHP_FE(zfs_mount, arginfo_zfs_mount)
	PHP_FE(zfs_mount_all, arginfo_zfs_mount_all)
	PHP_FE(zfs_mount_list, NULL)
	PHP_FE(zfs_unmount, arginfo_zfs_unmount)
	PHP_FE(zfs_unmount_all, arginfo_zfs_unmount_all)
	PHP_FE(zfs_settings_merge, arginfo_zfs_settings_merge)
	PHP_FE(zpool_create, arginfo_zpool_create)
	PHP_FE(zpool_all_props, NULL)
	PHP_FE(zpool_destroy, arginfo_zpool_destroy)
	PHP_FE(zpool_export, arginfo_zpool_export)
	PHP_FE(zpool_online, arginfo_zpool_online)
	PHP_FE(zpool_offline, arginfo_zpool_offline)
	PHP_FE(zpool_list, arginfo_zpool_list)
	PHP_FE(zpool_update, arginfo_zpool_update)
	PHP_FE(zpool_import, arginfo_zpool_import)
	PHP_FE(zpool_import_list, arginfo_zpool_import_list)
	PHP_FE(zpool_import_all, arginfo_zpool_import_all)
	PHP_FE(zpool_attach, arginfo_zpool_attach)
	PHP_FE(zpool_detach, arginfo_zpool_detach)
	PHP_FE(zpool_status, arginfo_zpool_status)
	PHP_FE(zpool_add, arginfo_zpool_add)
	PHP_FE(zpool_remove, arginfo_zpool_remove)
	PHP_FE(zpool_replace, arginfo_zpool_replace)
	PHP_FE(zpool_clear, arginfo_zpool_clear)
	PHP_FE(zpool_reguid, arginfo_zpool_reguid)
	PHP_FE(zpool_reopen, arginfo_zpool_reopen)
	PHP_FE(zpool_scrub, arginfo_zpool_scrub)
	PHP_FE(zpool_labelclear, arginfo_zpool_labelclear)
	PHP_FE(zpool_split, arginfo_zpool_split)
	PHP_FE(zpool_get_devs, arginfo_zpool_get_devs)
	PHP_FE(zpool_root_mount_from, NULL)
	PHP_FE(test1, NULL)
	{NULL, NULL, NULL}
};


zend_module_entry zfs_module_entry = {
	STANDARD_MODULE_HEADER,
	"zfs",
	zfs_functions,
	PHP_MINIT(zfs),
	PHP_MSHUTDOWN(zfs),
	NULL,
	NULL,
	NULL,
	"0.0.1",
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_ZFS
ZEND_GET_MODULE(zfs)
#endif

PHP_INI_BEGIN()
    PHP_INI_ENTRY("kcs.zfs.conf", "/etc/kinit.conf", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("kcs.zfs.snap.rules.cmd", "/etc/zfs/autosnap", PHP_INI_ALL, NULL)
    PHP_INI_ENTRY("kcs.zfs.snap.rules.file", "/etc/crontab", PHP_INI_ALL, NULL)
PHP_INI_END()

PHP_MINIT_FUNCTION(zfs)
{
	REGISTER_INI_ENTRIES();

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(zfs)
{
    UNREGISTER_INI_ENTRIES();

    return SUCCESS;
}

PHP_FUNCTION(test1)
{
	/*zval *ar;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &ar) == FAILURE) { 
	   return;
	}

	char *test = INI_STR("kcs.zfs.conf");
	if (test)
		printf("%s\n", test);
	char tmp[] = "pcache";
	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	HashTable *hash = Z_ARRVAL_P(ar);
	zval *data;
	HashPosition pointer;
	for(
        zend_hash_internal_pointer_reset_ex(hash, &pointer);
        (data = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
        zend_hash_move_forward_ex(hash, &pointer)
    ) {
		if (Z_TYPE_P(data) == IS_STRING) {
			zend_ulong num_index;
			zend_string *str_index;
			zend_hash_get_current_key_ex(hash, &str_index, &num_index, &pointer);
            printf("arr[%s]=[%s]\n", ZSTR_VAL(str_index), Z_STRVAL_P(data));
        }
	}

	libzfs_fini(g_zfs);*/

	/*char *zname;
	char *propname;
	size_t zname_len;
	size_t propname_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &zname, &zname_len, &propname, &propname_len) == FAILURE) { 
	   return;
	}

	int ret = zfs_delete_user_prop(zname, propname);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;*/


	zval *kek = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &kek) == FAILURE) { 
	   return;
	}


	if (Z_TYPE_P(kek) == IS_NULL)
		RETURN_FALSE;


	RETURN_TRUE;
}

PHP_FUNCTION(zfs_all_props)
{
	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	array_init(return_value);

	(void) zprop_iter(zfs_prop_cb, return_value, B_FALSE, B_TRUE,
		    ZFS_TYPE_DATASET);
    libzfs_fini(g_zfs);

}

PHP_FUNCTION(zpool_all_props)
{
	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	array_init(return_value);

	(void) zprop_iter(zpool_prop_cb, return_value, B_FALSE, B_TRUE,
		    ZFS_TYPE_POOL);
    libzfs_fini(g_zfs);

}
PHP_FUNCTION(zfs_unmount_all)
{
	zend_bool force = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|b", &force) == FAILURE) { 
	   return;
	}
	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	int ret = 0;
	int flags = 0;
	char nfs_mnt_prop[ZFS_MAXPROPLEN];
	char sharesmb[ZFS_MAXPROPLEN];
	if (force)
		flags = MS_FORCE;
	FILE *mnttab_file;

	if ((mnttab_file = fopen(MNTTAB, "r")) == NULL) {
	    libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	zfs_handle_t *zhp;
	struct mnttab entry;
	uu_avl_pool_t *pool;
	uu_avl_t *tree = NULL;
	unshare_unmount_node_t *node;
	uu_avl_index_t idx;
	uu_avl_walk_t *walk;

	if (((pool = uu_avl_pool_create("unmount_pool",
	    sizeof (unshare_unmount_node_t),
	    offsetof(unshare_unmount_node_t, un_avlnode),
	    unshare_unmount_compare, UU_DEFAULT)) == NULL) ||
	    ((tree = uu_avl_create(pool, NULL, UU_DEFAULT)) == NULL)) {
		ret = 1;
		goto out;
	}

	rewind(mnttab_file);
	while (getmntent(mnttab_file, &entry) == 0) {

		if (strcmp(entry.mnt_fstype, MNTTYPE_ZFS) != 0)
			continue;

		if (strchr(entry.mnt_special, '@') != NULL)
			continue;

		if ((zhp = zfs_open(g_zfs, entry.mnt_special,
		    ZFS_TYPE_FILESYSTEM)) == NULL) {
			ret = 1;
			continue;
		}

		if (zpool_skip_pool(zfs_get_pool_name(zhp))) {
			zfs_close(zhp);
			continue;
		}

		ret = zfs_prop_get(zhp, ZFS_PROP_MOUNTPOINT,
			    nfs_mnt_prop,
			    sizeof (nfs_mnt_prop),
			    NULL, NULL, 0, B_FALSE);
		if (ret) {
			zfs_close(zhp);
			fclose(mnttab_file);
			libzfs_fini(g_zfs);
			RETURN_FALSE;
		}
		if (strcmp(nfs_mnt_prop, "legacy") == 0)
			continue;
		if (zfs_prop_get_int(zhp, ZFS_PROP_CANMOUNT) ==
		    ZFS_CANMOUNT_NOAUTO)
			continue;

		node = malloc(sizeof (unshare_unmount_node_t));
		node->un_zhp = zhp;
		node->un_mountp = strdup(entry.mnt_mountp);

		uu_avl_node_init(node, &node->un_avlnode, pool);

		if (uu_avl_find(tree, node, NULL, &idx) == NULL) {
			uu_avl_insert(tree, node, idx);
		} else {
			zfs_close(node->un_zhp);
			free(node->un_mountp);
			free(node);
		}
	}

	if ((walk = uu_avl_walk_start(tree,
	    UU_WALK_REVERSE | UU_WALK_ROBUST)) == NULL) {
		ret = 1;
		goto out;
	}

	while ((node = uu_avl_walk_next(walk)) != NULL) {
		uu_avl_remove(tree, node);

		if (zfs_unmount(node->un_zhp,
		    node->un_mountp, flags) != 0)
			ret = 1;
		zfs_close(node->un_zhp);
		free(node->un_mountp);
		free(node);
	}

	uu_avl_walk_end(walk);
	uu_avl_destroy(tree);
	uu_avl_pool_destroy(pool);

out:
	fclose(mnttab_file);
    libzfs_fini(g_zfs);

	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfs_unmount)
{
	char *entry;
	size_t entry_len;
	zend_bool force = 0;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b", &entry, &entry_len, &force) == FAILURE) { 
	   return;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	int ret = 0;
	int flags = 0;

	if (force)
		flags = MS_FORCE;
	if (entry[0] == '/') {
		ret = unshare_unmount_path(entry, flags, B_FALSE);
		libzfs_fini(g_zfs);
		if (ret)
			RETURN_FALSE;
		RETURN_TRUE;
	}

	zfs_handle_t *zhp;
	char nfs_mnt_prop[ZFS_MAXPROPLEN];

	if ((zhp = zfs_open(g_zfs, entry,
	    ZFS_TYPE_FILESYSTEM)) == NULL) {
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	ret = zfs_prop_get(zhp, ZFS_PROP_MOUNTPOINT,
	    nfs_mnt_prop, sizeof (nfs_mnt_prop), NULL,
	    NULL, 0, B_FALSE);
	if (ret) {
		zfs_close(zhp);
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	if (strcmp(nfs_mnt_prop, "legacy") == 0) {
		ret = 1;
	} else if (!zfs_is_mounted(zhp, NULL)) {
		ret = 1;
	} else if (zfs_unmountall(zhp, flags) != 0) {
		ret = 1;
	}

	zfs_close(zhp);
    libzfs_fini(g_zfs);

	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfs_mount_all)
{
	zval *mntopts = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a", &mntopts) == FAILURE) { 
	   return;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	int ret = 0;
	char *options = NULL;
	int first = 1;
	if (mntopts != NULL) {
		HashTable *hash;
		zval *data;
		HashPosition pointer;

		hash = Z_ARRVAL_P(mntopts);

		for(
	        zend_hash_internal_pointer_reset_ex(hash, &pointer);
	        (data = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
	        zend_hash_move_forward_ex(hash, &pointer)
	    ) {
	    	if (Z_TYPE_P(data) == IS_STRING) {
				int len = 0;
				options = realloc(options, len + strlen(Z_STRVAL_P(data)) + 2);
				len = len + strlen(Z_STRVAL_P(data)) + 1;
				if (len >= MNT_LINE_MAX)
					if (!first) {
						free(options);
					    libzfs_fini(g_zfs);
						RETURN_FALSE;
					}

				if (first) {
					first = 0;
					snprintf(options, len, "%s", Z_STRVAL_P(data));
				} else {
					snprintf(options, len, "%s,%s", options, Z_STRVAL_P(data));
				}
	    	}
		}
	}
	int flags = 0;

#ifdef BSD112
	zfs_handle_t **dslist = NULL;
	size_t i, count = 0;
	char *protocol = NULL;
	get_all_datasets(g_zfs, &dslist, &count);
	if (count == 0) {
		if (!first)
			free(options);
	    libzfs_fini(g_zfs);
		RETURN_TRUE;
	}
	qsort(dslist, count, sizeof (void *), prop_cmp);

	for (i = 0; i < count; i++) {
		if (share_mount_one(dslist[i], OP_MOUNT, flags, protocol,
		    B_FALSE, options) != 0)
			ret = 1;
		zfs_close(dslist[i]);
	}

	free(dslist);
#elif defined(BSD113) || defined(BSD12)
    get_all_cb_t cb = { 0 };
    get_all_datasets(g_zfs, &cb, B_FALSE);

    if (cb.cb_used == 0) {
		if (!first)
			free(options);
        libzfs_fini(g_zfs);
		RETURN_FALSE;
    }

    share_mount_state_t share_mount_state = { 0 };
    share_mount_state.sm_op = OP_MOUNT;
    share_mount_state.sm_verbose = B_FALSE;
    share_mount_state.sm_flags = flags;
    share_mount_state.sm_options = options;
    share_mount_state.sm_proto = NULL;
    share_mount_state.sm_total = cb.cb_used;
    pthread_mutex_init(&share_mount_state.sm_lock, NULL);

    zfs_foreach_mountpoint(g_zfs, cb.cb_handles, cb.cb_used,
        share_mount_one_cb, &share_mount_state, 1);
    ret = share_mount_state.sm_status;

    for (int i = 0; i < cb.cb_used; i++)
        zfs_close(cb.cb_handles[i]);
    free(cb.cb_handles);
#endif
	if (!first)
		free(options);

    libzfs_fini(g_zfs);

	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfs_mount)
{
	char *zname;
	size_t zname_len;
	zval *mntopts = NULL;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|a", &zname, &zname_len, &mntopts) == FAILURE) { 
	   return;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	int ret = 0;
	char *options = NULL;
	int first = 1;
	if (mntopts != NULL) {
		HashTable *hash;
		zval *data;
		HashPosition pointer;

		hash = Z_ARRVAL_P(mntopts);

		for(
	        zend_hash_internal_pointer_reset_ex(hash, &pointer);
	        (data = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
	        zend_hash_move_forward_ex(hash, &pointer)
	    ) {
	    	if (Z_TYPE_P(data) == IS_STRING) {
				int len = 0;
				options = realloc(options, len + strlen(Z_STRVAL_P(data)) + 2);
				len = len + strlen(Z_STRVAL_P(data)) + 1;
				if (len >= MNT_LINE_MAX)
					if (!first) {
						free(options);
					    libzfs_fini(g_zfs);
						RETURN_FALSE;
					}

				if (first) {
					first = 0;
					snprintf(options, len, "%s", Z_STRVAL_P(data));
				} else {
					snprintf(options, len, "%s,%s", options, Z_STRVAL_P(data));
				}
	    	}
		}
	}
	zfs_handle_t *zhp;
	int flags = 0;

	if ((zhp = zfs_open(g_zfs, zname,
	    ZFS_TYPE_FILESYSTEM)) == NULL) {
		ret = 1;
	} else {
		ret = share_mount_one(zhp, OP_MOUNT, flags, NULL, B_TRUE,
		    options);
		zfs_close(zhp);
	}
		if (!first)
			free(options);
    libzfs_fini(g_zfs);

	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfs_mount_list)
{
	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	int ret = 0;
	array_init(return_value);
	struct mnttab entry;
	FILE *mnttab_file;

	if ((mnttab_file = fopen(MNTTAB, "r")) == NULL) {
	    libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	rewind(mnttab_file);
	while (getmntent(mnttab_file, &entry) == 0) {
		if (strcmp(entry.mnt_fstype, MNTTYPE_ZFS) != 0 ||
		    strchr(entry.mnt_special, '@') != NULL)
			continue;

		add_assoc_string(return_value, entry.mnt_special, entry.mnt_mountp);
		// (void) printf("%-30s  %s\n", entry.mnt_special,
		//     entry.mnt_mountp);
	}
	fclose(mnttab_file);
    libzfs_fini(g_zfs);

}

PHP_FUNCTION(zpool_add)
{
	char *zpool_name;
	zval *devs;
	size_t zpool_name_len;
	zend_bool force = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa|b", &zpool_name, &zpool_name_len, &devs, &force) == FAILURE) { 
	   return;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	int ret = 0;

	nvlist_t *nvroot;
	zpool_boot_label_t boot_type;
	uint64_t boot_size;
	zpool_handle_t *zhp;
	nvlist_t *config;

	if ((zhp = zpool_open(g_zfs, zpool_name)) == NULL) {
	    libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	if ((config = zpool_get_config(zhp, NULL)) == NULL) {
		zpool_close(zhp);
	    libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	if (zpool_is_bootable(zhp))
		boot_type = ZPOOL_COPY_BOOT_LABEL;
	else
		boot_type = ZPOOL_NO_BOOT_LABEL;

	boot_size = zpool_get_prop_int(zhp, ZPOOL_PROP_BOOTSIZE, NULL);
	nvroot = make_root_vdev(zhp, force, !force, B_FALSE, B_FALSE,
	    boot_type, boot_size, devs);
	if (nvroot == NULL) {
		zpool_close(zhp);
	    libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	ret = (zpool_add(zhp, nvroot) != 0);

	nvlist_free(nvroot);
	zpool_close(zhp);
    libzfs_fini(g_zfs);

	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zpool_remove)
{
	char *zpool_name;
	zval *devs;
	size_t zname_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa", &zpool_name, &zname_len, &devs) == FAILURE) { 
	   return;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	int i, ret = 0;
	zpool_handle_t *zhp;
	boolean_t stop = B_FALSE;
	boolean_t noop = B_FALSE;
	boolean_t parsable = B_FALSE;
	char c;

	if ((zhp = zpool_open(g_zfs, zpool_name)) == NULL) {
	    libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	HashTable *hash;
	zval *data;
	HashPosition pointer;

	hash = Z_ARRVAL_P(devs);

	for(
        zend_hash_internal_pointer_reset_ex(hash, &pointer);
        (data = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
        zend_hash_move_forward_ex(hash, &pointer)
    ) {
		if (zpool_vdev_remove(zhp, Z_STRVAL_P(data)) != 0)
			ret = 1;
	}
	
    libzfs_fini(g_zfs);

    if (ret)
    	RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfs_ds_create)
{
	char *zname;
	zval *params;
	size_t zname_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa", &zname, &zname_len, &params) == FAILURE) { 
	   return;
	}

	nvlist_t *props;
	zfs_type_t type = ZFS_TYPE_FILESYSTEM;
	zfs_handle_t *zhp = NULL;
	uint64_t volsize = 0;
	uint64_t intval;
	int mount = 0;

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}

	if (nvlist_alloc(&props, NV_UNIQUE_NAME, 0) != 0) {
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	zval *config;
	HashTable *hash;
	zval *data;
	HashPosition pointer;
	if ((config = zend_hash_str_find(Z_ARRVAL_P(params), "config", sizeof("config")-1)) != NULL && Z_TYPE_P(config) == IS_ARRAY) {
		hash = Z_ARRVAL_P(config);
		for(
	        zend_hash_internal_pointer_reset_ex(hash, &pointer);
	        (data = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
	        zend_hash_move_forward_ex(hash, &pointer)
	    ) {
			if (Z_TYPE_P(data) == IS_STRING) {
				zend_ulong num_index;
				zend_string *str_index;
				zend_hash_get_current_key_ex(hash, &str_index, &num_index, &pointer);
				zfs_prop_t prop_n = zfs_name_to_prop(ZSTR_VAL(str_index));
	            if (prop_n == ZFS_PROP_VOLSIZE) {
					char zname_tmp[strlen(zname) + 1];
					snprintf(zname_tmp, sizeof(zname_tmp), "%s", zname);
					type = ZFS_TYPE_VOLUME;
					if (zfs_nicestrtonum(g_zfs, Z_STRVAL_P(data), &intval) != 0) {
						nvlist_free(props);
						libzfs_fini(g_zfs);
						RETURN_FALSE;
					}
					if (nvlist_add_uint64(props, zfs_prop_to_name(ZFS_PROP_VOLSIZE), intval) != 0) {
						nvlist_free(props);
						libzfs_fini(g_zfs);
						RETURN_FALSE;
					}
					volsize = intval;

					if (nvlist_add_string(props, "volmode", "dev") != 0) {
						nvlist_free(props);
						libzfs_fini(g_zfs);
						RETURN_FALSE;
					}

					zpool_handle_t *zpool_handle;
					nvlist_t *real_props = NULL;
					uint64_t spa_version;
					char *p;
					zfs_prop_t resv_prop;
					char *strval;
					char msg[1024];
					if ((p = strchr(zname_tmp, '/')) != NULL)
						*p = '\0';
					zpool_handle = zpool_open(g_zfs, zname_tmp);
					if (p != NULL)
						*p = '/';
					if (zpool_handle == NULL) {
						nvlist_free(props);
						libzfs_fini(g_zfs);
						RETURN_FALSE;
					}
					spa_version = zpool_get_prop_int(zpool_handle,
					    ZPOOL_PROP_VERSION, NULL);
					if (spa_version >= SPA_VERSION_REFRESERVATION)
						resv_prop = ZFS_PROP_REFRESERVATION;
					else
						resv_prop = ZFS_PROP_RESERVATION;

					if (props && (real_props = zfs_valid_proplist(g_zfs, type,
					    props, 0, NULL, zpool_handle, msg)) == NULL) {
						zpool_close(zpool_handle);
						nvlist_free(props);
						libzfs_fini(g_zfs);
						RETURN_FALSE;
					}
					zpool_close(zpool_handle);

					volsize = zvol_volsize_to_reservation(volsize, real_props);
					nvlist_free(real_props);

					if (nvlist_lookup_string(props, zfs_prop_to_name(resv_prop),
					    &strval) != 0) {
						if (nvlist_add_uint64(props,
						    zfs_prop_to_name(resv_prop), volsize) != 0) {
							nvlist_free(props);
							libzfs_fini(g_zfs);
							RETURN_FALSE;
						}
					}
	            } else if (prop_n != ZPROP_INVAL) {
	            	if (prop_n  == ZFS_PROP_MOUNTPOINT) {
	            		mount = 1;
	            	}
	            	nvlist_add_string(props, zfs_prop_to_name(prop_n), Z_STRVAL_P(data));
	            }
	        }
	    }
	}

	if ((config = zend_hash_str_find(Z_ARRVAL_P(params), "ext", sizeof("ext")-1)) != NULL || Z_TYPE_P(config) == IS_ARRAY) {
		hash = Z_ARRVAL_P(config);
		for(
	        zend_hash_internal_pointer_reset_ex(hash, &pointer);
	        (data = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
	        zend_hash_move_forward_ex(hash, &pointer)
	    ) {
			zend_ulong num_index;
			zend_string *str_index;
			zend_hash_get_current_key_ex(hash, &str_index, &num_index, &pointer);

	    	nvlist_add_string(props, ZSTR_VAL(str_index), Z_STRVAL_P(data));
		}
	}
	if (zfs_create(g_zfs, zname, type, props) != 0) {
		nvlist_free(props);
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	if ((zhp = zfs_open(g_zfs, zname, ZFS_TYPE_DATASET)) == NULL) {
		nvlist_free(props);
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	if (mount == 1 && should_auto_mount(zhp)) {
		if (zfs_mount(zhp, NULL, 0) != 0) {
			nvlist_free(props);
			libzfs_fini(g_zfs);
			RETURN_FALSE;
		} else if (zfs_share(zhp) != 0) {
			nvlist_free(props);
			libzfs_fini(g_zfs);
			RETURN_FALSE;
		}
	}

	if (zhp)
		zfs_close(zhp);
	nvlist_free(props);
    libzfs_fini(g_zfs);
	RETURN_TRUE;
}

PHP_FUNCTION(zfs_ds_update)
{

	char *zname;
	zval *params;
	size_t zname_len;


	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa", &zname, &zname_len, &params) == FAILURE) { 
	   return;
	}

	nvlist_t *props;
	zfs_type_t type = ZFS_TYPE_FILESYSTEM;
	zfs_handle_t *zhp = NULL;

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}

	if ((zhp = zfs_open(g_zfs, zname, ZFS_TYPE_DATASET | ZFS_TYPE_VOLUME | ZFS_TYPE_SNAPSHOT)) == NULL) {
	    libzfs_fini(g_zfs);
    	RETURN_FALSE;
	}
	if (nvlist_alloc(&props, NV_UNIQUE_NAME, 0) != 0) {
		zfs_close(zhp);
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}
	zval *config;
	HashTable *hash;
	zval *data;
	HashPosition pointer;
	if ((config = zend_hash_str_find(Z_ARRVAL_P(params), "config", sizeof("config")-1)) != NULL && Z_TYPE_P(config) == IS_ARRAY) {
		hash = Z_ARRVAL_P(config);
		for(
	        zend_hash_internal_pointer_reset_ex(hash, &pointer);
	        (data = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
	        zend_hash_move_forward_ex(hash, &pointer)
	    ) {
			if (Z_TYPE_P(data) == IS_STRING) {
				zend_ulong num_index;
				zend_string *str_index;
				zend_hash_get_current_key_ex(hash, &str_index, &num_index, &pointer);
				zfs_prop_t prop_n = zfs_name_to_prop(ZSTR_VAL(str_index));
				if (prop_n != ZPROP_INVAL)
	            	nvlist_add_string(props, zfs_prop_to_name(prop_n), Z_STRVAL_P(data));
			}
		}
	}

	if ((config = zend_hash_str_find(Z_ARRVAL_P(params), "ext", sizeof("ext")-1)) != NULL && Z_TYPE_P(config) == IS_ARRAY) {
		hash = Z_ARRVAL_P(config);
		for(
	        zend_hash_internal_pointer_reset_ex(hash, &pointer);
	        (data = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
	        zend_hash_move_forward_ex(hash, &pointer)
	    ) {
			if (Z_TYPE_P(data) == IS_STRING) {
				zend_ulong num_index;
				zend_string *str_index;
				zend_hash_get_current_key_ex(hash, &str_index, &num_index, &pointer);

		    	nvlist_add_string(props, ZSTR_VAL(str_index), Z_STRVAL_P(data));
			}
		}
	}
	int ret = zfs_prop_set_list(zhp, props);
	if (zhp)
		zfs_close(zhp);
	nvlist_free(props);
    libzfs_fini(g_zfs);
    if (ret != 0)
    	RETURN_FALSE;
	RETURN_TRUE;
}


PHP_FUNCTION(zfs_ds_destroy)
{
	char *zname;
	zend_bool recursive = 0;
	size_t zname_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b", &zname, &zname_len, &recursive) == FAILURE) { 
	   return;
	}
	int ret = zfs_dataset_rb(zname, recursive);

	if (ret)
		RETURN_FALSE;
    RETURN_TRUE;
}

PHP_FUNCTION(zfs_snap_create)
{
	char *zname;
	char *snapname;
	char *desc;
	zend_bool recursive = 0;
	size_t zname_len;
	size_t snapname_len;
	size_t desc_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss|b", &zname, &zname_len, &snapname, &snapname_len, &desc, &desc_len, &recursive) == FAILURE) { 
	   return;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	libzfs_print_on_error(g_zfs, B_TRUE);
	libzfs_mnttab_cache(g_zfs, B_TRUE);
	nvlist_t *props;
	snap_cbdata_t sd = { 0 };
	if (nvlist_alloc(&props, NV_UNIQUE_NAME, 0) != 0) {
	    libzfs_fini(g_zfs);
		RETURN_FALSE;
	}
	if (nvlist_alloc(&sd.sd_nvl, NV_UNIQUE_NAME, 0) != 0) {
		nvlist_free(props);
	    libzfs_fini(g_zfs);
		RETURN_FALSE;
	}
	sd.sd_recursive = recursive;
	sd.sd_snapname = snapname;
	zfs_handle_t *zhp;
	zhp = zfs_open(g_zfs, zname, ZFS_TYPE_FILESYSTEM | ZFS_TYPE_VOLUME);
	if (zhp == NULL) {
		nvlist_free(sd.sd_nvl);
		nvlist_free(props);
	    libzfs_fini(g_zfs);
		RETURN_FALSE;
	}
	if (zfs_snapshot_cb(zhp, &sd) != 0) {
		nvlist_free(sd.sd_nvl);
		nvlist_free(props);
	    libzfs_fini(g_zfs);
    	RETURN_FALSE;
	}
    nvlist_add_string(props, "snap:desc", desc);
	int ret = zfs_snapshot_nvl(g_zfs, sd.sd_nvl, props);
	nvlist_free(sd.sd_nvl);
	nvlist_free(props);

	libzfs_mnttab_cache(g_zfs, B_FALSE);
    libzfs_fini(g_zfs);

    if (ret != 0)
    	RETURN_FALSE;
    RETURN_TRUE;
}

PHP_FUNCTION(zfs_snap_destroy)
{
	char *zname;
	char *snapname;
	zend_bool recursive = 0;
	size_t zname_len;
	size_t snapname_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|b", &zname, &zname_len, &snapname, &snapname_len, &recursive) == FAILURE) { 
	   return;
	}

	int ret = zfs_snapshot_rb(zname, snapname, recursive);
    if (ret != 0)
    	RETURN_FALSE;

    RETURN_TRUE;

}

PHP_FUNCTION(zfs_snap_update)
{
	char *zname;
	char *snapname;
	char *desc;

	size_t zname_len;
	size_t snapname_len;
	size_t desc_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss", &zname, &zname_len, &snapname, &snapname_len, &desc, &desc_len) == FAILURE) { 
	   return;
	}

	char snap[strlen(zname) + strlen(snapname) + 2];
	snprintf(snap, sizeof(snap), "%s@%s", zname, snapname);

	int ret = kzfs_set_uprop(snap, "snap:desc", desc);

	if (ret != 0)
		RETURN_FALSE;

    RETURN_TRUE;
}
PHP_FUNCTION(zfs_ds_props)
{
	char *zname = "";
	size_t zname_len = sizeof("")-1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &zname, &zname_len) == FAILURE) { 
	   return;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	zfs_handle_t *zhp = zfs_open(g_zfs, zname, ZFS_TYPE_FILESYSTEM | ZFS_TYPE_VOLUME);
	if (zhp != NULL) {
		array_init(return_value);
		nvlist_t *props_f = zfs_get_user_props(zhp);
		char *strval_f = NULL;
		nvpair_t *nvp_f = NULL;
		nvlist_t *nvlist_f = NULL;
		while ((nvp_f = nvlist_next_nvpair(props_f, nvp_f)) != NULL) {
			(void) nvpair_value_nvlist(nvp_f, &nvlist_f);
			nvpair_t *nvp = NULL;
			const char *propname_f = nvpair_name(nvp_f);
			while ((nvp = nvlist_next_nvpair(nvlist_f, nvp)) != NULL) {
			const char *name = nvpair_name(nvp);
			if (strcmp(name, "value") != 0)
				continue;
			(void) nvpair_value_string(nvp, &strval_f);
			add_assoc_string(return_value, propname_f, strval_f);
			}
		}
	} else {
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}
	

	zfs_close(zhp);
	libzfs_fini(g_zfs);
}
PHP_FUNCTION(zfs_ds_list)
{
	zval *params;
	char *zname = "";
	size_t zname_len = sizeof("")-1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|s", &params, &zname, &zname_len) == FAILURE) { 
	   return;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	zval *config;
	HashTable *hash;
	zval *data;
	HashPosition pointer;
	hash = Z_ARRVAL_P(params);

	callback_data_t cb = {0};
	char *tmp = NULL;
	zprop_list_t *pl = cb.cb_proplist;
	int len = 0;
	int first = 1;

	for(
        zend_hash_internal_pointer_reset_ex(hash, &pointer);
        (data = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
        zend_hash_move_forward_ex(hash, &pointer)
    ) {
		if (Z_TYPE_P(data) == IS_STRING) {
			zfs_prop_t prop_n = zfs_name_to_prop(Z_STRVAL_P(data));
			if (prop_n != ZPROP_INVAL) {
				cb.cb_props_table[prop_n] = B_TRUE;
			} else {
				tmp = realloc(tmp, len + strlen(Z_STRVAL_P(data)) + 2);
				len = len + strlen(Z_STRVAL_P(data)) + 1;
				if (first) {
					first = 0;
					snprintf(tmp, len, "%s", Z_STRVAL_P(data));
				} else {
					snprintf(tmp, len, "%s,%s", tmp, Z_STRVAL_P(data));
				}
				// zprop_list_t *pl_tmp = malloc(sizeof(zprop_list_t));
				// if (pl_tmp == NULL)
				// 	RETURN_FALSE;
				// pl_tmp->pl_user_prop = Z_STRVAL_P(data);
				// if (pl == NULL) {
				// 	pl = cb.cb_proplist = pl_tmp;
				// } else {
				// 	pl->pl_next = pl_tmp;
				// 	pl = pl_tmp;
				// }
			}
		}
	}
	if (!first)
		if (zprop_get_list(g_zfs, tmp, &cb.cb_proplist, ZFS_TYPE_DATASET)
		    != 0) {
			free(tmp);
			libzfs_fini(g_zfs);
			RETURN_FALSE;
		}

	array_init(return_value);
	cb.tmp = return_value;
	int ret = 0;
	if (zname[0]) {
		zfs_handle_t *zhp = zfs_open(g_zfs, zname, ZFS_TYPE_FILESYSTEM | ZFS_TYPE_VOLUME);
		if (zhp == NULL) {
			libzfs_fini(g_zfs);
			RETURN_FALSE;
		}
		zfs_callback(zhp, &cb);
	} else {
		zfs_iter_root(g_zfs, zfs_callback, &cb);
	}
	if (!first) {
		free(tmp);
	}
	zprop_free_list(cb.cb_proplist);
    libzfs_fini(g_zfs);
}

PHP_FUNCTION(zfs_ds_rename)
{
	char *zname;
	char *new_zname;
	zval *params = NULL;
	size_t zname_len, new_zname_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|z", &zname, &zname_len, &new_zname, &new_zname_len, &params) == FAILURE) {
	   return;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	int ret = 0;
	zfs_handle_t *zhp = NULL;
	renameflags_t flags = { 0 };
	int types;
	boolean_t parents = B_FALSE;
	zval *item;
	HashTable *ht;
	if (params != NULL) {
		ht = Z_ARRVAL_P(params);
		if ((item = zend_hash_str_find(ht, "force", sizeof("force")-1)) != NULL && Z_TYPE_P(item) == IS_TRUE) {
			flags.forceunmount = B_TRUE;
		}
		if ((item = zend_hash_str_find(ht, "nounmount", sizeof("nounmount")-1)) != NULL && Z_TYPE_P(item) == IS_TRUE) {
			flags.nounmount = B_TRUE;
		}
		if ((item = zend_hash_str_find(ht, "parents", sizeof("parents")-1)) != NULL && Z_TYPE_P(item) == IS_TRUE) {
			parents = B_TRUE;
		}
	}

	if (flags.nounmount && parents) {
		ret = 1;
		goto out;
	}

	if (flags.nounmount)
		types = ZFS_TYPE_FILESYSTEM;
	else if (parents)
		types = ZFS_TYPE_FILESYSTEM | ZFS_TYPE_VOLUME;
	else
		types = ZFS_TYPE_DATASET;

	if ((zhp = zfs_open(g_zfs, zname, types)) == NULL) {
		ret = 1;
		goto out;
	}

	if (parents && zfs_name_valid(new_zname, zfs_get_type(zhp)) &&
	    zfs_create_ancestors(g_zfs, new_zname) != 0) {
		ret = 1;
		goto out;
	}

	ret = (zfs_rename(zhp, NULL, new_zname, flags) != 0);

out:
	if (zhp)
		zfs_close(zhp);
    libzfs_fini(g_zfs);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfs_snap_rename)
{
	char *zname;
	char *snapname;
	char *new_snapname;
	zval *params = NULL;
	size_t zname_len, snapname_len, new_snapname_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss|z", &zname, &zname_len, &snapname, &snapname_len, &new_snapname, &new_snapname_len, &params) == FAILURE) {
	   return;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	int ret = 0;
	zfs_handle_t *zhp = NULL;
	renameflags_t flags = { 0 };
	int types;

	char tmp1[strlen(zname) + strlen(snapname) + 2];
	char tmp2[strlen(zname) + strlen(new_snapname) + 2];
	snprintf(tmp2, sizeof(tmp2), "%s@%s", zname, new_snapname);

	zval *item;
	HashTable *ht;
	if (params != NULL) {
		ht = Z_ARRVAL_P(params);
		if ((item = zend_hash_str_find(ht, "force", sizeof("force")-1)) != NULL && Z_TYPE_P(item) == IS_TRUE) {
			flags.forceunmount = B_TRUE;
		}
		if ((item = zend_hash_str_find(ht, "recursive", sizeof("recursive")-1)) != NULL && Z_TYPE_P(item) == IS_TRUE) {
			flags.recurse = B_TRUE;
			snprintf(tmp1, sizeof(tmp1), "%s", zname);
		} else {
			snprintf(tmp1, sizeof(tmp1), "%s@%s", zname, snapname);
			snapname = NULL;
		}
	}

	types = ZFS_TYPE_DATASET;

	if ((zhp = zfs_open(g_zfs, tmp1, types)) == NULL) {
	    libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	ret = (zfs_rename(zhp, snapname, tmp2, flags) != 0);

	zfs_close(zhp);
    libzfs_fini(g_zfs);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfs_snap_list)
{
	char *zname;
	size_t zname_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &zname, &zname_len) == FAILURE) { 
	   return;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	callback_data_t cb = {0};
	array_init(return_value);
	cb.cb_types = ZFS_TYPE_SNAPSHOT;
	cb.tmp = return_value;

	zfs_handle_t *zhp = zfs_open(g_zfs, zname,
	    ZFS_TYPE_FILESYSTEM);
	if (zhp == NULL) {
	    libzfs_fini(g_zfs);
		RETURN_FALSE;
	}
	zfs_callback(zhp, &cb);
    libzfs_fini(g_zfs);
}

PHP_FUNCTION(zfs_settings_merge)
{
	char *from;
	char *to;
	size_t from_len;
	size_t to_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &from, &from_len, &to, &to_len) == FAILURE) { 
	   return;
	}
	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}

	int ret = 0;
	char *at_f, *at_t;
	zfs_handle_t *zhp_f = NULL, *zhp_t = NULL;

	zfs_type_t type_f = ZFS_TYPE_DATASET, type_t = ZFS_TYPE_DATASET;
	if (strchr(from, '@') != NULL) {
		type_f = ZFS_TYPE_SNAPSHOT;
	}
	if (strchr(to, '@') != NULL) {
		type_t = ZFS_TYPE_SNAPSHOT;
	}

	if ((zhp_f = zfs_open(g_zfs, from, type_f)) == NULL) {
		ret = 1;
		goto out;
	}
	if ((zhp_t = zfs_open(g_zfs, to, type_t)) == NULL) {
		ret = 1;
		goto out;
	}

	nvlist_t *props_t = zfs_get_user_props(zhp_t);
	nvpair_t *nvp_t = NULL;

	while ((nvp_t = nvlist_next_nvpair(props_t, nvp_t)) != NULL) {
		const char *propname_t = nvpair_name(nvp_t);
		ret = zfs_prop_inherit(zhp_t, propname_t, B_FALSE);
	}

	nvlist_t *props_f = zfs_get_user_props(zhp_f);
	char *strval_f = NULL;
	nvpair_t *nvp_f = NULL;
	nvlist_t *nvlist_f = NULL;
	props_t = NULL;
    if ((ret = nvlist_alloc(&props_t, NV_UNIQUE_NAME, 0)) != 0) {
    	goto out;
    }

	while ((nvp_f = nvlist_next_nvpair(props_f, nvp_f)) != NULL) {
		(void) nvpair_value_nvlist(nvp_f, &nvlist_f);
		nvpair_t *nvp = NULL;
		const char *propname_f = nvpair_name(nvp_f);
		while ((nvp = nvlist_next_nvpair(nvlist_f, nvp)) != NULL) {
		const char *name = nvpair_name(nvp);
		if (strcmp(name, "value") != 0)
			continue;
		(void) nvpair_value_string(nvp, &strval_f);
		    if ((ret = nvlist_add_string(props_t, propname_f, strval_f)) != 0) {
		    	goto out;
		    }
		}
	}
    if ((ret = zfs_prop_set_list(zhp_t, props_t)) != 0) {
    	goto out;
    }
    
out:
    nvlist_free(props_t);
    if (zhp_t)
		zfs_close(zhp_t);
    if (zhp_f)
		zfs_close(zhp_f);
    libzfs_fini(g_zfs);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfs_snap_rollback)
{
	char *zname;
	char *snapname;

	size_t zname_len;
	size_t snapname_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &zname, &zname_len, &snapname, &snapname_len) == FAILURE) { 
	   return;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}

	char snap[strlen(zname) + strlen(snapname) + 2];
	snprintf(snap, sizeof(snap), "%s@%s", zname, snapname);

	zfs_handle_t *zhp_snap = zfs_open(g_zfs, snap,
	    ZFS_TYPE_SNAPSHOT);
	if (zhp_snap == NULL) {
	    libzfs_fini(g_zfs);
		RETURN_FALSE;
	}
	zfs_handle_t *zhp = zfs_open(g_zfs, zname,
	    ZFS_TYPE_FILESYSTEM);
	if (zhp == NULL) {
		zfs_close(zhp_snap);
	    libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	int ret = zfs_rollback(zhp, zhp_snap, B_TRUE);


	zfs_close(zhp_snap);
	zfs_close(zhp);
    libzfs_fini(g_zfs);
    if (ret)
    	RETURN_FALSE;
    RETURN_TRUE;
}

// PHP_FUNCTION(zfs_snap_rule_create)
// {
// 	char *zname;
// 	zval *sched;
// 	size_t zname_len;

// 	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa", &zname, &zname_len, &sched) == FAILURE) { 
// 	   return;
// 	}

// 	char cron_text[ZFS_CRON_RULE_LEN];
// 	snprintf(cron_text, sizeof(cron_text), "#%s:\n", zname);
// 	if (add_snap_rule(sched, cron_text, sizeof(cron_text), zname) != 0)
// 		RETURN_FALSE;
// 	char *cron_file = INI_STR("kcs.zfs.snap.rules.file");
// 	FILE *file = fopen(cron_file, "a");
// 	if (file == NULL)
// 		RETURN_FALSE;
// 	fprintf(file, "%s", cron_text);
// 	fclose(file);
// 	RETURN_TRUE;
// }

PHP_FUNCTION(zfs_snap_rule_delete)
{
	char *zname;
	size_t zname_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &zname, &zname_len) == FAILURE) { 
	   return;
	}

	char match_line[strlen(zname) + 3];
	snprintf(match_line, sizeof(match_line), "#%s:", zname);

	char *cron_file = INI_STR("kcs.zfs.snap.rules.file");
	if (remove_from_file(cron_file, match_line, 3) != 0)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zfs_snap_rule_set)
{
	char *zname;
	zval *sched;
	size_t zname_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa", &zname, &zname_len, &sched) == FAILURE) { 
	   return;
	}

	zval *item;
	char cron_text[ZFS_CRON_RULE_LEN];
	snprintf(cron_text, sizeof(cron_text), "#%s:", zname);
	if ((item = zend_hash_str_find(Z_ARRVAL_P(sched), "interval", sizeof("interval")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
		snprintf(cron_text, sizeof(cron_text), "%s%s\n", cron_text, Z_STRVAL_P(item));
	} else {
		RETURN_FALSE;
	}
	if (add_snap_rule(sched, cron_text, sizeof(cron_text), zname) != 0)
		RETURN_FALSE;
	char *cron_file = INI_STR("kcs.zfs.snap.rules.file");

	char match_line[strlen(zname) + 3];
	snprintf(match_line, sizeof(match_line), "#%s:", zname);
	remove_from_file(cron_file, match_line, 3);
	// if (remove_from_file(cron_file, match_line, 3) != 0)
	// 	RETURN_FALSE;
	FILE *file = fopen(cron_file, "a");
	if (file == NULL)
		RETURN_FALSE;
	fprintf(file, "%s", cron_text);
	fclose(file);

	RETURN_TRUE;
}

PHP_FUNCTION(zfs_snap_rule_get)
{
	char *zname;
	size_t zname_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &zname, &zname_len) == FAILURE) { 
	   return;
	}

	char create_line[ZFS_CRON_LINE_LEN];
	char delete_line[ZFS_CRON_LINE_LEN];
	char match_line[strlen(zname) + 3];
	snprintf(match_line, sizeof(match_line), "#%s:", zname);
	char *cron_file = INI_STR("kcs.zfs.snap.rules.file");
	char interval[32];
    FILE *file = fopen(cron_file, "r");
    int cnt = 0;
    if (file != NULL) {
        char str[ZFS_CRON_LINE_LEN];
        while(fscanf(file, "%[^\n]\n", str) != -1) {
            if (strncmp(match_line, str, strlen(match_line)) == 0) {
            		char *ch = strchr(str, ':');
            		*ch = '\0';
            		ch++;
            		snprintf(interval, sizeof(interval), "%s", ch);
                    cnt++;
                    continue;
            }
            if (cnt == 0)
            	continue;
            if (cnt == 1) {
            	snprintf(create_line, sizeof(create_line), "%s", str);
            	cnt++;
            	continue;
            }
            if (cnt == 2) {
            	snprintf(delete_line, sizeof(delete_line), "%s", str);
            	break;
            }
        }
        fclose(file);
    } else {
    	RETURN_FALSE;
    }

    if (cnt != 2) {
    	RETURN_FALSE;
    }
    array_init(return_value);
    add_assoc_string(return_value, "interval", interval);
    zval inner;
    array_init(&inner);

    char min[5], hour[9], mday[6], month[6], wday[14];
    sscanf(create_line, "%s %s %s %s %s", min, hour, mday, month, wday);

    add_assoc_string(&inner, "min", min);
    add_assoc_string(&inner, "hour", hour);
    add_assoc_string(&inner, "mday", mday);
    add_assoc_string(&inner, "month", month);
    add_assoc_string(&inner, "wday", wday);
    add_assoc_zval(return_value, "create", &inner);

    sscanf(delete_line, "%s %s %s %s %s", min, hour, mday, month, wday);

    array_init(&inner);
    add_assoc_string(&inner, "min", min);
    add_assoc_string(&inner, "hour", hour);
    add_assoc_string(&inner, "mday", mday);
    add_assoc_string(&inner, "month", month);
    add_assoc_string(&inner, "wday", wday);
    add_assoc_zval(return_value, "delete", &inner);

}

PHP_FUNCTION(zfs_send)
{
	char *zname;
	zval *params;
	zend_bool move = 0;
	size_t zname_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa|b", &zname, &zname_len, &params, &move) == FAILURE) { 
	   return;
	}

	char /**from_ip, */*remote_ip;
	long /*from_port, */remote_port;

	// zval *inner;
	// HashTable *hash;
	// zval *data;
	// HashPosition pointer;
	// if ((inner = zend_hash_str_find(Z_ARRVAL_P(params), "from", sizeof("from")-1)) != NULL && Z_TYPE_P(inner) == IS_ARRAY) {
	// 	zval *item;
	// 	if ((item = zend_hash_str_find(Z_ARRVAL_P(inner), "ip", sizeof("ip")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
	// 		from_ip = Z_STRVAL_P(item);
	// 	} else {
	// 		RETURN_FALSE;
	// 	}
	// 	if ((item = zend_hash_str_find(Z_ARRVAL_P(inner), "port", sizeof("port")-1)) != NULL && Z_TYPE_P(item) == IS_LONG) {
	// 		from_port = Z_LVAL_P(item);
	// 	} else {
	// 		RETURN_FALSE;
	// 	}
	// }

	// if ((inner = zend_hash_str_find(Z_ARRVAL_P(params), "remote", sizeof("remote")-1)) != NULL && Z_TYPE_P(inner) == IS_ARRAY) {
		zval *item;
		if ((item = zend_hash_str_find(Z_ARRVAL_P(params), "ip", sizeof("ip")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
			remote_ip = Z_STRVAL_P(item);
		} else {
			RETURN_FALSE;
		}
		if ((item = zend_hash_str_find(Z_ARRVAL_P(params), "port", sizeof("port")-1)) != NULL && Z_TYPE_P(item) == IS_LONG) {
			remote_port = Z_LVAL_P(item);
		} else {
			RETURN_FALSE;
		}
	// } else {
	// 	RETURN_FALSE;
	// }

	
	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	nvlist_t *props;
	snap_cbdata_t sd = { 0 };
	sd.sd_recursive = B_TRUE;

	if (nvlist_alloc(&props, NV_UNIQUE_NAME, 0) != 0) {
	    libzfs_fini(g_zfs);
		RETURN_FALSE;
	}
	if (nvlist_alloc(&sd.sd_nvl, NV_UNIQUE_NAME, 0) != 0) {
		nvlist_free(props);
	    libzfs_fini(g_zfs);
		RETURN_FALSE;
	}
	zfs_handle_t *zhp;
	zhp = zfs_open(g_zfs, zname,
	    ZFS_TYPE_FILESYSTEM | ZFS_TYPE_VOLUME);
	if (zhp == NULL) {
		nvlist_free(sd.sd_nvl);
		nvlist_free(props);
	    libzfs_fini(g_zfs);
		RETURN_FALSE;
	}
	sd.sd_snapname = "migrate";
	if (zfs_snapshot_cb(zhp, &sd) != 0) {
		nvlist_free(sd.sd_nvl);
		nvlist_free(props);
	    libzfs_fini(g_zfs);
    	RETURN_FALSE;
	}
	int ret = zfs_snapshot_nvl(g_zfs, sd.sd_nvl, props);

	nvlist_free(sd.sd_nvl);
	nvlist_free(props);

	zhp = zfs_open(g_zfs, zname,
	    ZFS_TYPE_FILESYSTEM | ZFS_TYPE_VOLUME);
	sendflags_t flags = { 0 };
	flags.replicate = B_TRUE;
	flags.doall = B_TRUE;

    int s;
    struct sockaddr_in servaddr, cli;
  
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1) {
    	RETURN_FALSE;
    }
    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(remote_ip);
    servaddr.sin_port = htons(remote_port);

    if (connect(s, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
    	close(s);
    	RETURN_FALSE;
    }

	int err = zfs_send(zhp, NULL, "migrate", &flags, s, NULL, 0,
	    NULL);
	zfs_close(zhp);
    libzfs_fini(g_zfs);
	close(s);
	zfs_snapshot_rb(zname, "migrate", 1);
	if (move) {
		zfs_dataset_rb(zname, 1);
	}

	if (err)
		RETURN_FALSE;

	RETURN_TRUE;
}

PHP_FUNCTION(zfs_recv)
{
	char *zname;
	zval *params;
	size_t zname_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa", &zname, &zname_len, &params) == FAILURE) { 
	   return;
	}

	char /**from_ip, */*local_ip;
	long /*from_port, */local_port;

	// zval *inner;
	// HashTable *hash;
	// zval *data;
	// HashPosition pointer;
	// if ((inner = zend_hash_str_find(Z_ARRVAL_P(params), "local", sizeof("local")-1)) != NULL && Z_TYPE_P(inner) == IS_ARRAY) {
	// 	zval *item;
	// 	if ((item = zend_hash_str_find(Z_ARRVAL_P(inner), "ip", sizeof("ip")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
	// 		from_ip = Z_STRVAL_P(item);
	// 	} else {
	// 		RETURN_FALSE;
	// 	}
	// 	if ((item = zend_hash_str_find(Z_ARRVAL_P(inner), "port", sizeof("port")-1)) != NULL && Z_TYPE_P(item) == IS_LONG) {
	// 		from_port = Z_LVAL_P(item);
	// 	} else {
	// 		RETURN_FALSE;
	// 	}
	// }

	// if ((inner = zend_hash_str_find(Z_ARRVAL_P(params), "local", sizeof("local")-1)) != NULL && Z_TYPE_P(inner) == IS_ARRAY) {
		zval *item;
		if ((item = zend_hash_str_find(Z_ARRVAL_P(params), "ip", sizeof("ip")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
			local_ip = Z_STRVAL_P(item);
		} else {
			RETURN_FALSE;
		}
		if ((item = zend_hash_str_find(Z_ARRVAL_P(params), "port", sizeof("port")-1)) != NULL && Z_TYPE_P(item) == IS_LONG) {
			local_port = Z_LVAL_P(item);
		} else {
			RETURN_FALSE;
		}
	// } else {
	// 	RETURN_FALSE;
	// }

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}

	nvlist_t *props;
	nvpair_t *nvp = NULL;
	recvflags_t flags = { 0 };
	flags.force = B_TRUE;

	if (nvlist_alloc(&props, NV_UNIQUE_NAME, 0) != 0) {
	    libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	while ((nvp = nvlist_next_nvpair(props, nvp))) {
		if (strcmp(nvpair_name(nvp), "origin") != 0) {
			nvlist_free(props);
		    libzfs_fini(g_zfs);
			RETURN_FALSE;
		}
	}


    int sockfd, connfd;
    unsigned int len; 
    struct sockaddr_in servaddr, cli; 
  
    sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd == -1) { 
		nvlist_free(props);
	    libzfs_fini(g_zfs);
	    RETURN_FALSE;
    } 
    bzero(&servaddr, sizeof(servaddr)); 
  
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = inet_addr(local_ip);
    servaddr.sin_port = htons(local_port); 
  
    if ((bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) != 0) { 
    	close(sockfd);
		nvlist_free(props);
	    libzfs_fini(g_zfs);
	    RETURN_FALSE;
    } 
  
    if ((listen(sockfd, 1)) != 0) { 
    	close(sockfd);
		nvlist_free(props);
	    libzfs_fini(g_zfs);
	    RETURN_FALSE;
    }
    len = sizeof(cli); 
  
    connfd = accept(sockfd, (struct sockaddr*)&cli, &len); 
    if (connfd < 0) {
    	close(sockfd);
		nvlist_free(props);
	    libzfs_fini(g_zfs);
	    RETURN_FALSE;
    }

	int err = zfs_receive(g_zfs, zname, props, &flags, connfd, NULL);
    libzfs_fini(g_zfs);
    close(sockfd); 
	zfs_snapshot_rb(zname, "migrate", 1);

    if (err)
    	RETURN_FALSE;
	RETURN_TRUE;
}


PHP_FUNCTION(zpool_create)
{
	char *zpool_name;
	zval *params = NULL;
	size_t zpool_name_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa", &zpool_name, &zpool_name_len, &params) == FAILURE) { 
	   return;
	}

	if (strchr(zpool_name, '/') != NULL)
		RETURN_FALSE;

	zval *inner;
	if ((inner = zend_hash_str_find(Z_ARRVAL_P(params), "devs", sizeof("devs")-1)) == NULL || Z_TYPE_P(inner) != IS_ARRAY) {
		RETURN_FALSE;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}

	nvlist_t *fsprops = NULL;
	nvlist_t *props = NULL;
	nvlist_t *nvroot = NULL;
	int ret = 0;
	boolean_t force = B_FALSE;
	boolean_t dryrun = B_FALSE;
	boolean_t enable_all_pool_feat = B_TRUE;
	char *altroot = NULL;
	char *mountpoint = NULL;
	zpool_boot_label_t boot_type = ZPOOL_NO_BOOT_LABEL;
	uint64_t boot_size = 0;
	char *tname = NULL;

	HashTable *ht = Z_ARRVAL_P(params);
	HashPosition pointer;
	zval *item;

	if ((item = zend_hash_str_find(ht, "force", sizeof("force")-1)) != NULL && Z_TYPE_P(item) == IS_TRUE) {
		force = B_TRUE;
	}
	if ((item = zend_hash_str_find(ht, "dryrun", sizeof("dryrun")-1)) != NULL && Z_TYPE_P(item) == IS_TRUE) {
		dryrun = B_TRUE;
	}
	if ((item = zend_hash_str_find(ht, "no_feat", sizeof("no_feat")-1)) != NULL && Z_TYPE_P(item) == IS_TRUE) {
		enable_all_pool_feat = B_FALSE;
	}
	if ((item = zend_hash_str_find(ht, "root", sizeof("root")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
		altroot = Z_STRVAL_P(item);
		if (add_prop_list(zpool_prop_to_name(
		    ZPOOL_PROP_ALTROOT), Z_STRVAL_P(item), &props, B_TRUE)) {
			ret = 1;
			goto error;
		}
		if (add_prop_list_default(zpool_prop_to_name(
		    ZPOOL_PROP_CACHEFILE), "none", &props, B_TRUE)) {
			ret = 1;
			goto error;
		}
	}
	if ((item = zend_hash_str_find(ht, "mountpoint", sizeof("mountpoint")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
		mountpoint = Z_STRVAL_P(item);
	}
	if ((item = zend_hash_str_find(ht, "props", sizeof("props")-1)) != NULL && Z_TYPE_P(item) == IS_ARRAY) {
		HashTable *ht_item = Z_ARRVAL_P(item);
		zval *data;
		for(
	        zend_hash_internal_pointer_reset_ex(ht_item, &pointer);
	        (data = zend_hash_get_current_data_ex(ht_item, &pointer)) != NULL;
	        zend_hash_move_forward_ex(ht_item, &pointer)
	    ) {
			if (Z_TYPE_P(data) == IS_STRING) {
				zend_ulong num_index;
				zend_string *str_index;
				zend_hash_get_current_key_ex(ht_item, &str_index, &num_index, &pointer);
				if (add_prop_list(ZSTR_VAL(str_index), Z_STRVAL_P(data),
				    &props, B_TRUE)) {
					ret = 1;
					goto error;
				}
				if (zpool_name_to_prop(ZSTR_VAL(str_index)) == ZPOOL_PROP_BOOTSIZE) {
					if (zfs_nicestrtonum(g_zfs, Z_STRVAL_P(data),
					    &boot_size) < 0 || boot_size == 0) {
						ret = 1;
						goto error;
					}
				}
				if (zpool_name_to_prop(ZSTR_VAL(str_index)) == ZPOOL_PROP_VERSION) {
					char *end;
					u_longlong_t ver;

					ver = strtoull(Z_STRVAL_P(data), &end, 10);
					if (*end == '\0' &&
					    ver < SPA_VERSION_FEATURES) {
						enable_all_pool_feat = B_FALSE;
					}
				}
				if (zpool_name_to_prop(ZSTR_VAL(str_index)) == ZPOOL_PROP_ALTROOT)
					altroot = Z_STRVAL_P(data);
			}
		}
	}
	if ((item = zend_hash_str_find(ht, "fs_props", sizeof("fs_props")-1)) != NULL && Z_TYPE_P(item) == IS_ARRAY) {
		HashTable *ht_item = Z_ARRVAL_P(item);
		zval *data;
		for(
	        zend_hash_internal_pointer_reset_ex(ht_item, &pointer);
	        (data = zend_hash_get_current_data_ex(ht_item, &pointer)) != NULL;
	        zend_hash_move_forward_ex(ht_item, &pointer)
	    ) {
			if (Z_TYPE_P(data) == IS_STRING) {
				zend_ulong num_index;
				zend_string *str_index;
				zend_hash_get_current_key_ex(ht_item, &str_index, &num_index, &pointer);
				if (0 == strcmp(ZSTR_VAL(str_index),
				    zfs_prop_to_name(ZFS_PROP_MOUNTPOINT))) {
					mountpoint = Z_STRVAL_P(data);
				} else if (add_prop_list(ZSTR_VAL(str_index), Z_STRVAL_P(data), &fsprops,
				    B_FALSE)) {
					ret = 1;
					goto error;
				}
			}
		}
	}
	if ((item = zend_hash_str_find(ht, "temp_name", sizeof("temp_name")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
		if (strchr(Z_STRVAL_P(item), '/') != NULL) {
			ret = 1;
			goto error;
		}
		if (add_prop_list(zpool_prop_to_name(
		    ZPOOL_PROP_TNAME), Z_STRVAL_P(item), &props, B_TRUE)) {
			ret = 1;
			goto error;
		}
		if (add_prop_list_default(zpool_prop_to_name(
		    ZPOOL_PROP_CACHEFILE), "none", &props, B_TRUE)) {
			ret = 1;
			goto error;
		}
		tname = Z_STRVAL_P(item);
	}

	if (boot_type == ZPOOL_CREATE_BOOT_LABEL) {
		const char *propname;
		char *strptr, *buf = NULL;
		int rv;

		propname = zpool_prop_to_name(ZPOOL_PROP_BOOTSIZE);
		if (nvlist_lookup_string(props, propname, &strptr) != 0) {
			(void) asprintf(&buf, "%" PRIu64, boot_size);
			if (buf == NULL) {
				ret = 1;
				goto error;
			}
			rv = add_prop_list(propname, buf, &props, B_TRUE);
			free(buf);
			if (rv != 0) {
				ret = 1;
				goto error;
			}
		}
	} else {
		const char *propname;
		char *strptr;

		propname = zpool_prop_to_name(ZPOOL_PROP_BOOTSIZE);
		if (nvlist_lookup_string(props, propname, &strptr) == 0) {
			ret = 1;
			goto error;
		}
	}
	nvroot = make_root_vdev(NULL, force, !force, B_FALSE, dryrun,
	    boot_type, boot_size, inner);
	if (nvroot == NULL) {
		ret = 1;
		goto error;
	}

	if (!zfs_allocatable_devs(nvroot)) {
		ret = 1;
		goto error;
	}
	if (altroot != NULL && altroot[0] != '/') {
		ret = 1;
		goto error;
	}
	if (!force && (mountpoint == NULL ||
	    (strcmp(mountpoint, ZFS_MOUNTPOINT_LEGACY) != 0 &&
	    strcmp(mountpoint, ZFS_MOUNTPOINT_NONE) != 0))) {
		char buf[MAXPATHLEN];
		DIR *dirp;

		if (mountpoint && mountpoint[0] != '/') {
			ret = 1;
			goto error;
		}

		if (mountpoint == NULL) {
			if (altroot != NULL)
				(void) snprintf(buf, sizeof (buf), "%s/%s",
				    altroot, zpool_name);
			else
				(void) snprintf(buf, sizeof (buf), "/%s",
				    zpool_name);
		} else {
			if (altroot != NULL)
				(void) snprintf(buf, sizeof (buf), "%s%s",
				    altroot, mountpoint);
			else
				(void) snprintf(buf, sizeof (buf), "%s",
				    mountpoint);
		}

		if ((dirp = opendir(buf)) == NULL && errno != ENOENT) {
			ret = 1;
			goto error;
		} else if (dirp) {
			int count = 0;

			while (count < 3 && readdir(dirp) != NULL)
				count++;
			(void) closedir(dirp);

			if (count > 2) {
				ret = 1;
				goto error;
			}
		}
	}

	if (mountpoint != NULL) {
		ret = add_prop_list(zfs_prop_to_name(ZFS_PROP_MOUNTPOINT),
		    mountpoint, &fsprops, B_FALSE);
		if (ret != 0)
			goto error;
	}

	ret = 1;
	if (enable_all_pool_feat) {
		spa_feature_t i;
		for (i = 0; i < SPA_FEATURES; i++) {
			char propname[MAXPATHLEN];
			zfeature_info_t *feat = &spa_feature_table[i];

			(void) snprintf(propname, sizeof (propname),
			    "feature@%s", feat->fi_uname);

			if (nvlist_exists(props, propname))
				continue;

			ret = add_prop_list(propname,
			    ZFS_FEATURE_ENABLED, &props, B_TRUE);
			if (ret != 0)
				goto error;
		}
	}

	ret = 1;
	if (zpool_create(g_zfs, zpool_name,
	    nvroot, props, fsprops) == 0) {
		zfs_handle_t *pool = zfs_open(g_zfs,
		    tname ? tname : zpool_name, ZFS_TYPE_FILESYSTEM);
		if (pool != NULL) {
			if (zfs_mount(pool, NULL, 0) == 0)
				ret = zfs_shareall(pool);
			zfs_close(pool);
		}
	}

error:

	nvlist_free(nvroot);
	nvlist_free(fsprops);
	nvlist_free(props);
    libzfs_fini(g_zfs);
    if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zpool_destroy)
{
	char *zpool_name;
	size_t zpool_name_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &zpool_name, &zpool_name_len) == FAILURE) { 
	   return;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	zpool_handle_t *zhp;

	if ((zhp = zpool_open_canfail(g_zfs, zpool_name)) == NULL) {
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	if (zpool_disable_datasets(zhp, B_TRUE) != 0) {
		zpool_close(zhp);
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	int ret = zpool_destroy(zhp, NULL);

	libzfs_fini(g_zfs);
	if (ret)
		RETURN_FALSE;

	RETURN_TRUE;

}

PHP_FUNCTION(zpool_export)
{
	char *zpool_name;
	zend_bool force = 0;
	size_t zpool_name_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b", &zpool_name, &zpool_name_len, &force) == FAILURE) { 
	   return;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	zpool_handle_t *zhp;

	if ((zhp = zpool_open_canfail(g_zfs, zpool_name)) == NULL) {
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	if (zpool_disable_datasets(zhp, force) != 0) {
		zpool_close(zhp);
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	int ret = 0;
	ret = zpool_export(zhp, force, NULL);
	libzfs_fini(g_zfs);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zpool_online)
{
	char *zpool_name;
	zval *devs = NULL;
	zend_bool expand = 0;
	size_t zpool_name_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa|b", &zpool_name, &zpool_name_len, &devs, &expand) == FAILURE) { 
	   return;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}

	zpool_handle_t *zhp;

	if ((zhp = zpool_open(g_zfs, zpool_name)) == NULL) {
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}
	int ret = 0;
	vdev_state_t newstate;

	zval *config;
	HashTable *hash;
	zval *data;
	HashPosition pointer;
	hash = Z_ARRVAL_P(devs);

	int flags = 0;
	if (expand)
		flags |= ZFS_ONLINE_EXPAND;
	for(
        zend_hash_internal_pointer_reset_ex(hash, &pointer);
        (data = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
        zend_hash_move_forward_ex(hash, &pointer)
    ) {
		if (Z_TYPE_P(data) == IS_STRING) {
			if (zpool_vdev_online(zhp, Z_STRVAL_P(data), flags, &newstate) != 0) {
				ret = 1;
			}
		}
	}

	libzfs_fini(g_zfs);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zpool_root_mount_from)
{
	char val[KENV_MVALLEN + 1];
	if(!kenv(KENV_GET, "vfs.root.mountfrom", val, sizeof(val)))
		RETURN_FALSE;
	RETURN_STRING(val);
}

PHP_FUNCTION(zpool_offline)
{
	char *zpool_name;
	zval *devs = NULL;
	zend_bool temp = 0;
	size_t zpool_name_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa|b", &zpool_name, &zpool_name_len, &devs, &temp) == FAILURE) { 
	   return;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}

	zpool_handle_t *zhp;

	if ((zhp = zpool_open(g_zfs, zpool_name)) == NULL) {
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}
	int ret = 0;
	vdev_state_t newstate;

	zval *config;
	HashTable *hash;
	zval *data;
	HashPosition pointer;
	hash = Z_ARRVAL_P(devs);

	for(
        zend_hash_internal_pointer_reset_ex(hash, &pointer);
        (data = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
        zend_hash_move_forward_ex(hash, &pointer)
    ) {
		if (Z_TYPE_P(data) == IS_STRING) {
			if (zpool_vdev_offline(zhp, Z_STRVAL_P(data), temp) != 0) {
				ret = 1;
			}
		}
	}

	libzfs_fini(g_zfs);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zpool_list)
{
	zval *params = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &params) == FAILURE) { 
	   return;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	zval *config;
	HashTable *hash;
	zval *data;
	zpool_cb_data_t cb = {0};
	HashPosition pointer;
	if (params != NULL && Z_TYPE_P(params) == IS_ARRAY) {
		hash = Z_ARRVAL_P(params);
		for(
	        zend_hash_internal_pointer_reset_ex(hash, &pointer);
	        (data = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
	        zend_hash_move_forward_ex(hash, &pointer)
	    ) {
			if (Z_TYPE_P(data) == IS_STRING) {
				zpool_prop_t prop_n = zpool_name_to_prop(Z_STRVAL_P(data));
				if (prop_n != ZPOOL_PROP_INVAL) {
					cb.cb_props_table[prop_n] = B_TRUE;
				}
			}
		}
	}
	array_init(return_value);
	cb.tmp = return_value;
	int ret = zpool_iter(g_zfs, zpool_callback, &cb);
    libzfs_fini(g_zfs);

}

PHP_FUNCTION(zpool_get_devs)
{
	char *zpool_name;
	size_t zpool_name_len;
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &zpool_name, &zpool_name_len) == FAILURE) { 
	   return;
	}
	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	int ret = 0;

	zpool_handle_t *zhp = NULL;

	if ((zhp = zpool_open(g_zfs, zpool_name)) == NULL) {
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}
	nvlist_t *config;
	config = zpool_get_config(zhp, NULL);

	nvlist_t *nv;
	nvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE,
		    &nv);
	array_init(return_value);
	zpool_devs_list(zhp, nv, NULL, 0, return_value);
	zpool_close(zhp);
	libzfs_fini(g_zfs);
	if (ret)
		RETURN_FALSE;
}

PHP_FUNCTION(zpool_split)
{
	char *zpool_name, *new_zpool_name;
	size_t zpool_name_len, new_zpool_name_len;
	zval *params = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|z", &zpool_name, &zpool_name_len, &new_zpool_name, &new_zpool_name_len, &params) == FAILURE) { 
	   return;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}

	char *propval;
	char *mntopts = NULL;
	splitflags_t flags;
	int c, ret = 0;
	zpool_handle_t *zhp = NULL;
	nvlist_t *config, *props = NULL;
	flags.dryrun = B_FALSE;
	flags.import = B_FALSE;

	HashTable *ht;
	HashPosition pointer;
	zval *item;
	int first = 1;

	zval *devs = NULL;
	if (params != NULL) {
		ht = Z_ARRVAL_P(params);
		if ((devs = zend_hash_str_find(Z_ARRVAL_P(params), "devs", sizeof("devs")-1)) != NULL) {
			if (Z_TYPE_P(devs) != IS_ARRAY) {
				libzfs_fini(g_zfs);
				RETURN_FALSE;
			}
		}
		if ((item = zend_hash_str_find(ht, "root", sizeof("root")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
			flags.import = B_TRUE;
			if (add_prop_list(zpool_prop_to_name(
			    ZPOOL_PROP_ALTROOT), Z_STRVAL_P(item), &props, B_TRUE)) {
				nvlist_free(props);
				ret = 1;
				goto out;
			}
		}
		if ((item = zend_hash_str_find(ht, "mntopts", sizeof("mntopts")-1)) != NULL && Z_TYPE_P(item) == IS_ARRAY) {
			HashTable *ht_item = Z_ARRVAL_P(item);
			zval *data;
			int len = 0;
			for(
		        zend_hash_internal_pointer_reset_ex(ht_item, &pointer);
		        (data = zend_hash_get_current_data_ex(ht_item, &pointer)) != NULL;
		        zend_hash_move_forward_ex(ht_item, &pointer)
		    ) {
				if (Z_TYPE_P(data) == IS_STRING) {
					mntopts = realloc(mntopts, len + strlen(Z_STRVAL_P(data)) + 2);
					len = len + strlen(Z_STRVAL_P(data)) + 1;
					if (first) {
						first = 0;
						snprintf(mntopts, len, "%s", Z_STRVAL_P(data));
					} else {
						snprintf(mntopts, len, "%s,%s", mntopts, Z_STRVAL_P(data));
					}
				}
			}
		}
		if ((item = zend_hash_str_find(ht, "props", sizeof("props")-1)) != NULL && Z_TYPE_P(item) == IS_ARRAY) {
			HashTable *ht_item = Z_ARRVAL_P(item);
			zval *data;
			for(
		        zend_hash_internal_pointer_reset_ex(ht_item, &pointer);
		        (data = zend_hash_get_current_data_ex(ht_item, &pointer)) != NULL;
		        zend_hash_move_forward_ex(ht_item, &pointer)
		    ) {
				if (Z_TYPE_P(data) == IS_STRING) {
					zend_ulong num_index;
					zend_string *str_index;
					zend_hash_get_current_key_ex(ht_item, &str_index, &num_index, &pointer);
					if (add_prop_list(ZSTR_VAL(str_index), Z_STRVAL_P(data),
					    &props, B_TRUE)) {
						nvlist_free(props);
						ret = 1;
						goto out;
					}
				}
			}
		}
	}

	if (!flags.import && mntopts != NULL) {
		ret = 1;
		goto out;
	}

	if ((zhp = zpool_open(g_zfs, zpool_name)) == NULL) {
		ret = 1;
		goto out;
	}

	config = split_mirror_vdev(zhp, new_zpool_name, props, flags, devs);

	if (config == NULL) {
		ret = 1;
	} else {
		nvlist_free(config);
	}
	zpool_close(zhp);

	if (ret != 0 || !flags.import) {
		goto out;
	}

	if ((zhp = zpool_open_canfail(g_zfs, new_zpool_name)) == NULL) {
		ret = 1;
		goto out;
	}
	if (zpool_get_state(zhp) != POOL_STATE_UNAVAIL &&
	    zpool_enable_datasets(zhp, mntopts, 0) != 0) {
		ret = 1;
	}
	zpool_close(zhp);

out:
	if (!first) {
		free(mntopts);
	}
	libzfs_fini(g_zfs);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zpool_reguid)
{
	char *zpool_name;
	size_t zpool_name_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &zpool_name, &zpool_name_len) == FAILURE) { 
	   return;
	}
	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}

	int ret = 0;

	libzfs_fini(g_zfs);

	zpool_handle_t *zhp;

	if ((zhp = zpool_open(g_zfs, zpool_name)) == NULL) {
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	ret = zpool_reguid(zhp);

	zpool_close(zhp);
	libzfs_fini(g_zfs);

	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zpool_reopen)
{
	char *zpool_name;
	size_t zpool_name_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &zpool_name, &zpool_name_len) == FAILURE) { 
	   return;
	}
	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}

	int ret = 0;
	zpool_handle_t *zhp;
	if ((zhp = zpool_open_canfail(g_zfs, zpool_name)) == NULL) {
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	ret = zpool_reopen(zhp);
	zpool_close(zhp);
	libzfs_fini(g_zfs);

	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zpool_labelclear)
{
	char *dev;
	size_t dev_len;
	zend_bool force = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|b", &dev, &dev_len, &force) == FAILURE) { 
	   return;
	}
	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	int ret = 0, fd;
	struct stat st;
	char vdev[MAXPATHLEN];
	nvlist_t *config;
	boolean_t inuse = B_FALSE;
	char *name = NULL;
	pool_state_t state;

	(void) strlcpy(vdev, dev, sizeof (vdev));
	if (vdev[0] != '/' && stat(vdev, &st) != 0) {
		(void) snprintf(vdev, sizeof (vdev), "%s/%s",
			"/dev", dev);
		if (stat(vdev, &st) != 0) {
			libzfs_fini(g_zfs);
			RETURN_FALSE;
		}
	}

	if ((fd = open(vdev, O_RDWR)) < 0) {
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}
	if (zpool_read_label(fd, &config) != 0) {
		(void) close(fd);
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}
	nvlist_free(config);
	ret = zpool_in_use(g_zfs, fd, &state, &name, &inuse);
	if (ret != 0) {
		(void) close(fd);
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}
	if (!inuse)
		goto wipe_label;

	switch (state) {
	default:
	case POOL_STATE_ACTIVE:
	case POOL_STATE_SPARE:
	case POOL_STATE_L2CACHE:
	case POOL_STATE_DESTROYED:
		ret = 1;
		goto errout;
	case POOL_STATE_EXPORTED:
	case POOL_STATE_POTENTIALLY_ACTIVE:
		if (force)
			break;
		ret = 1;
		goto errout;
	}

wipe_label:
	ret = zpool_clear_label(fd);

errout:
	free(name);
	(void) close(fd);
	libzfs_fini(g_zfs);

	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;

}

PHP_FUNCTION(zpool_scrub)
{
	zval *zpool_list;
	zend_long cmd = PHP_ZPOOL_SCRUB_START;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z|l", &zpool_list, &cmd) == FAILURE) { 
	   return;
	}
	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}

	int ret = 0;
	scrub_cbdata_t cb;
	cb.cb_type = POOL_SCAN_SCRUB;
	cb.cb_scrub_cmd = POOL_SCRUB_NORMAL;
	if (cmd == PHP_ZPOOL_SCRUB_STOP)
		cb.cb_type = POOL_SCAN_NONE;
	if (cmd == PHP_ZPOOL_SCRUB_PAUSE)
		cb.cb_scrub_cmd = POOL_SCRUB_PAUSE;

	HashTable *hash;
	zval *data;
	HashPosition pointer;
	if (Z_TYPE_P(zpool_list) != IS_NULL) {
		hash = Z_ARRVAL_P(zpool_list);

		for(
	        zend_hash_internal_pointer_reset_ex(hash, &pointer);
	        (data = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
	        zend_hash_move_forward_ex(hash, &pointer)
	    ) {
			zpool_handle_t *zhp;

			if ((zhp = zpool_open_canfail(g_zfs, Z_STRVAL_P(data))) != NULL) {
		    	ret = zpool_scrub_callback(zhp, &cb);
		    	zpool_close(zhp);
		    }

		}
	} else {
		ret = 1;
	}

	libzfs_fini(g_zfs);

	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zpool_clear)
{
	char *zpool_name;
	char *dev = "";
	zend_bool do_rewind = 0;
	size_t zpool_name_len;
	size_t dev_len = sizeof("") - 1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|bs", &zpool_name, &zpool_name_len, &do_rewind, &dev, &dev_len) == FAILURE) { 
	   return;
	}
	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	int ret = 0;
	nvlist_t *policy = NULL;
	zpool_handle_t *zhp;

	uint32_t rewind_policy = ZPOOL_NO_REWIND;
	if (do_rewind)
		rewind_policy = ZPOOL_DO_REWIND;

	if (nvlist_alloc(&policy, NV_UNIQUE_NAME, 0) != 0 ||
	    nvlist_add_uint32(policy, ZPOOL_LOAD_REWIND_POLICY,
	    rewind_policy) != 0) {
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	if ((zhp = zpool_open_canfail(g_zfs, zpool_name)) == NULL) {
		nvlist_free(policy);
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}
	
	ret = zpool_clear(zhp, dev[0] ? dev : NULL, policy);

	zpool_close(zhp);
	nvlist_free(policy);
	libzfs_fini(g_zfs);

	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zpool_update)
{
	char *zpool_name;
	size_t zpool_name_len;
	zval *params;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa", &zpool_name, &zpool_name_len, &params) == FAILURE) { 
	   return;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	zpool_handle_t *zhp;

	if ((zhp = zpool_open(g_zfs, zpool_name)) == NULL) {
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}
	zval *config;
	HashTable *hash;
	zval *data;
	HashPosition pointer;
	hash = Z_ARRVAL_P(params);
	int ret = 0;

	for(
        zend_hash_internal_pointer_reset_ex(hash, &pointer);
        (data = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
        zend_hash_move_forward_ex(hash, &pointer)
    ) {
		if (Z_TYPE_P(data) == IS_STRING) {
			zend_ulong num_index;
			zend_string *str_index;
			zend_hash_get_current_key_ex(hash, &str_index, &num_index, &pointer);
			zpool_prop_t prop_n = zpool_name_to_prop(ZSTR_VAL(str_index));
			if (prop_n != ZPROP_INVAL)
            	ret = zpool_set_prop(zhp, ZSTR_VAL(str_index), Z_STRVAL_P(data));
		}
	}
	zpool_close(zhp);
    libzfs_fini(g_zfs);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zpool_import)
{
	char *zpool_name;
	size_t zpool_name_len;
	zval *params = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|a", &zpool_name, &zpool_name_len, &params) == FAILURE) { 
	   return;
	}

	int ret = 0;
	zpool_imp_cb_t cb = { 0 };
	cb.cb_type = 0;
	ret = zpool_import_callback(zpool_name, params, &cb);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;

}

PHP_FUNCTION(zpool_import_list)
{
	zval *params = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a", &params) == FAILURE) { 
	   return;
	}

	int ret = 0;
	zpool_imp_cb_t cb = { 0 };
	cb.cb_type = 2;
	array_init(return_value);
	cb.cb_ret = return_value;
	ret = zpool_import_callback(NULL, params, &cb);
	if (ret)
		RETURN_FALSE;
}

PHP_FUNCTION(zpool_import_all)
{
	zval *params = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|a", &params) == FAILURE) { 
	   return;
	}

	int ret = 0;
	zpool_imp_cb_t cb = { 0 };
	cb.cb_type = 1;
	ret = zpool_import_callback(NULL, params, &cb);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zpool_attach)
{
	char *zpool_name;
	char *dev;
	char *new_dev;
	zend_bool force = 0;
	size_t zpool_name_len;
	size_t dev_len;
	size_t new_dev_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sss|b", &zpool_name, &zpool_name_len, &dev, &dev_len, &new_dev, &new_dev_len, &force) == FAILURE) { 
	   return;
	}
	int ret = zpool_attach_or_replace(zpool_name, dev, new_dev, force, 0);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zpool_replace)
{
	char *zpool_name;
	char *dev;
	zval *new_dev;
	zend_bool force = 0;
	size_t zpool_name_len;
	size_t dev_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|zb", &zpool_name, &zpool_name_len, &dev, &dev_len, &new_dev, &force) == FAILURE) { 
	   return;
	}
	char *tmp_dev;
	if (Z_TYPE_P(new_dev) == IS_STRING)
		tmp_dev = Z_STRVAL_P(new_dev);
	else
		tmp_dev = dev;
	int ret = zpool_attach_or_replace(zpool_name, dev, tmp_dev, force, 1);
	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zpool_detach)
{
	char *zpool_name;
	char *dev;
	size_t zpool_name_len;
	size_t dev_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &zpool_name, &zpool_name_len, &dev, &dev_len) == FAILURE) { 
	   return;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}

	zpool_handle_t *zhp;

	if ((zhp = zpool_open(g_zfs, zpool_name)) == NULL) {
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	int ret = zpool_vdev_detach(zhp, dev);

	zpool_close(zhp);
    libzfs_fini(g_zfs);

	if (ret)
		RETURN_FALSE;
	RETURN_TRUE;
}

PHP_FUNCTION(zpool_status)
{
	char *zpool_name = "";
	size_t zpool_name_len = sizeof("") - 1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s", &zpool_name, &zpool_name_len) == FAILURE) { 
	   return;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}

	int ret = 0;
	array_init(return_value);
	status_cbdata_t cb = { 0 };
	cb.cb_ret = return_value;
	if (zpool_name[0]) {
		zpool_handle_t *zhp;

		if ((zhp = zpool_open(g_zfs, zpool_name)) == NULL) {
			libzfs_fini(g_zfs);
			RETURN_FALSE;
		}
		zpool_status_callback(zhp, &cb);
	} else {
		cb.cb_allpools = B_TRUE;
		(void) zpool_iter(g_zfs, zpool_status_callback, &cb);
	}

    libzfs_fini(g_zfs);

	if (ret)
		RETURN_FALSE;
}

static boolean_t
should_auto_mount(zfs_handle_t *zhp)
{
	if (!zfs_prop_valid_for_type(ZFS_PROP_CANMOUNT, zfs_get_type(zhp)))
		return (B_FALSE);
	return (zfs_prop_get_int(zhp, ZFS_PROP_CANMOUNT) == ZFS_CANMOUNT_ON);
}

static int
destroy_check_dependent(zfs_handle_t *zhp, void *data)
{
	destroy_cbdata_t *cbp = data;
	const char *tname = zfs_get_name(cbp->cb_target);
	const char *name = zfs_get_name(zhp);

	if (strncmp(tname, name, strlen(tname)) == 0 &&
	    (name[strlen(tname)] == '/' || name[strlen(tname)] == '@')) {
		if (cbp->cb_recurse) {
			zfs_close(zhp);
			return (0);
		}

		if (cbp->cb_first) {
			cbp->cb_first = B_FALSE;
			cbp->cb_error = B_TRUE;
		}
	} else {
		if (!cbp->cb_recurse &&
		    zfs_get_type(cbp->cb_target) != ZFS_TYPE_SNAPSHOT) {
				zfs_close(zhp);
				return (0);
			}
		if (cbp->cb_first) {
			cbp->cb_first = B_FALSE;
			cbp->cb_error = B_TRUE;
			cbp->cb_dryrun = B_TRUE;
		}

	}

	zfs_close(zhp);
	return (0);
}

static int
destroy_callback(zfs_handle_t *zhp, void *data)
{
	destroy_cbdata_t *cb = data;
	const char *name = zfs_get_name(zhp);

	if (strchr(zfs_get_name(zhp), '/') == NULL &&
	    zfs_get_type(zhp) == ZFS_TYPE_FILESYSTEM) {
		zfs_close(zhp);
		return (0);
	}
	if (cb->cb_dryrun) {
		zfs_close(zhp);
		return (0);
	}

	if (zfs_get_type(zhp) == ZFS_TYPE_SNAPSHOT) {
		fnvlist_add_boolean(cb->cb_batchedsnaps, name);
	} else {
		int error = zfs_destroy_snaps_nvl(g_zfs,
		    cb->cb_batchedsnaps, B_FALSE);
		fnvlist_free(cb->cb_batchedsnaps);
		cb->cb_batchedsnaps = fnvlist_alloc();

		if (error != 0 ||
		    zfs_unmount(zhp, NULL, cb->cb_force ? MS_FORCE : 0) != 0 ||
		    zfs_destroy(zhp, cb->cb_defer_destroy) != 0) {
			zfs_close(zhp);
			return (-1);
		}
	}

	zfs_close(zhp);
	return (0);
}

static int
zfs_callback(zfs_handle_t *zhp, void *data)
{
	callback_data_t *cb = data;
	char buf[ZFS_MAXPROPLEN];
	char source[ZFS_MAX_DATASET_NAME_LEN];
	zprop_source_t sourcetype;
	boolean_t should_close = B_TRUE;
	if ((zfs_get_type(zhp) == ZFS_TYPE_FILESYSTEM && (cb->cb_types & ZFS_TYPE_SNAPSHOT)) || zfs_get_type(zhp) == ZFS_TYPE_SNAPSHOT) {
		if (zfs_get_type(zhp) == ZFS_TYPE_SNAPSHOT) {
			const char *zname = zfs_get_name(zhp);
			zval subitem;
			array_init(&subitem);
		zfs_prop_get(zhp, ZFS_PROP_USED, buf,
			    sizeof (buf), &sourcetype, source,
			    sizeof (source),
			    B_FALSE);
		add_assoc_string(&subitem, "used", buf);
			uint64_t date = 0;
			zfs_prop_get_numeric(zhp, ZFS_PROP_CREATION, &date,
				NULL, NULL, 0);
			add_assoc_long(&subitem, "creation", date);
			char *strval = kzfs_get_uprop(zname, "snap:desc");
			add_assoc_string(&subitem, "desc", strval ? strval : "");
			char *p = strchr(zname, '@');
			p++;
			add_assoc_zval((zval *)cb->tmp, p, &subitem);
		} else {
			(void) zfs_iter_snapshots(zhp,
			    B_FALSE, zfs_callback,
			    data);
		}
	} else if ((zfs_get_type(zhp) == ZFS_TYPE_FILESYSTEM || zfs_get_type(zhp) == ZFS_TYPE_VOLUME) && (cb->cb_types & ZFS_TYPE_SNAPSHOT) == 0) {
		zval subitem;
		array_init(&subitem);
		for (int i = 0; i < ZFS_NUM_PROPS; i++)
			if (cb->cb_props_table[i]) {
				if (i == ZFS_PROP_CREATION) {
					uint64_t date = 0;
					if (zfs_prop_get_numeric(zhp, ZFS_PROP_CREATION, &date,
						NULL, NULL, 0) == 0)
						add_assoc_long(&subitem, zfs_prop_to_name(i), date);
				} else {
					if (zfs_prop_get(zhp, i, buf,
						    sizeof (buf), &sourcetype, source,
						    sizeof (source),
						    B_FALSE) == 0)
						add_assoc_string(&subitem, zfs_prop_to_name(i), buf);
				}
			}
		zprop_list_t *pl = cb->cb_proplist;
		nvlist_t *userprops = zfs_get_user_props(zhp);
		for (; pl != NULL; pl = pl->pl_next) {
			nvlist_t *propval;
			char *sourceval;
			if (nvlist_lookup_nvlist(userprops,
			    pl->pl_user_prop, &propval) == 0) {
				char *propstr;
				if (nvlist_lookup_string(propval,
				    ZPROP_SOURCE, &sourceval) == 0) {
					if (strcmp(sourceval,
				    zfs_get_name(zhp)) == 0 || strcmp(sourceval,
				    ZPROP_SOURCE_VAL_RECVD) == 0) {
						if (nvlist_lookup_string(propval,
						    ZPROP_VALUE, &propstr) == 0) {
						    add_assoc_string(&subitem, pl->pl_user_prop, propstr);
						} else {
							add_assoc_null(&subitem, pl->pl_user_prop);
						}
				    } else {
				    	add_assoc_null(&subitem, pl->pl_user_prop);
				    }
				} else {
					add_assoc_null(&subitem, pl->pl_user_prop);
				}
			} else {
				add_assoc_null(&subitem, pl->pl_user_prop);
			}
		}

		if (zend_hash_num_elements(Z_ARRVAL_P(&subitem)) > 0)
			add_assoc_zval((zval *)cb->tmp, zfs_get_name(zhp), &subitem);
		(void) zfs_iter_filesystems(zhp, zfs_callback, cb);
	}
	if (should_close)
		zfs_close(zhp);

	return 0;
}

static int
zpool_callback(zpool_handle_t *zhp, void *data)
{
	zpool_cb_data_t *cb = data;
	char buf[ZPOOL_MAXPROPLEN];
	zprop_source_t sourcetype;
	zval subitem;
	array_init(&subitem);
	add_assoc_string(&subitem, "name", zpool_get_name(zhp));
	for (int i = 0; i < ZPOOL_NUM_PROPS; i++)
		if (i != ZPOOL_PROP_GUID && i != ZPOOL_PROP_NAME)
		if (cb->cb_props_table[i]) {
			zpool_get_prop(zhp, i, buf,
						    sizeof (buf), &sourcetype,
						    B_FALSE);
			if (strcmp(buf, "-") != 0)
				add_assoc_string(&subitem, zpool_prop_to_name(i), buf);
			else
				add_assoc_null(&subitem, zpool_prop_to_name(i));
		}
	zpool_get_prop(zhp, ZPOOL_PROP_GUID, buf,
				    sizeof (buf), &sourcetype,
				    B_FALSE);
	if (zend_hash_num_elements(Z_ARRVAL_P(&subitem)) > 0)
		add_assoc_zval((zval *)cb->tmp, buf, &subitem);
	zpool_close(zhp);
	return 0;
}

static int
fake_sort(const void *larg, const void *rarg, void *data)
{
	return 0;
}


static int
add_prop_list(const char *propname, char *propval, nvlist_t **props,
    boolean_t poolprop)
{
	zpool_prop_t prop = ZPOOL_PROP_INVAL;
	zfs_prop_t fprop;
	nvlist_t *proplist;
	const char *normnm;
	char *strval;

	if (*props == NULL &&
	    nvlist_alloc(props, NV_UNIQUE_NAME, 0) != 0) {
		return (1);
	}

	proplist = *props;

	if (poolprop) {
		const char *vname = zpool_prop_to_name(ZPOOL_PROP_VERSION);

		if ((prop = zpool_name_to_prop(propname)) == ZPROP_INVAL &&
		    !zpool_prop_feature(propname)) {
			return (2);
		}

		/*
		 * feature@ properties and version should not be specified
		 * at the same time.
		 */
		if ((prop == ZPOOL_PROP_INVAL && zpool_prop_feature(propname) &&
		    nvlist_exists(proplist, vname)) ||
		    (prop == ZPOOL_PROP_VERSION &&
		    prop_list_contains_feature(proplist))) {
			return (2);
		}


		if (zpool_prop_feature(propname))
			normnm = propname;
		else
			normnm = zpool_prop_to_name(prop);
	} else {
		if ((fprop = zfs_name_to_prop(propname)) != ZPROP_INVAL) {
			normnm = zfs_prop_to_name(fprop);
		} else {
			normnm = propname;
		}
	}

	if (nvlist_lookup_string(proplist, normnm, &strval) == 0 &&
	    prop != ZPOOL_PROP_CACHEFILE) {
		return (2);
	}

	if (nvlist_add_string(proplist, normnm, propval) != 0) {
		return (1);
	}

	return (0);
}

static boolean_t
prop_list_contains_feature(nvlist_t *proplist)
{
	nvpair_t *nvp;
	for (nvp = nvlist_next_nvpair(proplist, NULL); NULL != nvp;
	    nvp = nvlist_next_nvpair(proplist, nvp)) {
		if (zpool_prop_feature(nvpair_name(nvp)))
			return (B_TRUE);
	}
	return (B_FALSE);
}

static nvlist_t *
make_root_vdev(zpool_handle_t *zhp, int force, int check_rep,
    boolean_t replacing, boolean_t dryrun, zpool_boot_label_t boot_type,
    uint64_t boot_size, zval *devs)
{
	nvlist_t *newroot = NULL;
	nvlist_t *poolconfig = NULL;

	if ((newroot = construct_spec(devs)) == NULL) {
		return (NULL);
	}

	if (zhp && ((poolconfig = zpool_get_config(zhp, NULL)) == NULL))
		return (NULL);

	if (is_device_in_use(NULL, newroot, force, replacing, B_FALSE)) {
		nvlist_free(newroot);
		return (NULL);
	}

	if (check_rep && check_replication(poolconfig, newroot) != 0) {
		nvlist_free(newroot);
		return (NULL);
	}

#ifdef illumos
	if (!dryrun && make_disks(zhp, newroot, boot_type, boot_size) != 0) {
		nvlist_free(newroot);
		return (NULL);
	}
#endif

	return (newroot);
}

static nvlist_t *
construct_spec(zval *devs)
{
	nvlist_t *nvroot, *nv, **top, **spares, **l2cache;
	int t, toplevels, mindev, maxdev, nspares, nlogs, nl2cache;
	const char *type;
	uint64_t is_log;
	boolean_t seen_logs;

	top = NULL;
	toplevels = 0;
	spares = NULL;
	l2cache = NULL;
	nspares = 0;
	nlogs = 0;
	nl2cache = 0;
	is_log = B_FALSE;
	seen_logs = B_FALSE;

	HashTable *hash;
	zval *data;
	HashPosition pointer;
	if (devs != NULL) {
		hash = Z_ARRVAL_P(devs);

		for(
	        zend_hash_internal_pointer_reset_ex(hash, &pointer);
	        (data = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
	        zend_hash_move_forward_ex(hash, &pointer)
	    ) {
	    	if (Z_TYPE_P(data) == IS_ARRAY) {
	    		zval *d_type;
				if ((d_type = zend_hash_str_find(Z_ARRVAL_P(data), "type", sizeof("type")-1)) == NULL || Z_TYPE_P(d_type) != IS_STRING) {
					return (NULL);
				}
				zval *d_children;
				if ((d_children = zend_hash_str_find(Z_ARRVAL_P(data), "children", sizeof("children")-1)) == NULL || Z_TYPE_P(d_children) != IS_ARRAY) {
					return (NULL);
				}
				zval *d_log;
				if ((d_log = zend_hash_str_find(Z_ARRVAL_P(data), "is_log", sizeof("is_log")-1)) != NULL && Z_TYPE_P(d_log) == IS_TRUE) {
					is_log = B_TRUE;
					seen_logs = B_TRUE;
				}

				nv = NULL;

				if ((type = is_grouping(Z_STRVAL_P(d_type), &mindev, &maxdev)) != NULL) {
					nvlist_t **child = NULL;
					int c, children = 0;
					zval *dev;
					HashPosition dev_pointer;

					if (strcmp(type, VDEV_TYPE_SPARE) == 0) {
						if (spares != NULL) {
							return (NULL);
						}
						is_log = B_FALSE;
					}

					if (strcmp(type, VDEV_TYPE_L2CACHE) == 0) {
						if (l2cache != NULL) {
							return (NULL);
						}
						is_log = B_FALSE;
					}
					if (is_log) {
						if (strcmp(type, VDEV_TYPE_MIRROR) != 0) {
							return (NULL);
						}
						nlogs++;
					}
					for(
				        zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(d_children), &dev_pointer);
				        (dev = zend_hash_get_current_data_ex(Z_ARRVAL_P(d_children), &dev_pointer)) != NULL;
				        zend_hash_move_forward_ex(Z_ARRVAL_P(d_children), &dev_pointer)
				    ) {
						children++;
						child = realloc(child,
						    children * sizeof (nvlist_t *));
						if (child == NULL)
							return (NULL);
						if ((nv = make_leaf_vdev(Z_STRVAL_P(dev), B_FALSE))
						    == NULL)
							return (NULL);
						child[children - 1] = nv;
					}

					if (children < mindev) {
						return (NULL);
					}

					if (children > maxdev) {
						return (NULL);
					}


					if (strcmp(type, VDEV_TYPE_SPARE) == 0) {
						spares = child;
						nspares = children;
						continue;
					} else if (strcmp(type, VDEV_TYPE_L2CACHE) == 0) {
						l2cache = child;
						nl2cache = children;
						continue;
					} else {
						nvlist_alloc(&nv, NV_UNIQUE_NAME,
						    0);
						nvlist_add_string(nv, ZPOOL_CONFIG_TYPE,
						    type);
						nvlist_add_uint64(nv,
						    ZPOOL_CONFIG_IS_LOG, is_log);
						if (strcmp(type, VDEV_TYPE_RAIDZ) == 0) {
							nvlist_add_uint64(nv,
							    ZPOOL_CONFIG_NPARITY,
							    mindev - 1);
						}
						nvlist_add_nvlist_array(nv,
						    ZPOOL_CONFIG_CHILDREN, child,
						    children);

						for (c = 0; c < children; c++)
							nvlist_free(child[c]);
						free(child);
					}

					toplevels++;
					top = realloc(top, toplevels * sizeof (nvlist_t *));
					if (top == NULL)
						zpool_no_memory();
					top[toplevels - 1] = nv;
				} else {
					zval *dev;
					HashPosition dev_pointer;
					for(
				        zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(d_children), &dev_pointer);
				        (dev = zend_hash_get_current_data_ex(Z_ARRVAL_P(d_children), &dev_pointer)) != NULL;
				        zend_hash_move_forward_ex(Z_ARRVAL_P(d_children), &dev_pointer)
				    ) {
						if ((nv = make_leaf_vdev(Z_STRVAL_P(dev), is_log)) == NULL) {
							return (NULL);
						}
						if (is_log)
							nlogs++;

						toplevels++;
						top = realloc(top, toplevels * sizeof (nvlist_t *));
						if (top == NULL)
							zpool_no_memory();
						top[toplevels - 1] = nv;
					}
				}
			}
		}
	}
	if (toplevels == 0 && nspares == 0 && nl2cache == 0) {
		return (NULL);
	}

	if (seen_logs && nlogs == 0) {
		return (NULL);
	}

	/*
	 * Finally, create nvroot and add all top-level vdevs to it.
	 */
	nvlist_alloc(&nvroot, NV_UNIQUE_NAME, 0);
	nvlist_add_string(nvroot, ZPOOL_CONFIG_TYPE,
	    VDEV_TYPE_ROOT);
	nvlist_add_nvlist_array(nvroot, ZPOOL_CONFIG_CHILDREN,
	    top, toplevels);
	if (nspares != 0)
		nvlist_add_nvlist_array(nvroot, ZPOOL_CONFIG_SPARES,
		    spares, nspares);
	if (nl2cache != 0)
		nvlist_add_nvlist_array(nvroot, ZPOOL_CONFIG_L2CACHE,
		    l2cache, nl2cache);

	for (t = 0; t < toplevels; t++)
		nvlist_free(top[t]);
	for (t = 0; t < nspares; t++)
		nvlist_free(spares[t]);
	for (t = 0; t < nl2cache; t++)
		nvlist_free(l2cache[t]);
	if (spares)
		free(spares);
	if (l2cache)
		free(l2cache);
	free(top);

	return (nvroot);
}
static boolean_t
is_spare(nvlist_t *config, const char *path)
{
	int fd;
	pool_state_t state;
	char *name = NULL;
	nvlist_t *label;
	uint64_t guid, spareguid;
	nvlist_t *nvroot;
	nvlist_t **spares;
	uint_t i, nspares;
	boolean_t inuse;

	if ((fd = open(path, O_RDONLY)) < 0)
		return (B_FALSE);

	if (zpool_in_use(g_zfs, fd, &state, &name, &inuse) != 0 ||
	    !inuse ||
	    state != POOL_STATE_SPARE ||
	    zpool_read_label(fd, &label) != 0) {
		free(name);
		(void) close(fd);
		return (B_FALSE);
	}
	free(name);
	(void) close(fd);

	nvlist_lookup_uint64(label, ZPOOL_CONFIG_GUID, &guid);
	nvlist_free(label);

	nvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE,
	    &nvroot);
	if (nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_SPARES,
	    &spares, &nspares) == 0) {
		for (i = 0; i < nspares; i++) {
			nvlist_lookup_uint64(spares[i],
			    ZPOOL_CONFIG_GUID, &spareguid);
			if (spareguid == guid)
				return (B_TRUE);
		}
	}

	return (B_FALSE);
}
static boolean_t
is_device_in_use(nvlist_t *config, nvlist_t *nv, boolean_t force,
    boolean_t replacing, boolean_t isspare)
{
	nvlist_t **child;
	uint_t c, children;
	char *type, *path;
	int ret = 0;
	char buf[MAXPATHLEN];
	uint64_t wholedisk;
	boolean_t anyinuse = B_FALSE;

	nvlist_lookup_string(nv, ZPOOL_CONFIG_TYPE, &type);

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0) {

		nvlist_lookup_string(nv, ZPOOL_CONFIG_PATH, &path);

		if (replacing) {
#ifdef illumos
			if (nvlist_lookup_uint64(nv, ZPOOL_CONFIG_WHOLE_DISK,
			    &wholedisk) == 0 && wholedisk)
				(void) snprintf(buf, sizeof (buf), "%ss0",
				    path);
			else
#endif
				(void) strlcpy(buf, path, sizeof (buf));

			if (is_spare(config, buf))
				return (B_FALSE);
		}

		if (strcmp(type, VDEV_TYPE_DISK) == 0)
			ret = check_device(path, force, isspare);
		else if (strcmp(type, VDEV_TYPE_FILE) == 0)
			ret = check_file(path, force, isspare);

		return (ret != 0);
	}

	for (c = 0; c < children; c++)
		if (is_device_in_use(config, child[c], force, replacing,
		    B_FALSE))
			anyinuse = B_TRUE;

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_SPARES,
	    &child, &children) == 0)
		for (c = 0; c < children; c++)
			if (is_device_in_use(config, child[c], force, replacing,
			    B_TRUE))
				anyinuse = B_TRUE;

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_L2CACHE,
	    &child, &children) == 0)
		for (c = 0; c < children; c++)
			if (is_device_in_use(config, child[c], force, replacing,
			    B_FALSE))
				anyinuse = B_TRUE;

	return (anyinuse);
}

static const char *
is_grouping(const char *type, int *mindev, int *maxdev)
{
	if (strncmp(type, "raidz", 5) == 0) {
		const char *p = type + 5;
		char *end;
		long nparity;

		if (*p == '\0') {
			nparity = 1;
		} else if (*p == '0') {
			return (NULL);
		} else {
			errno = 0;
			nparity = strtol(p, &end, 10);
			if (errno != 0 || nparity < 1 || nparity >= 255 ||
			    *end != '\0')
				return (NULL);
		}

		if (mindev != NULL)
			*mindev = nparity + 1;
		if (maxdev != NULL)
			*maxdev = 255;
		return (VDEV_TYPE_RAIDZ);
	}

	if (maxdev != NULL)
		*maxdev = INT_MAX;

	if (strcmp(type, "mirror") == 0) {
		if (mindev != NULL)
			*mindev = 2;
		return (VDEV_TYPE_MIRROR);
	}

	if (strcmp(type, "spare") == 0) {
		if (mindev != NULL)
			*mindev = 1;
		return (VDEV_TYPE_SPARE);
	}

	if (strcmp(type, "log") == 0) {
		if (mindev != NULL)
			*mindev = 1;
		return (VDEV_TYPE_LOG);
	}

	if (strcmp(type, "cache") == 0) {
		if (mindev != NULL)
			*mindev = 1;
		return (VDEV_TYPE_L2CACHE);
	}

	return (NULL);
}

static int
check_file(const char *file, boolean_t force, boolean_t isspare)
{
	char  *name;
	int fd;
	int ret = 0;
	int err;
	pool_state_t state;
	boolean_t inuse;


	if ((fd = open(file, O_RDONLY)) < 0)
		return (0);

	if (zpool_in_use(g_zfs, fd, &state, &name, &inuse) == 0 && inuse) {
		const char *desc;

		switch (state) {
		case POOL_STATE_ACTIVE:
			desc = gettext("active");
			break;

		case POOL_STATE_EXPORTED:
			desc = gettext("exported");
			break;

		case POOL_STATE_POTENTIALLY_ACTIVE:
			desc = gettext("potentially active");
			break;

		default:
			desc = gettext("unknown");
			break;
		}

		/*
		 * Allow hot spares to be shared between pools.
		 */
		if (state == POOL_STATE_SPARE && isspare)
			return (0);

		if (state == POOL_STATE_ACTIVE ||
		    state == POOL_STATE_SPARE || !force) {
			switch (state) {
			case POOL_STATE_SPARE:
				break;
			default:
				break;
			}
			ret = -1;
		}

		free(name);
	}

	(void) close(fd);
	return (ret);
}

static int
check_device(const char *name, boolean_t force, boolean_t isspare)
{
	char path[MAXPATHLEN];

	if (strncmp(name, _PATH_DEV, sizeof(_PATH_DEV) - 1) != 0)
		snprintf(path, sizeof(path), "%s%s", _PATH_DEV, name);
	else
		strlcpy(path, name, sizeof(path));

	return (check_file(path, force, isspare));
}

static boolean_t
is_whole_disk(const char *arg)
{
	int fd;

	fd = g_open(arg, 0);
	if (fd >= 0) {
		g_close(fd);
		return (B_TRUE);
	}
	return (B_FALSE);
}

static nvlist_t *
make_leaf_vdev(const char *arg, uint64_t is_log)
{
	char path[MAXPATHLEN];
	struct stat64 statbuf;
	nvlist_t *vdev = NULL;
	char *type = NULL;
	boolean_t wholedisk = B_FALSE;

	/*
	 * Determine what type of vdev this is, and put the full path into
	 * 'path'.  We detect whether this is a device of file afterwards by
	 * checking the st_mode of the file.
	 */
	if (arg[0] == '/') {
		/*
		 * Complete device or file path.  Exact type is determined by
		 * examining the file descriptor afterwards.
		 */
		wholedisk = is_whole_disk(arg);
		if (!wholedisk && (stat64(arg, &statbuf) != 0)) {
			return (NULL);
		}

		(void) strlcpy(path, arg, sizeof (path));
	} else {
		/*
		 * This may be a short path for a device, or it could be total
		 * gibberish.  Check to see if it's a known device in
		 * /dev/dsk/.  As part of this check, see if we've been given a
		 * an entire disk (minus the slice number).
		 */
		if (strncmp(arg, _PATH_DEV, sizeof(_PATH_DEV) - 1) == 0)
			strlcpy(path, arg, sizeof (path));
		else
			snprintf(path, sizeof (path), "%s%s", _PATH_DEV, arg);
		wholedisk = is_whole_disk(path);
		if (!wholedisk && (stat64(path, &statbuf) != 0)) {
			/*
			 * If we got ENOENT, then the user gave us
			 * gibberish, so try to direct them with a
			 * reasonable error message.  Otherwise,
			 * regurgitate strerror() since it's the best we
			 * can do.
			 */
			if (errno == ENOENT) {
				return (NULL);
			} else {
				return (NULL);
			}
		}
	}

#ifdef __FreeBSD__
	if (S_ISCHR(statbuf.st_mode)) {
		statbuf.st_mode &= ~S_IFCHR;
		statbuf.st_mode |= S_IFBLK;
		wholedisk = B_FALSE;
	}
#endif

	/*
	 * Determine whether this is a device or a file.
	 */
	if (wholedisk || S_ISBLK(statbuf.st_mode)) {
		type = VDEV_TYPE_DISK;
	} else if (S_ISREG(statbuf.st_mode)) {
		type = VDEV_TYPE_FILE;
	} else {
		return (NULL);
	}

	/*
	 * Finally, we have the complete device or file, and we know that it is
	 * acceptable to use.  Construct the nvlist to describe this vdev.  All
	 * vdevs have a 'path' element, and devices also have a 'devid' element.
	 */
	nvlist_alloc(&vdev, NV_UNIQUE_NAME, 0);
	nvlist_add_string(vdev, ZPOOL_CONFIG_PATH, path);
	nvlist_add_string(vdev, ZPOOL_CONFIG_TYPE, type);
	nvlist_add_uint64(vdev, ZPOOL_CONFIG_IS_LOG, is_log);
	if (strcmp(type, VDEV_TYPE_DISK) == 0)
		nvlist_add_uint64(vdev, ZPOOL_CONFIG_WHOLE_DISK,
		    (uint64_t)wholedisk);

#ifdef have_devid
	/*
	 * For a whole disk, defer getting its devid until after labeling it.
	 */
	if (S_ISBLK(statbuf.st_mode) && !wholedisk) {
		/*
		 * Get the devid for the device.
		 */
		int fd;
		ddi_devid_t devid;
		char *minor = NULL, *devid_str = NULL;

		if ((fd = open(path, O_RDONLY)) < 0) {
			nvlist_free(vdev);
			return (NULL);
		}

		if (devid_get(fd, &devid) == 0) {
			if (devid_get_minor_name(fd, &minor) == 0 &&
			    (devid_str = devid_str_encode(devid, minor)) !=
			    NULL) {
				nvlist_add_string(vdev,
				    ZPOOL_CONFIG_DEVID, devid_str);
			}
			if (devid_str != NULL)
				devid_str_free(devid_str);
			if (minor != NULL)
				devid_str_free(minor);
			devid_free(devid);
		}

		(void) close(fd);
	}
#endif

	return (vdev);
}

static void
zpool_no_memory(void)
{
	assert(errno == ENOMEM);
	exit(1);
}

static int
remove_from_file(char *filename, char *match_line, int line_num)
{
	char repname[strlen(filename) + 5];
	snprintf(repname, sizeof(repname), "%s.rep", filename);
    FILE *file = fopen(filename, "r");
    FILE *rep = fopen(repname, "w");
    if (file != NULL && rep != NULL) {
        char str[ZFS_CRON_LINE_LEN];
        int cnt = 0;
        long prev = 0;
        while(fscanf(file, "%[^\n]\n", str) != -1) {
            if (strncmp(match_line, str, strlen(match_line)) == 0) {
                    cnt++;
                    continue;
            }
            if (cnt == line_num)
                    cnt = 0;
            if (cnt != 0) {
                    cnt++;
                    continue;
            }
            fprintf(rep, "%s\n", str);
        }
        fclose(file);
        fclose(rep);
        rename(repname, filename);
        return 0;
    } else {
    	return -1;
    }
}


static int
add_snap_rule(zval *sched, char *cron_text, size_t size, char *zname)
{
	zval *inner;
	HashTable *hash;
	zval *data;
	HashPosition pointer;
	if ((inner = zend_hash_str_find(Z_ARRVAL_P(sched), "create", sizeof("create")-1)) != NULL && Z_TYPE_P(inner) == IS_ARRAY) {
		zval *item;
		if ((item = zend_hash_str_find(Z_ARRVAL_P(inner), "min", sizeof("min")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
			snprintf(cron_text, size, "%s%s ", cron_text, Z_STRVAL_P(item));
		} else {
			return -1;
		}
		if ((item = zend_hash_str_find(Z_ARRVAL_P(inner), "hour", sizeof("hour")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
			snprintf(cron_text, size, "%s%s ", cron_text, Z_STRVAL_P(item));
		} else {
			return -1;
		}
		if ((item = zend_hash_str_find(Z_ARRVAL_P(inner), "mday", sizeof("mday")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
			snprintf(cron_text, size, "%s%s ", cron_text, Z_STRVAL_P(item));
		} else {
			return -1;
		}
		if ((item = zend_hash_str_find(Z_ARRVAL_P(inner), "month", sizeof("month")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
			snprintf(cron_text, size, "%s%s ", cron_text, Z_STRVAL_P(item));
		} else {
			return -1;
		}
		if ((item = zend_hash_str_find(Z_ARRVAL_P(inner), "wday", sizeof("wday")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
			snprintf(cron_text, size, "%s%s ", cron_text, Z_STRVAL_P(item));
		} else {
			return -1;
		}
	} else {
		return -1;
			return -1;
	}

	char *cron_cmd = INI_STR("kcs.zfs.snap.rules.cmd");
	snprintf(cron_text, size, "%s root %s create %s\n", cron_text, cron_cmd, zname);

	if ((inner = zend_hash_str_find(Z_ARRVAL_P(sched), "delete", sizeof("delete")-1)) != NULL && Z_TYPE_P(inner) == IS_ARRAY) {
		zval *item;
		if ((item = zend_hash_str_find(Z_ARRVAL_P(inner), "min", sizeof("min")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
			snprintf(cron_text, size, "%s%s ", cron_text, Z_STRVAL_P(item));
		} else {
			return -1;
		}
		if ((item = zend_hash_str_find(Z_ARRVAL_P(inner), "hour", sizeof("hour")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
			snprintf(cron_text, size, "%s%s ", cron_text, Z_STRVAL_P(item));
		} else {
			return -1;
		}
		if ((item = zend_hash_str_find(Z_ARRVAL_P(inner), "mday", sizeof("mday")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
			snprintf(cron_text, size, "%s%s ", cron_text, Z_STRVAL_P(item));
		} else {
			return -1;
		}
		if ((item = zend_hash_str_find(Z_ARRVAL_P(inner), "month", sizeof("month")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
			snprintf(cron_text, size, "%s%s ", cron_text, Z_STRVAL_P(item));
		} else {
			return -1;
		}
		if ((item = zend_hash_str_find(Z_ARRVAL_P(inner), "wday", sizeof("wday")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
			snprintf(cron_text, size, "%s%s ", cron_text, Z_STRVAL_P(item));
		} else {
			return -1;
		}
	} else {
		return -1;
	}

	snprintf(cron_text, size, "%s root %s delete %s\n", cron_text, cron_cmd, zname);
	return 0;
}


static int
zfs_snapshot_cb(zfs_handle_t *zhp, void *arg)
{
	snap_cbdata_t *sd = arg;
	char *name;
	int rv = 0;
	int error;

	if (sd->sd_recursive &&
	    zfs_prop_get_int(zhp, ZFS_PROP_INCONSISTENT) != 0) {
		zfs_close(zhp);
		return (0);
	}

	error = asprintf(&name, "%s@%s", zfs_get_name(zhp), sd->sd_snapname);
	if (error == -1) {
		return (-1);
	}
	fnvlist_add_boolean(sd->sd_nvl, name);
	free(name);

	if (sd->sd_recursive)
		rv = zfs_iter_filesystems(zhp, zfs_snapshot_cb, sd);
	zfs_close(zhp);
	return (rv);
}


static int
snapshot_to_nvl_cb(zfs_handle_t *zhp, void *arg)
{
	destroy_cbdata_t *cb = arg;
	int err = 0;

	cb->cb_target = zhp;
	cb->cb_first = B_TRUE;
	err = zfs_iter_dependents(zhp, B_TRUE,
	    destroy_check_dependent, cb);
	if (err == 0) {
		nvlist_add_boolean(cb->cb_nvl, zfs_get_name(zhp));
	}
	zfs_close(zhp);
	return (err);
}

static int
gather_snapshots(zfs_handle_t *zhp, void *arg)
{
	destroy_cbdata_t *cb = arg;
	int err = 0;

	err = zfs_iter_snapspec(zhp, cb->cb_snapspec, snapshot_to_nvl_cb, cb);
	if (err == ENOENT)
		err = 0;
	if (err != 0) {
		zfs_close(zhp);
		return (err);
	}

	if (cb->cb_recurse)
		err = zfs_iter_filesystems(zhp, gather_snapshots, cb);

	zfs_close(zhp);
	return (err);
}

static int
zfs_dataset_rb(char *zname, long recursive)
{
	if ((g_zfs = libzfs_init()) == NULL) {
		return (1);
	}

	destroy_cbdata_t cb = { 0 };
	int rv = 0;
	int err = 0;
	zfs_handle_t *zhp = NULL;
	char *at, *pound;
	zfs_type_t type = ZFS_TYPE_DATASET;
	cb.cb_force = B_TRUE;
	cb.cb_recurse = recursive;

	if ((zhp = zfs_open(g_zfs, zname, type)) == NULL) {
	    libzfs_fini(g_zfs);
		return (1);
	}

	cb.cb_target = zhp;

	if (!cb.cb_recurse && strchr(zfs_get_name(zhp), '/') == NULL &&
	    zfs_get_type(zhp) == ZFS_TYPE_FILESYSTEM) {
	    libzfs_fini(g_zfs);
		return (1);
	}

	cb.cb_first = B_TRUE;

	cb.cb_batchedsnaps = fnvlist_alloc();
	if (zfs_iter_dependents(zhp, B_FALSE, destroy_callback,
	    &cb) != 0) {
		rv = 1;
		fnvlist_free(cb.cb_batchedsnaps);
		fnvlist_free(cb.cb_nvl);
		if (zhp != NULL)
			zfs_close(zhp);
	    libzfs_fini(g_zfs);
		return (1);
	}

	err = destroy_callback(zhp, &cb);
	zhp = NULL;
	if (err == 0) {
		err = zfs_destroy_snaps_nvl(g_zfs,
		    cb.cb_batchedsnaps, cb.cb_defer_destroy);
	}
	if (err != 0)
		rv = 1;
	

	fnvlist_free(cb.cb_batchedsnaps);
	fnvlist_free(cb.cb_nvl);
	if (zhp != NULL)
		zfs_close(zhp);
    libzfs_fini(g_zfs);
    return (0);
}

static int
zfs_snapshot_rb(const char *zname, char *snapname, long recursive)
{
	if ((g_zfs = libzfs_init()) == NULL) {
		return (1);
	}

	destroy_cbdata_t cb = { 0 };
	zfs_handle_t *zhp = NULL;
	cb.cb_recurse = recursive;
	cb.cb_nvl = fnvlist_alloc();
	zhp = zfs_open(g_zfs, zname,
	    ZFS_TYPE_FILESYSTEM | ZFS_TYPE_VOLUME);
	if (zhp == NULL) {
		fnvlist_free(cb.cb_nvl);
	    libzfs_fini(g_zfs);
	    return (1);
	}
	cb.cb_snapspec = snapname;
	int ret = gather_snapshots(zfs_handle_dup(zhp), &cb);
	zfs_close(zhp);

	if (ret != 0 || cb.cb_error) {
		fnvlist_free(cb.cb_nvl);
		libzfs_fini(g_zfs);
		return (1);
	}

	if (nvlist_empty(cb.cb_nvl)) {
		fnvlist_free(cb.cb_nvl);
		libzfs_fini(g_zfs);
		return (1);
	}
	ret = zfs_destroy_snaps_nvl(g_zfs, cb.cb_nvl,
				    B_FALSE);

	fnvlist_free(cb.cb_nvl);

    libzfs_fini(g_zfs);
    return (ret);
}

static int
zfs_delete_user_prop(char *zname, char *propname)
{
	int st = 0;
	if (g_zfs == NULL) {
		if ((g_zfs = libzfs_init()) == NULL) {
			return (1);
		}
		st = 1;
	}

	int ret = 0;
	zfs_prop_t prop;
	if ((prop = zfs_name_to_prop(propname)) != ZPROP_INVAL &&
		 !zfs_prop_user(propname)) {
	    libzfs_fini(g_zfs);
	    return (1);
	}

	zfs_handle_t *zhp = NULL;
	zhp = zfs_open(g_zfs, zname, ZFS_TYPE_DATASET | ZFS_TYPE_VOLUME | ZFS_TYPE_SNAPSHOT);
	if (zhp == NULL) {
		libzfs_fini(g_zfs);
		return (1);
	}

	ret = zfs_prop_inherit(zhp, propname, B_FALSE);
	if (st)
	    libzfs_fini(g_zfs);
    return (ret);
}

PHP_FUNCTION(zfs_ds_props_remove)
{
	char *zname;
	zval *params;
	size_t zname_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sa", &zname, &zname_len, &params) == FAILURE) { 
	   return;
	}
	if (Z_TYPE_P(params) != IS_ARRAY) {
		RETURN_FALSE;
	}

	if ((g_zfs = libzfs_init()) == NULL) {
		RETURN_FALSE;
	}
	zfs_handle_t *zhp = NULL;
	zfs_prop_t prop;
	zhp = zfs_open(g_zfs, zname, ZFS_TYPE_DATASET | ZFS_TYPE_VOLUME | ZFS_TYPE_SNAPSHOT);
	if (zhp == NULL) {
		libzfs_fini(g_zfs);
		RETURN_FALSE;
	}

	HashTable *hash;
	zval *data;
	HashPosition pointer;
	hash = Z_ARRVAL_P(params);
	for(
        zend_hash_internal_pointer_reset_ex(hash, &pointer);
        (data = zend_hash_get_current_data_ex(hash, &pointer)) != NULL;
        zend_hash_move_forward_ex(hash, &pointer)
    ) {
		if (Z_TYPE_P(data) == IS_STRING) {
			if ((prop = zfs_name_to_prop(Z_STRVAL_P(data))) != ZPROP_INVAL &&
				 !zfs_prop_user(Z_STRVAL_P(data))) {
				continue;
			}
			zfs_prop_inherit(zhp, Z_STRVAL_P(data), B_FALSE);
		}
	}


	zfs_close(zhp);
    libzfs_fini(g_zfs);
	RETURN_TRUE;
}

static int
do_import(nvlist_t *config, const char *newname, const char *mntopts,
    nvlist_t *props, int flags)
{
	zpool_handle_t *zhp;
	char *name;
	uint64_t state;
	uint64_t version;
	nvlist_lookup_string(config, ZPOOL_CONFIG_POOL_NAME, &name);
	nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_STATE, &state);
	nvlist_lookup_uint64(config, ZPOOL_CONFIG_VERSION, &version);
	if (!SPA_VERSION_IS_SUPPORTED(version)) {
		return (1);
	} else if (state != POOL_STATE_EXPORTED &&
	    !(flags & ZFS_IMPORT_ANY_HOST)) {
		uint64_t hostid;

		if (nvlist_lookup_uint64(config, ZPOOL_CONFIG_HOSTID,
		    &hostid) == 0) {
			if ((unsigned long)hostid != gethostid()) {
				char *hostname;
				uint64_t timestamp;
				time_t t;

				nvlist_lookup_string(config,
				    ZPOOL_CONFIG_HOSTNAME, &hostname);
				nvlist_lookup_uint64(config,
				    ZPOOL_CONFIG_TIMESTAMP, &timestamp);
				t = timestamp;
				return (1);
			}
		} else {
			return (1);
		}
	}

	if (zpool_import_props(g_zfs, config, newname, props, flags) != 0)
		return (1);

	if (newname != NULL)
		name = (char *)newname;

	if ((zhp = zpool_open_canfail(g_zfs, name)) == NULL)
		return (1);

	if (zpool_get_state(zhp) != POOL_STATE_UNAVAIL &&
	    !(flags & ZFS_IMPORT_ONLY) &&
	    zpool_enable_datasets(zhp, mntopts, 0) != 0) {
		zpool_close(zhp);
		return (1);
	}
	zpool_close(zhp);
	return (0);
}

static int
add_prop_list_default(const char *propname, char *propval, nvlist_t **props,
    boolean_t poolprop)
{
	char *pval;

	if (nvlist_lookup_string(*props, propname, &pval) == 0)
		return (0);

	return (add_prop_list(propname, propval, props, poolprop));
}

static int
zpool_import_callback(char *zpool_name, zval *params, zpool_imp_cb_t *cb)
{
	if ((g_zfs = libzfs_init()) == NULL) {
		return 1;
	}
	int ret = 0;
	char *cachefile = NULL;
	char **searchdirs = NULL;
	char *mntopts = NULL;
	nvlist_t *props = NULL;
	nvlist_t *pools = NULL;
	nvlist_t *policy = NULL;
	int nsearch = 0;
	boolean_t do_destroyed = B_FALSE;
	int flags = ZFS_IMPORT_NORMAL;
	boolean_t do_rewind = B_FALSE;
	boolean_t dryrun = B_FALSE;
	boolean_t xtreme_rewind = B_FALSE;
	uint32_t rewind_policy = ZPOOL_NO_REWIND;
	uint64_t searchguid = 0;
	char *searchname = NULL;
	importargs_t idata = { 0 };
	uint64_t pool_state, txg = -1ULL;
	nvlist_t *found_config = NULL;
	nvpair_t *elem;
	nvlist_t *config = NULL;
	char *newname = NULL;
	int first = 1;

	if (params != NULL) {
		HashTable *ht = Z_ARRVAL_P(params);
		HashPosition pointer;
		zval *item;
		if (cb->cb_type == 0 || cb->cb_type == 1) {
			if ((item = zend_hash_str_find(ht, "newname", sizeof("newname")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
				newname = Z_STRVAL_P(item);
				if ((item = zend_hash_str_find(ht, "temp_name", sizeof("temp_name")-1)) != NULL && Z_TYPE_P(item) == IS_TRUE) {
					flags |= ZFS_IMPORT_TEMP_NAME;
					if (add_prop_list_default(zpool_prop_to_name(
					    ZPOOL_PROP_CACHEFILE), "none", &props, B_TRUE)) {
						ret = 1;
						goto error;
					}
				}
			}
			if ((item = zend_hash_str_find(ht, "force", sizeof("force")-1)) != NULL && Z_TYPE_P(item) == IS_TRUE) {
				flags |= ZFS_IMPORT_ANY_HOST;
			}
			if ((item = zend_hash_str_find(ht, "rewind", sizeof("rewind")-1)) != NULL && Z_TYPE_P(item) == IS_TRUE) {
				do_rewind = B_TRUE;
			}
			if ((item = zend_hash_str_find(ht, "missing_log", sizeof("missing_log")-1)) != NULL && Z_TYPE_P(item) == IS_TRUE) {
				flags |= ZFS_IMPORT_MISSING_LOG;
			}
			if ((item = zend_hash_str_find(ht, "dryrun", sizeof("dryrun")-1)) != NULL && Z_TYPE_P(item) == IS_TRUE) {
				dryrun = B_TRUE;
			}
			if ((item = zend_hash_str_find(ht, "no_mount", sizeof("no_mount")-1)) != NULL && Z_TYPE_P(item) == IS_TRUE) {
				flags |= ZFS_IMPORT_ONLY;
			}
			if ((item = zend_hash_str_find(ht, "mntopts", sizeof("mntopts")-1)) != NULL && Z_TYPE_P(item) == IS_ARRAY) {
				HashTable *ht_item = Z_ARRVAL_P(item);
				zval *data;
				int len = 0;
				for(
			        zend_hash_internal_pointer_reset_ex(ht_item, &pointer);
			        (data = zend_hash_get_current_data_ex(ht_item, &pointer)) != NULL;
			        zend_hash_move_forward_ex(ht_item, &pointer)
			    ) {
					if (Z_TYPE_P(data) == IS_STRING) {
						mntopts = realloc(mntopts, len + strlen(Z_STRVAL_P(data)) + 2);
						len = len + strlen(Z_STRVAL_P(data)) + 1;
						if (first) {
							first = 0;
							snprintf(mntopts, len, "%s", Z_STRVAL_P(data));
						} else {
							snprintf(mntopts, len, "%s,%s", mntopts, Z_STRVAL_P(data));
						}
					}
				}
			}
			if ((item = zend_hash_str_find(ht, "props", sizeof("props")-1)) != NULL && Z_TYPE_P(item) == IS_ARRAY) {
				HashTable *ht_item = Z_ARRVAL_P(item);
				zval *data;
				for(
			        zend_hash_internal_pointer_reset_ex(ht_item, &pointer);
			        (data = zend_hash_get_current_data_ex(ht_item, &pointer)) != NULL;
			        zend_hash_move_forward_ex(ht_item, &pointer)
			    ) {
					if (Z_TYPE_P(data) == IS_STRING) {
						zend_ulong num_index;
						zend_string *str_index;
						zend_hash_get_current_key_ex(ht_item, &str_index, &num_index, &pointer);
						if (add_prop_list(ZSTR_VAL(str_index), Z_STRVAL_P(data),
						    &props, B_TRUE)) {
							ret = 1;
							goto error;
						}
					}
				}
			}
			if ((item = zend_hash_str_find(ht, "cachefile", sizeof("cachefile")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
				cachefile = Z_STRVAL_P(item);
			}
			if ((item = zend_hash_str_find(ht, "root", sizeof("root")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
				if (add_prop_list(zpool_prop_to_name(
				    ZPOOL_PROP_ALTROOT), Z_STRVAL_P(item), &props, B_TRUE)) {
					ret = 1;
					goto error;
				}
				if (add_prop_list_default(zpool_prop_to_name(
				    ZPOOL_PROP_CACHEFILE), "none", &props, B_TRUE)) {
					ret = 1;
					goto error;
				}
			}
		} else {
			if ((item = zend_hash_str_find(ht, "cachefile", sizeof("cachefile")-1)) != NULL && Z_TYPE_P(item) == IS_STRING) {
				cachefile = Z_STRVAL_P(item);
			}
		}

		if ((item = zend_hash_str_find(ht, "searchdirs", sizeof("searchdirs")-1)) != NULL && Z_TYPE_P(item) == IS_ARRAY) {
			HashTable *ht_item = Z_ARRVAL_P(item);
			zval *data;
			for(
		        zend_hash_internal_pointer_reset_ex(ht_item, &pointer);
		        (data = zend_hash_get_current_data_ex(ht_item, &pointer)) != NULL;
		        zend_hash_move_forward_ex(ht_item, &pointer)
		    ) {
				if (Z_TYPE_P(data) == IS_STRING) {
					if (searchdirs == NULL) {
						searchdirs = malloc(sizeof(char *));
					} else {
						char **tmp = malloc((nsearch + 1) *
						    sizeof (char *));
						bcopy(searchdirs, tmp, nsearch *
						    sizeof (char *));
						free(searchdirs);
						searchdirs = tmp;
					}
					searchdirs[nsearch++] = Z_STRVAL_P(data);
				}
			}
		}

		if ((item = zend_hash_str_find(ht, "destroyed", sizeof("destroyed")-1)) != NULL && Z_TYPE_P(item) == IS_TRUE) {
			do_destroyed = B_TRUE;
		}
	}
	if (cachefile && nsearch != 0) {
		ret = 1;
		goto error;
	}

	if ((dryrun || xtreme_rewind) && !do_rewind) {
		ret = 1;
		goto error;
	}

	if (dryrun)
		rewind_policy = ZPOOL_TRY_REWIND;
	else if (do_rewind)
		rewind_policy = ZPOOL_DO_REWIND;
	if (xtreme_rewind)
		rewind_policy |= ZPOOL_EXTREME_REWIND;

	if (nvlist_alloc(&policy, NV_UNIQUE_NAME, 0) != 0 ||
	    nvlist_add_uint64(policy, ZPOOL_LOAD_REQUEST_TXG, txg) != 0 ||
	    nvlist_add_uint32(policy, ZPOOL_LOAD_REWIND_POLICY,
	    rewind_policy) != 0) {
		ret = 1;
		goto error;
	}

	if (searchdirs == NULL) {
		searchdirs = malloc(sizeof (char *));
		searchdirs[0] = "/dev";
		nsearch = 1;
	}

	if (zpool_name != NULL) {
		char *endptr;
		errno = 0;
		searchguid = strtoull(zpool_name, &endptr, 10);
		if (errno != 0 || *endptr != '\0') {
			searchname = zpool_name;
			searchguid = 0;
		}
		found_config = NULL;
		idata.unique = B_TRUE;
	}

	idata.path = searchdirs;
	idata.paths = nsearch;
	idata.poolname = searchname;
	idata.guid = searchguid;
	idata.cachefile = cachefile;
	idata.policy = policy;


	pools = zpool_search_import(g_zfs, &idata);
	if (pools != NULL && idata.exists &&
	    (newname != NULL || strcmp(zpool_name, newname) == 0)) {
		ret = 1;
	} else if (pools == NULL && idata.exists) {
		ret = 1;
	} else if (pools == NULL) {
		ret = 1;
	}

	if (ret == 1) {
		free(searchdirs);
		nvlist_free(policy);
	    libzfs_fini(g_zfs);
	    if (!first)
	    	free(mntopts);
	    return 1;
	}

	ret = 0;
	elem = NULL;
	while ((elem = nvlist_next_nvpair(pools, elem)) != NULL) {
		nvpair_value_nvlist(elem, &config);

		nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_STATE,
		    &pool_state);
		if (!do_destroyed && pool_state == POOL_STATE_DESTROYED)
			continue;
		if (do_destroyed && pool_state != POOL_STATE_DESTROYED)
			continue;

		nvlist_add_nvlist(config, ZPOOL_LOAD_POLICY,
		    policy);
		if (zpool_name == NULL) {
			if (cb->cb_type == 1) {
				ret |= do_import(config, NULL, mntopts, props, flags);
			} else if (cb->cb_type == 2) {
				zval subitem;
				array_init(&subitem);
				uint64_t guid;
				nvlist_lookup_uint64(config,
				    ZPOOL_CONFIG_POOL_GUID, &guid);
				char tmp_guid[25];
				snprintf(tmp_guid, sizeof(tmp_guid), "%lu", guid);
				fill_import_info(config, &subitem);
				if (zend_hash_num_elements(Z_ARRVAL_P(&subitem)) > 0)
					add_assoc_zval(cb->cb_ret, tmp_guid, &subitem);
			}
		} else if (searchname != NULL) {
			char *name;
			nvlist_lookup_string(config,
			    ZPOOL_CONFIG_POOL_NAME, &name);
			if (strcmp(name, searchname) == 0) {
				if (found_config != NULL) {
					ret = B_TRUE;
				}
				found_config = config;
				break;
			}
		} else {
			uint64_t guid;
			nvlist_lookup_uint64(config,
			    ZPOOL_CONFIG_POOL_GUID, &guid);
			if (guid == searchguid) {
				found_config = config;
			}
		}
	}
	if (cb->cb_type == 0)
		ret |= do_import(found_config, newname, mntopts, props, flags);
error:
    if (!first)
    	free(mntopts);
	nvlist_free(props);
	nvlist_free(pools);
	nvlist_free(policy);
	free(searchdirs);
    libzfs_fini(g_zfs);
    return (ret);
}

static void
fill_import_info(nvlist_t *config, zval *subitem)
{
	char *name;
	uint64_t pool_state;
	vdev_stat_t *vs;
	uint_t vsc;
	char *msgid;
	char *comment;
	nvlist_t *nvroot;
	int reason = 0;

	nvlist_lookup_string(config,
	    ZPOOL_CONFIG_POOL_NAME, &name);
	add_assoc_string(subitem, "name", name);

	nvlist_lookup_uint64(config, ZPOOL_CONFIG_POOL_STATE,
	    &pool_state);
	nvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE,
	    &nvroot);
	nvlist_lookup_uint64_array(nvroot, ZPOOL_CONFIG_VDEV_STATS,
	    (uint64_t **)&vs, &vsc);
	add_assoc_string(subitem, "state", zpool_state_to_name(vs->vs_state, vs->vs_aux));

	if (pool_state == POOL_STATE_DESTROYED)
		add_assoc_bool(subitem, "destroyed", 1);
	else
		add_assoc_bool(subitem, "destroyed", 0);

	reason = zpool_import_status(config, &msgid);
	switch (reason) {
	case ZPOOL_STATUS_MISSING_DEV_R:
	case ZPOOL_STATUS_MISSING_DEV_NR:
	case ZPOOL_STATUS_BAD_GUID_SUM:
		add_assoc_string(subitem, "reason", "One or more devices are "
		    "missing from the system.");
		break;

	case ZPOOL_STATUS_CORRUPT_LABEL_R:
	case ZPOOL_STATUS_CORRUPT_LABEL_NR:
		add_assoc_string(subitem, "reason", "One or more devices contains "
		    "corrupted data.");
		break;

	case ZPOOL_STATUS_CORRUPT_DATA:
		add_assoc_string(subitem, "reason", "The pool data is corrupted.");
		break;

	case ZPOOL_STATUS_OFFLINE_DEV:
		add_assoc_string(subitem, "reason", "One or more devices "
		    "are offlined.");
		break;

	case ZPOOL_STATUS_CORRUPT_POOL:
		add_assoc_string(subitem, "reason", "The pool metadata is "
		    "corrupted.");
		break;

	case ZPOOL_STATUS_VERSION_OLDER:
		add_assoc_string(subitem, "reason", "The pool is formatted using a "
		    "legacy on-disk version.");
		break;

	case ZPOOL_STATUS_VERSION_NEWER:
		add_assoc_string(subitem, "reason", "The pool is formatted using an "
		    "incompatible version.");
		break;

	case ZPOOL_STATUS_FEAT_DISABLED:
		add_assoc_string(subitem, "reason", "Some supported features are "
		    "not enabled on the pool.");
		break;

	case ZPOOL_STATUS_UNSUP_FEAT_READ:
		add_assoc_string(subitem, "reason", "The pool uses "
		    "feature(s) that is not supported on this system.");
		break;

	case ZPOOL_STATUS_UNSUP_FEAT_WRITE:
		add_assoc_string(subitem, "reason", "The pool can only be accessed "
		    "in read-only mode on this system. It cannot be "
		    "accessed in read-write mode because it uses "
		    "feature(s) that is not supported on this system.");
		break;

	case ZPOOL_STATUS_HOSTID_MISMATCH:
		add_assoc_string(subitem, "reason", "The pool was last accessed by "
		    "another system.");
		break;

	case ZPOOL_STATUS_FAULTED_DEV_R:
	case ZPOOL_STATUS_FAULTED_DEV_NR:
		add_assoc_string(subitem, "reason", "One or more devices are "
		    "faulted.");
		break;

	case ZPOOL_STATUS_BAD_LOG:
		add_assoc_string(subitem, "reason", "An intent log record cannot be "
		    "read.");
		break;

	case ZPOOL_STATUS_RESILVERING:
		add_assoc_string(subitem, "reason", "One or more devices were being "
		    "resilvered.");
		break;

	case ZPOOL_STATUS_NON_NATIVE_ASHIFT:
		add_assoc_string(subitem, "reason", "One or more devices were "
		    "configured to use a non-native block size. "
		    "Expect reduced performance.");
		break;

	default:
		break;
	}

	if (vs->vs_state == VDEV_STATE_HEALTHY) {
		if (reason == ZPOOL_STATUS_VERSION_OLDER ||
		    reason == ZPOOL_STATUS_FEAT_DISABLED) {
			add_assoc_string(subitem, "action", "The pool can be "
			    "imported using its name or numeric identifier, "
			    "though some features will not be available "
			    "without an explicit zpool upgrade.");
		} else if (reason == ZPOOL_STATUS_HOSTID_MISMATCH) {
			add_assoc_string(subitem, "action", "The pool can be "
			    "imported using its name or numeric "
			    "identifier and the force flag.");
		} else {
			add_assoc_string(subitem, "action", "The pool can be "
			    "imported using its name or numeric "
			    "identifier.");
		}
	} else if (vs->vs_state == VDEV_STATE_DEGRADED) {
		add_assoc_string(subitem, "action", "The pool can be imported "
		    "despite missing or damaged devices.  The fault "
		    "tolerance of the pool may be compromised if imported.");
	} else {
		switch (reason) {
		case ZPOOL_STATUS_VERSION_NEWER:
			add_assoc_string(subitem, "action", "The pool cannot be "
			    "imported.  Access the pool on a system running "
			    "newer software, or recreate the pool from "
			    "backup.");
			break;
		case ZPOOL_STATUS_UNSUP_FEAT_READ:
			add_assoc_string(subitem, "action", "The pool cannot be "
			    "imported. Access the pool on a system that "
			    "supports the required feature(s), or recreate "
			    "the pool from backup.");
			break;
		case ZPOOL_STATUS_UNSUP_FEAT_WRITE:
			add_assoc_string(subitem, "action", "The pool cannot be "
			    "imported in read-write mode. Import the pool "
			    "with readonly property, access the pool on a system "
			    "that supports the required feature(s), or "
			    "recreate the pool from backup.");
			break;
		case ZPOOL_STATUS_MISSING_DEV_R:
		case ZPOOL_STATUS_MISSING_DEV_NR:
		case ZPOOL_STATUS_BAD_GUID_SUM:
			add_assoc_string(subitem, "action", "The pool cannot be "
			    "imported. Attach the missing devices and try "
			    "again.");
			break;
		default:
			add_assoc_string(subitem, "action", "The pool cannot be "
			    "imported due to damaged devices or data.");
		}
	}

	if (nvlist_lookup_string(config, ZPOOL_CONFIG_COMMENT, &comment) == 0) {
		add_assoc_string(subitem, "comment", comment);

	}

	if (((vs->vs_state == VDEV_STATE_CLOSED) ||
	    (vs->vs_state == VDEV_STATE_CANT_OPEN)) &&
	    (vs->vs_aux == VDEV_AUX_CORRUPT_DATA)) {
		if (pool_state == POOL_STATE_DESTROYED)
			add_assoc_string(subitem, "msg", "The pool was destroyed, "
			    "but can be imported using the destroyed and force flags.");
		else if (pool_state != POOL_STATE_EXPORTED)
			add_assoc_string(subitem, "msg", "The pool may be active on "
			    "another system, but can be imported using"
			    "the force flag.");
	}

	zpool_get_import_config(name, nvroot, 0, subitem);
	if (num_logs(nvroot) > 0)
		zpool_get_logs(NULL, nvroot, B_FALSE, subitem);

	if (reason == ZPOOL_STATUS_BAD_GUID_SUM) {
		add_assoc_string(subitem, "warn", "Additional devices are known to "
		    "be part of this pool, though their exact "
		    "configuration cannot be determined.");
	}

}

static void
zpool_get_import_config(const char *name, nvlist_t *nv, int depth, zval *subitem)
{
	nvlist_t **child;
	uint_t c, children;
	vdev_stat_t *vs;
	char *type, *vname;

	nvlist_lookup_string(nv, ZPOOL_CONFIG_TYPE, &type);
	if (strcmp(type, VDEV_TYPE_MISSING) == 0 ||
	    strcmp(type, VDEV_TYPE_HOLE) == 0)
		return;

	nvlist_lookup_uint64_array(nv, ZPOOL_CONFIG_VDEV_STATS,
	    (uint64_t **)&vs, &c);

	if (depth != 0) {
		add_assoc_string(subitem, "name", name);
		add_assoc_string(subitem, "state", zpool_state_to_name(vs->vs_state, vs->vs_aux));

		if (vs->vs_aux != 0) {
			(void) printf("  ");

			switch (vs->vs_aux) {
			case VDEV_AUX_OPEN_FAILED:
				add_assoc_string(subitem, "msg", "cannot open");
				break;

			case VDEV_AUX_BAD_GUID_SUM:
				add_assoc_string(subitem, "msg", "missing device");
				break;

			case VDEV_AUX_NO_REPLICAS:
				add_assoc_string(subitem, "msg", "insufficient replicas");
				break;

			case VDEV_AUX_VERSION_NEWER:
				add_assoc_string(subitem, "msg", "newer version");
				break;

			case VDEV_AUX_UNSUP_FEAT:
				add_assoc_string(subitem, "msg", "unsupported feature(s)");
				break;

			case VDEV_AUX_ERR_EXCEEDED:
				add_assoc_string(subitem, "msg", "too many errors");
				break;

			case VDEV_AUX_CHILDREN_OFFLINE:
				add_assoc_string(subitem, "msg", "all children offline");
				break;

			default:
				add_assoc_string(subitem, "msg", "corrupted data");
				break;
			}
		}
	}

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0)
		return;

	zval subsub;
	array_init(&subsub);
	for (c = 0; c < children; c++) {
		uint64_t is_log = B_FALSE;

		(void) nvlist_lookup_uint64(child[c], ZPOOL_CONFIG_IS_LOG,
		    &is_log);
		if (is_log)
			continue;
		zval subsubsub;
		array_init(&subsubsub);
		vname = zpool_vdev_name(g_zfs, NULL, child[c], B_TRUE);
		zpool_get_import_config(vname, child[c], depth + 2, &subsubsub);
		free(vname);
		add_next_index_zval(&subsub, &subsubsub);
	}

	if (zend_hash_num_elements(Z_ARRVAL_P(&subsub)) > 0)
		add_assoc_zval(subitem, "config", &subsub);

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_L2CACHE,
	    &child, &children) == 0) {
		array_init(&subsub);
		for (c = 0; c < children; c++) {
			vname = zpool_vdev_name(g_zfs, NULL, child[c], B_FALSE);
			add_next_index_string(&subsub, vname);
			free(vname);
		}
		if (zend_hash_num_elements(Z_ARRVAL_P(&subsub)) > 0)
			add_assoc_zval(subitem, "cache", &subsub);
	}

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_SPARES,
	    &child, &children) == 0) {
		array_init(&subsub);
		for (c = 0; c < children; c++) {
			vname = zpool_vdev_name(g_zfs, NULL, child[c], B_FALSE);
			add_next_index_string(&subsub, vname);
			free(vname);
		}
		if (zend_hash_num_elements(Z_ARRVAL_P(&subsub)) > 0)
			add_assoc_zval(subitem, "spares", &subsub);
	}
}


static int
zpool_status_callback(zpool_handle_t *zhp, void *data)
{
	status_cbdata_t *cbp = data;
	nvlist_t *config, *nvroot;
	char *msgid;
	int reason;
	const char *health;
	uint_t c;
	vdev_stat_t *vs;
	zval *item;
	zval info;
	if (cbp->cb_allpools) {
		array_init(&info);
		item = &info;
	} else {
		item = cbp->cb_ret;
	}

	config = zpool_get_config(zhp, NULL);
	reason = zpool_get_status(zhp, &msgid);

	cbp->cb_count++;
	nvroot = fnvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE);
	nvlist_lookup_uint64_array(nvroot, ZPOOL_CONFIG_VDEV_STATS,
	    (uint64_t **)&vs, &c);
	health = zpool_state_to_name(vs->vs_state, vs->vs_aux);

	add_assoc_string(item, "name", zpool_get_name(zhp));
	add_assoc_string(item, "state", health);

	switch (reason) {
	case ZPOOL_STATUS_MISSING_DEV_R:
		add_assoc_string(item, "status", "One or more devices could not "
		    "be opened.  Sufficient replicas exist for the pool to "
		    "continue functioning in a degraded state.");
		add_assoc_string(item, "action", "Attach the missing device and "
		    "online it using 'zpool online'.");
		break;

	case ZPOOL_STATUS_MISSING_DEV_NR:
		add_assoc_string(item, "status", "One or more devices could not "
		    "be opened.  There are insufficient replicas for the "
		    "pool to continue functioning.");
		add_assoc_string(item, "action", "Attach the missing device and "
		    "online it using 'zpool online'.");
		break;

	case ZPOOL_STATUS_CORRUPT_LABEL_R:
		add_assoc_string(item, "status", "One or more devices could not "
		    "be used because the label is missing or invalid.  "
		    "Sufficient replicas exist for the pool to continue "
		    "functioning in a degraded state.");
		add_assoc_string(item, "action", "Replace the device using "
		    "'zpool replace'.");
		break;

	case ZPOOL_STATUS_CORRUPT_LABEL_NR:
		add_assoc_string(item, "status", "One or more devices could not "
		    "be used because the label is missing or invalid.  "
		    "There are insufficient replicas for the pool to "
		    "continue functioning.");
		// zpool_explain_recover(zpool_get_handle(zhp),
		//     zpool_get_name(zhp), reason, config);
		break;

	case ZPOOL_STATUS_FAILING_DEV:
		add_assoc_string(item, "status", "One or more devices has "
		    "experienced an unrecoverable error.  An attempt was "
		    "made to correct the error.  Applications are "
		    "unaffected.");
		add_assoc_string(item, "action", "Determine if the device needs "
		    "to be replaced, and clear the errors using "
		    "'zpool clear' or replace the device with 'zpool "
		    "replace'.");
		break;

	case ZPOOL_STATUS_OFFLINE_DEV:
		add_assoc_string(item, "status", "One or more devices has "
		    "been taken offline by the administrator. Sufficient "
		    "replicas exist for the pool to continue functioning in "
		    "a degraded state.");
		add_assoc_string(item, "action", "Online the device using "
		    "'zpool online' or replace the device with 'zpool "
		    "replace'.");
		break;

	case ZPOOL_STATUS_REMOVED_DEV:
		add_assoc_string(item, "status", "One or more devices has "
		    "been removed by the administrator. Sufficient "
		    "replicas exist for the pool to continue functioning in "
		    "a degraded state.");
		add_assoc_string(item, "action", "Online the device using "
		    "'zpool online' or replace the device with 'zpool "
		    "replace'.");
		break;

	case ZPOOL_STATUS_RESILVERING:
		add_assoc_string(item, "status", "One or more devices is "
		    "currently being resilvered.  The pool will continue "
		    "to function, possibly in a degraded state.");
		add_assoc_string(item, "action", "Wait for the resilver to "
		    "complete.");
		break;

	case ZPOOL_STATUS_CORRUPT_DATA:
		add_assoc_string(item, "status", "One or more devices has "
		    "experienced an error resulting in data corruption.  "
		    "Applications may be affected.");
		add_assoc_string(item, "action", "Restore the file in question "
		    "if possible.  Otherwise restore the entire pool from "
		    "backup.");
		break;

	case ZPOOL_STATUS_CORRUPT_POOL:
		add_assoc_string(item, "status", "The pool metadata is corrupted "
		    "and the pool cannot be opened.");
		// zpool_explain_recover(zpool_get_handle(zhp),
		//     zpool_get_name(zhp), reason, config);
		break;

	case ZPOOL_STATUS_VERSION_OLDER:
		add_assoc_string(item, "status", "The pool is formatted using a "
		    "legacy on-disk format.  The pool can still be used, "
		    "but some features are unavailable.");
		add_assoc_string(item, "action", "Upgrade the pool using 'zpool "
		    "upgrade'.  Once this is done, the pool will no longer "
		    "be accessible on software that does not support feature"
		    " flags.");
		break;

	case ZPOOL_STATUS_VERSION_NEWER:
		add_assoc_string(item, "status", "The pool has been upgraded to a "
		    "newer, incompatible on-disk version. The pool cannot "
		    "be accessed on this system.");
		add_assoc_string(item, "action", "Access the pool from a system "
		    "running more recent software, or restore the pool from "
		    "backup.");
		break;

	case ZPOOL_STATUS_FEAT_DISABLED:
		add_assoc_string(item, "status", "Some supported features are not "
		    "enabled on the pool. The pool can still be used, but "
		    "some features are unavailable.");
		add_assoc_string(item, "action", "Enable all features using "
		    "'zpool upgrade'. Once this is done, the pool may no "
		    "longer be accessible by software that does not support "
		    "the features. See zpool-features(7) for details.");
		break;

	case ZPOOL_STATUS_UNSUP_FEAT_READ:
		add_assoc_string(item, "status", "The pool cannot be accessed on "
		    "this system because it uses feature(s) "
		    "that is not supported on this system.");
		add_assoc_string(item, "action", "Access the pool from a system "
		    "that supports the required feature(s), or restore the "
		    "pool from backup.");
		break;

	case ZPOOL_STATUS_UNSUP_FEAT_WRITE:
		add_assoc_string(item, "status", "The pool can only be accessed "
		    "in read-only mode on this system. It cannot be "
		    "accessed in read-write mode because it uses "
		    "feature(s) that is not supported on this system.");
		add_assoc_string(item, "action", "The pool cannot be accessed in "
		    "read-write mode. Import the pool with"
		    "readonly property, access the pool from a system that "
		    "supports the required feature(s), or restore the "
		    "pool from backup.");
		break;

	case ZPOOL_STATUS_FAULTED_DEV_R:
		add_assoc_string(item, "status", "One or more devices are "
		    "faulted in response to persistent errors. Sufficient "
		    "replicas exist for the pool to continue functioning "
		    "in a degraded state.");
		add_assoc_string(item, "action", "Replace the faulted device, "
		    "or use 'zpool clear' to mark the device repaired.");
		break;

	case ZPOOL_STATUS_FAULTED_DEV_NR:
		add_assoc_string(item, "status", "One or more devices are "
		    "faulted in response to persistent errors.  There are "
		    "insufficient replicas for the pool to continue "
		    "functioning.");
		add_assoc_string(item, "action", "Destroy and re-create the pool "
		    "from a backup source.  Manually marking the device"
		    " repaired using 'zpool clear' may allow some data "
		    "to be recovered.");
		break;

	case ZPOOL_STATUS_IO_FAILURE_WAIT:
	case ZPOOL_STATUS_IO_FAILURE_CONTINUE:
		add_assoc_string(item, "status", "One or more devices are "
		    "faulted in response to IO failures.");
		add_assoc_string(item, "action", "Make sure the affected devices "
		    "are connected, then run 'zpool clear'.");
		break;

	case ZPOOL_STATUS_BAD_LOG:
		add_assoc_string(item, "status", "An intent log record "
		    "could not be read."
		    " Waiting for adminstrator intervention to fix the "
		    "faulted pool.");
		add_assoc_string(item, "action", "Either restore the affected "
		    "device(s) and run 'zpool online', "
		    "or ignore the intent log records by running "
		    "'zpool clear'.");
		break;

	case ZPOOL_STATUS_NON_NATIVE_ASHIFT:
		add_assoc_string(item, "status", "One or more devices are "
		    "configured to use a non-native block size. "
		    "Expect reduced performance.");
		add_assoc_string(item, "action", "Replace affected devices with "
		    "devices that support the configured block size, or "
		    "migrate data to a properly configured pool.");
		break;
	default:
		break;
	}

	if (config != NULL) {
		int namewidth;
		uint64_t nerr;
		nvlist_t **spares, **l2cache;
		uint_t nspares, nl2cache;
		pool_checkpoint_stat_t *pcs = NULL;
		pool_scan_stat_t *ps = NULL;
		pool_removal_stat_t *prs = NULL;

		(void) nvlist_lookup_uint64_array(nvroot,
		    ZPOOL_CONFIG_CHECKPOINT_STATS, (uint64_t **)&pcs, &c);
		(void) nvlist_lookup_uint64_array(nvroot,
		    ZPOOL_CONFIG_SCAN_STATS, (uint64_t **)&ps, &c);
		(void) nvlist_lookup_uint64_array(nvroot,
		    ZPOOL_CONFIG_REMOVAL_STATS, (uint64_t **)&prs, &c);

		zpool_get_scan_status(ps, item);
		zpool_get_checkpoint_scan_warning(ps, pcs, item);
		zpool_get_removal_status(zhp, prs, item);
		zpool_get_checkpoint_status(pcs, item);

		zpool_get_status_config(zhp, zpool_get_name(zhp), nvroot, 0, B_FALSE, item);

		if (num_logs(nvroot) > 0)
			zpool_get_logs(zhp, nvroot, B_TRUE, item);
		if (nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_L2CACHE,
		    &l2cache, &nl2cache) == 0)
			zpool_get_l2cache(zhp, l2cache, nl2cache, item);

		if (nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_SPARES,
		    &spares, &nspares) == 0)
			zpool_get_spares(zhp, spares, nspares, item);

		if (nvlist_lookup_uint64(config, ZPOOL_CONFIG_ERRCOUNT,
		    &nerr) == 0) {
			nvlist_t *nverrlist = NULL;

			/*
			 * If the approximate error count is small, get a
			 * precise count by fetching the entire log and
			 * uniquifying the results.
			 */
			if (nerr > 0 && nerr < 100 && !cbp->cb_verbose &&
			    zpool_get_errlog(zhp, &nverrlist) == 0) {
				nvpair_t *elem;

				elem = NULL;
				nerr = 0;
				while ((elem = nvlist_next_nvpair(nverrlist,
				    elem)) != NULL) {
					nerr++;
				}
			}
			nvlist_free(nverrlist);

			if (nerr == 0)
				add_assoc_string(item, "err", "No known data errors");
			else if (!cbp->cb_verbose)
				add_assoc_string(item, "err", "data errors");
			else
				zpool_get_error_log(zhp, item);
		}

	} else {
		add_assoc_string(item, "config", "The configuration cannot be determined.");
	}

	if (cbp->cb_allpools) {
		add_next_index_zval(cbp->cb_ret, item);
	}
	return (0);
}

static void
zpool_devs_list(zpool_handle_t *zhp, nvlist_t *nv, const char *name, 
    int depth, zval *info)
{
	nvlist_t **child;
	char *vname;
	uint_t c, children;
	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0)
		return;
	for (c = 0; c < children; c++) {
		uint64_t islog = B_FALSE, ishole = B_FALSE;

		(void) nvlist_lookup_uint64(child[c], ZPOOL_CONFIG_IS_LOG,
		    &islog);
		(void) nvlist_lookup_uint64(child[c], ZPOOL_CONFIG_IS_HOLE,
		    &ishole);
		if (islog || ishole)
			continue;
		vname = zpool_vdev_name(g_zfs, zhp, child[c], B_TRUE);
		// zpool_devs_list(zhp, child[c], vname, depth + 2, NULL);
		if (!depth)
			add_next_index_string(info, vname);
		free(vname);
	}
}

static void
zpool_get_status_config(zpool_handle_t *zhp, const char *name, nvlist_t *nv, int depth, boolean_t isspare, zval *item)
{
	nvlist_t **child;
	uint_t c, vsc, children;
	pool_scan_stat_t *ps = NULL;
	vdev_stat_t *vs;
	char rbuf[6], wbuf[6], cbuf[6];
	char *vname;
	uint64_t notpresent;
	uint64_t ashift;
	spare_cbdata_t cb;
	const char *state;
	char *type;

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0)
		children = 0;
	if (depth != 0) {

		nvlist_lookup_uint64_array(nv, ZPOOL_CONFIG_VDEV_STATS,
		    (uint64_t **)&vs, &vsc);

		nvlist_lookup_string(nv, ZPOOL_CONFIG_TYPE, &type);

		if (strcmp(type, VDEV_TYPE_INDIRECT) == 0)
			return;

		state = zpool_state_to_name(vs->vs_state, vs->vs_aux);
		if (isspare) {
			/*
			 * For hot spares, we use the terms 'INUSE' and 'AVAILABLE' for
			 * online drives.
			 */
			if (vs->vs_aux == VDEV_AUX_SPARED)
				state = "INUSE";
			else if (vs->vs_state == VDEV_STATE_HEALTHY)
				state = "AVAIL";
		}

		add_assoc_string(item, "name", name);
		add_assoc_string(item, "state", state);

		if (!isspare) {
			zfs_nicenum(vs->vs_read_errors, rbuf, sizeof (rbuf));
			zfs_nicenum(vs->vs_write_errors, wbuf, sizeof (wbuf));
			zfs_nicenum(vs->vs_checksum_errors, cbuf, sizeof (cbuf));
			add_assoc_string(item, "read", rbuf);
			add_assoc_string(item, "write", wbuf);
			add_assoc_string(item, "cksum", cbuf);
		}

		if (nvlist_lookup_uint64(nv, ZPOOL_CONFIG_NOT_PRESENT,
		    &notpresent) == 0 ||
		    vs->vs_state <= VDEV_STATE_CANT_OPEN) {
			char *path;
			if (nvlist_lookup_string(nv, ZPOOL_CONFIG_PATH, &path) == 0)
				add_assoc_string(item, "config_path", path);
		} else if (vs->vs_aux != 0) {
			switch (vs->vs_aux) {
			case VDEV_AUX_OPEN_FAILED:
				add_assoc_string(item, "msg", "cannot open");
				break;

			case VDEV_AUX_BAD_GUID_SUM:
				add_assoc_string(item, "msg", "missing device");
				break;

			case VDEV_AUX_NO_REPLICAS:
				add_assoc_string(item, "msg", "insufficient replicas");
				break;

			case VDEV_AUX_VERSION_NEWER:
				add_assoc_string(item, "msg", "newer version");
				break;

			case VDEV_AUX_UNSUP_FEAT:
				add_assoc_string(item, "msg", "unsupported feature(s)");
				break;

			case VDEV_AUX_ASHIFT_TOO_BIG:
				add_assoc_string(item, "msg", "unsupported minimum blocksize");
				break;

			case VDEV_AUX_SPARED:
				nvlist_lookup_uint64(nv, ZPOOL_CONFIG_GUID,
				    &cb.cb_guid);
				if (zpool_iter(g_zfs, find_spare, &cb) == 1) {
					if (strcmp(zpool_get_name(cb.cb_zhp),
					    zpool_get_name(zhp)) == 0)
						add_assoc_string(item, "msg", "currently in "
						    "use");
					else
						add_assoc_string(item, "msg", "currently in "
						    "use");
					zpool_close(cb.cb_zhp);
				} else {
						add_assoc_string(item, "msg", "currently in "
						    "use");
				}
				break;

			case VDEV_AUX_ERR_EXCEEDED:
				add_assoc_string(item, "msg", "too many errors");
				break;

			case VDEV_AUX_IO_FAILURE:
				add_assoc_string(item, "msg", "experienced I/O failures");
				break;

			case VDEV_AUX_BAD_LOG:
				add_assoc_string(item, "msg", "bad intent log");
				break;

			case VDEV_AUX_EXTERNAL:
				add_assoc_string(item, "msg", "external device fault");
				break;

			case VDEV_AUX_SPLIT_POOL:
				add_assoc_string(item, "msg", "split into new pool");
				break;

			case VDEV_AUX_CHILDREN_OFFLINE:
				add_assoc_string(item, "msg", "all children offline");
				break;

			default:
				add_assoc_string(item, "msg", "corrupted data");
				break;
			}
		} else if (children == 0 && !isspare &&
		    VDEV_STAT_VALID(vs_physical_ashift, vsc) &&
		    vs->vs_configured_ashift < vs->vs_physical_ashift) {
			zval bl_sz;
			array_init(&bl_sz);
			add_assoc_long(&bl_sz, "configured", 1 << vs->vs_configured_ashift);
			add_assoc_long(&bl_sz, "native", 1 << vs->vs_physical_ashift);
			add_assoc_zval(item, "block_size", &bl_sz);
		}

		(void) nvlist_lookup_uint64_array(nv, ZPOOL_CONFIG_SCAN_STATS,
		    (uint64_t **)&ps, &c);

		if (ps && ps->pss_state == DSS_SCANNING &&
		    vs->vs_scan_processed != 0 && children == 0) {
			add_assoc_string(item, "act", (ps->pss_func == POOL_SCAN_RESILVER) ?
			    "resilvering" : "repairing");
		}
	}

	zval subsub;
	array_init(&subsub);
	for (c = 0; c < children; c++) {
		uint64_t islog = B_FALSE, ishole = B_FALSE;

		(void) nvlist_lookup_uint64(child[c], ZPOOL_CONFIG_IS_LOG,
		    &islog);
		(void) nvlist_lookup_uint64(child[c], ZPOOL_CONFIG_IS_HOLE,
		    &ishole);
		if (islog || ishole)
			continue;
		zval subsubsub;
		array_init(&subsubsub);
		vname = zpool_vdev_name(g_zfs, zhp, child[c], B_TRUE);
		zpool_get_status_config(zhp, vname, child[c], depth + 2, isspare, &subsubsub);
		add_next_index_zval(&subsub, &subsubsub);
		free(vname);
	}
	if (zend_hash_num_elements(Z_ARRVAL_P(&subsub)) > 0)
		add_assoc_zval(item, "config", &subsub);
}


static inline int
prop_cmp(const void *a, const void *b)
{
	const char *str1 = *(const char **)a;
	const char *str2 = *(const char **)b;
	return (strcmp(str1, str2));
}


static void
zpool_get_scan_status(pool_scan_stat_t *ps, zval *item)
{
	time_t start, end, pause;
	uint64_t elapsed, mins_left, hours_left;
	uint64_t pass_exam, examined, total;
	uint_t rate;
	double fraction_done;
	char processed_buf[7], examined_buf[7], total_buf[7], rate_buf[7];

	/* If there's never been a scan, there's not much to say. */
	if (ps == NULL || ps->pss_func == POOL_SCAN_NONE ||
	    ps->pss_func >= POOL_SCAN_FUNCS) {
		add_assoc_string(item, "scan", "none requested");
		return;
	}

	start = ps->pss_start_time;
	end = ps->pss_end_time;
	pause = ps->pss_pass_scrub_pause;
	zfs_nicenum(ps->pss_processed, processed_buf, sizeof (processed_buf));

	assert(ps->pss_func == POOL_SCAN_SCRUB ||
	    ps->pss_func == POOL_SCAN_RESILVER);
	/*
	 * Scan is finished or canceled.
	 */
	if (ps->pss_state == DSS_FINISHED) {
		uint64_t minutes_taken = (end - start) / 60;
		char *fmt = NULL;

		if (ps->pss_func == POOL_SCAN_SCRUB) {
			fmt = gettext("scrub repaired %s in %lluh%um with "
			    "%llu errors on %s");
		} else if (ps->pss_func == POOL_SCAN_RESILVER) {
			fmt = gettext("resilvered %s in %lluh%um with "
			    "%llu errors on %s");
		}
		/* LINTED */
		char tmp_fmt[strlen(fmt) + 50];
		snprintf(tmp_fmt, sizeof(tmp_fmt),fmt, processed_buf,
		    (u_longlong_t)(minutes_taken / 60),
		    (uint_t)(minutes_taken % 60),
		    (u_longlong_t)ps->pss_errors,
		    ctime((time_t *)&end));
		add_assoc_string(item, "scan", tmp_fmt);
		return;
	} else if (ps->pss_state == DSS_CANCELED) {
		if (ps->pss_func == POOL_SCAN_SCRUB) {
			char tmp_fmt[strlen("scrub canceled on %s") + 15];
			snprintf(tmp_fmt, sizeof(tmp_fmt), "scrub canceled on %s",
			    ctime(&end));
			add_assoc_string(item, "scan", tmp_fmt);
		} else if (ps->pss_func == POOL_SCAN_RESILVER) {
			char tmp_fmt[strlen("resilver canceled on %s") + 15];
			snprintf(tmp_fmt, sizeof(tmp_fmt), "resilver canceled on %s",
			    ctime(&end));
			add_assoc_string(item, "scan", tmp_fmt);
		}
		return;
	}

	assert(ps->pss_state == DSS_SCANNING);

	char tmp_fmt[512];
	if (ps->pss_func == POOL_SCAN_SCRUB) {
		if (pause == 0) {
			snprintf(tmp_fmt, sizeof(tmp_fmt), "%s scrub in progress since %s", tmp_fmt,
			    ctime(&start));
		} else {
			char buf[32];
			struct tm *p = localtime(&pause);
			(void) strftime(buf, sizeof (buf), "%a %b %e %T %Y", p);
			snprintf(tmp_fmt, sizeof(tmp_fmt), "%s scrub paused since %s, scrub started on %s", tmp_fmt, buf, ctime(&start));
		}
	} else if (ps->pss_func == POOL_SCAN_RESILVER) {
		snprintf(tmp_fmt, sizeof(tmp_fmt), "%s resilver in progress since %s", tmp_fmt,
		    ctime(&start));
	}

	examined = ps->pss_examined ? ps->pss_examined : 1;
	total = ps->pss_to_examine;
	fraction_done = (double)examined / total;

	/* elapsed time for this pass */
	elapsed = time(NULL) - ps->pss_pass_start;
	elapsed -= ps->pss_pass_scrub_spent_paused;
	elapsed = elapsed ? elapsed : 1;
	pass_exam = ps->pss_pass_exam ? ps->pss_pass_exam : 1;
	rate = pass_exam / elapsed;
	rate = rate ? rate : 1;
	mins_left = ((total - examined) / rate) / 60;
	hours_left = mins_left / 60;

	zfs_nicenum(examined, examined_buf, sizeof (examined_buf));
	zfs_nicenum(total, total_buf, sizeof (total_buf));

	/*
	 * do not print estimated time if hours_left is more than 30 days
	 * or we have a paused scrub
	 */
	if (pause == 0) {
		zfs_nicenum(rate, rate_buf, sizeof (rate_buf));
		snprintf(tmp_fmt, sizeof(tmp_fmt), "%s, %s scanned out of %s at %s", tmp_fmt,
		    examined_buf, total_buf, rate_buf);
		if (hours_left < (30 * 24)) {
			snprintf(tmp_fmt, sizeof(tmp_fmt), "%s, %lluh%um to go", tmp_fmt,
			    (u_longlong_t)hours_left, (uint_t)(mins_left % 60));
		} else {
			snprintf(tmp_fmt, sizeof(tmp_fmt), "%s, (scan is slow, no estimated time)", tmp_fmt);
		}
	} else {
		snprintf(tmp_fmt, sizeof(tmp_fmt), "%s, %s scanned out of %s", tmp_fmt,
		    examined_buf, total_buf);
	}

	if (ps->pss_func == POOL_SCAN_RESILVER) {
		snprintf(tmp_fmt, sizeof(tmp_fmt), "%s, %s resilvered, %.2f%% done", tmp_fmt,
		    processed_buf, 100 * fraction_done);
	} else if (ps->pss_func == POOL_SCAN_SCRUB) {
		snprintf(tmp_fmt, sizeof(tmp_fmt), "%s, %s repaired, %.2f%% done", tmp_fmt,
		    processed_buf, 100 * fraction_done);
	}
	add_assoc_string(item, "scan", tmp_fmt);
}

static void
zpool_get_checkpoint_scan_warning(pool_scan_stat_t *ps, pool_checkpoint_stat_t *pcs, zval *item)
{
	if (ps == NULL || pcs == NULL)
		return;

	if (pcs->pcs_state == CS_NONE ||
	    pcs->pcs_state == CS_CHECKPOINT_DISCARDING)
		return;

	assert(pcs->pcs_state == CS_CHECKPOINT_EXISTS);

	if (ps->pss_state == DSS_NONE)
		return;

	if ((ps->pss_state == DSS_FINISHED || ps->pss_state == DSS_CANCELED) &&
	    ps->pss_end_time < pcs->pcs_start_time)
		return;

	if (ps->pss_state == DSS_FINISHED || ps->pss_state == DSS_CANCELED) {
		add_assoc_string(item, "scan_warn", "skipped blocks "
		    "that are only referenced by the checkpoint.");
	} else {
		assert(ps->pss_state == DSS_SCANNING);
		add_assoc_string(item, "scan_warn", "skipping blocks "
		    "that are only referenced by the checkpoint.");
	}
}

static void
zpool_get_removal_status(zpool_handle_t *zhp, pool_removal_stat_t *prs, zval *item)
{
	char copied_buf[7], examined_buf[7], total_buf[7], rate_buf[7];
	time_t start, end;
	nvlist_t *config, *nvroot;
	nvlist_t **child;
	uint_t children;
	char *vdev_name;

	if (prs == NULL || prs->prs_state == DSS_NONE)
		return;

	config = zpool_get_config(zhp, NULL);
	nvroot = fnvlist_lookup_nvlist(config,
	    ZPOOL_CONFIG_VDEV_TREE);
	nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_CHILDREN,
	    &child, &children);
	assert(prs->prs_removing_vdev < children);
	vdev_name = zpool_vdev_name(g_zfs, zhp,
	    child[prs->prs_removing_vdev], B_TRUE);


	start = prs->prs_start_time;
	end = prs->prs_end_time;
	zfs_nicenum(prs->prs_copied, copied_buf, sizeof (copied_buf));

	/*
	 * Removal is finished or canceled.
	 */
	char tmp[512];
	if (prs->prs_state == DSS_FINISHED) {
		uint64_t minutes_taken = (end - start) / 60;
		snprintf(tmp, sizeof(tmp), "Removal of vdev %llu copied %s "
		    "in %lluh%um, completed on %s", (longlong_t)prs->prs_removing_vdev,
		    copied_buf,
		    (u_longlong_t)(minutes_taken / 60),
		    (uint_t)(minutes_taken % 60),
		    ctime((time_t *)&end));
	} else if (prs->prs_state == DSS_CANCELED) {
		snprintf(tmp, sizeof(tmp), "Removal of %s canceled on %s",
		    vdev_name, ctime(&end));
	} else {
		uint64_t copied, total, elapsed, mins_left, hours_left;
		double fraction_done;
		uint_t rate;

		assert(prs->prs_state == DSS_SCANNING);
		snprintf(tmp, sizeof(tmp), "Evacuation of %s in progress since %s",
		    vdev_name, ctime(&start));

		copied = prs->prs_copied > 0 ? prs->prs_copied : 1;
		total = prs->prs_to_copy;
		fraction_done = (double)copied / total;

		/* elapsed time for this pass */
		elapsed = time(NULL) - prs->prs_start_time;
		elapsed = elapsed > 0 ? elapsed : 1;
		rate = copied / elapsed;
		rate = rate > 0 ? rate : 1;
		mins_left = ((total - copied) / rate) / 60;
		hours_left = mins_left / 60;

		zfs_nicenum(copied, examined_buf, sizeof (examined_buf));
		zfs_nicenum(total, total_buf, sizeof (total_buf));
		zfs_nicenum(rate, rate_buf, sizeof (rate_buf));

		/*
		 * do not print estimated time if hours_left is more than
		 * 30 days
		 */
		snprintf(tmp, sizeof(tmp), "%s, %s copied out of %s at %s/s, "
		    "%.2f%% done", tmp,
		    examined_buf, total_buf, rate_buf, 100 * fraction_done);
		if (hours_left < (30 * 24)) {
			snprintf(tmp, sizeof(tmp), "%s, %lluh%um to go", tmp,
			    (u_longlong_t)hours_left, (uint_t)(mins_left % 60));
		} else {
			snprintf(tmp, sizeof(tmp), "%s, (copy is slow, no estimated time)", tmp);
		}
	}

	if (prs->prs_mapping_memory > 0) {
		char mem_buf[7];
		zfs_nicenum(prs->prs_mapping_memory, mem_buf, sizeof (mem_buf));
		snprintf(tmp, sizeof(tmp), "%s, %s memory used for "
		    "removed device mappings", tmp,
		    mem_buf);
	}
	add_assoc_string(item, "remove", tmp);
}

static void
zpool_get_checkpoint_status(pool_checkpoint_stat_t *pcs, zval *item)
{
	time_t start;
	char space_buf[7];

	if (pcs == NULL || pcs->pcs_state == CS_NONE)
		return;


	start = pcs->pcs_start_time;
	zfs_nicenum(pcs->pcs_space, space_buf, sizeof (space_buf));
	char tmp[512];
	if (pcs->pcs_state == CS_CHECKPOINT_EXISTS) {
		char *date = ctime(&start);
		snprintf(tmp, sizeof(tmp), "created %.*s, consumes %s",
		    strlen(date) - 1, date, space_buf);
		add_assoc_string(item, "checkpoint", tmp);
		return;
	}

	assert(pcs->pcs_state == CS_CHECKPOINT_DISCARDING);
	snprintf(tmp, sizeof(tmp), "discarding, %s remaining.",
	    space_buf);
	add_assoc_string(item, "checkpoint", tmp);
}

static void
zpool_get_error_log(zpool_handle_t *zhp, zval *item)
{
	nvlist_t *nverrlist = NULL;
	nvpair_t *elem;
	char *pathname;
	size_t len = MAXPATHLEN * 2;

	if (zpool_get_errlog(zhp, &nverrlist) != 0) {
		add_assoc_string(item, "errors", "List of errors unavailable "
		    "(insufficient privileges)");
		return;
	}
	char tmp[512];
	snprintf(tmp, sizeof(tmp), "Permanent errors have been "
	    "detected in the following files:");

	pathname = malloc(len);
	elem = NULL;
	while ((elem = nvlist_next_nvpair(nverrlist, elem)) != NULL) {
		nvlist_t *nv;
		uint64_t dsobj, obj;

		nvpair_value_nvlist(elem, &nv);
		nvlist_lookup_uint64(nv, ZPOOL_ERR_DATASET,
		    &dsobj);
		nvlist_lookup_uint64(nv, ZPOOL_ERR_OBJECT,
		    &obj);
		zpool_obj_to_path(zhp, dsobj, obj, pathname, len);
		snprintf(tmp, sizeof(tmp), "%s, %s", tmp, pathname);
	}
	add_assoc_string(item, "errors", tmp);
	free(pathname);
	nvlist_free(nverrlist);
}

static void
zpool_get_spares(zpool_handle_t *zhp, nvlist_t **spares, uint_t nspares, zval *item)
{
	uint_t i;
	char *name;

	if (nspares == 0)
		return;
	zval subitem;
	array_init(&subitem);
	for (i = 0; i < nspares; i++) {
		name = zpool_vdev_name(g_zfs, zhp, spares[i], B_FALSE);
		zpool_get_status_config(zhp, name, spares[i], 2, B_TRUE, &subitem);
		free(name);
	}
	if (zend_hash_num_elements(Z_ARRVAL_P(&subitem)) > 0)
		add_assoc_zval(item, "spares", &subitem);
}

static void
zpool_get_l2cache(zpool_handle_t *zhp, nvlist_t **l2cache, uint_t nl2cache, zval *item)
{
	uint_t i;
	char *name;

	if (nl2cache == 0)
		return;

	zval subitem;
	array_init(&subitem);

	for (i = 0; i < nl2cache; i++) {
		name = zpool_vdev_name(g_zfs, zhp, l2cache[i], B_FALSE);
		zpool_get_status_config(zhp, name, l2cache[i], 2, B_FALSE, &subitem);
		free(name);
	}
	if (zend_hash_num_elements(Z_ARRVAL_P(&subitem)) > 0)
		add_assoc_zval(item, "cache", &subitem);
}
static boolean_t
find_vdev(nvlist_t *nv, uint64_t search)
{
	uint64_t guid;
	nvlist_t **child;
	uint_t c, children;

	if (nvlist_lookup_uint64(nv, ZPOOL_CONFIG_GUID, &guid) == 0 &&
	    search == guid)
		return (B_TRUE);

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) == 0) {
		for (c = 0; c < children; c++)
			if (find_vdev(child[c], search))
				return (B_TRUE);
	}

	return (B_FALSE);
}

static int
find_spare(zpool_handle_t *zhp, void *data)
{
	spare_cbdata_t *cbp = data;
	nvlist_t *config, *nvroot;

	config = zpool_get_config(zhp, NULL);
	nvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE,
	    &nvroot);

	if (find_vdev(nvroot, cbp->cb_guid)) {
		cbp->cb_zhp = zhp;
		return (1);
	}

	zpool_close(zhp);
	return (0);
}
static void
zpool_get_logs(zpool_handle_t *zhp, nvlist_t *nv, boolean_t verbose, zval *item)
{
	uint_t c, children;
	nvlist_t **child;

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN, &child,
	    &children) != 0)
		return;

	zval subitem;
	array_init(&subitem);
	for (c = 0; c < children; c++) {
		uint64_t is_log = B_FALSE;
		char *name;

		(void) nvlist_lookup_uint64(child[c], ZPOOL_CONFIG_IS_LOG,
		    &is_log);
		if (!is_log)
			continue;
		name = zpool_vdev_name(g_zfs, zhp, child[c], B_TRUE);
		if (verbose)
			zpool_get_status_config(zhp, name, child[c], 2, B_FALSE, item);
		else
			zpool_get_import_config(name, child[c], 2, &subitem);
		free(name);
	}
	if (zend_hash_num_elements(Z_ARRVAL_P(&subitem)) > 0)
		add_assoc_zval(item, "logs", &subitem);
}
static uint_t
num_logs(nvlist_t *nv)
{
	uint_t nlogs = 0;
	uint_t c, children;
	nvlist_t **child;

	if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0)
		return (0);

	for (c = 0; c < children; c++) {
		uint64_t is_log = B_FALSE;

		(void) nvlist_lookup_uint64(child[c], ZPOOL_CONFIG_IS_LOG,
		    &is_log);
		if (is_log)
			nlogs++;
	}
	return (nlogs);
}

static int
check_replication(nvlist_t *config, nvlist_t *newroot)
{
	nvlist_t **child;
	uint_t	children;
	replication_level_t *current = NULL, *new;
	int ret;
	if (config != NULL) {
		nvlist_t *nvroot;

		nvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE,
		    &nvroot);
		if ((current = get_replication(nvroot, B_FALSE)) == NULL)
			return (0);
	}
	if ((nvlist_lookup_nvlist_array(newroot, ZPOOL_CONFIG_CHILDREN,
	    &child, &children) != 0) || (children == 0)) {
		free(current);
		return (0);
	}

	if (num_logs(newroot) == children) {
		free(current);
		return (0);
	}

	if ((new = get_replication(newroot, B_TRUE)) == NULL) {
		free(current);
		return (-1);
	}

	ret = 0;
	if (current != NULL) {
		if (strcmp(current->zprl_type, new->zprl_type) != 0) {
			ret = -1;
		} else if (current->zprl_parity != new->zprl_parity) {
			ret = -1;
		} else if (current->zprl_children != new->zprl_children) {
			ret = -1;
		}
	}

	free(new);
	if (current != NULL)
		free(current);

	return (ret);
}

boolean_t error_seen;
boolean_t is_force;


static void
vdev_error(const char *fmt, ...)
{
	va_list ap;

	if (!error_seen) {
		(void) fprintf(stderr, gettext("invalid vdev specification\n"));
		if (!is_force)
			(void) fprintf(stderr, gettext("use '-f' to override "
			    "the following errors:\n"));
		else
			(void) fprintf(stderr, gettext("the following errors "
			    "must be manually repaired:\n"));
		error_seen = B_TRUE;
	}

	va_start(ap, fmt);
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
}
static replication_level_t *
get_replication(nvlist_t *nvroot, boolean_t fatal)
{
	nvlist_t **top;
	uint_t t, toplevels;
	nvlist_t **child;
	uint_t c, children;
	nvlist_t *nv;
	char *type;
	replication_level_t lastrep = {0};
	replication_level_t rep;
	replication_level_t *ret;
	boolean_t dontreport;

	ret = malloc(sizeof (replication_level_t));

	nvlist_lookup_nvlist_array(nvroot, ZPOOL_CONFIG_CHILDREN,
	    &top, &toplevels);

	for (t = 0; t < toplevels; t++) {
		uint64_t is_log = B_FALSE;

		nv = top[t];

		(void) nvlist_lookup_uint64(nv, ZPOOL_CONFIG_IS_LOG, &is_log);
		if (is_log)
			continue;

		nvlist_lookup_string(nv, ZPOOL_CONFIG_TYPE,
		    &type);
		if (nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN,
		    &child, &children) != 0) {
			/*
			 * This is a 'file' or 'disk' vdev.
			 */
			rep.zprl_type = type;
			rep.zprl_children = 1;
			rep.zprl_parity = 0;
		} else {
			uint64_t vdev_size;

			/*
			 * This is a mirror or RAID-Z vdev.  Go through and make
			 * sure the contents are all the same (files vs. disks),
			 * keeping track of the number of elements in the
			 * process.
			 *
			 * We also check that the size of each vdev (if it can
			 * be determined) is the same.
			 */
			rep.zprl_type = type;
			rep.zprl_children = 0;

			if (strcmp(type, VDEV_TYPE_RAIDZ) == 0) {
				nvlist_lookup_uint64(nv,
				    ZPOOL_CONFIG_NPARITY,
				    &rep.zprl_parity);
				assert(rep.zprl_parity != 0);
			} else {
				rep.zprl_parity = 0;
			}

			/*
			 * The 'dontreport' variable indicates that we've
			 * already reported an error for this spec, so don't
			 * bother doing it again.
			 */
			type = NULL;
			dontreport = 0;
			vdev_size = -1ULL;
			for (c = 0; c < children; c++) {
				boolean_t is_replacing, is_spare;
				nvlist_t *cnv = child[c];
				char *path;
				struct stat64 statbuf;
				uint64_t size = -1ULL;
				char *childtype;
				int fd, err;

				rep.zprl_children++;

				nvlist_lookup_string(cnv,
				    ZPOOL_CONFIG_TYPE, &childtype);

				/*
				 * If this is a replacing or spare vdev, then
				 * get the real first child of the vdev.
				 */
				is_replacing = strcmp(childtype,
				    VDEV_TYPE_REPLACING) == 0;
				is_spare = strcmp(childtype,
				    VDEV_TYPE_SPARE) == 0;
				if (is_replacing || is_spare) {
					nvlist_t **rchild;
					uint_t rchildren;

					nvlist_lookup_nvlist_array(cnv,
					    ZPOOL_CONFIG_CHILDREN, &rchild,
					    &rchildren);
					assert((is_replacing && rchildren == 2)
					    || (is_spare && rchildren >= 2));
					cnv = rchild[0];

					nvlist_lookup_string(cnv,
					    ZPOOL_CONFIG_TYPE,
					    &childtype);
					if (strcmp(childtype,
					    VDEV_TYPE_SPARE) == 0) {
						/* We have a replacing vdev with
						 * a spare child.  Get the first
						 * real child of the spare
						 */
						
						    nvlist_lookup_nvlist_array(
							cnv,
							ZPOOL_CONFIG_CHILDREN,
							&rchild,
						    &rchildren);
						assert(rchildren >= 2);
						cnv = rchild[0];
					}
				}

				nvlist_lookup_string(cnv,
				    ZPOOL_CONFIG_PATH, &path);

				/*
				 * If we have a raidz/mirror that combines disks
				 * with files, report it as an error.
				 */
				if (!dontreport && type != NULL &&
				    strcmp(type, childtype) != 0) {
					if (ret != NULL)
						free(ret);
					ret = NULL;
					if (fatal)
						vdev_error(gettext(
						    "mismatched replication "
						    "level: %s contains both "
						    "files and devices\n"),
						    rep.zprl_type);
					else
						return (NULL);
					dontreport = B_TRUE;
				}

				/*
				 * According to stat(2), the value of 'st_size'
				 * is undefined for block devices and character
				 * devices.  But there is no effective way to
				 * determine the real size in userland.
				 *
				 * Instead, we'll take advantage of an
				 * implementation detail of spec_size().  If the
				 * device is currently open, then we (should)
				 * return a valid size.
				 *
				 * If we still don't get a valid size (indicated
				 * by a size of 0 or MAXOFFSET_T), then ignore
				 * this device altogether.
				 */
				if ((fd = open(path, O_RDONLY)) >= 0) {
					err = fstat64(fd, &statbuf);
					(void) close(fd);
				} else {
					err = stat64(path, &statbuf);
				}

				if (err != 0 ||
				    statbuf.st_size == 0 ||
				    statbuf.st_size == MAXOFFSET_T)
					continue;

				size = statbuf.st_size;

				/*
				 * Also make sure that devices and
				 * slices have a consistent size.  If
				 * they differ by a significant amount
				 * (~16MB) then report an error.
				 */
				if (!dontreport &&
				    (vdev_size != -1ULL &&
				    (labs(size - vdev_size) >
				    ZPOOL_FUZZ))) {
					if (ret != NULL)
						free(ret);
					ret = NULL;
					if (fatal)
						vdev_error(gettext(
						    "%s contains devices of "
						    "different sizes\n"),
						    rep.zprl_type);
					else
						return (NULL);
					dontreport = B_TRUE;
				}

				type = childtype;
				vdev_size = size;
			}
		}

		/*
		 * At this point, we have the replication of the last toplevel
		 * vdev in 'rep'.  Compare it to 'lastrep' to see if its
		 * different.
		 */
		if (lastrep.zprl_type != NULL) {
			if (strcmp(lastrep.zprl_type, rep.zprl_type) != 0) {
				if (ret != NULL)
					free(ret);
				ret = NULL;
				if (fatal)
					vdev_error(gettext(
					    "mismatched replication level: "
					    "both %s and %s vdevs are "
					    "present\n"),
					    lastrep.zprl_type, rep.zprl_type);
				else
					return (NULL);
			} else if (lastrep.zprl_parity != rep.zprl_parity) {
				if (ret)
					free(ret);
				ret = NULL;
				if (fatal)
					vdev_error(gettext(
					    "mismatched replication level: "
					    "both %llu and %llu device parity "
					    "%s vdevs are present\n"),
					    lastrep.zprl_parity,
					    rep.zprl_parity,
					    rep.zprl_type);
				else
					return (NULL);
			} else if (lastrep.zprl_children != rep.zprl_children) {
				if (ret)
					free(ret);
				ret = NULL;
				if (fatal)
					vdev_error(gettext(
					    "mismatched replication level: "
					    "both %llu-way and %llu-way %s "
					    "vdevs are present\n"),
					    lastrep.zprl_children,
					    rep.zprl_children,
					    rep.zprl_type);
				else
					return (NULL);
			}
		}
		lastrep = rep;
	}

	if (ret != NULL)
		*ret = rep;

	return (ret);
}

static int
zpool_attach_or_replace(char *zpool_name, char *old_dev, char *new_dev, int force, int replacing)
{
	if ((g_zfs = libzfs_init()) == NULL) {
		return -1;
	}

	int c;
	nvlist_t *nvroot;
	zpool_boot_label_t boot_type;
	uint64_t boot_size;
	int ret;

	zpool_handle_t *zhp;
	if ((zhp = zpool_open(g_zfs, zpool_name)) == NULL) {
		libzfs_fini(g_zfs);
		return -1;
	}

	if (zpool_get_config(zhp, NULL) == NULL) {
		zpool_close(zhp);
		libzfs_fini(g_zfs);
		return -1;
	}

	if (zpool_is_bootable(zhp))
		boot_type = ZPOOL_COPY_BOOT_LABEL;
	else
		boot_type = ZPOOL_NO_BOOT_LABEL;

	zval item;
	array_init(&item);
	zval subitem;
	array_init(&subitem);
	zval child;
	array_init(&child);
	add_next_index_string(&child, new_dev);
	add_assoc_zval(&subitem, "children", &child);
	add_assoc_string(&subitem, "type", "stripe");
	add_next_index_zval(&item, &subitem);

	boot_size = zpool_get_prop_int(zhp, ZPOOL_PROP_BOOTSIZE, NULL);
	nvroot = make_root_vdev(zhp, force, B_FALSE, replacing, B_FALSE,
	    boot_type, boot_size, &item);
	if (nvroot == NULL) {
		zpool_close(zhp);
		libzfs_fini(g_zfs);
		return -1;
	}
	zend_array_destroy(Z_ARRVAL_P(&child));
	zend_array_destroy(Z_ARRVAL_P(&subitem));
	zend_array_destroy(Z_ARRVAL_P(&item));
	ret = zpool_vdev_attach(zhp, old_dev, new_dev, nvroot, replacing);

	nvlist_free(nvroot);
	zpool_close(zhp);
    libzfs_fini(g_zfs);

    return 0;
}

static boolean_t
zpool_has_checkpoint(zpool_handle_t *zhp)
{
	nvlist_t *config, *nvroot;

	config = zpool_get_config(zhp, NULL);

	if (config != NULL) {
		pool_checkpoint_stat_t *pcs = NULL;
		uint_t c;

		nvroot = fnvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE);
		(void) nvlist_lookup_uint64_array(nvroot,
		    ZPOOL_CONFIG_CHECKPOINT_STATS, (uint64_t **)&pcs, &c);

		if (pcs == NULL || pcs->pcs_state == CS_NONE)
			return (B_FALSE);

		assert(pcs->pcs_state == CS_CHECKPOINT_EXISTS ||
		    pcs->pcs_state == CS_CHECKPOINT_DISCARDING);
		return (B_TRUE);
	}

	return (B_FALSE);
}

static int
zpool_scrub_callback(zpool_handle_t *zhp, void *data)
{
	scrub_cbdata_t *cb = data;
	int err;

	if (zpool_get_state(zhp) == POOL_STATE_UNAVAIL) {
		return (1);
	}

	err = zpool_scan(zhp, cb->cb_type, cb->cb_scrub_cmd);

	if (err == 0 && zpool_has_checkpoint(zhp) &&
	    cb->cb_type == POOL_SCAN_SCRUB) {
	}

	return (err != 0);
}

static nvlist_t *split_mirror_vdev(zpool_handle_t *zhp, char *newname, nvlist_t *props,
    splitflags_t flags, zval *devs)
{
	nvlist_t *newroot = NULL, **child;
	uint_t c, children;
	if (devs != NULL) {
		zval subitem;
		array_init(&subitem);
		add_assoc_zval(&subitem, "children", devs);
		add_assoc_string(&subitem, "type", "stripe");
		zval item;
		array_init(&item);
		add_next_index_zval(&item, &subitem);
		if ((newroot = construct_spec(&item)) == NULL) {
			zend_array_destroy(Z_ARR_P(&subitem));
			zend_array_destroy(Z_ARR_P(&item));
			return (NULL);
		}
		zend_array_destroy(Z_ARR_P(&subitem));
		zend_array_destroy(Z_ARR_P(&item));
		nvlist_lookup_nvlist_array(newroot,
		    ZPOOL_CONFIG_CHILDREN, &child, &children);
		for (c = 0; c < children; c++) {
			char *path;
			const char *type;
			int min, max;

			nvlist_lookup_string(child[c],
			    ZPOOL_CONFIG_PATH, &path);
			if ((type = is_grouping(path, &min, &max)) != NULL) {
				nvlist_free(newroot);
				return (NULL);
			}
		}
	}

	if (zpool_vdev_split(zhp, newname, &newroot, props, flags) != 0) {
		nvlist_free(newroot);
		return (NULL);
	}

	return (newroot);
}


static int
share_mount_one(zfs_handle_t *zhp, int op, int flags, char *protocol,
    boolean_t explicit, const char *options)
{
	char mountpoint[ZFS_MAXPROPLEN];
	char shareopts[ZFS_MAXPROPLEN];
	char smbshareopts[ZFS_MAXPROPLEN];
	const char *cmdname = op == OP_SHARE ? "share" : "mount";
	struct mnttab mnt;
	uint64_t zoned, canmount;
	boolean_t shared_nfs, shared_smb;

	assert(zfs_get_type(zhp) & ZFS_TYPE_FILESYSTEM);

	zoned = zfs_prop_get_int(zhp, ZFS_PROP_ZONED);

	if (zoned && getzoneid() == GLOBAL_ZONEID) {
		if (!explicit)
			return (0);
		return (1);

	} else if (!zoned && getzoneid() != GLOBAL_ZONEID) {
		if (!explicit)
			return (0);
		return (1);
	}

	zfs_prop_get(zhp, ZFS_PROP_MOUNTPOINT, mountpoint,
	    sizeof (mountpoint), NULL, NULL, 0, B_FALSE);
	zfs_prop_get(zhp, ZFS_PROP_SHARENFS, shareopts,
	    sizeof (shareopts), NULL, NULL, 0, B_FALSE);
	zfs_prop_get(zhp, ZFS_PROP_SHARESMB, smbshareopts,
	    sizeof (smbshareopts), NULL, NULL, 0, B_FALSE);

	if (op == OP_SHARE && strcmp(shareopts, "off") == 0 &&
	    strcmp(smbshareopts, "off") == 0) {
		if (!explicit)
			return (0);
		return (1);
	}
	if (strcmp(mountpoint, "legacy") == 0) {
		if (!explicit)
			return (0);
		return (1);
	}

	if (strcmp(mountpoint, "none") == 0) {
		if (!explicit)
			return (0);
		return (1);
	}

	canmount = zfs_prop_get_int(zhp, ZFS_PROP_CANMOUNT);
	if (canmount == ZFS_CANMOUNT_OFF) {
		if (!explicit)
			return (0);
		return (1);
	} else if (canmount == ZFS_CANMOUNT_NOAUTO && !explicit) {
		return (0);
	}

	if (zfs_prop_get_int(zhp, ZFS_PROP_INCONSISTENT) &&
	    zfs_prop_get(zhp, ZFS_PROP_RECEIVE_RESUME_TOKEN,
	    NULL, 0, NULL, NULL, 0, B_TRUE) == 0) {
		if (!explicit)
			return (0);
		return (1);
	}

	switch (op) {
	case OP_SHARE:

		shared_nfs = zfs_is_shared_nfs(zhp, NULL);
		shared_smb = zfs_is_shared_smb(zhp, NULL);

		if ((shared_nfs && shared_smb) ||
		    (shared_nfs && strcmp(shareopts, "on") == 0 &&
		    strcmp(smbshareopts, "off") == 0) ||
		    (shared_smb && strcmp(smbshareopts, "on") == 0 &&
		    strcmp(shareopts, "off") == 0)) {
			if (!explicit)
				return (0);
			return (1);
		}

		if (!zfs_is_mounted(zhp, NULL) &&
		    zfs_mount(zhp, NULL, 0) != 0)
			return (1);

		if (protocol == NULL) {
			if (zfs_shareall(zhp) != 0)
				return (1);
		} else if (strcmp(protocol, "nfs") == 0) {
			if (zfs_share_nfs(zhp))
				return (1);
		} else if (strcmp(protocol, "smb") == 0) {
			if (zfs_share_smb(zhp))
				return (1);
		} else {
			return (1);
		}

		break;

	case OP_MOUNT:
		if (options == NULL)
			mnt.mnt_mntopts = "";
		else
			mnt.mnt_mntopts = (char *)options;

		if (!hasmntopt(&mnt, MNTOPT_REMOUNT) &&
		    zfs_is_mounted(zhp, NULL)) {
			if (!explicit)
				return (0);
			return (1);
		}

		if (zfs_mount(zhp, options, flags) != 0)
			return (1);
		break;
	}

	return (0);
}

#ifdef BSD112

static void
get_all_datasets(libzfs_handle_t *g_zfs, zfs_handle_t ***dslist, size_t *count)
{
    get_all_cb_t cb = { 0 };
    cb.cb_verbose = 0;
    cb.cb_getone = get_one_dataset;

    (void) zfs_iter_root(g_zfs, get_one_dataset, &cb);

    *dslist = cb.cb_handles;
    *count = cb.cb_used;
}

#elif defined(BSD12) || defined(BSD113)
static void
get_all_datasets(libzfs_handle_t *g_zfs, get_all_cb_t *cbp, boolean_t verbose)
{
    get_all_state_t state = {
        .ga_verbose = verbose,
        .ga_cbp = cbp
    };

    (void) zfs_iter_root(g_zfs, get_one_dataset, &state);
}
static int
share_mount_one_cb(zfs_handle_t *zhp, void *arg)
{
	share_mount_state_t *sms = arg;
	int ret;

	ret = share_mount_one(zhp, sms->sm_op, sms->sm_flags, sms->sm_proto,
	    B_FALSE, sms->sm_options);

	pthread_mutex_lock(&sms->sm_lock);
	if (ret != 0)
		sms->sm_status = ret;
	sms->sm_done++;
	pthread_mutex_unlock(&sms->sm_lock);
	return (ret);
}
#endif
static int
get_one_dataset(zfs_handle_t *zhp, void *data)
{
	static char *spin[] = { "-", "\\", "|", "/" };
	static int spinval = 0;
	static int spincheck = 0;
	static time_t last_spin_time = (time_t)0;
	get_all_cb_t *cbp = data;
	zfs_type_t type = zfs_get_type(zhp);

	if (zfs_iter_filesystems(zhp, get_one_dataset, data) != 0) {
		zfs_close(zhp);
		return (1);
	}

	if ((type & ZFS_TYPE_FILESYSTEM) == 0) {
		zfs_close(zhp);
		return (0);
	}
	libzfs_add_handle(cbp, zhp);
	assert(cbp->cb_used <= cbp->cb_alloc);

	return (0);
}

static int
unshare_unmount_path(char *path, int flags, boolean_t is_manual)
{
	zfs_handle_t *zhp;
	int ret = 0;
	struct stat64 statbuf;
	struct extmnttab entry;
	const char *cmdname = "unmount";
	ino_t path_inode;

	if (stat64(path, &statbuf) != 0) {
		return (1);
	}
	path_inode = statbuf.st_ino;
	struct statfs sfs;

	if (statfs(path, &sfs) != 0) {
		ret = -1;
	}
	statfs2mnttab(&sfs, &entry);
	if (ret != 0) {
		return (ret != 0);
	}

	if (strcmp(entry.mnt_fstype, MNTTYPE_ZFS) != 0) {
		return (1);
	}

	if ((zhp = zfs_open(g_zfs, entry.mnt_special,
	    ZFS_TYPE_FILESYSTEM)) == NULL)
		return (1);

	ret = 1;
	if (stat64(entry.mnt_mountp, &statbuf) != 0) {
		goto out;
	} else if (statbuf.st_ino != path_inode) {
		goto out;
	}

	char mtpt_prop[ZFS_MAXPROPLEN];

	zfs_prop_get(zhp, ZFS_PROP_MOUNTPOINT, mtpt_prop,
	    sizeof (mtpt_prop), NULL, NULL, 0, B_FALSE);

	if (is_manual) {
		ret = zfs_unmount(zhp, NULL, flags);
	} else if (strcmp(mtpt_prop, "legacy") == 0) {
	} else {
		ret = zfs_unmountall(zhp, flags);
	}

out:
	zfs_close(zhp);

	return (ret != 0);
}

static int
unshare_unmount_compare(const void *larg, const void *rarg, void *unused)
{
	const unshare_unmount_node_t *l = larg;
	const unshare_unmount_node_t *r = rarg;

	return (strcmp(l->un_mountp, r->un_mountp));
}


static int
zfs_prop_cb(int prop, void *cb)
{
	zval subitem;
	array_init(&subitem);
	add_assoc_string(&subitem, "name", zfs_prop_to_name(prop));
	if (zfs_prop_readonly(prop))
		add_assoc_bool(&subitem, "readonly", 1);
	else
		add_assoc_bool(&subitem, "readonly", 0);
	if (zfs_prop_values(prop) == NULL)
		add_assoc_null(&subitem, "values");
	else
		add_assoc_string(&subitem, "values", zfs_prop_values(prop));
	add_next_index_zval((zval*)cb, &subitem);
	return (ZPROP_CONT);
}

static int
zpool_prop_cb(int prop, void *cb)
{
	zval subitem;
	array_init(&subitem);
	add_assoc_string(&subitem, "name", zpool_prop_to_name(prop));
	if (zpool_prop_readonly(prop))
		add_assoc_bool(&subitem, "readonly", 1);
	else
		add_assoc_bool(&subitem, "readonly", 0);
	if (zpool_prop_values(prop) == NULL)
		add_assoc_null(&subitem, "values");
	else
		add_assoc_string(&subitem, "values", zpool_prop_values(prop));
	add_next_index_zval((zval*)cb, &subitem);
	return (ZPROP_CONT);
}
