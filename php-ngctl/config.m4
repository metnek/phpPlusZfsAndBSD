PHP_ARG_ENABLE(ngctl, Enable test support)

if test "$PHP_NGCTL" = "yes"; then
  AC_DEFINE([HAVE_NGCTL], 1, [ ])
  if test "$SINGLE" = "yes"; then
     PHP_ADD_INCLUDE("/usr/include")
     PHP_ADD_LIBRARY_WITH_PATH(ngraph, "/usr/lib", NGCTL_SHARED_LIBADD)
     PHP_SUBST(NGCTL_SHARED_LIBADD)
     EXTRA_CFLAGS="-DCOMPILE_DL_NGCTL -DBDB_ENABLE -DZDEBUG"
     PHP_SUBST(EXTRA_CFLAGS)
   fi
   PHP_NEW_EXTENSION(ngctl, ngctl.c, $ext_shared)
fi

