PHP_ARG_ENABLE(system_os, Enable test support)

if test "$PHP_SYSTEM_OS" = "yes"; then
   AC_DEFINE([HAVE_SYSTEM_OS], 1, [ ])
   PHP_ADD_LIBRARY(archive, 0, SYSTEM_OS_SHARED_LIBADD)
   PHP_ADD_LIBRARY(kvm, 0, SYSTEM_OS_SHARED_LIBADD)
   PHP_ADD_LIBRARY(base, 0, SYSTEM_OS_SHARED_LIBADD)
   PHP_SUBST(SYSTEM_OS_SHARED_LIBADD)

   if test "$SINGLE" = "yes" ; then
       EXTRA_CFLAGS="-DCOMPILE_DL_SYSTEM_OS"
       PHP_SUBST(EXTRA_CFLAGS)
   fi
   PHP_NEW_EXTENSION(system_os, system_os.c, $ext_shared)
fi

