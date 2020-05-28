#include <windows.h>
#include <fcntl.h>
#include <errno.h>
#include "pipe.h"
#include "ChildData.h"
#include <sys/stat.h>


#define STDIN (0)
#define STDOUT (1)
#define STDERR (2)

// low-level io

typedef struct _FD_STRUCT {
        Pipe*                           pipe;                           // if non-NULL, use this instead of hFile
        unsigned char           pipeChannel;            // fd2 of RedirArg for pipe
        HANDLE                          hFile;
        BOOL                            binary;
        BOOL                            eof;
} _FD_STRUCT;

#define FD_MAX                  (2048)
#define FD_BLOCK_SIZE   (32)    /* changing this will screw up "in_use" code below */
#define FD_MAX_BLOCKS   (FD_MAX/FD_BLOCK_SIZE)

typedef struct _FD_BLOCK {
        unsigned long           in_use;                         // bitmask of in-use entries, LSB=fds[0], MSB=fds[31]
        _FD_STRUCT                      fds[FD_BLOCK_SIZE];     // fd's
} _FD_BLOCK;

_FD_BLOCK       _fd_block0 = {
        0x00000007,     // first three in use (reserved)
        {
                { NULL, 0xff, INVALID_HANDLE_VALUE, FALSE, FALSE },
                { NULL, 0xff, INVALID_HANDLE_VALUE, FALSE, FALSE },
                { NULL, 0xff, INVALID_HANDLE_VALUE, FALSE, FALSE }
        }
};
_FD_BLOCK*      _fd_blocks[FD_MAX_BLOCKS] = { &_fd_block0 };

typedef struct _FILE_BLOCK _FILE_BLOCK;
typedef struct _FILE {
        int                                     file_index;
        int                                     fd;
        int                                     bufferedChar;
        BOOL                            error;
} _FILE;

#define FILE_MAX                (512)
#define FILE_BLOCK_SIZE (32)    /* changing this will screw up "in_use" code below */
#define FILE_MAX_BLOCKS (FILE_MAX/FILE_BLOCK_SIZE)

typedef struct _FILE_BLOCK {
        unsigned long           in_use;                         // bitmask of in-use entries, LSB=file[0], MSB=file[31]
        _FILE                           files[FILE_BLOCK_SIZE]; // file's
} _FILE_BLOCK;


_FILE_BLOCK             _file_block0 = {
        0x00000007,     // first three in use (reserved)
        {
                { 0, STDIN, -1 },
                { 1, STDOUT, -1 },
                { 2, STDERR, -1 },
                // maybe there should get initialised at runtime, but that means re-initialising uneccesarily on each use
                { 3 },
                { 4 },
                { 5 },
                { 6 },
                { 7 },
                { 8 },
                { 9 },
                { 10 },
                { 11 },
                { 12 },
                { 13 },
                { 14 },
                { 15 },
                { 16 },
                { 17 },
                { 18 },
                { 19 },
                { 20 },
                { 21 },
                { 22 },
                { 23 },
                { 24 },
                { 25 },
                { 26 },
                { 27 },
                { 28 },
                { 29 },
                { 30 },
                { 31 }
        }
};
_FILE_BLOCK*    _file_blocks[FILE_MAX_BLOCKS] = { &_file_block0 };


#define fd_stdin (&_fd_block0.fds[0])
#define fd_stdout (&_fd_block0.fds[1])
#define fd_stderr (&_fd_block0.fds[2])

inline bool valid_fd(int fd)
{
        if (fd < FD_BLOCK_SIZE)
                return (_fd_block0.in_use & (1 << fd)) == 0 ? false : true;
        else
        {
                int     block = fd / FD_BLOCK_SIZE;
                if (_fd_blocks[block] == NULL)
                        return false;
                int     index = fd % FD_BLOCK_SIZE;
                return (_fd_blocks[block]->in_use & (1 << index)) == 0 ? false : true;
        }
}




static ChildData*       g_childData = NULL;



inline _FD_STRUCT* fds_from_index(int fd)
{
        if (fd < FD_BLOCK_SIZE)
                return &_fd_block0.fds[fd];
        else
        {
                int     block = fd / FD_BLOCK_SIZE;
                if (_fd_blocks[block] == NULL)
                        return NULL;
                int     index = fd % FD_BLOCK_SIZE;
                return &_fd_blocks[block]->fds[index];
        }
}



static bool _wopen_fds(const WCHAR* filename, int flags, int mode, _FD_STRUCT* fds)
{
        bool    result = false;
        bool    share_read = false;
        DWORD   dwDesiredAccess = 0;
        DWORD   dwShareMode = 0;
        DWORD   dwCreationDisposition = 0;
        DWORD   dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
        HANDLE  hFile = INVALID_HANDLE_VALUE;

        if (filename == NULL || fds == NULL)
                return NULL;

        if ((flags & O_BINARY) && (flags & O_TEXT))
                goto cleanup;

        if (!(flags & O_WRONLY))
        {
                share_read = true;
                dwDesiredAccess |= GENERIC_READ;
        }
        if ((flags & O_WRONLY) || (flags & O_RDWR))
        {
                share_read = false;
                dwDesiredAccess |= GENERIC_WRITE;
        }
        if (share_read)
                dwShareMode |= FILE_SHARE_READ;

        if (flags & O_CREAT)
        {
                if (flags & O_TRUNC)
                        dwCreationDisposition = CREATE_ALWAYS;
                else if (flags & O_EXCL)
                        dwCreationDisposition = CREATE_NEW;
                else
                        dwCreationDisposition = OPEN_ALWAYS;
        }
        else if (flags & O_TRUNC)
                dwCreationDisposition = TRUNCATE_EXISTING;
        else
                dwCreationDisposition = OPEN_EXISTING;

        if ((flags & O_CREAT) && !(mode & S_IWRITE))
                dwFlagsAndAttributes = FILE_ATTRIBUTE_READONLY;

        hFile = CreateFile(filename, dwDesiredAccess, dwShareMode,
                        NULL, dwCreationDisposition, dwFlagsAndAttributes, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
                goto cleanup;

        if (flags & O_APPEND)
        {
                if (SetFilePointer(hFile, 0, NULL, FILE_END) == 0xFFFFFFFF)
                        goto cleanup;
        }

        fds->pipe = NULL;
        fds->pipeChannel = 0xff;
        fds->hFile = hFile;
        fds->binary = (flags & O_BINARY);
        fds->eof = FALSE;

        result = true;

cleanup:

        // close file on failure
        if (!result && hFile != INVALID_HANDLE_VALUE)
                CloseHandle(hFile);

        return result;
}


inline bool valid_file(int file_index)
{
        if (file_index < FILE_BLOCK_SIZE)
                return (_file_block0.in_use & (1 << file_index)) == 0 ? false : true;
        else
        {
                int     block = file_index / FILE_BLOCK_SIZE;
                if (_file_blocks[block] == NULL)
                        return false;
                int     index = file_index % FILE_BLOCK_SIZE;
                return (_file_blocks[block]->in_use & (1 << index)) == 0 ? false : true;
        }
}


static void fd_release(int fd)
{
        // mask as not in use
        int                             block = fd / FD_BLOCK_SIZE;
        int                             index = fd % FD_BLOCK_SIZE;
        unsigned long   index_bit = 1 << index;
        _fd_blocks[block]->in_use &= ~index_bit;
}

static _FILE* file_allocate()
{
        for (int block=0; block<FILE_MAX_BLOCKS; block++)
        {
                if (_file_blocks[block] == NULL)
                {       // unused block, allocate it
                        _file_blocks[block] = (_FILE_BLOCK*)malloc(sizeof(_FILE_BLOCK));
                        if (_file_blocks[block] == NULL)
                                return NULL;
                        // we'll use the first index
                        _file_blocks[block]->in_use = 0x00000001;
                        // set all file_index's
                        for (int index=0; index<FILE_BLOCK_SIZE; index++)
                                _file_blocks[block]->files[index].file_index = block*FILE_BLOCK_SIZE + index;
                        // return file at first index
                        return &_file_blocks[block]->files[0];
                }
                if (_file_blocks[block]->in_use != 0xffffffff)
                {       // there's an unused entry in this block, find it
                        int                             index;
                        unsigned long   index_bit = 0x00000001;
                        for (index=0; index<FILE_BLOCK_SIZE; index++)
                        {
                                if ((_file_blocks[block]->in_use & index_bit) == 0)
                                        break;  // found it
                                index_bit <<= 1;
                        }
                        // mark it as in use and return it
                        _file_blocks[block]->in_use |= index_bit;
                        return &_file_blocks[block]->files[index];
                }
        }
        // if we get here there are no free files
        return NULL;
}


extern "C" int close(int fd)
{
        bool            result = false;
        _FD_STRUCT*     fds;

        fds = fds_from_index(fd);
        if (fds == NULL)
                goto cleanup;

        if (!CloseHandle(fds->hFile))
                goto cleanup;

        fd_release(fd);

        result = true;

cleanup:

       // InSAT
		/* if (result == false)
                errno = -1;*/

        return result ? 0 : -1;
}




static bool initialisedStdHandles = false;


static void uninitStdHandles()
{
        if (!initialisedStdHandles)
                return;
        if (g_childData != NULL)
        {
                delete g_childData;
                g_childData = NULL;
        }
        if (valid_file(STDIN))
                fclose(stdin);
        if (valid_file(STDOUT))
                fclose(stdout);
        if (valid_file(STDERR))
                fclose(stderr);
        if (valid_fd(STDIN))
                close(STDIN);
        if (valid_fd(STDOUT))
                close(STDOUT);
        if (valid_fd(STDERR))
                close(STDERR);
        initialisedStdHandles = false;
}



static void shutdownIo()
{
        // TODO: Flush and close all _FILE's and then _FD_STRUCT's.
        // If we implement redirection of handles through other handles then we
        // probably need to shutdown all of the redirecting handles first, and
        // then the remaining handles.

        uninitStdHandles();
}



// returns true only if pipes have been initialised successfully
bool initStdHandles()
{
        if (initialisedStdHandles)
                return true;

#if 0
        // Get I/O redirection arguments from command-line
        char*   stdoutFilename;
        char*   stderrFilename;
        char*   stdinFilename;
        BOOL    stdoutAppend;
        BOOL    stderrAppend;
        BOOL    stdinAppend;
        if (getRedirArgs(GetCommandLine(),
                        &stdinFilename, &stdoutFilename, &stderrFilename, &stdinAppend, &stdoutAppend, &stderrAppend))
        {
        }
#endif

        TCHAR                   name[100];
        HANDLE                  hFileMapping = NULL;
        unsigned char*  pBuffer = NULL;

        _stprintf(name, TEXT("wcecompat.%08x.child_data"), GetCurrentProcessId());
        hFileMapping = CreateFileMapping((HANDLE)INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 1, name);
        if (hFileMapping == NULL)
                goto cleanup;
        else if (GetLastError() != ERROR_ALREADY_EXISTS)
        {
                CloseHandle(hFileMapping);
                hFileMapping = NULL;

                HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, TEXT("wcecompat.starting_child"));
                if (hEvent == NULL)
                {       // failed to create named event
                }
                else if (GetLastError() == ERROR_ALREADY_EXISTS)
                {       // we're in DllMain, so do nothing
                }
                else
                {
                        CloseHandle(hEvent);
                }
        }
        else
        {
                pBuffer = (unsigned char*)MapViewOfFile(hFileMapping, FILE_MAP_WRITE, 0, 0, 0);
                if (pBuffer == NULL)
                {       // failed to map buffer
                }
                else
                {
                        g_childData = new ChildData;
                        if (g_childData == NULL)
                                goto cleanup;
                        if (!g_childData->decode(pBuffer))//, 16384);
                                goto cleanup;
                        g_childData->restoreEnvironment();
                        RedirArg* stdinRedir = g_childData->getRedirArg(0);
                        RedirArg* stdoutRedir = g_childData->getRedirArg(1);
                        RedirArg* stderrRedir = g_childData->getRedirArg(2);
                        if (stdinRedir != NULL && stdinRedir->redirType != RT_NONE)
                        {
                                if (stdinRedir->redirType == RT_PIPE_UNSPEC)
                                {
                                        _FD_STRUCT* fds = fds_from_index(STDIN);
                                        if (fds == NULL)
                                                goto cleanup;
                                        fds->pipe = createPipe(stdinRedir->filename, OPEN_EXISTING);
                                        if (fds->pipe == NULL)
                                        {       // failed to open stdin pipe
                                                goto cleanup;
                                        }
                                        fds->pipeChannel = (unsigned char)stdinRedir->fd2;
                                }
                                else if (stdinRedir->redirType == RT_HANDLE)
                                {
                                }
                                else if (stdinRedir->redirType == RT_FILE)
                                {
//                                      WCHAR*  mode = L"r";    // default to "r" for the cases we don't know how to handle
                                        bool    r = stdinRedir->openForRead;
                                        bool    w = stdinRedir->openForWrite;
                                        bool    a = stdinRedir->append;
/*
                                        // rwa  mode
                                        // 010  "w"
                                        // 011  "a"
                                        // 100  "r"
                                        // 110  "r+"
                                        // 111  "a+"
                                        if (a)
                                        {
                                                if (r)
                                                        mode = L"a+";
                                                else
                                                        mode = L"a";
                                        }
                                        else if (r)
                                        {
                                                if (w)
                                                        mode = L"r+";
                                                else
                                                        mode = L"r";
                                        }
                                        else if (w)
                                                mode = L"w";
                                        FILE*   f = _wfopen(stdinRedir->filename, mode);
                                        if (f == NULL)
                                                goto cleanup;
                                        memcpy(&mystdin, f, sizeof(_FD_STRUCT));
                                        free(f);
*/
                                        // rwa  mode
                                        // 010  "w"             w,   CREATE_ALWAYS                                      O_WRONLY                                O_CREAT|O_TRUNC
                                        // 011  "a"             w,   OPEN_ALWAYS   (APPEND DATA)        O_WRONLY        O_APPEND        O_CREAT
                                        // 100  "r"             r,   OPEN_EXISTING                                      O_RDONLY                                
                                        // 110  "r+"    r/w, OPEN_EXISTING                                      O_RDWR                                  
                                        // 111  "a+"    r/w, OPEN_ALWAYS   (APPEND DATA)        O_RDWR          O_APPEND        O_CREAT
                                        int     flags = 0;
                                        int     mode = 0;
                                        if (r && w)
                                                flags |= O_RDWR;
                                        else if (r)
                                                flags |= O_RDONLY;
                                        else if (w)
                                                flags |= O_WRONLY;
                                        if (w)
                                        {
                                                if (!(r && !a))
                                                {
                                                        flags |= O_CREAT;
                                                        mode = S_IREAD | S_IWRITE;
                                                }
                                                if (!r && !a)
                                                        flags |= O_TRUNC;
                                        }
                                        if (a)
                                                flags |= O_APPEND;
                                        _FD_STRUCT* fds = fds_from_index(STDIN);
                                        if (fds == NULL)
                                                goto cleanup;
                                        if (!_wopen_fds(stdinRedir->filename, flags, mode, fds))
                                                goto cleanup;
                                }
                        }
                        if (stdoutRedir != NULL && stdoutRedir->redirType != RT_NONE)
                        {
                                if (stdoutRedir->redirType == RT_PIPE_UNSPEC)
                                {
                                        _FD_STRUCT* fds = fds_from_index(STDOUT);
                                        if (fds == NULL)
                                                goto cleanup;
                                        fds->pipe = createPipe(stdoutRedir->filename, OPEN_EXISTING);
                                        if (fds->pipe == NULL)
                                        {       // failed to open stdout pipe
                                                goto cleanup;
                                        }
                                        fds->pipeChannel = (unsigned char)stdoutRedir->fd2;
                                }
                                else if (stdoutRedir->redirType == RT_HANDLE)
                                {
                                }
                                else if (stdoutRedir->redirType == RT_FILE)
                                {
//                                      WCHAR*  mode = L"r";    // default to "r" for the cases we don't know how to handle
                                        bool    r = stdoutRedir->openForRead;
                                        bool    w = stdoutRedir->openForWrite;
                                        bool    a = stdoutRedir->append;
/*
                                        // rwa  mode
                                        // 010  "w"
                                        // 011  "a"
                                        // 100  "r"
                                        // 110  "r+"
                                        // 111  "a+"
                                        if (a)
                                        {
                                                if (r)
                                                        mode = L"a+";
                                                else
                                                        mode = L"a";
                                        }
                                        else if (r)
                                        {
                                                if (w)
                                                        mode = L"r+";
                                                else
                                                        mode = L"r";
                                        }
                                        else if (w)
                                                mode = L"w";
                                        FILE*   f = _wfopen(stdoutRedir->filename, mode);
                                        if (f == NULL)
                                                goto cleanup;
                                        memcpy(&mystdout, f, sizeof(_FD_STRUCT));
                                        free(f);
*/
                                        // rwa  mode
                                        // 010  "w"             w,   CREATE_ALWAYS                                      O_WRONLY                                O_CREAT|O_TRUNC
                                        // 011  "a"             w,   OPEN_ALWAYS   (APPEND DATA)        O_WRONLY        O_APPEND        O_CREAT
                                        // 100  "r"             r,   OPEN_EXISTING                                      O_RDONLY                                
                                        // 110  "r+"    r/w, OPEN_EXISTING                                      O_RDWR                                  
                                        // 111  "a+"    r/w, OPEN_ALWAYS   (APPEND DATA)        O_RDWR          O_APPEND        O_CREAT
                                        int     flags = 0;
                                        int     mode = 0;
                                        if (r && w)
                                                flags |= O_RDWR;
                                        else if (r)
                                                flags |= O_RDONLY;
                                        else if (w)
                                                flags |= O_WRONLY;
                                        if (w)
                                        {
                                                if (!(r && !a))
                                                {
                                                        flags |= O_CREAT;
                                                        mode = S_IREAD | S_IWRITE;
                                                }
                                                if (!r && !a)
                                                        flags |= O_TRUNC;
                                        }
                                        if (a)
                                                flags |= O_APPEND;
                                        _FD_STRUCT* fds = fds_from_index(STDOUT);
                                        if (fds == NULL)
                                                goto cleanup;
                                        if (!_wopen_fds(stdoutRedir->filename, flags, mode, fds))
                                                goto cleanup;
                                }
                        }
                        if (stderrRedir != NULL && stderrRedir->redirType != RT_NONE)
                        {
                                if (stderrRedir->redirType == RT_PIPE_UNSPEC)
                                {
                                        _FD_STRUCT* fds = fds_from_index(STDERR);
                                        if (fds == NULL)
                                                goto cleanup;
                                        if (stdoutRedir != NULL && stdoutRedir->redirType == RT_PIPE_UNSPEC &&
                                                        wcscmp(stderrRedir->filename, stdoutRedir->filename) == 0)
                                        {
                                                _FD_STRUCT* fds_stdout = fds_from_index(STDOUT);
                                                if (fds_stdout == NULL)
                                                        goto cleanup;
                                                fds->pipe = fds_stdout->pipe;
                                        }
                                        else
                                        {
                                                fds->pipe = createPipe(stderrRedir->filename, OPEN_EXISTING);
                                                if (fds->pipe == NULL)
                                                {       // failed to open stderr pipe
                                                        goto cleanup;
                                                }
                                        }
                                        fds->pipeChannel = (unsigned char)stderrRedir->fd2;
                                }
                                else if (stderrRedir->redirType == RT_HANDLE)
                                {
                                }
                                else if (stderrRedir->redirType == RT_FILE)
                                {
//                                      WCHAR*  mode = L"r";    // default to "r" for the cases we don't know how to handle
                                        bool    r = stderrRedir->openForRead;
                                        bool    w = stderrRedir->openForWrite;
                                        bool    a = stderrRedir->append;
/*
                                        // rwa  mode
                                        // 010  "w"
                                        // 011  "a"
                                        // 100  "r"
                                        // 110  "r+"
                                        // 111  "a+"
                                        if (a)
                                        {
                                                if (r)
                                                        mode = L"a+";
                                                else
                                                        mode = L"a";
                                        }
                                        else if (r)
                                        {
                                                if (w)
                                                        mode = L"r+";
                                                else
                                                        mode = L"r";
                                        }
                                        else if (w)
                                                mode = L"w";
                                        FILE*   f = _wfopen(stderrRedir->filename, mode);
                                        if (f == NULL)
                                                goto cleanup;
                                        memcpy(&mystderr, f, sizeof(_FD_STRUCT));
                                        free(f);
*/
                                        // rwa  mode
                                        // 010  "w"             w,   CREATE_ALWAYS                                      O_WRONLY                                O_CREAT|O_TRUNC
                                        // 011  "a"             w,   OPEN_ALWAYS   (APPEND DATA)        O_WRONLY        O_APPEND        O_CREAT
                                        // 100  "r"             r,   OPEN_EXISTING                                      O_RDONLY                                
                                        // 110  "r+"    r/w, OPEN_EXISTING                                      O_RDWR                                  
                                        // 111  "a+"    r/w, OPEN_ALWAYS   (APPEND DATA)        O_RDWR          O_APPEND        O_CREAT
                                        int     flags = 0;
                                        int     mode = 0;
                                        if (r && w)
                                                flags |= O_RDWR;
                                        else if (r)
                                                flags |= O_RDONLY;
                                        else if (w)
                                                flags |= O_WRONLY;
                                        if (w)
                                        {
                                                if (!(r && !a))
                                                {
                                                        flags |= O_CREAT;
                                                        mode = S_IREAD | S_IWRITE;
                                                }
                                                if (!r && !a)
                                                        flags |= O_TRUNC;
                                        }
                                        if (a)
                                                flags |= O_APPEND;
                                        _FD_STRUCT* fds = fds_from_index(STDERR);
                                        if (fds == NULL)
                                                goto cleanup;
                                        if (!_wopen_fds(stderrRedir->filename, flags, mode, fds))
                                                goto cleanup;
                                }
                        }
                }
        }

        initialisedStdHandles = true;
        atexit(shutdownIo);

cleanup:

        if (!initialisedStdHandles)
                uninitStdHandles();

        if (pBuffer != NULL)
                UnmapViewOfFile(pBuffer);
        if (hFileMapping != NULL)
                CloseHandle(hFileMapping);

        return initialisedStdHandles;
}


static inline bool initStdHandlesInline()
{
        if (initialisedStdHandles)
                return true;
        return initStdHandles();
}

extern "C" int kbhit(void)
{
        if (!valid_fd(STDIN))
        {
			printf("valid_fd\n\r");
                if (!initStdHandlesInline())
				{
					printf("initStdHandlesInline\n\r");
                        return 0;
				}
                if (!valid_fd(STDIN))
				{
								printf("valid_fd 2\n\r");
                        return 0;
				}
        }
//		printf("fd_stdin->pipe %d\n\r",fd_stdin->pipe);
        if (fd_stdin->pipe != NULL)
        {
			printf("pipeReadable(fd_stdin->pipe) %d\n\r",pipeReadable(fd_stdin->pipe));
                return pipeReadable(fd_stdin->pipe) ? 1 : 0;
        }
        else
                return 0;
}
