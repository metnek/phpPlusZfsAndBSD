PHP_ARG_ENABLE(zfw, Enable test support)

if test "$PHP_ZFW" = "yes"; then
   AC_DEFINE([HAVE_ZFW], 1, [ ])
   if test "$SINGLE" = "yes"; then
       PHP_ADD_LIBRARY(zfw, 0, ZFW_SHARED_LIBADD)
       EXTRA_CFLAGS="-DCOMPILE_DL_ZFW"
       PHP_SUBST(EXTRA_CFLAGS)
   fi
   PHP_SUBST(ZFW_SHARED_LIBADD)
   PHP_NEW_EXTENSION(zfw, zfw.c, $ext_shared)
fi

