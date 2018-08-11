#include "lagarith_testing.h"
#include "test_framework.h"

int LagarithTesting::runTests() {
	return Test::runTests();
}

Test::Test(const char* testName, const TestFunction& testFunction)
  : function(testFunction), name(testName), index(s_tests().size()) {
	s_tests().push_back(this);
}

// static
int Test::runTests() {
	int testsRun    = 0;
	int testsPassed = 0;

	for (const Test* test : s_tests()) {
		printf("running %s\n", test->name.c_str());
		bool result = test->function();
		if (result) {
			testsPassed++;
			printf("%s passed.\n", test->name.c_str());
		} else {
			printf("!! %s FAILED !!\n", test->name.c_str());
		}
		testsRun++;
	}

	const int testsFailed = testsRun - testsPassed;

	if (!testsFailed) {
		printf("%d test%s passed.\n", testsRun, testsRun == 1 ? "" : "s");
	} else {
		printf("!! %d TEST%s FAILED out of %d test%s run !!\n",
						testsFailed, testsFailed == 1 ? "" : "S",
						testsRun,
						testsRun == 1 ? "" : "s");
	}

	return testsFailed;
}
