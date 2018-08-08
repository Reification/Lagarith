#include "tests.h"
#include <string.h>

#if defined(_WINDOWS)
#	include <direct.h>
#	define PATH_SEP '\\'
#	define chdir _chdir
#else
#	include <unistd.h>
#	define PATH_SEP '/'
#endif

// set via cmake compiler flags when building test as library instead of executable.
#if !defined(LAGARITH_TEST_IS_LIB)
int main(int argc, const char* argv[]) {
	(void)argc;
	if (const char* pathEnd = strrchr(argv[0], PATH_SEP)) {
		char tpath[512] = {};
		strncpy(tpath, argv[0], pathEnd - argv[0]);
		chdir(tpath);
	}

	const int numFailures = Lagarith::runTests();

	return numFailures;
}
#endif // !LAGARITH_TEST_IS_LIB

int Lagarith::runTests()
{
	int testsRun    = 0;
	int testsPassed = 0;

	std::vector<TestFunction> testFunctions;
	registerTests(testFunctions);

	for (const TestFunction& test : testFunctions) {
		testsPassed += test();
		testsRun++;
	}

	printf("%d/%d tests passed.\n", testsPassed, testsRun);

	return testsRun - testsPassed;
}
