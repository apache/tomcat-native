dnl copied from httpd-2.0/os/config.m4
dnl OS changed to OS_APACHE and OS_DIR to OS_APACHE_DIR

AC_MSG_CHECKING(for target platform)

#PLATFORM=`${CONFIG_SHELL-/bin/sh} $ac_config_guess`
PLATFORM=$host

case "$PLATFORM" in
*beos*)
  OS_APACHE="beos"
  OS_APACHE_DIR=$OS_APACHE
  ;;
*pc-os2_emx*)
  OS_APACHE="os2"
  OS_APACHE_DIR=$OS_APACHE
  ;;
bs2000*)
  OS_APACHE="unix"
  OS_APACHE_DIR=bs2000  # only the OS_APACHE_DIR is platform specific.
  ;;
*)
  OS_APACHE="unix"
  OS_APACHE_DIR=$OS_APACHE;;
esac

AC_MSG_RESULT($OS_APACHE)
