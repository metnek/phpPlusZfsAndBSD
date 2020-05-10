PHP_ARG_ENABLE(zfs, Enable test support)

if test "$PHP_ZFS" = "yes"; then
   AC_DEFINE([HAVE_ZFS], 1, [ ])
   if test -z "$DIR_SRCTOP"; then
	DIR_SRCTOP=/usr/src
	PREFIX=/usr/local
   fi
   PHP_ADD_INCLUDE("${PREFIX}/include/")
   PHP_ADD_INCLUDE("${DIR_SRCTOP}/cddl/contrib/opensolaris/lib/libzpool/common")
   PHP_ADD_INCLUDE("${DIR_SRCTOP}/cddl/compat/opensolaris/include")
   PHP_ADD_INCLUDE("${DIR_SRCTOP}/cddl/compat/opensolaris/lib/libumem")
   PHP_ADD_INCLUDE("${DIR_SRCTOP}/sys/cddl/compat/opensolaris")
   PHP_ADD_INCLUDE("${DIR_SRCTOP}/cddl/contrib/opensolaris/head")
   PHP_ADD_INCLUDE("${DIR_SRCTOP}/cddl/contrib/opensolaris/lib/libuutil/common")
   PHP_ADD_INCLUDE("${DIR_SRCTOP}/cddl/contrib/opensolaris/lib/libumem/common")
   PHP_ADD_INCLUDE("${DIR_SRCTOP}/cddl/contrib/opensolaris/lib/libzfs/common")
   PHP_ADD_INCLUDE("${DIR_SRCTOP}/cddl/contrib/opensolaris/lib/libzfs_core/common")
   PHP_ADD_INCLUDE("${DIR_SRCTOP}/cddl/contrib/opensolaris/lib/libnvpair")
   PHP_ADD_INCLUDE("${DIR_SRCTOP}/sys/cddl/contrib/opensolaris/common/zfs")
   PHP_ADD_INCLUDE("${DIR_SRCTOP}/sys/cddl/contrib/opensolaris/uts/common")
   PHP_ADD_INCLUDE("${DIR_SRCTOP}/sys/cddl/contrib/opensolaris/uts/common/fs/zfs")
   PHP_ADD_INCLUDE("${DIR_SRCTOP}/sys/cddl/contrib/opensolaris/uts/common/sys")
   PHP_ADD_INCLUDE("${DIR_SRCTOP}/cddl/contrib/opensolaris/lib/libzpool/common")
   PHP_ADD_INCLUDE("${DIR_SRCTOP}/cddl/contrib/opensolaris/cmd/stat/common")
   PHP_ADD_INCLUDE("${DIR_SRCTOP}/cddl/lib/libzfs")
   EXTRA_CFLAGS="-DNEED_SOLARIS_BOOLEAN"

   if test "$SINGLE" = "yes" ; then
       EXTRA_CFLAGS="-DCOMPILE_DL_ZFS -DNEED_SOLARIS_BOOLEAN"
   fi
   PHP_SUBST(EXTRA_CFLAGS)
#   PHP_DEFINE("NEED_SOLARIS_BOOLEAN")
   PHP_ADD_LIBRARY(zfs, 0, ZFS_SHARED_LIBADD)
   PHP_ADD_LIBRARY(zfs_core, 0, ZFS_SHARED_LIBADD)
   PHP_ADD_LIBRARY(nvpair, 0, ZFS_SHARED_LIBADD)
   PHP_ADD_LIBRARY(geom, 0, ZFS_SHARED_LIBADD)
   PHP_ADD_LIBRARY(uutil, 0, ZFS_SHARED_LIBADD)
   PHP_ADD_LIBRARY(jail, 0, ZFS_SHARED_LIBADD)
   PHP_ADD_LIBRARY_WITH_PATH(intl, "${PREFIX}/lib", ZFS_SHARED_LIBADD)
   PHP_ADD_LIBRARY_WITH_PATH(kzfs, "${PREFIX}/lib", ZFS_SHARED_LIBADD)
   PHP_SUBST(ZFS_SHARED_LIBADD)
   PHP_NEW_EXTENSION(zfs, zfs.c, $ext_shared)
fi

