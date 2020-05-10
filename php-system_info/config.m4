PHP_ARG_ENABLE(system_info, Enable test support)

if test "$PHP_SYSTEM_INFO" = "yes"; then
   AC_DEFINE([HAVE_SYSTEM_INFO], 1, [ ])
   PHP_ADD_LIBRARY(kvm, 0, SYSTEM_INFO_SHARED_LIBADD)
   PHP_ADD_LIBRARY(elf, 0, SYSTEM_INFO_SHARED_LIBADD)
   PHP_ADD_LIBRARY(util, 0, SYSTEM_INFO_SHARED_LIBADD)
   PHP_SUBST(SYSTEM_INFO_SHARED_LIBADD)

   if test "$SINGLE" = "yes" ; then
       EXTRA_CFLAGS="-DCOMPILE_DL_SYSTEM_INFO"
       PHP_SUBST(EXTRA_CFLAGS)
   fi
   PHP_NEW_EXTENSION(system_info, system_info.c, $ext_shared)
fi

