
#define NAME_MAX 256

typedef unsigned long off_t;

struct dirent
	{
	long d_ino;
	off_t d_off;
	unsigned short d_reclen;
	char d_name[NAME_MAX + 1];
	};

typedef struct t_dir
	{
	unsigned long handle;
	int gotOne;
	struct dirent cur;
	} DIR;

DIR *opendir(const char *path);
struct dirent *readdir(DIR *dir);
int closedir(DIR *dir);

