PHP_ARG_ENABLE(xlsxwriter, Enable test support)
if test "$PHP_XLSXWRITER" = "yes"; then
   AC_DEFINE([HAVE_XLSXWRITER], 1, [ ])
   if test "$SINGLE" = "yes"; then
     PHP_ADD_INCLUDE("/usr/local/include/")
     PHP_ADD_LIBRARY_WITH_PATH(xlsxwriter, "/usr/local/lib/", XLSXWRITER_SHARED_LIBADD)
     PHP_ADD_LIBRARY_WITH_PATH(expat, "/usr/local/lib/", XLSXWRITER_SHARED_LIBADD)
     PHP_ADD_LIBRARY_WITH_PATH(zip, "/usr/local/lib/", XLSXWRITER_SHARED_LIBADD)
     PHP_SUBST(XLSXWRITER_SHARED_LIBADD)

       EXTRA_CFLAGS="-DCOMPILE_DL_XLSXWRITER"
       PHP_SUBST(EXTRA_CFLAGS)
   fi
   PHP_NEW_EXTENSION(xlsxwriter, xlsxwriter.c, $ext_shared)
fi
