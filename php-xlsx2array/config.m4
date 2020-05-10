PHP_ARG_ENABLE(xlsx2array, Enable test support)

if test "$PHP_XLSX2ARRAY" = "yes"; then
   AC_DEFINE([HAVE_XLSX2ARRAY], 1, [ ])
   if test "$SINGLE" = "yes"; then
      PHP_ADD_INCLUDE("/usr/local/include/")
      PHP_ADD_LIBRARY_WITH_PATH(expat, "/usr/local/lib/", XLSX2ARRAY_SHARED_LIBADD)
      PHP_ADD_LIBRARY_WITH_PATH(zip, "/usr/local/lib/", XLSX2ARRAY_SHARED_LIBADD)
      PHP_SUBST(XLSX2ARRAY_SHARED_LIBADD)

       EXTRA_CFLAGS="-DCOMPILE_DL_XLSX2ARRAY"
       PHP_SUBST(EXTRA_CFLAGS)
   fi
   PHP_NEW_EXTENSION(xlsx2array, xlsx2array.c xlsxio_read.c xlsxio_read_sharedstrings.c, $ext_shared)
fi
