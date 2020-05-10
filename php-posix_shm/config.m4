PHP_ARG_ENABLE(posix_shm, Enable test support)

if test "$PHP_POSIX_SHM" = "yes"; then
   AC_DEFINE([HAVE_POSIX_SHM], 1, [ ])
   if test "$SINGLE" = "yes"; then
   		EXTRA_CFLAGS="-DZDEBUG -DCOMPILE_DL_POSIX_SHM"
	    PHP_SUBST(EXTRA_CFLAGS)
   fi
   PHP_NEW_EXTENSION(posix_shm, posix_shm.c, $ext_shared)
fi

