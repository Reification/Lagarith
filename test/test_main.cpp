#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "tests.h"
#include <direct.h>

int main(int argc, const char* argv[]) {
	(void)argc;
#if defined(_WIN32)
#	define PATH_SEP '\\'
# define chdir _chdir
#else
#	define PATH_SEP '/'
#endif

	if (const char* pathEnd = strrchr(argv[0], PATH_SEP)) {
		char tpath[512] = {};
		strncpy(tpath, argv[0], pathEnd - argv[0]);
		chdir(tpath);
	}
	chdir("..");
	chdir("test_data");

	int testsRun    = 0;
	int testsPassed = 0;

	testsPassed += testEncodeDecodeRGB();
	testsRun++;

	testsPassed += testEncodeDecodeRGBA();
	testsRun++;

	printf("%d/%d tests passed.\n", testsPassed, testsRun);

	return testsPassed - testsRun;
}
