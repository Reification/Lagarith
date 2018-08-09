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
			printf("!! %s failed.\n", test->name.c_str());
		}
		testsRun++;
	}

	printf("%d/%d tests passed.\n", testsPassed, testsRun);

	return testsRun - testsPassed;
}
