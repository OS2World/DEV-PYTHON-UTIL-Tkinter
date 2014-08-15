/* Minimal main program -- everything is loaded from the library */
/*
 * This is a hacked version of the minimal python program that allows us to
 * use the "extproc" hack in OS/2 in cases where the python scripts resides
 * somewhere other than the current directory.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

extern int Py_Main();

int 
endsIn(name, suffix)
	char *name, *suffix;
{
	int len = strlen(name), sfxLen = strlen(suffix);
	if (len < sfxLen)
		return 0;
	
	return !stricmp(&name[len - sfxLen], suffix);
}

char *
findFile(file)
	char *file;
{
	struct stat st;
	char *cur, *path, *orgPath;

	/* append a ".cmd" to the file if there isn't one already */
	if (!endsIn(file, ".cmd")) {
		file = strcpy(malloc(strlen(file) + 5), file);
		strcat(file, ".cmd");
	}
	
	/* first we check the current directory */
	if (!stat(file, &st))
		return file;
	
	/* make a copy of the PATH environment variable that we can
	 * carve up with strtok.
	 */
	path = getenv("PATH");
	orgPath = path = strcpy(malloc(strlen(path) + 1), path);
	
	while (cur = strtok(path, ";")) {
		char *temp = malloc(strlen(cur) + strlen(file) + 2);
		
		sprintf(temp, "%s\\%s", cur, file);
		if (!stat(temp, &st)) {
			free(orgPath);
			return temp;
		}
		free(temp);

		/* path must be null the after first time through */
		if (path)
			path = NULL;
	}
	free(orgPath);
	return file;
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	int rc;
	/* copy the arguments into a local array, allocating enough space
	 * for an additional "-x" argument
	 */
	char **localArgv = (char**)malloc(sizeof(char*) * (argc + 1));
	int i;
	localArgv[0] = argv[0];
	localArgv[1] = "-x";
	for (i = 1; i < argc; ++i)
		localArgv[i + 1] = argv[i];
	
	/* find the exact location of the first parameter (the program file) */
	if (argc >= 2)
		localArgv[2] = findFile(localArgv[2]);
	
	rc = Py_Main(argc + 1, localArgv);
	return rc;
}
