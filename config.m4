dnl $Id$
dnl config.m4 for extension libthai

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

PHP_ARG_WITH(libthai, for libthai support,
dnl Make sure that the comment is aligned:
[  --with-libthai             Include libthai support])

dnl Otherwise use enable:

PHP_ARG_ENABLE(libthai, whether to enable libthai support,
dnl Make sure that the comment is aligned:
[  --enable-libthai           Enable libthai support])

if test "$PHP_LIBTHAI" != "no"; then
  dnl # --with-libthai -> check with-path
  SEARCH_PATH="/usr/local /usr"
  SEARCH_FOR="/include/thai/thbrk.h"
  if test -r $PHP_LIBTHAI/$SEARCH_FOR; then
    LIBTHAI_DIR=$PHP_LIBTHAI
  else # search default path list
    AC_MSG_CHECKING([for libthai files in default path])
    for i in $SEARCH_PATH ; do
      if test -r $i/$SEARCH_FOR; then
        LIBTHAI_DIR=$i
        AC_MSG_RESULT(found in $i)
      fi
    done
  fi

  if test -z "$LIBTHAI_DIR"; then
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Please reinstall the libthai distribution])
  fi

  dnl # --with-libthai -> add include path
  PHP_ADD_INCLUDE($LIBTHAI_DIR/include)

  dnl # --with-libthai -> check for lib and symbol presence
  LIBNAME=thai
  LIBSYMBOL=th_brk_wc_find_breaks

  PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  [
    PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $LIBTHAI_DIR/$PHP_LIBDIR, LIBTHAI_SHARED_LIBADD)
    AC_DEFINE(HAVE_LIBTHAILIB,1,[ ])
  ],[
    AC_MSG_ERROR([wrong libthai lib version or lib not found])
  ],[
    -L$LIBTHAI_DIR/$PHP_LIBDIR -lm
  ])

  PHP_SUBST(LIBTHAI_SHARED_LIBADD)

  PHP_NEW_EXTENSION(libthai, libthai.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi

