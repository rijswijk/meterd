################################################################################
# Version information                                                          #
################################################################################

# Program version

define([METERD_VERSION_MAJOR], [0])
define([METERD_VERSION_MINOR], [1])
define([METERD_VERSION_BUILDREV], esyscmd([sh -c "git rev-list --count HEAD|tr -d '\n'"]))
define([PACKAGE_SUFFIX], [])

################################################################################
# Configuration functions                                                      #
################################################################################

# Init
AC_PREREQ(2.61)
AC_INIT([meterd],[METERD_VERSION_MAJOR.METERD_VERSION_MINOR.METERD_VERSION_BUILDREV[]PACKAGE_SUFFIX])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_SRCDIR([src/Makefile.am])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE(foreign)
ACX_PREFIXHACK

# Checks for compilers and other programs
AC_PROG_CC_C99
AC_PROG_INSTALL

# Compiler flags
ACX_PEDANTIC
ACX_STRICT

# Libtool settings
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

# Log level
AC_ARG_WITH(
	[loglevel],
	[AS_HELP_STRING([--with-loglevel=INT],[The log level. 0=No log 1=Error 2=Warning 3=Info 4=Debug (default INT=3)])],
	[METERD_LOGLEVEL="$withval"],
	[METERD_LOGLEVEL=3]
)

# Check for libraries
PKG_CHECK_MODULES([LIBCONFIG], [libconfig >= 1.3.2],, AC_MSG_ERROR([libconfig 1.3.2 or newer not found]))
ACX_PTHREAD
AX_LIB_SQLITE3(3.7.0)

# Check for headers
AC_CHECK_HEADERS([unistd.h])
AC_CHECK_HEADERS([syslog.h])
AC_CHECK_HEADERS([signal.h])
AC_HEADER_STDC

# Check for functions
AC_FUNC_MEMCMP
AC_CHECK_FUNCS(syslog,,AC_MSG_ERROR([syslog is required to build meterd]))

# Define default paths
full_sysconfdir=`eval eval eval eval eval echo "${sysconfdir}" | sed "s#NONE#${prefix}#" | sed "s#NONE#${ac_default_prefix}#"`
full_localstatedir=`eval eval eval eval eval echo "${localstatedir}" | sed "s#NONE#${prefix}#" | sed "s#NONE#${ac_default_prefix}#"`
full_libdir=`eval eval eval eval eval echo "${libdir}" | sed "s#NONE#${prefix}#" | sed "s#NONE#${ac_default_prefix}#"`
default_meterd_conf="`eval echo ${full_sysconfdir} | sed s,NONE,$ac_default_prefix,g`/meterd.conf"
default_pid_file="`eval echo ${full_localstatedir} | sed s,NONE,$ac_default_prefix,g`/run/meterd.pid"

# Define some variables for the code
AC_DEFINE_UNQUOTED(
	[VERSION_MAJOR],
	[METERD_VERSION_MAJOR],
	[Major version number]
)
AC_DEFINE_UNQUOTED(
	[VERSION_MINOR],
	[METERD_VERSION_MINOR],
	[Minor version number]
)
AC_DEFINE_UNQUOTED(
	[METERD_LOGLEVEL],
	[$METERD_LOGLEVEL],
	[The log level set by the user]
)
AC_DEFINE_UNQUOTED(
	[DEFAULT_METERD_CONF],
	["$default_meterd_conf"],
	[The default configuration file]
)
AC_DEFINE_UNQUOTED(
	[DEFAULT_METERD_PIDFILE],
	["$default_pid_file"],
	[The default PID file]
)

# Generate theinstall script
AC_PROG_INSTALL

# Generate the makefiles
AC_CONFIG_FILES([
	Makefile
	src/Makefile
	config/Makefile
])

AC_OUTPUT
