/*
     This file is part of libextractor.
     (C) 2004, 2005 Christian Grothoff (and other contributing authors)

     libextractor is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 2, or (at your
     option) any later version.

     libextractor is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with libextractor; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
*/

/**
 * @file util/winproc.c
 * @brief Functions for MS Windows
 * @author Nils Durner
 */

/*** Note: this file differs from GNUnet's winproc.c ***/

#include "platform.h"

#ifdef	__cplusplus
extern "C" {
#endif

const char *errlist[] = {
  gettext_noop("No error"),
  gettext_noop("Unknown host"),                       /* 1 HOST_NOT_FOUND */
  gettext_noop("Host name lookup failure"),           /* 2 TRY_AGAIN */
  gettext_noop("Unknown server error"),               /* 3 NO_RECOVERY */
  gettext_noop("No address associated with name"),    /* 4 NO_ADDRESS */
  gettext_noop("Internal resolver error"),            /* errno < 0 */
  gettext_noop("Unknown resolver error")              /* errno > 4 */
};

typedef struct {
  char *pStart;
  HANDLE hMapping;
} TMapping;

static char szRootDir[_MAX_PATH + 1];
static long lRootDirLen;
static char szHomeDir[_MAX_PATH + 2];
static long lHomeDirLen;
static char szUser[261];
static OSVERSIONINFO theWinVersion;
unsigned int uiSockCount = 0;
Winsock *pSocks;
static char __langinfo[251];
static unsigned int uiMappingsCount = 0;
static TMapping *pMappings;
HANDLE hMappingsLock;

static HINSTANCE hNTDLL, hIphlpapi;
TNtQuerySystemInformation GNNtQuerySystemInformation;
TGetIfEntry GNGetIfEntry;
TGetIpAddrTable GNGetIpAddrTable;
TGetIfTable GNGetIfTable;

/**
 * @author Prof. A Olowofoyeku (The African Chief)
 * @author Frank Heckenbach
 * source: http://gd.tuwien.ac.at/gnu/mingw/os-hacks.h
 */

int truncate(const char *fname, int distance)
{
  int i;
  HANDLE hFile;

  i = -1;
  hFile = CreateFile(fname, GENERIC_READ | GENERIC_WRITE,
                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                     NULL, OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
  if(hFile != INVALID_HANDLE_VALUE)
  {
    if(SetFilePointer(hFile, distance, NULL, FILE_BEGIN) != 0xFFFFFFFF)
    {
      if(SetEndOfFile(hFile))
        i = 0;
    }
    CloseHandle(hFile);
  }
  return i;
}

/*********************** statfs ****************************/

/**
 * @author Prof. A Olowofoyeku (The African Chief)
 * @author Frank Heckenbach
 * source: http://gd.tuwien.ac.at/gnu/mingw/os-hacks.h
 */

int statfs(const char *path, struct statfs *buf)
{
  HINSTANCE h;
  FARPROC f;
  char tmp[MAX_PATH], resolved_path[MAX_PATH];
  int retval = 0;

  realpath(path, resolved_path);
  if(!resolved_path)
    retval = -1;
  else
  {
    /* check whether GetDiskFreeSpaceExA is supported */
    h = LoadLibraryA("kernel32.dll");
    if(h)
      f = GetProcAddress(h, "GetDiskFreeSpaceExA");
    else
      f = NULL;
    if(f)
    {
      ULARGE_INTEGER bytes_free, bytes_total, bytes_free2;
      if(!f(resolved_path, &bytes_free2, &bytes_total, &bytes_free))
      {
        errno = ENOENT;
        retval = -1;
      }
      else
      {
        buf->f_bsize = FAKED_BLOCK_SIZE;
        buf->f_bfree = (bytes_free.QuadPart) / FAKED_BLOCK_SIZE;
        buf->f_files = buf->f_blocks =
          (bytes_total.QuadPart) / FAKED_BLOCK_SIZE;
        buf->f_ffree = buf->f_bavail =
          (bytes_free2.QuadPart) / FAKED_BLOCK_SIZE;
      }
    }
    else
    {
      DWORD sectors_per_cluster, bytes_per_sector;
      if(h)
        FreeLibrary(h);
      if(!GetDiskFreeSpaceA(resolved_path, &sectors_per_cluster,
                            &bytes_per_sector, &buf->f_bavail,
                            &buf->f_blocks))
      {
        errno = ENOENT;
        retval = -1;
      }
      else
      {
        buf->f_bsize = sectors_per_cluster * bytes_per_sector;
        buf->f_files = buf->f_blocks;
        buf->f_ffree = buf->f_bavail;
        buf->f_bfree = buf->f_bavail;
      }
    }
    if(h)
      FreeLibrary(h);
  }

  /* get the FS volume information */
  if(strspn(":", resolved_path) > 0)
    resolved_path[3] = '\0';    /* we want only the root */
  if(GetVolumeInformation
     (resolved_path, NULL, 0, &buf->f_fsid, &buf->f_namelen, NULL, tmp,
      MAX_PATH))
  {
    if(strcasecmp("NTFS", tmp) == 0)
    {
      buf->f_type = NTFS_SUPER_MAGIC;
    }
    else
    {
      buf->f_type = MSDOS_SUPER_MAGIC;
    }
  }
  else
  {
    errno = ENOENT;
    retval = -1;
  }
  return retval;
}

/*********************** End of statfs **********************/

const char *hstrerror(int err)
{
  if(err < 0)
    err = 5;
  else if(err > 4)
    err = 6;

  return _(errlist[err]);
}

void gettimeofday(struct timeval *tp, void *tzp)
{
  struct _timeb theTime;

  _ftime(&theTime);
  tp->tv_sec = theTime.time;
  tp->tv_usec = theTime.millitm * 1000;
}

int mkstemp(char *tmplate)
{
  static const char letters[]
    = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  int iLen, iRnd;
  char *pChr;
  char szDest[_MAX_PATH + 1];

  iLen = strlen(tmplate);
  if(iLen >= 6)
  {
    pChr = tmplate + iLen - 6;
    srand((unsigned int) time(NULL));

    if(strncmp(pChr, "XXXXXX", 6) == 0)
    {
      int iChr;
      for(iChr = 0; iChr < 6; iChr++)
      {
        /* 528.5 = RAND_MAX / letters */
        iRnd = rand() / 528.5;
        *(pChr++) = letters[iRnd > 0 ? iRnd - 1 : 0];
      }
    }
    else
    {
      errno = EINVAL;
      return -1;
    }
  }
  else
  {
    errno = EINVAL;
    return -1;
  }

  conv_to_win_path(tmplate, szDest);

  return _open(szDest, _O_CREAT | _O_EXCL, _S_IREAD | _S_IWRITE);
}

/*********************** posix path -> win path ****************************/

/**
 * Get information from the Windows registry
 * @param hMainKey handle to (main-)key to query (HKEY_LOCAL_MACHINE etc.)
 * @param pszKey name of key to query
 * @param pszSubKey name of subkey to query
 * @param pszBuffer buffer for returned string
 * @param pdLength receives size of returned string
 * @return Error code from winerror.h, ERROR_SUCCESS on success
 */
long QueryRegistry(HKEY hMainKey, char *pszKey, char *pszSubKey,
              char *pszBuffer, long *pdLength)
{
  HKEY hKey;
  long lRet;

  if((lRet = RegOpenKeyEx(hMainKey, pszKey, 0, KEY_EXECUTE, &hKey)) ==
     ERROR_SUCCESS)
  {
    lRet = RegQueryValueEx(hKey, pszSubKey, 0, NULL, pszBuffer, pdLength);

    RegCloseKey(hKey);
  }

  return lRet;
}

/**
 * Determine the Windows path of our / directory
 * @return Error code from winerror.h, ERROR_SUCCESS on success
 */
long DetermineRootDir()
{
  char szModule[_MAX_PATH], szDrv[_MAX_DRIVE], szDir[_MAX_DIR];
  long lDirLen;

  /* Get the path of the calling module.
     It should be located in one of the "bin" directories */
  GetModuleFileName(NULL, szModule, MAX_PATH);
  _splitpath(szModule, szDrv, szDir, NULL, NULL);

  lDirLen = strlen(szDir);

  if(stricmp(szDir + lDirLen - 15, "\\usr\\local\\bin\\") == 0)
    szDir[lDirLen -= 14] = 0;
  /* "\\local\\bin" is right, "/usr" points to "/" under MinGW */
  else if(stricmp(szDir + lDirLen - 11, "\\local\\bin\\") == 0)
    szDir[lDirLen -= 10] = 0;
  else if(stricmp(szDir + lDirLen - 9, "\\usr\\bin\\") == 0)
    szDir[lDirLen -= 8] = 0;
  else if(stricmp(szDir + lDirLen - 5, "\\bin\\") == 0)
    szDir[lDirLen -= 4] = 0;
  else
  {
    /* Get the installation path from the registry */
    lDirLen = _MAX_PATH - 1;

    if(QueryRegistry
       (HKEY_CURRENT_USER, "Software\\GNU\\libextractor", "InstallDir",
        szRootDir, &lDirLen) != ERROR_SUCCESS)
    {
      lDirLen = _MAX_PATH - 1;

      if(QueryRegistry
         (HKEY_LOCAL_MACHINE, "Software\\GNU\\libextractor", "InstallDir",
          szRootDir, &lDirLen) != ERROR_SUCCESS)
      {
        return ERROR_BAD_ENVIRONMENT;
      }
    }
    strcat(szRootDir, "\\");
    lRootDirLen = lDirLen;
    szDrv[0] = 0;
  }

  if(szDrv[0])
  {
    strcpy(szRootDir, szDrv);
    lRootDirLen = 3 + lDirLen - 1;      /* 3 = strlen(szDir) */
    if(lRootDirLen > _MAX_PATH)
      return ERROR_BUFFER_OVERFLOW;

    strcat(szRootDir, szDir);
  }

  return ERROR_SUCCESS;
}

/**
 * Determine the user's home directory
 * @return Error code from winerror.h, ERROR_SUCCESS on success
*/
long DetermineHomeDir()
{
  char *lpszProfile = getenv("USERPROFILE");
  if(lpszProfile != NULL && lpszProfile[0] != 0)        /* Windows NT */
  {
    lHomeDirLen = strlen(lpszProfile);
    if(lHomeDirLen + 1 > _MAX_PATH)
      return ERROR_BUFFER_OVERFLOW;

    strcpy(szHomeDir, lpszProfile);
    if(szHomeDir[lHomeDirLen - 1] != '\\')
    {
      szHomeDir[lHomeDirLen] = '\\';
      szHomeDir[++lHomeDirLen] = 0;
    }
  }
  else
  {
    /* C:\My Documents */
    long lRet;

    lHomeDirLen = _MAX_PATH;
    lRet = QueryRegistry(HKEY_CURRENT_USER,
                         "Software\\Microsoft\\Windows\\CurrentVersion\\"
                         "Explorer\\Shell Folders",
                         "Personal", szHomeDir, &lHomeDirLen);

    if(lRet == ERROR_BUFFER_OVERFLOW)
      return ERROR_BUFFER_OVERFLOW;
    else if(lRet == ERROR_SUCCESS)
      lHomeDirLen--;
    else
    {
      /* C:\Program Files\GNUnet\home\... */
      /* 5 = strlen("home\\") */
      lHomeDirLen = strlen(szRootDir) + strlen(szUser) + 5 + 1;

      if(_MAX_PATH < lHomeDirLen)
        return ERROR_BUFFER_OVERFLOW;

      strcpy(szHomeDir, szRootDir);
      strcat(szHomeDir, szUser);
      strcat(szHomeDir, "\\");
    }
  }

  return ERROR_SUCCESS;
}

/**
 * Initialize POSIX emulation and set up Windows environment
 * @return Error code from winerror.h, ERROR_SUCCESS on success
*/
void InitWinEnv()
{
  long lRet;
  enum {ROOT, USER, HOME} eAction = ROOT;

  /* Init path translation */
  if((lRet = DetermineRootDir()) == ERROR_SUCCESS)
  {
    DWORD dwSize = 261;

    GetUserName(szUser, &dwSize);

    eAction = HOME;
    lRet = DetermineHomeDir();
  }

  if(lRet != ERROR_SUCCESS)
  {
    char *pszMsg, *pszMsg2;

    lRet =
      FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                    FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lRet, 0,
                    (LPTSTR) & pszMsg, 0, NULL);

    pszMsg2 = (char *) malloc(lRet + 1);
    strcpy(pszMsg2, pszMsg);
    if(pszMsg2[lRet - 2] == '\r')
      pszMsg2[lRet - 2] = 0;

    fprintf(stderr,
            eAction == ROOT
	    ? _("Cannot determine root directory (%s)\n")
	    : _("Cannot determine home directory (%s)\n"),
	    pszMsg2);
    LocalFree(pszMsg);
    free(pszMsg2);

    exit(1);
  }

  /* To keep track of mapped files */
  pMappings = (TMapping *) malloc(sizeof(TMapping));
  pMappings[0].pStart = NULL;
  hMappingsLock = CreateMutex(NULL, FALSE, NULL);

  /* Open files in binary mode */
  _fmode = _O_BINARY;

  /* Get Windows version */
  theWinVersion.dwOSVersionInfoSize = sizeof(theWinVersion);
  GetVersionEx(&theWinVersion);

  hNTDLL = LoadLibrary("ntdll.dll");

  /* Function to get CPU usage under Win NT */
  if (hNTDLL)
  {
    GNNtQuerySystemInformation = (TNtQuerySystemInformation)
      GetProcAddress(hNTDLL, "NtQuerySystemInformation");
  }
  else
  {
    GNNtQuerySystemInformation = NULL;
  }

  /* Functions to get information about a network adapter */
  hIphlpapi = LoadLibrary("iphlpapi.dll");
  if (hIphlpapi)
  {
    GNGetIfEntry = (TGetIfEntry) GetProcAddress(hIphlpapi, "GetIfEntry");
    GNGetIpAddrTable = (TGetIpAddrTable) GetProcAddress(hIphlpapi,
      "GetIpAddrTable");
    GNGetIfTable = (TGetIfTable) GetProcAddress(hIphlpapi, "GetIfTable");
  }
  else
  {
    GNGetIfEntry = NULL;
    GNGetIpAddrTable = NULL;
    GNGetIfTable = NULL;
  }

  /* Use ANSI codepage for console IO */
  SetConsoleCP(CP_ACP);
  SetConsoleOutputCP(CP_ACP);
  setlocale( LC_ALL, ".OCP" );
}

/**
 * Clean up Windows environment
 */
void ShutdownWinEnv()
{
  free(pMappings);
  CloseHandle(hMappingsLock);

  FreeLibrary(hNTDLL);
  FreeLibrary(hIphlpapi);
}

/**
 * Convert a POSIX-sytle path to a Windows-style path
 * @param pszUnix POSIX path
 * @param pszWindows Windows path
 * @return Error code from winerror.h, ERROR_SUCCESS on success
*/
int conv_to_win_path(const char *pszUnix, char *pszWindows)
{
  char *pSrc, *pDest;
  long iSpaceUsed;

  /* Check if we already have a windows path */
  if((strchr(pszUnix, '\\') != NULL) || (strchr(pszUnix, ':') != NULL))
  {
    if(strlen(pszUnix) > MAX_PATH)
      return ERROR_BUFFER_OVERFLOW;
    strcpy(pszWindows, pszUnix);
  }

  /* Is the unix path a full path? */
  if(pszUnix[0] == '/')
  {
    strcpy(pszWindows, szRootDir);
    iSpaceUsed = lRootDirLen;
    pDest = pszWindows + lRootDirLen;
    pSrc = (char *) pszUnix + 1;
  }
  /* Temp. dir? */
  else if(strncmp(pszUnix, "/tmp/", 5) == 0)
  {
    iSpaceUsed = GetTempPath(_MAX_PATH, pszWindows);
    if (iSpaceUsed > _MAX_PATH)
      return ERROR_BUFFER_OVERFLOW;
    pDest = pszWindows + iSpaceUsed;
    pSrc = (char *) pszUnix + 5;
  }
  /* Home dir? */
  else if(strncmp(pszUnix, "~/", 2) == 0)
  {
    strcpy(pszWindows, szHomeDir);
    iSpaceUsed = lHomeDirLen;
    pDest = pszWindows + lHomeDirLen;
    pSrc = (char *) pszUnix + 2;
  }
  /* Bit bucket? */
  else if (strncmp(pszUnix, "/dev/null", 9) == 0)
  {
    strcpy(pszWindows, "nul");
    iSpaceUsed = 3;
    pDest = pszWindows + lHomeDirLen;
    pSrc = (char *) pszUnix + 9;
  }
  else
  {
    pDest = pszWindows;
    iSpaceUsed = 0;
    pSrc = (char *) pszUnix;
  }

  if(iSpaceUsed + strlen(pSrc) + 1 > _MAX_PATH)
    return ERROR_BUFFER_OVERFLOW;

  /* substitute all slashes */
  while(*pSrc)
  {
    if(*pSrc == '/')
      *pDest = '\\';
    else
      *pDest = *pSrc;

    pDest++;
    pSrc++;
  }
  *pDest = 0;

  return ERROR_SUCCESS;
}

/*********************** posix path -> win path ****************************/

/**
 * Set errno according to a Windows error
 * @param lWinError Error code defined in winerror.h
 */
void _SetErrnoFromWinError(long lWinError, char *pszCaller, int iLine)
{
  switch(lWinError)
  {
    case ERROR_SUCCESS:
      errno = 0;
      break;

    case ERROR_INVALID_FUNCTION:
      errno = EBADRQC;
      break;

    case ERROR_FILE_NOT_FOUND:
      errno = ENOENT;
      break;

    case ERROR_PATH_NOT_FOUND:
      errno = ENOENT;
      break;

    case ERROR_TOO_MANY_OPEN_FILES:
      errno = EMFILE;
      break;

    case ERROR_ACCESS_DENIED:
      errno = EACCES;
      break;

    case ERROR_INVALID_HANDLE:
      errno = EBADF;
      break;

    case ERROR_NOT_ENOUGH_MEMORY:
      errno = ENOMEM;
      break;

    case ERROR_INVALID_DATA:
      errno = EINVAL;
      break;

    case ERROR_OUTOFMEMORY:
      errno = ENOMEM;
      break;

    case ERROR_INVALID_DRIVE:
      errno = ENODEV;
      break;

    case ERROR_NOT_SAME_DEVICE:
      errno = EXDEV;
      break;

    case ERROR_NO_MORE_FILES:
      errno = ENMFILE;
      break;

    case ERROR_WRITE_PROTECT:
      errno = EROFS;
      break;

    case ERROR_BAD_UNIT:
      errno = ENODEV;
      break;

    case ERROR_SHARING_VIOLATION:
      errno = EACCES;
      break;

    case ERROR_LOCK_VIOLATION:
      errno = EACCES;
      break;

    case ERROR_SHARING_BUFFER_EXCEEDED:
      errno = ENOLCK;
      break;

    case ERROR_HANDLE_EOF:
      errno = ENODATA;
      break;

    case ERROR_HANDLE_DISK_FULL:
      errno = ENOSPC;
      break;

    case ERROR_NOT_SUPPORTED:
      errno = ENOSYS;
      break;

    case ERROR_REM_NOT_LIST:
      errno = ENONET;
      break;

    case ERROR_DUP_NAME:
      errno = ENOTUNIQ;
      break;

    case ERROR_BAD_NETPATH:
      errno = ENOSHARE;
      break;

    case ERROR_BAD_NET_NAME:
      errno = ENOSHARE;
      break;

    case ERROR_FILE_EXISTS:
      errno = EEXIST;
      break;

    case ERROR_CANNOT_MAKE:
      errno = EPERM;
      break;

    case ERROR_INVALID_PARAMETER:
      errno = EINVAL;
      break;

    case ERROR_NO_PROC_SLOTS:
      errno = EAGAIN;
      break;

    case ERROR_BROKEN_PIPE:
      errno = EPIPE;
      break;

    case ERROR_OPEN_FAILED:
      errno = EIO;
      break;

    case ERROR_NO_MORE_SEARCH_HANDLES:
      errno = ENFILE;
      break;

    case ERROR_CALL_NOT_IMPLEMENTED:
      errno = ENOSYS;
      break;

    case ERROR_INVALID_NAME:
      errno = ENOENT;
      break;

    case ERROR_WAIT_NO_CHILDREN:
      errno = ECHILD;
      break;

    case ERROR_CHILD_NOT_COMPLETE:
      errno = EBUSY;
      break;

    case ERROR_DIR_NOT_EMPTY:
      errno = ENOTEMPTY;
      break;

    case ERROR_SIGNAL_REFUSED:
      errno = EIO;
      break;

    case ERROR_BAD_PATHNAME:
      errno = ENOENT;
      break;

    case ERROR_SIGNAL_PENDING:
      errno = EBUSY;
      break;

    case ERROR_MAX_THRDS_REACHED:
      errno = EAGAIN;
      break;

    case ERROR_BUSY:
      errno = EBUSY;
      break;

    case ERROR_ALREADY_EXISTS:
      errno = EEXIST;
      break;

    case ERROR_NO_SIGNAL_SENT:
      errno = EIO;
      break;

    case ERROR_FILENAME_EXCED_RANGE:
      errno = EINVAL;
      break;

    case ERROR_META_EXPANSION_TOO_LONG:
      errno = EINVAL;
      break;

    case ERROR_INVALID_SIGNAL_NUMBER:
      errno = EINVAL;
      break;

    case ERROR_THREAD_1_INACTIVE:
      errno = EINVAL;
      break;

    case ERROR_BAD_PIPE:
      errno = EINVAL;
      break;

    case ERROR_PIPE_BUSY:
      errno = EBUSY;
      break;

    case ERROR_NO_DATA:
      errno = EPIPE;
      break;

    case ERROR_PIPE_NOT_CONNECTED:
      errno = ECOMM;
      break;

    case ERROR_MORE_DATA:
      errno = EAGAIN;
      break;

    case ERROR_DIRECTORY:
      errno = ENOTDIR;
      break;

    case ERROR_PIPE_CONNECTED:
      errno = EBUSY;
      break;

    case ERROR_PIPE_LISTENING:
      errno = ECOMM;
      break;

    case ERROR_NO_TOKEN:
      errno = EINVAL;
      break;

    case ERROR_PROCESS_ABORTED:
      errno = EFAULT;
      break;

    case ERROR_BAD_DEVICE:
      errno = ENODEV;
      break;

    case ERROR_BAD_USERNAME:
      errno = EINVAL;
      break;

    case ERROR_NOT_CONNECTED:
      errno = ENOLINK;
      break;

    case ERROR_OPEN_FILES:
      errno = EAGAIN;
      break;

    case ERROR_ACTIVE_CONNECTIONS:
      errno = EAGAIN;
      break;

    case ERROR_DEVICE_IN_USE:
      errno = EAGAIN;
      break;

    case ERROR_INVALID_AT_INTERRUPT_TIME:
      errno = EINTR;
      break;

    case ERROR_IO_DEVICE:
      errno = EIO;
      break;

    case ERROR_NOT_OWNER:
      errno = EPERM;
      break;

    case ERROR_END_OF_MEDIA:
      errno = ENOSPC;
      break;

    case ERROR_EOM_OVERFLOW:
      errno = ENOSPC;
      break;

    case ERROR_BEGINNING_OF_MEDIA:
      errno = ESPIPE;
      break;

    case ERROR_SETMARK_DETECTED:
      errno = ESPIPE;
      break;

    case ERROR_NO_DATA_DETECTED:
      errno = ENOSPC;
      break;

    case ERROR_POSSIBLE_DEADLOCK:
      errno = EDEADLOCK;
      break;

    case ERROR_CRC:
      errno = EIO;
      break;

    case ERROR_NEGATIVE_SEEK:
      errno = EINVAL;
      break;

    case ERROR_NOT_READY:
      errno = ENOMEDIUM;
      break;

    case ERROR_DISK_FULL:
      errno = ENOSPC;
      break;

    case ERROR_NOACCESS:
      errno = EFAULT;
      break;

    case ERROR_FILE_INVALID:
      errno = ENXIO;
      break;

    case ERROR_INVALID_ADDRESS:
      errno = EFAULT;
      break;

    case ERROR_BUFFER_OVERFLOW:
      errno = ENOMEM;
      break;

    default:
      errno = ESTALE;
      fprintf(stderr, "ERROR: Unknown error %i in SetErrnoFromWinError(). " \
          "Source: %s:%i\n", lWinError, pszCaller, iLine);
      break;
  }
}

/**
 * Apply or remove an advisory lock on an open file
 */
int flock(int fd, int operation)
{
  DWORD dwFlags;
  HANDLE hFile;
  OVERLAPPED theOvInfo;
  BOOL bRet;

  hFile = (HANDLE) _get_osfhandle(fd);
  memset(&theOvInfo, 0, sizeof(OVERLAPPED));

  /* Don't deadlock ourselves */
  if (theWinVersion.dwPlatformId == VER_PLATFORM_WIN32_NT)
    bRet = UnlockFileEx(hFile, 0, 1, 0, &theOvInfo);
  else
    bRet = UnlockFile(hFile, 0, 0, 1, 0);

  if (operation & LOCK_UN)
  {
    if (!bRet && ((dwFlags = GetLastError()) != ERROR_NOT_LOCKED))
    {
      SetErrnoFromWinError(dwFlags);
      return -1;
    }
    else
      return 0;
  }

  if (operation & LOCK_EX)
  {
    dwFlags = LOCKFILE_EXCLUSIVE_LOCK;
  }
  else if (operation & LOCK_SH)
  {
    dwFlags = 0;
  }
  else
  {
    errno = EINVAL;
    return -1;
  }

  if (operation & LOCK_NB)
    dwFlags |= LOCKFILE_FAIL_IMMEDIATELY;

  if (theWinVersion.dwPlatformId == VER_PLATFORM_WIN32_NT)
    bRet = LockFileEx(hFile, dwFlags, 0, 1, 0, &theOvInfo);
  else
    bRet = LockFile(hFile, 0, 0, 1, 0);

  if (! bRet)
  {
    SetErrnoFromWinError(GetLastError());
    return -1;
  }
  else
    return 0;
}

/**
 * Synchronize changes to a file
 */
int fsync(int fildes)
{
  if (!FlushFileBuffers((HANDLE) _get_osfhandle(fildes)))
  {
    SetErrnoFromWinError(GetLastError());
    return -1;
  }
  else
    return 0;
}

/**
 * Open a file
 */
FILE *_win_fopen(const char *filename, const char *mode)
{
  char szFile[_MAX_PATH + 1];
  if (conv_to_win_path(filename, szFile) != ERROR_SUCCESS)
  {
    return NULL;
  }

  return fopen(szFile, mode);
}

/**
 * Open a directory
 */
DIR *_win_opendir(const char *dirname)
{
  char szDir[_MAX_PATH + 1];
  long lRet;

  if ((lRet = conv_to_win_path(dirname, szDir)) != ERROR_SUCCESS)
  {
    SetErrnoFromWinError(lRet);
    return NULL;
  }

  return opendir(szDir);
}

/**
 * Change directory
 */
int _win_chdir(const char *path)
{
  char szDir[_MAX_PATH + 1];
  long lRet;

  if ((lRet = conv_to_win_path(path, szDir)) != ERROR_SUCCESS)
  {
    SetErrnoFromWinError(lRet);
    return -1;
  }

  return chdir(szDir);
}

/**
 * Get information about an open file.
 */
int _win_fstat(int handle, struct stat *buffer)
{
  /* File */
  if (fstat(handle, buffer) == -1)
  {
    /* We just check for a valid handle here */

    /* Handle */
    memset(buffer, sizeof(struct stat), 0);
    GetFileType(handle);
    if (GetLastError() != NO_ERROR)
    {
      /* Invalid handle */
      return -1;
    }
  }

  return 0;
}

/**
 * Remove directory
 */
int _win_rmdir(const char *path)
{
  char szDir[_MAX_PATH + 1];
  long lRet;

  if ((lRet = conv_to_win_path(path, szDir)) != ERROR_SUCCESS)
  {
    SetErrnoFromWinError(lRet);
    return -1;
  }

  return rmdir(szDir);
}

/**
 * Create a pipe for reading and writing
 */
int _win_pipe(int *phandles)
{
  if (!CreatePipe((HANDLE *) &phandles[0],(HANDLE *) &phandles[1], NULL, 0))
  {
    SetErrnoFromWinError(GetLastError());

  	return -1;
  }
  else
  {
    return 0;
  }
}

/**
 * map files into memory
 * @author Cygwin team
 * @author Nils Durner
 */
void *_win_mmap(void *start, size_t len, int access, int flags, int fd,
                unsigned long long off) {
  DWORD protect, high, low, access_param;
  HANDLE h, hFile;
  SECURITY_ATTRIBUTES sec_none;
  void *base;
  BOOL bFound = FALSE;
  unsigned int uiIndex;

  errno = 0;

  switch(access)
  {
    case PROT_WRITE:
      protect = PAGE_READWRITE;
      access_param = FILE_MAP_WRITE;
      break;
    case PROT_READ:
      protect = PAGE_READONLY;
      access_param = FILE_MAP_READ;
      break;
    default:
      protect = PAGE_WRITECOPY;
      access_param = FILE_MAP_COPY;
      break;
  }

  sec_none.nLength = sizeof(SECURITY_ATTRIBUTES);
  sec_none.bInheritHandle = TRUE;
  sec_none.lpSecurityDescriptor = NULL;

  hFile = (HANDLE) _get_osfhandle(fd);

  h = CreateFileMapping(hFile, &sec_none, protect, 0, 0, NULL);

  if (! h)
  {
    SetErrnoFromWinError(GetLastError());
    return (void *) -1;
  }

  high = off >> 32;
  low = off & ULONG_MAX;
  base = NULL;

  /* If a non-zero start is given, try mapping using the given address first.
     If it fails and flags is not MAP_FIXED, try again with NULL address. */
  if (start)
    base = MapViewOfFileEx(h, access_param, high, low, len, start);
  if (!base && !(flags & MAP_FIXED))
    base = MapViewOfFileEx(h, access_param, high, low, len, NULL);

  if (!base || ((flags & MAP_FIXED) && base != start))
  {
    if (!base)
      SetErrnoFromWinError(GetLastError());
    else
      errno = EINVAL;

    CloseHandle(h);
    return (void *) -1;
  }

  /* Save mapping handle */
  WaitForSingleObject(hMappingsLock, INFINITE);

  for(uiIndex = 0; uiIndex <= uiMappingsCount; uiIndex++)
  {
    if (pMappings[uiIndex].pStart == base)
    {
      bFound = 1;
      break;
    }
  }

  if (! bFound)
  {
    uiIndex = 0;

    while(TRUE)
    {
      if (pMappings[uiIndex].pStart == NULL)
      {
        pMappings[uiIndex].pStart = base;
        pMappings[uiIndex].hMapping = h;
      }
      if (uiIndex == uiMappingsCount)
      {
        uiMappingsCount++;
        pMappings = (TMapping *) realloc(pMappings, (uiMappingsCount + 1) * sizeof(TMapping));
        pMappings[uiMappingsCount].pStart = NULL;

        break;
      }
      uiIndex++;
    }
  }
  ReleaseMutex(hMappingsLock);

  return base;
}

/**
 * Unmap files from memory
 * @author Cygwin team
 * @author Nils Durner
 */
int _win_munmap(void *start, size_t length)
{
  unsigned uiIndex;
  BOOL success = UnmapViewOfFile(start);
  SetErrnoFromWinError(GetLastError());

  if (success)
  {
    /* Release mapping handle */
    WaitForSingleObject(hMappingsLock, INFINITE);

    for(uiIndex = 0; uiIndex <= uiMappingsCount; uiIndex++)
    {
      if (pMappings[uiIndex].pStart == start)
      {
        success = CloseHandle(pMappings[uiIndex].hMapping);
        SetErrnoFromWinError(GetLastError());
        pMappings[uiIndex].pStart = NULL;
        pMappings[uiIndex].hMapping = NULL;

        break;
      }
    }

    ReleaseMutex(hMappingsLock);
  }

  return success ? 0 : -1;
}

/**
 * Determine file-access permission.
 */
int _win_access( const char *path, int mode )
{
  char szFile[_MAX_PATH + 1];
  long lRet;

  if ((lRet = conv_to_win_path(path, szFile)) != ERROR_SUCCESS)
  {
    SetErrnoFromWinError(lRet);
    return -1;
  }

  return access(szFile, mode);
}

/**
 * Change the file-permission settings.
 */
int _win_chmod(const char *filename, int pmode)
{
  char szFile[_MAX_PATH + 1];
  long lRet;

  if ((lRet = conv_to_win_path(filename, szFile)) != ERROR_SUCCESS)
  {
    SetErrnoFromWinError(lRet);
    return -1;
  }

  return access(szFile, pmode);
}


char *realpath(const char *file_name, char *resolved_name)
{
  char szFile[_MAX_PATH + 1];
  long lRet;

  if ((lRet = conv_to_win_path(file_name, szFile)) != ERROR_SUCCESS)
  {
    SetErrnoFromWinError(lRet);
    return NULL;
  }

  return _fullpath(szFile, resolved_name, MAX_PATH);
}

/**
 * Delete a file
 */
int _win_remove(const char *path)
{
  char szFile[_MAX_PATH + 1];
  long lRet;

  if ((lRet = conv_to_win_path(path, szFile)) != ERROR_SUCCESS)
  {
    SetErrnoFromWinError(lRet);
    return -1;
  }

  return remove(szFile);
}

/**
 * Rename a file
 */
int _win_rename(const char *oldname, const char *newname)
{
  char szOldName[_MAX_PATH + 1];
  char szNewName[_MAX_PATH + 1];
  long lRet;

  if ((lRet = conv_to_win_path(oldname, szOldName)) != ERROR_SUCCESS)
  {
    SetErrnoFromWinError(lRet);
    return -1;
  }

  if ((lRet = conv_to_win_path(newname, szNewName)) != ERROR_SUCCESS)
  {
    SetErrnoFromWinError(lRet);
    return -1;
  }

  return rename(szOldName, szNewName);
}

/**
 * Get status information on a file
 */
int _win_stat(const char *path, struct stat *buffer)
{
  char szFile[_MAX_PATH + 1];
  long lRet;

  if ((lRet = conv_to_win_path(path, szFile)) != ERROR_SUCCESS)
  {
    SetErrnoFromWinError(lRet);
    return -1;
  }

  /* Remove trailing slash */
  lRet = strlen(szFile) - 1;
  if (szFile[lRet] == '\\')
  {
    szFile[lRet] = 0;
  }

  return stat(szFile, buffer);
}

/**
 * Delete a file
 */
int _win_unlink(const char *filename)
{
  char szFile[_MAX_PATH + 1];
  long lRet;

  if ((lRet = conv_to_win_path(filename, szFile)) != ERROR_SUCCESS)
  {
    SetErrnoFromWinError(lRet);
    return -1;
  }

  return unlink(szFile);
}

/**
 * Write on a file
 */
int _win_write(int fildes, const void *buf, size_t nbyte)
{
  int iRet;
  if ((iRet = write(fildes, buf, nbyte)) == -1)
  {
    DWORD dwWritten;
    if (!WriteFile((HANDLE) fildes, buf, nbyte, &dwWritten, NULL))
    {
      SetErrnoFromWinError(GetLastError());
      return -1;
    }
    else
      return dwWritten;
  }
  else
    return iRet;
}

/**
 * Reads data from a file.
 */
int _win_read(int fildes, void *buf, size_t nbyte)
{
  int iRet;
  if ((iRet = read(fildes, buf, nbyte)) == -1)
  {
    DWORD dwRead;
    if (!ReadFile((HANDLE) fildes, buf, nbyte, &dwRead, NULL))
    {
      SetErrnoFromWinError(GetLastError());
      return -1;
    }
    else
      return dwRead;
  }
  else
    return iRet;
}

/**
 * Writes data to a stream
 */
size_t _win_fwrite(const void *buffer, size_t size, size_t count, FILE *stream)
{
  DWORD dwWritten;
  int iError;

  WriteFile((HANDLE) _get_osfhandle(fileno(stream)), buffer, size, &dwWritten,
            NULL);
  if ((iError = GetLastError()) != ERROR_SUCCESS)
  {
    SetErrnoFromWinError(iError);
  }

  return dwWritten;
}

/**
 * Reads data from a stream
 */
size_t _win_fread( void *buffer, size_t size, size_t count, FILE *stream )
{
  DWORD dwRead;
  int iItemsRead, iError;
  void *pDest = buffer;

  for(iItemsRead = 0; iItemsRead < count; iItemsRead++)
  {
    if (!ReadFile((HANDLE) _get_osfhandle(fileno(stream)), pDest, size,
                  &dwRead, NULL))
      break;
    pDest += size;
  }

  if ((iError = GetLastError()) != ERROR_SUCCESS)
  {
    SetErrnoFromWinError(iError);
  }

  return iItemsRead;
}

/**
 * Get a system error message
 */
char *_win_strerror(int errnum)
{
  char *error;

  switch (errnum)
    {
#ifdef EPERM
    case EPERM:
      error = _("Not super-user");
      break;
#endif
#ifdef ENOENT
    case ENOENT:
      error = _("No such file or directory");
      break;
#endif
#ifdef ESRCH
    case ESRCH:
      error = _("No such process");
      break;
#endif
#ifdef EINTR
    case EINTR:
      error = _("Interrupted system call");
      break;
#endif
#ifdef EIO
    case EIO:
      error = _("I/O error");
      break;
#endif
#ifdef ENXIO
    case ENXIO:
      error = _("No such device or address");
      break;
#endif
#ifdef E2BIG
    case E2BIG:
      error = _("Arg list too long");
      break;
#endif
#ifdef ENOEXEC
    case ENOEXEC:
      error = _("Exec format error");
      break;
#endif
#ifdef EBADF
    case EBADF:
      error = _("Bad file number");
      break;
#endif
#ifdef ECHILD
    case ECHILD:
      error = _("No children");
      break;
#endif
#ifdef EAGAIN
    case EAGAIN:
      error = _("Resource unavailable or operation would block, try again");
      break;
#endif
#ifdef ENOMEM
    case ENOMEM:
      error = _("Not enough memory");
      break;
#endif
#ifdef EACCES
    case EACCES:
      error = _("Permission denied");
      break;
#endif
#ifdef EFAULT
    case EFAULT:
      error = _("Bad address");
      break;
#endif
#ifdef ENOTBLK
    case ENOTBLK:
      error = _("Block device required");
      break;
#endif
#ifdef EBUSY
    case EBUSY:
      error = _("Mount device busy");
      break;
#endif
#ifdef EEXIST
    case EEXIST:
      error = _("File exists");
      break;
#endif
#ifdef EXDEV
    case EXDEV:
      error = _("Cross-device link");
      break;
#endif
#ifdef ENODEV
    case ENODEV:
      error = _("No such device");
      break;
#endif
#ifdef ENOTDIR
    case ENOTDIR:
      error = _("Not a directory");
      break;
#endif
#ifdef EISDIR
    case EISDIR:
      error = _("Is a directory");
      break;
#endif
#ifdef EINVAL
    case EINVAL:
      error = _("Invalid argument");
      break;
#endif
#ifdef ENFILE
    case ENFILE:
      error = _("Too many open files in system");
      break;
#endif
#ifdef EMFILE
    case EMFILE:
      error = _("Too many open files");
      break;
#endif
#ifdef ENOTTY
    case ENOTTY:
      error = _("Not a typewriter");
      break;
#endif
#ifdef ETXTBSY
    case ETXTBSY:
      error = _("Text file busy");
      break;
#endif
#ifdef EFBIG
    case EFBIG:
      error = _("File too large");
      break;
#endif
#ifdef ENOSPC
    case ENOSPC:
      error = _("No space left on device");
      break;
#endif
#ifdef ESPIPE
    case ESPIPE:
      error = _("Illegal seek");
      break;
#endif
#ifdef EROFS
    case EROFS:
      error = _("Read only file system");
      break;
#endif
#ifdef EMLINK
    case EMLINK:
      error = _("Too many links");
      break;
#endif
#ifdef EPIPE
    case EPIPE:
      error = _("Broken pipe");
      break;
#endif
#ifdef EDOM
    case EDOM:
      error = _("Math arg out of domain of func");
      break;
#endif
#ifdef ERANGE
    case ERANGE:
      error = _("Math result not representable");
      break;
#endif
#ifdef ENOMSG
    case ENOMSG:
      error = _("No message of desired type");
      break;
#endif
#ifdef EIDRM
    case EIDRM:
      error = _("Identifier removed");
      break;
#endif
#ifdef ECHRNG
    case ECHRNG:
      error = _("Channel number out of range");
      break;
#endif
#ifdef EL2NSYNC
    case EL2NSYNC:
      error = _("Level 2 not synchronized");
      break;
#endif
#ifdef L3HLT
    case L3HLT:
      error = _("Level 3 halted");
      break;
#endif
#ifdef EL3RST
    case EL3RST:
      error = _("Level 3 reset");
      break;
#endif
#ifdef ELNRNG
    case ELNRNG:
      error = _("Link number out of range");
      break;
#endif
#ifdef EUNATCH
    case EUNATCH:
      error = _("Protocol driver not attached");
      break;
#endif
#ifdef ENOCSI
    case ENOCSI:
      error = _("No CSI structure available");
      break;
#endif
#ifdef EL2HLT
    case EL2HLT:
      error = _("Level 2 halted");
      break;
#endif
#ifdef EDEADLK
    case EDEADLK:
      error = _("Deadlock condition");
      break;
#endif
#ifdef ENOLCK
    case ENOLCK:
      error = _("No record locks available");
      break;
#endif
#ifdef EBADE
    case EBADE:
      error = _("Invalid exchange");
      break;
#endif
#ifdef EBADR
    case EBADR:
      error = _("Invalid request descriptor");
      break;
#endif
#ifdef EXFULL
    case EXFULL:
      error = _("Exchange full");
      break;
#endif
#ifdef ENOANO
    case ENOANO:
      error = _("No anode");
      break;
#endif
#ifdef EBADRQC
    case EBADRQC:
      error = _("Invalid request code");
      break;
#endif
#ifdef EBADSLT
    case EBADSLT:
      error = _("Invalid slot");
      break;
#endif
#ifdef EDEADLOCK
    case EDEADLOCK:
      error = _("File locking deadlock error");
      break;
#endif
#ifdef EBFONT
    case EBFONT:
      error = _("Bad font file fmt");
      break;
#endif
#ifdef ENOSTR
    case ENOSTR:
      error = _("Device not a stream");
      break;
#endif
#ifdef ENODATA
    case ENODATA:
      error = _("No data (for no delay io)");
      break;
#endif
#ifdef ETIME
    case ETIME:
      error = _("Timer expired");
      break;
#endif
#ifdef ENOSR
    case ENOSR:
      error = _("Out of streams resources");
      break;
#endif
#ifdef ENONET
    case ENONET:
      error = _("Machine is not on the network");
      break;
#endif
#ifdef ENOPKG
    case ENOPKG:
      error = _("Package not installed");
      break;
#endif
#ifdef EREMOTE
    case EREMOTE:
      error = _("The object is remote");
      break;
#endif
#ifdef ENOLINK
    case ENOLINK:
      error = _("The link has been severed");
      break;
#endif
#ifdef EADV
    case EADV:
      error = _("Advertise error");
      break;
#endif
#ifdef ESRMNT
    case ESRMNT:
      error = _("Srmount error");
      break;
#endif
#ifdef ECOMM
    case ECOMM:
      error = _("Communication error on send");
      break;
#endif
#ifdef EPROTO
    case EPROTO:
      error = _("Protocol error");
      break;
#endif
#ifdef EMULTIHOP
    case EMULTIHOP:
      error = _("Multihop attempted");
      break;
#endif
#ifdef ELBIN
    case ELBIN:
      error = _("Inode is remote (not really error)");
      break;
#endif
#ifdef EDOTDOT
    case EDOTDOT:
      error = _("Cross mount point (not really error)");
      break;
#endif
#ifdef EBADMSG
    case EBADMSG:
      error = _("Trying to read unreadable message");
      break;
#endif
#ifdef ENOTUNIQ
    case ENOTUNIQ:
      error = _("Given log. name not unique");
      break;
#endif
#ifdef EBADFD
    case EBADFD:
      error = _("f.d. invalid for this operation");
      break;
#endif
#ifdef EREMCHG
    case EREMCHG:
      error = _("Remote address changed");
      break;
#endif
#ifdef ELIBACC
    case ELIBACC:
      error = _("Can't access a needed shared lib");
      break;
#endif
#ifdef ELIBBAD
    case ELIBBAD:
      error = _("Accessing a corrupted shared lib");
      break;
#endif
#ifdef ELIBSCN
    case ELIBSCN:
      error = _(".lib section in a.out corrupted");
      break;
#endif
#ifdef ELIBMAX
    case ELIBMAX:
      error = _("Attempting to link in too many libs");
      break;
#endif
#ifdef ELIBEXEC
    case ELIBEXEC:
      error = _("Attempting to exec a shared library");
      break;
#endif
#ifdef ENOSYS
    case ENOSYS:
      error = _("Function not implemented");
      break;
#endif
#ifdef ENMFILE
    case ENMFILE:
      error = _("No more files");
      break;
#endif
#ifdef ENOTEMPTY
    case ENOTEMPTY:
      error = _("Directory not empty");
      break;
#endif
#ifdef ENAMETOOLONG
    case ENAMETOOLONG:
      error = _("File or path name too long");
      break;
#endif
#ifdef ELOOP
    case ELOOP:
      error = _("Too many symbolic links");
      break;
#endif
#ifdef EOPNOTSUPP
    case EOPNOTSUPP:
      error = _("Operation not supported on transport endpoint");
      break;
#endif
#ifdef EPFNOSUPPORT
    case EPFNOSUPPORT:
      error = _("Protocol family not supported");
      break;
#endif
#ifdef ECONNRESET
    case ECONNRESET:
      error = _("Connection reset by peer");
      break;
#endif
#ifdef ENOBUFS
    case ENOBUFS:
      error = _("No buffer space available");
      break;
#endif
#ifdef EAFNOSUPPORT
    case EAFNOSUPPORT:
      error = _("Address family not supported by protocol family");
      break;
#endif
#ifdef EPROTOTYPE
    case EPROTOTYPE:
      error = _("Protocol wrong type for socket");
      break;
#endif
#ifdef ENOTSOCK
    case ENOTSOCK:
      error = _("Socket operation on non-socket");
      break;
#endif
#ifdef ENOPROTOOPT
    case ENOPROTOOPT:
      error = _("Protocol not available");
      break;
#endif
#ifdef ESHUTDOWN
    case ESHUTDOWN:
      error = _("Can't send after socket shutdown");
      break;
#endif
#ifdef ECONNREFUSED
    case ECONNREFUSED:
      error = _("Connection refused");
      break;
#endif
#ifdef EADDRINUSE
    case EADDRINUSE:
      error = _("Address already in use");
      break;
#endif
#ifdef ECONNABORTED
    case ECONNABORTED:
      error = _("Connection aborted");
      break;
#endif
#ifdef ENETUNREACH
    case ENETUNREACH:
      error = _("Network is unreachable");
      break;
#endif
#ifdef ENETDOWN
    case ENETDOWN:
      error = _("Network interface is not configured");
      break;
#endif
#ifdef ETIMEDOUT
    case ETIMEDOUT:
      error = _("Connection timed out");
      break;
#endif
#ifdef EHOSTDOWN
    case EHOSTDOWN:
      error = _("Host is down");
      break;
#endif
#ifdef EHOSTUNREACH
    case EHOSTUNREACH:
      error = _("Host is unreachable");
      break;
#endif
#ifdef EINPROGRESS
    case EINPROGRESS:
      error = _("Connection already in progress");
      break;
#endif
#ifdef EALREADY
    case EALREADY:
      error = _("Socket already connected");
      break;
#endif
#ifdef EDESTADDRREQ
    case EDESTADDRREQ:
      error = _("Destination address required");
      break;
#endif
#ifdef EMSGSIZE
    case EMSGSIZE:
      error = _("Message too long");
      break;
#endif
#ifdef EPROTONOSUPPORT
    case EPROTONOSUPPORT:
      error = _("Unknown protocol");
      break;
#endif
#ifdef ESOCKTNOSUPPORT
    case ESOCKTNOSUPPORT:
      error = _("Socket type not supported");
      break;
#endif
#ifdef EADDRNOTAVAIL
    case EADDRNOTAVAIL:
      error = _("Address not available");
      break;
#endif
#ifdef ENETRESET
    case ENETRESET:
      error = _("Connection aborted by network");
      break;
#endif
#ifdef EISCONN
    case EISCONN:
      error = _("Socket is already connected");
      break;
#endif
#ifdef ENOTCONN
    case ENOTCONN:
      error = _("Socket is not connected");
      break;
#endif
#ifdef ETOOMANYREFS
    case ETOOMANYREFS:
      error = _("Too many references: cannot splice");
      break;
#endif
#ifdef EPROCLIM
    case EPROCLIM:
      error = _("Too many processes");
      break;
#endif
#ifdef EUSERS
    case EUSERS:
      error = _("Too many users");
      break;
#endif
#ifdef EDQUOT
    case EDQUOT:
      error = _("Disk quota exceeded");
      break;
#endif
#ifdef ESTALE
    case ESTALE:
      error = _("Unknown error");
      break;
#endif
#ifdef ENOTSUP
    case ENOTSUP:
      error = _("Not supported");
      break;
#endif
#ifdef ENOMEDIUM
    case ENOMEDIUM:
      error = _("No medium (in tape drive)");
      break;
#endif
#ifdef ENOSHARE
    case ENOSHARE:
      error = _("No such host or network path");
      break;
#endif
#ifdef ECASECLASH
    case ECASECLASH:
      error = _("Filename exists with different case");
      break;
#endif
	case 0:
		error = _("No error");
		break;
    default:
	  error = _("Unknown error");
	  fprintf(stderr,
		  _("ERROR: Unknown error %i in %s\n"),
		  errnum,
		  __FUNCTION__);
      break;
    }

  return error;
}

#if !HAVE_ATOLL
long long atoll(const char *nptr)
{
  return atol(nptr);
}
#endif

#if !HAVE_STRNDUP
/**
 * return a malloc'd copy of at most the specified
 * number of bytes of a string
 * @author glibc-Team
 */
char *strndup (const char *s, size_t n)
{
  size_t len = strnlen (s, n);
  char *new = (char *) malloc (len + 1);

  if (new == NULL)
    return NULL;

  new[len] = '\0';
  return (char *) memcpy (new, s, len);
}
#endif

#if !HAVE_STRNLEN
/**
 * Determine the length of a fixed-size string
 * @author Jakub Jelinek <jakub at redhat dot com>
 */
size_t strnlen (const char *str, size_t maxlen)
{
  const char *char_ptr, *end_ptr = str + maxlen;
  const unsigned long int *longword_ptr;
  unsigned long int longword, magic_bits, himagic, lomagic;

  if (maxlen == 0)
    return 0;

  if (__builtin_expect (end_ptr < str, 0))
    end_ptr = (const char *) ~0UL;

  /* Handle the first few characters by reading one character at a time.
     Do this until CHAR_PTR is aligned on a longword boundary.  */
  for (char_ptr = str; ((unsigned long int) char_ptr
			& (sizeof (longword) - 1)) != 0;
       ++char_ptr)
    if (*char_ptr == '\0')
      {
	if (char_ptr > end_ptr)
	  char_ptr = end_ptr;
	return char_ptr - str;
      }

  /* All these elucidatory comments refer to 4-byte longwords,
     but the theory applies equally well to 8-byte longwords.  */

  longword_ptr = (unsigned long int *) char_ptr;

  /* Bits 31, 24, 16, and 8 of this number are zero.  Call these bits
     the "holes."  Note that there is a hole just to the left of
     each byte, with an extra at the end:

     bits:  01111110 11111110 11111110 11111111
     bytes: AAAAAAAA BBBBBBBB CCCCCCCC DDDDDDDD

     The 1-bits make sure that carries propagate to the next 0-bit.
     The 0-bits provide holes for carries to fall into.  */
  magic_bits = 0x7efefeffL;
  himagic = 0x80808080L;
  lomagic = 0x01010101L;
  if (sizeof (longword) > 4)
    {
      /* 64-bit version of the magic.  */
      /* Do the shift in two steps to avoid a warning if long has 32 bits.  */
      magic_bits = ((0x7efefefeL << 16) << 16) | 0xfefefeffL;
      himagic = ((himagic << 16) << 16) | himagic;
      lomagic = ((lomagic << 16) << 16) | lomagic;
    }
  if (sizeof (longword) > 8)
    abort ();

  /* Instead of the traditional loop which tests each character,
     we will test a longword at a time.  The tricky part is testing
     if *any of the four* bytes in the longword in question are zero.  */
  while (longword_ptr < (unsigned long int *) end_ptr)
    {
      /* We tentatively exit the loop if adding MAGIC_BITS to
	 LONGWORD fails to change any of the hole bits of LONGWORD. */

      longword = *longword_ptr++;

      if ((longword - lomagic) & himagic)
	{
	  /* Which of the bytes was the zero?  If none of them were, it was
	     a misfire; continue the search.  */

	  const char *cp = (const char *) (longword_ptr - 1);

	  char_ptr = cp;
	  if (cp[0] == 0)
	    break;
	  char_ptr = cp + 1;
	  if (cp[1] == 0)
	    break;
	  char_ptr = cp + 2;
	  if (cp[2] == 0)
	    break;
	  char_ptr = cp + 3;
	  if (cp[3] == 0)
	    break;
	  if (sizeof (longword) > 4)
	    {
	      char_ptr = cp + 4;
	      if (cp[4] == 0)
		break;
	      char_ptr = cp + 5;
	      if (cp[5] == 0)
		break;
	      char_ptr = cp + 6;
	      if (cp[6] == 0)
		break;
	      char_ptr = cp + 7;
	      if (cp[7] == 0)
		break;
	    }
	}
      char_ptr = end_ptr;
    }

  if (char_ptr > end_ptr)
    char_ptr = end_ptr;
  return char_ptr - str;
}
#endif

/**
 * language information
 */
#ifndef HAVE_LANGINFO_H
char *nl_langinfo(int item)
{
  unsigned int loc;

  loc = GetThreadLocale();

  switch(item)
  {
    case CODESET:
      {
        unsigned int cp = GetACP();

        if (cp)
          sprintf(__langinfo, "CP%u", cp);
        else
          strcpy(__langinfo, "UTF-8"); /* ? */
        return __langinfo;
      }
    case D_T_FMT:
    case T_FMT_AMPM:
    case ERA_D_T_FMT:
      strcpy(__langinfo, "%c");
      return __langinfo;
    case D_FMT:
    case ERA_D_FMT:
      strcpy(__langinfo, "%x");
      return __langinfo;
    case T_FMT:
    case ERA_T_FMT:
      strcpy(__langinfo, "%X");
      return __langinfo;
    case AM_STR:
      GetLocaleInfo(loc, LOCALE_S1159, __langinfo, 251);
      return __langinfo;
    case PM_STR:
      GetLocaleInfo(loc, LOCALE_S2359, __langinfo, 251);
      return __langinfo;
    case DAY_1:
      GetLocaleInfo(loc, LOCALE_SDAYNAME1, __langinfo, 251);
      return __langinfo;
    case DAY_2:
      GetLocaleInfo(loc, LOCALE_SDAYNAME2, __langinfo, 251);
      return __langinfo;
    case DAY_3:
      GetLocaleInfo(loc, LOCALE_SDAYNAME3, __langinfo, 251);
      return __langinfo;
    case DAY_4:
      GetLocaleInfo(loc, LOCALE_SDAYNAME4, __langinfo, 251);
      return __langinfo;
    case DAY_5:
      GetLocaleInfo(loc, LOCALE_SDAYNAME5, __langinfo, 251);
      return __langinfo;
    case DAY_6:
      GetLocaleInfo(loc, LOCALE_SDAYNAME6, __langinfo, 251);
      return __langinfo;
    case DAY_7:
      GetLocaleInfo(loc, LOCALE_SDAYNAME7, __langinfo, 251);
      return __langinfo;
    case ABDAY_1:
      GetLocaleInfo(loc, LOCALE_SABBREVDAYNAME1, __langinfo, 251);
      return __langinfo;
    case ABDAY_2:
      GetLocaleInfo(loc, LOCALE_SABBREVDAYNAME2, __langinfo, 251);
      return __langinfo;
    case ABDAY_3:
      GetLocaleInfo(loc, LOCALE_SABBREVDAYNAME3, __langinfo, 251);
      return __langinfo;
    case ABDAY_4:
      GetLocaleInfo(loc, LOCALE_SABBREVDAYNAME4, __langinfo, 251);
      return __langinfo;
    case ABDAY_5:
      GetLocaleInfo(loc, LOCALE_SABBREVDAYNAME5, __langinfo, 251);
      return __langinfo;
    case ABDAY_6:
      GetLocaleInfo(loc, LOCALE_SABBREVDAYNAME6, __langinfo, 251);
      return __langinfo;
    case ABDAY_7:
      GetLocaleInfo(loc, LOCALE_SABBREVDAYNAME7, __langinfo, 251);
      return __langinfo;
    case MON_1:
      GetLocaleInfo(loc, LOCALE_SMONTHNAME1, __langinfo, 251);
      return __langinfo;
    case MON_2:
      GetLocaleInfo(loc, LOCALE_SMONTHNAME2, __langinfo, 251);
      return __langinfo;
    case MON_3:
      GetLocaleInfo(loc, LOCALE_SMONTHNAME3, __langinfo, 251);
      return __langinfo;
    case MON_4:
      GetLocaleInfo(loc, LOCALE_SMONTHNAME4, __langinfo, 251);
      return __langinfo;
    case MON_5:
      GetLocaleInfo(loc, LOCALE_SMONTHNAME5, __langinfo, 251);
      return __langinfo;
    case MON_6:
      GetLocaleInfo(loc, LOCALE_SMONTHNAME6, __langinfo, 251);
      return __langinfo;
    case MON_7:
      GetLocaleInfo(loc, LOCALE_SMONTHNAME7, __langinfo, 251);
      return __langinfo;
    case MON_8:
      GetLocaleInfo(loc, LOCALE_SMONTHNAME8, __langinfo, 251);
      return __langinfo;
    case MON_9:
      GetLocaleInfo(loc, LOCALE_SMONTHNAME9, __langinfo, 251);
      return __langinfo;
    case MON_10:
      GetLocaleInfo(loc, LOCALE_SMONTHNAME10, __langinfo, 251);
      return __langinfo;
    case MON_11:
      GetLocaleInfo(loc, LOCALE_SMONTHNAME11, __langinfo, 251);
      return __langinfo;
    case MON_12:
      GetLocaleInfo(loc, LOCALE_SMONTHNAME12, __langinfo, 251);
      return __langinfo;
    case ABMON_1:
      GetLocaleInfo(loc, LOCALE_SABBREVMONTHNAME1, __langinfo, 251);
      return __langinfo;
    case ABMON_2:
      GetLocaleInfo(loc, LOCALE_SABBREVMONTHNAME2, __langinfo, 251);
      return __langinfo;
    case ABMON_3:
      GetLocaleInfo(loc, LOCALE_SABBREVMONTHNAME3, __langinfo, 251);
      return __langinfo;
    case ABMON_4:
      GetLocaleInfo(loc, LOCALE_SABBREVMONTHNAME4, __langinfo, 251);
      return __langinfo;
    case ABMON_5:
      GetLocaleInfo(loc, LOCALE_SABBREVMONTHNAME5, __langinfo, 251);
      return __langinfo;
    case ABMON_6:
      GetLocaleInfo(loc, LOCALE_SABBREVMONTHNAME6, __langinfo, 251);
      return __langinfo;
    case ABMON_7:
      GetLocaleInfo(loc, LOCALE_SABBREVMONTHNAME7, __langinfo, 251);
      return __langinfo;
    case ABMON_8:
      GetLocaleInfo(loc, LOCALE_SABBREVMONTHNAME8, __langinfo, 251);
      return __langinfo;
    case ABMON_9:
      GetLocaleInfo(loc, LOCALE_SABBREVMONTHNAME9, __langinfo, 251);
      return __langinfo;
    case ABMON_10:
      GetLocaleInfo(loc, LOCALE_SABBREVMONTHNAME10, __langinfo, 251);
      return __langinfo;
    case ABMON_11:
      GetLocaleInfo(loc, LOCALE_SABBREVMONTHNAME11, __langinfo, 251);
      return __langinfo;
    case ABMON_12:
      GetLocaleInfo(loc, LOCALE_SABBREVMONTHNAME12, __langinfo, 251);
      return __langinfo;
    case ERA:
      /* Not implemented */
      __langinfo[0] = 0;
      return __langinfo;
    case ALT_DIGITS:
      GetLocaleInfo(loc, LOCALE_SNATIVEDIGITS, __langinfo, 251);
      return __langinfo;
    case RADIXCHAR:
      GetLocaleInfo(loc, LOCALE_SDECIMAL, __langinfo, 251);
      return __langinfo;
    case THOUSEP:
      GetLocaleInfo(loc, LOCALE_STHOUSAND, __langinfo, 251);
      return __langinfo;
    case YESEXPR:
      /* Not localized */
      strcpy(__langinfo, "^[yY]");
      return __langinfo;
    case NOEXPR:
      /* Not localized */
      strcpy(__langinfo, "^[nN]");
      return __langinfo;
    case CRNCYSTR:
      GetLocaleInfo(loc, LOCALE_STHOUSAND, __langinfo, 251);
      if (__langinfo[0] == '0' || __langinfo[0] == '2')
        __langinfo[0] = '-';
      else
        __langinfo[0] = '+';
      GetLocaleInfo(loc, LOCALE_SCURRENCY, __langinfo + 1, 251);
    default:
      __langinfo[0] = 0;
      return __langinfo;
  }
}
#endif

#ifdef	__cplusplus
}
#endif
