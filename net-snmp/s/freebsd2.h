#include "freebsd.h"

/* this is not good enough before freebsd3! */
#undef HAVE_NET_IF_MIB_H
#ifdef _PATH_UTMP
#define UTMP_FILE _PATH_UTMP
#else
#define UTMP_FILE "/var/run/utmp"
#endif
#undef PROC_SYMBOL
#undef NPROC_SYMBOL
#undef LOADAVE_SYMBOL
#undef TOTAL_MEMORY_SYMBOL
