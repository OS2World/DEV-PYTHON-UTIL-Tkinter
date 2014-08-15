
#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>
#include "unixlib.h"
#include <string.h>

#include <stdlib.h>

DIR *opendir(const char *path)
	{
	DIR *dir = malloc(sizeof(DIR));
	FILEFINDBUF3 findBuf;
	ULONG numEntries = 1;
	int rc;

	dir->handle = HDIR_CREATE;

	rc = DosFindFirst("*", &dir->handle, FILE_NORMAL, &findBuf,
							sizeof(findBuf), &numEntries, FIL_STANDARD);
	
	if (rc && rc != ERROR_NO_MORE_FILES && rc != ERROR_FILE_NOT_FOUND)
		{
		free(dir);
		return 0;
		}
	else if (rc)
		{
		dir->gotOne = 0;
		return dir;
		}
	
	dir->gotOne = 1;
	dir->cur.d_ino = 0;
	dir->cur.d_off = findBuf.cbFile;
	strcpy(dir->cur.d_name, findBuf.achName);
	
	return dir;
	}

struct dirent *readdir(DIR *dir)
	{
	FILEFINDBUF3 findBuf;
	ULONG numEntries = 1;
	int rc;
	
	if (dir->gotOne)
		{
		dir->gotOne = 0;
		return &dir->cur;
		}
	
	rc = DosFindNext(dir->handle, &findBuf, sizeof(findBuf), &numEntries);
	
	if (rc)
		return 0;
	
	dir->cur.d_ino = 0;
	dir->cur.d_off = findBuf.cbFile;
	strcpy(dir->cur.d_name, findBuf.achName);
	return &dir->cur;
	}

int closedir(DIR *dir)
	{
	if (DosFindClose(dir->handle))
		return -1;
	free(dir);
	return 0;
	}


