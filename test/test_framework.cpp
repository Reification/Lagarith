#include "lagarith_testing.h"
#include "test_framework.h"
#include <ctime>
#include <chrono>

int LagarithTesting::runTests() {
	return Test::runTests();
}

Test::Test(const char* testName, const TestFunction& testFunction)
  : function(testFunction), name(testName), index(s_tests().size()) {
	s_tests().push_back(this);
}

static std::string getDateTime() {
	time_t     rawtime;
	struct tm* timeinfo;
	char       buffer[80];

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	strftime(buffer, sizeof(buffer), "%I:%M:%S %Y-%m-%d", timeinfo);
	return std::string(buffer);
}

// static
int Test::runTests() {
	int testsRun    = 0;
	int testsPassed = 0;
	using namespace std::chrono;

	std::string curTimeStr = getDateTime();
	printf("tests starting: %s\n\n", curTimeStr.c_str());

	const high_resolution_clock::time_point startTime = high_resolution_clock::now();

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

	const high_resolution_clock::time_point endTime = high_resolution_clock::now();
	duration<double>                        elapsed = endTime - startTime;

	const uint32_t elapsedH = (uint32_t)(elapsed.count() / 3600.0);
	const uint32_t elapsedM = (uint32_t)((elapsed.count() - (elapsedH * 3600.0)) / 60.0);
	const double   elapsedS = elapsed.count() - (elapsedH * 3600.0 + elapsedM * 60.0);

	curTimeStr = getDateTime();
	printf("\ntests done: %s (%02d:%02d:%.02f)\n\n", curTimeStr.c_str(), elapsedH, elapsedM, elapsedS);

	const int testsFailed = testsRun - testsPassed;

	if (!testsFailed) {
		printf("%d test%s passed.\n", testsRun, testsRun == 1 ? "" : "s");
	} else {
		printf("!! %d TEST%s FAILED out of %d test%s run !!\n", testsFailed,
		       testsFailed == 1 ? "" : "S", testsRun, testsRun == 1 ? "" : "s");
	}

	return testsFailed;
}
