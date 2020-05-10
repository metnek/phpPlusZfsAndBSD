PHP_ARG_ENABLE(gpart, Enable test support)

if test "$PHP_GPART" = "yes"; then
   AC_DEFINE([HAVE_GPART], 1, [ ])
   PHP_ADD_INCLUDE("/usr/include/geom/")
   PHP_ADD_LIBRARY(geom, 0, GPART_SHARED_LIBADD)
   PHP_ADD_LIBRARY(util, 0, GPART_SHARED_LIBADD)
   PHP_SUBST(GPART_SHARED_LIBADD)

   if test "$SINGLE" = "yes" ; then
       EXTRA_CFLAGS="-DCOMPILE_DL_GPART"
       PHP_SUBST(EXTRA_CFLAGS)
   fi
   PHP_NEW_EXTENSION(gpart, gpart.c, $ext_shared)
fi

