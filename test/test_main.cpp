#include "tests.h"

int main(int argc, const char* argv[]) {
	(void)argc;
	if (const char* pathEnd = strrchr(argv[0], PATH_SEP)) {
		char tpath[512] = {};
		strncpy(tpath, argv[0], pathEnd - argv[0]);
		chdir(tpath);
	}
	chdir("..");
	chdir("test_data");

	int testsRun    = 0;
	int testsPassed = 0;

	std::vector<TestFunction> testFunctions;
	registerTests(testFunctions);

	for ( const TestFunction& test : testFunctions )
	{
		testsPassed += test();
		testsRun++;
	}

	printf("%d/%d tests passed.\n", testsPassed, testsRun);

	return testsPassed - testsRun;
}
