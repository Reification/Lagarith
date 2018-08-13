#include "lagarith_testing.h"
#include "test_framework.h"

// set via cmake compiler flags when building test as library instead of executable.
#if !defined(LAGARITH_TEST_IS_LIB)
int main(int argc, const char* argv[]) {
	(void)argc;
	if (const char* pathEnd = strrchr(argv[0], PATH_SEP)) {
		char tpath[512] = {};
		strncpy(tpath, argv[0], pathEnd - argv[0]);
		_chdir(tpath);
	}

	const int numFailures = LagarithTesting::runTests();

	return numFailures;
}
#endif // !LAGARITH_TEST_IS_LIB
