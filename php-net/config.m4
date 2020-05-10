PHP_ARG_ENABLE(net, Enable test support)

if test "$PHP_NET" = "yes"; then
    AC_DEFINE([HAVE_NET], 1, [ ])
    if test "$ZWARN" = "yes"; then
    	EXTRA_CFLAGS="-DZWARN"
	fi
	if test "$SINGLE" = "yes"; then
    	EXTRA_CFLAGS="-DZFS_ENABLE -DDEBUG -DCOMPILE_DL_NET"
	    PHP_ADD_LIBRARY_WITH_PATH(kzfs, "/usr/lib", NET_SHARED_LIBADD)
	    PHP_ADD_LIBRARY_WITH_PATH(base, "/usr/lib", NET_SHARED_LIBADD)
	fi
    PHP_SUBST(NET_SHARED_LIBADD)
    PHP_SUBST(EXTRA_CFLAGS)
    PHP_NEW_EXTENSION(net, net.c, $ext_shared)
fi

