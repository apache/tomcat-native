AC_MSG_CHECKING(for mod_jk module)
AC_ARG_WITH(mod_jk,
  [  --with-mod_jk  Include the mod_jk (static).],
  [
    modpath_current=modules/jk
    module=jk
    libname=lib_jk.la
    BUILTIN_LIBS="$BUILTIN_LIBS $modpath_current/$libname"
    cat >>$modpath_current/modules.mk<<EOF
DISTCLEAN_TARGETS = modules.mk
static = $libname
shared =
EOF
  MODLIST="$MODLIST $module"
  AC_MSG_RESULT(added $withval)
  ],
  [ AC_MSG_RESULT(no mod_jk added) 
  ])
