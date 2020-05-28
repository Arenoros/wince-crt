#ifndef __CRT_WINCE_H__
#define __CRT_WINCE_H__

/*
 * Copyright 2013 Marco Lizza (marco.lizza@gmail.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. 
 */

/* used by lauxlib.c, in posix each system defines its
 * default buffer size for I/O
 */

#include <windef.h>
#include <winbase.h>
#define BUFSIZ 256

/* size of strerror() message buffer */
#define STRERRORLEN 129

int strcoll(const char *s1,const char *s2);
char *strerror(int errnum);

FILE *freopen(const char * filename, const char * mode, FILE * stream);
char *getenv(const char *name);
int remove(const char *pathname);
int rename(const char *oldname, const char *newname);

int system(const char *command);

/* for whatever reason, it seems that the following constants
 * are not declared in the WinCE header files
 */
#define _IOFBF          0x0000
#define _IOLBF          0x0040
#define _IONBF          0x0004

/* tmpfile on WinCE create files named 'prefixxxx.TMP' where each
 * x is a hexadecimal character
 */

char *tmpnam(char *s);
FILE *tmpfile(void);

/* used by ldo.c
 */
void abort();

//WINBASEAPI DWORD WINAPI GetCurrentDirectory(DWORD  nBufferLength, LPTSTR lpBuffer);
EXTERN_C DWORD GetCurrentDirectory(DWORD  nBufferLength, LPTSTR lpBuffer);

#define STD_INPUT_HANDLE    ((DWORD)-10)
#define STD_OUTPUT_HANDLE   ((DWORD)-11)
#define STD_ERROR_HANDLE    ((DWORD)-12)



char * __cdecl getenv(const char *name);
int    __cdecl _putenv(const char *);
int    __cdecl _wputenv(const wchar_t *);
extern char** _environ;    /* pointer to environment table */
extern wchar_t** _wenviron;    /* pointer to wide environment table */
#define putenv _putenv
#define environ _environ
#ifdef UNICODE
#define _tputenv    _wputenv
#define _tenviron   _wenviron
#else
#define _tputenv    _putenv
#define _tenviron   _environ
#endif

char * _i64toa(__int64 __val, char *__string, int __radix);

#endif