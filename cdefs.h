#ifndef SH_LOCAL_CDEFS_H
#define SH_LOCAL_CDEFS_H

#ifndef __dead2
#define __dead2 __attribute__((__noreturn__))
#endif

#ifndef __printflike
#define __printflike(fmtarg, firstvararg) \
	__attribute__((__format__(__printf__, fmtarg, firstvararg)))
#endif

#ifndef __printf0like
#define __printf0like(fmtarg, firstvararg) \
	__attribute__((__format__(__printf__, fmtarg, firstvararg)))
#endif

#ifndef __nonstring
#define __nonstring
#endif

#endif
