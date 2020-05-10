PHP_ARG_ENABLE(xls2array, Enable test support)

if test "$PHP_XLS2ARRAY" = "yes"; then
   AC_DEFINE([HAVE_XLS2ARRAY], 1, [ ])

   if test "$SINGLE" = "yes" ; then
       EXTRA_CFLAGS="-DCOMPILE_DL_XLS2ARRAY"
       PHP_SUBST(EXTRA_CFLAGS)
   fi
   PHP_NEW_EXTENSION(xls2array, xls2array.c endian.c ole.c xls.c xlstool.c, $ext_shared)
fi

