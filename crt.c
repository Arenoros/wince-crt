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

#include <windows.h>
#include <time.h>

#include <crt.h>

#include "signal.h"

int strcoll(const char *s1,const char *s2)
{
    /* we won't be using locale for strings, since we are't UNICODE */
    return strcmp(s1, s2);
}

/* strerror()
 * used in lauxlib, liolib, loslib, luac
 */
char *strerror(int errnum)
{
    static char _strerror[STRERRORLEN];
    DWORD result;

    result = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errnum, 0, _strerror, STRERRORLEN, NULL);

    if (result == 0) {
        return NULL;
    }

    return _strerror;
}

/* FormatMessageA(): Format..W() and convert to char
 * used in loadlib.c
 */
DWORD WINAPI FormatMessageA(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId,
                            DWORD dwLanguageId, LPSTR lpBuffer, DWORD nSize,
                            va_list *Arguments)
{
    wchar_t bufferW[STRERRORLEN];
    int lengthW;

    lengthW = FormatMessageW(dwFlags, lpSource, dwMessageId, dwLanguageId,
        bufferW, STRERRORLEN, Arguments);

    if (lengthW == 0) {
        return 0;
    }

    /* always add null character when converting */
    return WideCharToMultiByte(CP_UTF8, 0, bufferW, lengthW + 1, lpBuffer, nSize,
        NULL, NULL);
}

/* LoadLibraryA(): convert to wchar_t and Load..W()
 * used in loadlib.c
 */
HMODULE WINAPI LoadLibraryExA(LPCSTR lpFileName, HANDLE hFile, DWORD dwFlags)
{
    wchar_t lpFileNameW[MAX_PATH + 1];
    size_t length = strlen(lpFileName);
    int lengthW;

    lengthW = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        lpFileName, length + 1, lpFileNameW, MAX_PATH);

    return LoadLibraryExW(lpFileNameW, hFile, dwFlags);
}

/*GetModuleFileNameA():
 * used in loadlib.c
 */
DWORD GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize)
{
    wchar_t filenameW[MAX_PATH + 1];
    int lengthW;

    lengthW = GetModuleFileNameW(hModule, filenameW, MAX_PATH);

    if (lengthW == 0) {
        return 0;
    }

    return WideCharToMultiByte(CP_UTF8, 0, filenameW, lengthW + 1,
        lpFilename, nSize, NULL, NULL);
}

DWORD GetCurrentDirectory(DWORD  nBufferLength, LPTSTR lpBuffer)
{
	DWORD len ;
	TCHAR buffer[MAX_PATH];
	TCHAR  *lCharEntryPtr ;
    GetModuleFileName( NULL, buffer, MAX_PATH );
	lCharEntryPtr = strrchr(buffer,'\\');
	if(lCharEntryPtr!=NULL)
	{
     len = lCharEntryPtr - buffer;
	} else
		return 0;
	if(len > nBufferLength)
		return len+1;
	else
	{
		memcpy(lpBuffer,buffer,len);
		lpBuffer[len]=0;
	}
    return len;
	//string( buffer ).substr( 0, pos);
}


/* freopen()
 * used by lauxlib,
 * implemented anew, though there is _wfreopen(), but that one
 * would be more trouble converting filenames than it's worth.
 */
FILE *freopen(const char * filename, const char * mode, FILE * stream)
{
	/* no support for changing the mode of an open file */
	if(!filename) return NULL;

	(void)fflush(stream);
	(void)fclose(stream);
	stream = fopen(filename, mode);
	return stream;
}

/* WinCE has no environment variables
 * we just return NULL pointers.  The caller must think, that the
 * requested environement variable is not defined.
 */
char *getenv(const char *name)
{
    return NULL;
}

/* tmpnam() implementation,
 * used in loslib.c
 * create unique names and(!) files \Temp\luaxxxx.TMP
 * TODO: investigate, if these are cleaned on exit
 */
static char _tmpnam[MAX_PATH];

char *tmpnam(char *s)
{
	wchar_t tmp[MAX_PATH+1];
	wchar_t tfn[MAX_PATH+1];
	DWORD len;

	len=GetTempPath(MAX_PATH,tmp);
	if(!len || len>=MAX_PATH) return NULL;

	if (!(GetTempFileName(
		tmp,
		L"lua",
		0,
		tfn)))
		return NULL;
	if(!s) s=_tmpnam;

    len = wcslen(tfn);
    WideCharToMultiByte(CP_UTF8, 0, tfn, len + 1, s, MAX_PATH, NULL, NULL);
    return s;
};

/* tmpfile() implementation 
 * used in liolib.c
 */
FILE *tmpfile(void) {
    char filename[MAX_PATH + 1];
	if(!tmpnam(filename))
        return NULL;
	return fopen(filename, "wr+");
}

/* Workaround: indicate that no command processor is available
 * or return -1 and set errno (LastError) to something..
 */
int system(const char *command)
{
	int len, lenW;
	wchar_t *commandW;
	PROCESS_INFORMATION pi;
	DWORD rc;

	if(!command) {/* check for cmd.exe */
		if(GetFileAttributesW(L"\\Windows\\cmd.exe")==0xFFFFFFFF) {
			SetLastError(ERROR_FILE_NOT_FOUND);
			return -1;
		}
		else return 0;
	}

	len = strlen("/c ") + strlen(command) + 1;
	if(!(commandW=(wchar_t *)calloc(len, sizeof(wchar_t)))) {
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		return -1;
	}

    wcscpy(commandW, L"/c ");
    len = strlen(command);
    lenW = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, command, len + 1, 
        commandW + wcslen(commandW), len + 1);

	if(!CreateProcessW(L"cmd",commandW,NULL,NULL,FALSE,0,NULL,NULL,NULL,&pi))
		goto exit_fail;
	if(!GetExitCodeProcess(pi.hProcess,&rc))
        goto exit_fail;
	free(commandW);
	return(rc);

exit_fail:
	free(commandW);
	return -1;
}

/* remove()
 * used in loslib.c
 * works on files and directories and returns 0 on success
 */
int remove(const char *pathname)
{
	wchar_t pathnameW[MAX_PATH + 1];
	DWORD dwFileAttributes;
    int len;

    len = strlen(pathname);
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, pathname, len + 1,
        pathnameW, MAX_PATH))
        return -1;

	dwFileAttributes = GetFileAttributes(pathnameW);
	if (dwFileAttributes == 0xFFFFFFFF)
        return -1;

    if (dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
        if (!SetFileAttributes(pathnameW, dwFileAttributes & ~FILE_ATTRIBUTE_READONLY))
            return -1;
    }

	if (dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		if (!RemoveDirectory(pathnameW))
            return -1;
    } else {
		if (!DeleteFile(pathnameW))
            return -1;
    }

	return 0;
}

/* rename() - rename a file or directory
 * used in loslib.c
 */
int rename(const char *oldname, const char *newname)
{
	wchar_t oldnameW[MAX_PATH + 1], newnameW[MAX_PATH + 1];
	int result, len;

    len = strlen(oldname);
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, oldname, len + 1,
        oldnameW, MAX_PATH))
        return -1;

    len = strlen(newname);
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, newname, len + 1,
        newnameW, MAX_PATH))
        return -1;

    result = MoveFileW(oldnameW, newnameW);

    return result?0:-1;
}

/* signal():
 * used only in lua.c
 * Workaround: fail to set signals
 */
void (*signal(int sig, void (*func)(int)))(int)
{
	SetLastError(ERROR_PROC_NOT_FOUND);	
	return SIG_ERR;
}

/* abort():
 */
void abort()
{
    exit(-1);
}


void *bsearch(const void *key, const void *base,
size_t nmemb, size_t size,
int (*compar)(const void *, const void *))
{
size_t odd_mask, bytes;
const char *center, *high, *low;
int comp;

odd_mask = ((size ^ (size - 1)) >> 1) + 1;
low = base;
bytes = nmemb == 0 ? size : size + 1;
center = low + nmemb * size;
comp = 0;
while (bytes != size) {
if (comp > 0) {
low = center;
} else {
high = center;
}
bytes = high - low;
center = low + ((bytes & odd_mask ? bytes - size : bytes) >> 1);
comp = compar(key, center);
if (comp == 0) {
return (void *)center;
}
}
return NULL;
}

