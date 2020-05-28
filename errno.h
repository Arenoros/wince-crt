#ifndef __wcecompat__ERRNO_H__
#define __wcecompat__ERRNO_H__

#include <windows.h>

#define errno ((int)GetLastError())



#ifdef __cplusplus
extern "C" {
#endif


#define ENOENT  (2)
#define EBADF   (9)
#define EAGAIN  (11)
#define ENOMEM  (12)
#define EACCES  (13)    /* Permission denied */
#define EINVAL  (22)
#define ENOSPC  (28)    /* No space left on device */
#define ESPIPE  (29)

#ifdef __cplusplus
}
#endif


#endif // __wcecompat__ERRNO_H__


