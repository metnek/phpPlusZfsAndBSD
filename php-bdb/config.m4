PHP_ARG_ENABLE(bdb, Enable bdb support)

if test "$PHP_BDB" = "yes"; then
   AC_DEFINE([HAVE_BDB], 1, [ ])
   if test "$SINGLE" = "yes" ; then
       PHP_ADD_LIBRARY(bdb, 0, BDB_SHARED_LIBADD)
       PHP_SUBST(BDB_SHARED_LIBADD)
       EXTRA_CFLAGS="-DCOMPILE_DL_BDB -DBDB_ENABLE -DZDEBUG"
       PHP_SUBST(EXTRA_CFLAGS)
   fi
       PHP_NEW_EXTENSION(bdb, bdb.c, $ext_shared)
fi

