dnl $Id$
dnl config.m4 for extension rxserver

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

dnl PHP_ARG_WITH(rxserver, for rxserver support,
dnl Make sure that the comment is aligned:
dnl [  --with-rxserver             Include rxserver support])

dnl Otherwise use enable:

dnl PHP_ARG_ENABLE(rxserver, whether to enable rxserver support,
dnl Make sure that the comment is aligned:
dnl [  --enable-rxserver           Enable rxserver support])

if test "$PHP_RXSERVER" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-rxserver -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/rxserver.h"  # you most likely want to change this
  dnl if test -r $PHP_RXSERVER/$SEARCH_FOR; then # path given as parameter
  dnl   RXSERVER_DIR=$PHP_RXSERVER
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for rxserver files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       RXSERVER_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$RXSERVER_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the rxserver distribution])
  dnl fi

  dnl # --with-rxserver -> add include path
  dnl PHP_ADD_INCLUDE($RXSERVER_DIR/include)

  dnl # --with-rxserver -> check for lib and symbol presence
  dnl LIBNAME=rxserver # you may want to change this
  dnl LIBSYMBOL=rxserver # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $RXSERVER_DIR/$PHP_LIBDIR, RXSERVER_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_RXSERVERLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong rxserver lib version or lib not found])
  dnl ],[
  dnl   -L$RXSERVER_DIR/$PHP_LIBDIR -lm
  dnl ])
  dnl
  dnl PHP_SUBST(RXSERVER_SHARED_LIBADD)

  PHP_NEW_EXTENSION(rxserver, rxserver.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi
