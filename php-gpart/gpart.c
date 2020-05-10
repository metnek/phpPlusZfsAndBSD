#ifdef NEED_SOLARIS_BOOLEAN
#undef NEED_SOLARIS_BOOLEAN
#endif
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <libutil.h>
#include <string.h>
#include <unistd.h>
#include <libgeom.h>
#include <geom.h>
#include "php.h"
#include "php_gpart.h"

static struct gclass *find_class(struct gmesh *mesh, const char *name);
static void list_one_provider(struct gprovider *pp, const char *prefix, zval *info);
static void list_one_consumer(struct gconsumer *cp, const char *prefix, zval *info);
static void list_one_geom(struct ggeom *gp, zval *info);


PHP_FUNCTION(gpart_disk_list);

ZEND_BEGIN_ARG_INFO(arginfo_gpart_disk_list, 0)
ZEND_END_ARG_INFO()

const zend_function_entry gpart_functions[] = {
	PHP_FE(gpart_disk_list, NULL)
	{NULL, NULL, NULL}
};

zend_module_entry gpart_module_entry = {
	STANDARD_MODULE_HEADER,
	"gpart",
	gpart_functions,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"0.0.1",
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_GPART

ZEND_GET_MODULE(gpart)
#endif

PHP_FUNCTION(gpart_disk_list)
{
	struct gctl_req *req;
	req = gctl_get_handle();

	int all, error, i, nargs;
	struct gmesh mesh;
	struct gclass *classp;
	struct ggeom *gp;

	error = geom_gettree(&mesh);
	if (error != 0) {
		RETURN_FALSE;
	}
	char *gclass_name = "DISK";
	classp = find_class(&mesh, gclass_name);
	if (classp == NULL) {
		geom_deletetree(&mesh);
		RETURN_FALSE;
	}
	array_init(return_value);
	zval item;
	LIST_FOREACH(gp, &classp->lg_geom, lg_geom) {
		if (LIST_EMPTY(&gp->lg_provider) && !all) {
			continue;
		}
		array_init(&item);
		list_one_geom(gp, &item);
		add_next_index_zval(return_value, &item);
	}

	geom_deletetree(&mesh);

}

static struct gclass *
find_class(struct gmesh *mesh, const char *name)
{
	struct gclass *classp;

	LIST_FOREACH(classp, &mesh->lg_class, lg_class) {
		if (strcmp(classp->lg_name, name) == 0)
			return (classp);
	}
	return (NULL);
}


static void
list_one_provider(struct gprovider *pp, const char *prefix, zval *info)
{
	struct gconfig *conf;
	char buf[5];

	add_assoc_string(info, "name", pp->lg_name);
	humanize_number(buf, sizeof(buf), (int64_t)pp->lg_mediasize, "",
	    HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL);
	add_assoc_string(info, "mediasize", buf);
	add_assoc_long(info, "sectorsize", pp->lg_sectorsize);
	if (pp->lg_stripesize > 0 || pp->lg_stripeoffset > 0) {
		add_assoc_long(info, "stripesize", pp->lg_stripesize);
		add_assoc_long(info, "stripeoffset", pp->lg_stripeoffset);
	}
	add_assoc_string(info, "mode", pp->lg_mode);
	LIST_FOREACH(conf, &pp->lg_config, lg_config) {
		if (conf->lg_val != NULL)
			add_assoc_string(info, conf->lg_name, conf->lg_val);
		else
			add_assoc_null(info, conf->lg_name);
	}
}

static void
list_one_consumer(struct gconsumer *cp, const char *prefix, zval *info)
{
	struct gprovider *pp;
	struct gconfig *conf;

	pp = cp->lg_provider;
	if (pp == NULL) {
		return;
	} else {
		char buf[5];
		add_assoc_string(info, "name", pp->lg_name);
		humanize_number(buf, sizeof(buf), (int64_t)pp->lg_mediasize, "",
		    HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL);
		add_assoc_string(info, "mediasize", buf);
		add_assoc_long(info, "sectorsize", pp->lg_sectorsize);
		if (pp->lg_stripesize > 0 || pp->lg_stripeoffset > 0) {
			add_assoc_long(info, "stripesize", pp->lg_stripesize);
			add_assoc_long(info, "stripeoffset", pp->lg_stripeoffset);
		}
		add_assoc_string(info, "mode", cp->lg_mode);
	}
	LIST_FOREACH(conf, &cp->lg_config, lg_config) {
		if (conf->lg_val != NULL)
			add_assoc_string(info, conf->lg_name, conf->lg_val);
		else
			add_assoc_null(info, conf->lg_name);
	}
}

static void
list_one_geom(struct ggeom *gp, zval *info)
{
	struct gprovider *pp;
	struct gconsumer *cp;
	struct gconfig *conf;
	unsigned n;

	add_assoc_string(info, "geom", gp->lg_name);
	LIST_FOREACH(conf, &gp->lg_config, lg_config) {
		add_assoc_string(info, conf->lg_name, conf->lg_val);
	}
	if (!LIST_EMPTY(&gp->lg_provider)) {
		zval subitem;
		array_init(&subitem);
		LIST_FOREACH(pp, &gp->lg_provider, lg_provider) {
			zval subsub;
			array_init(&subsub);
			list_one_provider(pp, "   ", &subsub);
			if (zend_hash_num_elements(Z_ARRVAL_P(&subsub)) > 0)
				add_next_index_zval(&subitem, &subsub);
			else
				zend_array_destroy(Z_ARRVAL_P(&subsub));
		}
		if (zend_hash_num_elements(Z_ARRVAL_P(&subitem)) > 0)
			add_assoc_zval(info, "providers", &subitem);
		else
			zend_array_destroy(Z_ARRVAL_P(&subitem));
	}
	if (!LIST_EMPTY(&gp->lg_consumer)) {
		zval subitem;
		array_init(&subitem);
		LIST_FOREACH(cp, &gp->lg_consumer, lg_consumer) {
			zval subsub;
			array_init(&subsub);
			list_one_consumer(cp, "   ", &subsub);
			if (zend_hash_num_elements(Z_ARRVAL_P(&subsub)) > 0)
				add_next_index_zval(&subitem, &subsub);
			else
				zend_array_destroy(Z_ARRVAL_P(&subsub));
		}
		if (zend_hash_num_elements(Z_ARRVAL_P(&subitem)) > 0)
			add_assoc_zval(info, "consumers", &subitem);
		else
			zend_array_destroy(Z_ARRVAL_P(&subitem));
	}
}
