
PHP_ARG_WITH(rxserver, for rxserver support,
[  --with-rxserver             Include rxserver support])

dnl PHP_ADD_MAKEFILE_FRAGMENT(Makefile.deps)

if test "$PHP_RXSERVER" != "no"; then

  SOURCES=""
  PHP_NEW_EXTENSION(rxserver, rxserver.c $SOURCES, $ext_shared, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)

  PHP_ADD_EXTENSION_DEP(uv, true)
  
  AC_PATH_PROG(PKG_CONFIG, pkg-config, no)

  AC_MSG_CHECKING(for libuv)

  if test -x "$PKG_CONFIG" && $PKG_CONFIG --exists libuv; then
      if $PKG_CONFIG libuv --atleast-version 1.0.0; then
        LIBUV_INCLINE=`$PKG_CONFIG libuv --cflags`
        LIBUV_LIBLINE=`$PKG_CONFIG libuv --libs`
        LIBUV_VERSION=`$PKG_CONFIG libuv --modversion`
        AC_MSG_RESULT(from pkgconfig: found version $LIBUV_VERSION)
        AC_DEFINE(HAVE_UVLIB,1,[ ])
      else
        AC_MSG_ERROR(system libuv must be upgraded to version >= 1.0.0)
      fi
      PHP_EVAL_LIBLINE($LIBUV_LIBLINE, RXSERVER_SHARED_LIBADD)
      PHP_EVAL_INCLINE($LIBUV_INCLINE)

    else
      SEARCH_PATH="/usr/local /usr $(pwd)/deps/libuv"
      SEARCH_FOR="/include/uv.h"
      if test -r $PHP_UV/$SEARCH_FOR; then # path given as parameter
         UV_DIR=$PHP_UV
         AC_MSG_RESULT(from option: found in $UV_DIR)
      else # search default path list
         for i in $SEARCH_PATH ; do
             if test -r $i/$SEARCH_FOR; then
               UV_DIR=$i
               AC_MSG_RESULT(from default path: found in $i)
             fi
         done
      fi
      PHP_ADD_INCLUDE($UV_DIR/include)
      PHP_LIBDIR=".libs"
      PHP_CHECK_LIBRARY(uv, uv_version,
      [
        PHP_ADD_LIBRARY_WITH_PATH(uv, $UV_DIR/$PHP_LIBDIR, RXSERVER_SHARED_LIBADD)
        AC_DEFINE(HAVE_UVLIB,1,[ ])
      ],[
        AC_MSG_ERROR([wrong uv library version or library not found])
      ],[
        -L$UV_DIR/$PHP_LIBDIR -lm
      ])
      case $host in
          *linux*)
              CFLAGS="$CFLAGS -lrt"
      esac
    fi

  PHP_SUBST([CFLAGS])
  PHP_SUBST(RXSERVER_SHARED_LIBADD)

fi
