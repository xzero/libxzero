// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef xzero_sysconfig_h
#define xzero_sysconfig_h (1)

//#include <base/sysconfig.h>

#cmakedefine LIBXZERO_VERSION "@LIBXZERO_VERSION@"

// --------------------------------------------------------------------------
// feature tests

// Build with inotify support
#cmakedefine XZERO_ENABLE_INOTIFY

#cmakedefine ENABLE_ACCEPT4
#cmakedefine ENABLE_PIPE2

#cmakedefine ENABLE_MULTI_ACCEPT

#cmakedefine ENABLE_PCRE

#cmakedefine ENABLE_INOTIFY

// Enable support for TCP_DEFER_ACCEPT
#cmakedefine ENABLE_TCP_DEFER_ACCEPT

// Try to open temporary files with O_TMPFILE flag before falling back
// to the standard behaviour.
#cmakedefine XZERO_ENABLE_O_TMPFILE

#cmakedefine XZERO_ENABLE_NOEXCEPT

// Builds with support for opportunistic write() calls to client sockets
#cmakedefine XZERO_OPPORTUNISTIC_WRITE 1

// --------------------------------------------------------------------------
// header tests

#cmakedefine HAVE_SYS_INOTIFY_H
#cmakedefine HAVE_SYS_SENDFILE_H
#cmakedefine HAVE_SYS_RESOURCE_H
#cmakedefine HAVE_SYS_LIMITS_H
#cmakedefine HAVE_SYS_MMAN_H
#cmakedefine HAVE_SYSLOG_H
#cmakedefine HAVE_DLFCN_H
#cmakedefine HAVE_EXECINFO_H
#cmakedefine HAVE_PWD_H
#cmakedefine HAVE_UNISTD_H
#cmakedefine HAVE_PTHREAD_H

#cmakedefine HAVE_NETDB_H
#cmakedefine HAVE_AIO_H
#cmakedefine HAVE_LIBAIO_H
#cmakedefine HAVE_ZLIB_H
#cmakedefine HAVE_BZLIB_H
#cmakedefine HAVE_GNUTLS_H
#cmakedefine HAVE_LUA_H
#cmakedefine HAVE_PCRE_H
#cmakedefine HAVE_SYS_UTSNAME_H
#cmakedefine HAVE_SECURITY_PAM_APPL_H

// --------------------------------------------------------------------------
// functional tests

#cmakedefine HAVE_INOTIFY_INIT1
#cmakedefine HAVE_CHROOT
#cmakedefine HAVE_PATHCONF
#cmakedefine HAVE_SENDFILE
#cmakedefine HAVE_POSIX_FADVISE
#cmakedefine HAVE_READAHEAD
#cmakedefine HAVE_PREAD
#cmakedefine HAVE_SYSCONF
#cmakedefine HAVE_PATHCONF
#cmakedefine HAVE_ACCEPT4
#cmakedefine HAVE_PIPE2
#cmakedefine HAVE_DUP2
#cmakedefine HAVE_FORK
#cmakedefine HAVE_BACKTRACE
#cmakedefine HAVE_CLOCK_GETTIME
#cmakedefine HAVE_PTHREAD_SETNAME_NP
#cmakedefine HAVE_PTHREAD_SETAFFINITY_NP

#endif