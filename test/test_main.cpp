#include "lagarith_testing.h"
#include "test_internal.h"
#include <vector>

// set via cmake compiler flags when building test as library instead of executable.
#if !defined(LAGARITH_TEST_IS_LIB)
int main(int argc, const char* argv[]) {
	(void)argc;
	if (const char* pathEnd = strrchr(argv[0], PATH_SEP)) {
		char tpath[512] = {};
		strncpy(tpath, argv[0], pathEnd - argv[0]);
		chdir(tpath);
	}

	const int numFailures = LagarithTesting::runTests();

	return numFailures;
}
#endif // !LAGARITH_TEST_IS_LIB

int LagarithTesting::runTests() {
	int testsRun    = 0;
	int testsPassed = 0;

	for (const TestRegistrar::Test& test : TestRegistrar::GetTests()) {
		printf("running %s\n", test.name.c_str());
		bool result = test.function();
		if (result) {
			testsPassed++;
			printf("%s passed.\n", test.name.c_str());
		} else {
			fprintf(stderr, "%s failed.\n", test.name.c_str());
		}
		testsRun++;
	}

	printf("%d/%d tests passed.\n", testsPassed, testsRun);

	return testsRun - testsPassed;
}
