PHP_ARG_ENABLE(spwd, Enable test support)

if test "$PHP_SPWD" = "yes"; then
	AC_DEFINE([HAVE_SPWD], 1, [ ])
	if test "$SINGLE" = "yes"; then
		EXTRA_CFLAGS="-DNEED_MASTER_SPWD -DCOMPILE_DL_SPWD"
	    PHP_SUBST(EXTRA_CFLAGS)
	fi
	PHP_NEW_EXTENSION(spwd, spwd.c, $ext_shared)
fi

