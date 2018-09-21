#include "pch.h"
#include "TestRunner.hpp"
#include <thread>
#include <stdio.h>
#include <string>

#include "test/lagarith_testing.h"

static void runTests(const std::string& packageName) {
	// TODO: query programmatically!
	const char* storage = getenv("EXTERNAL_STORAGE");

	if (!storage) {
		LOGE("failed getting EXTERNAL_STORAGE envar value!");
		return;
	}

	std::string testWorkingDir = std::string(storage) + "/Android/data/" + packageName;

	const char* testOutLog = "test_out_log.txt";
	const char* testErrLog = "test_err_log.txt";

	chdir(testWorkingDir.c_str());

	freopen(testOutLog, "w", stdout);
	freopen(testErrLog, "w", stderr);

	printf("running tests from %s\n", testWorkingDir.c_str());

	const bool assertTestsPass = false;
	LagarithTesting::runTests(assertTestsPass);

	fclose(stdout);
	fclose(stderr);
}

bool LaunchTestRunner(int* pIsRunning, const char* pPackageName) {
	static std::thread s_testRunnerThread;
	static int         s_testsRunning = 0;

	static const std::string s_packageName(pPackageName);

	using namespace std::chrono_literals;

	if (s_testsRunning) {
		LOGE("LaunchTestRunner: last test batch still running.");
		return false;
	}

	if (s_testRunnerThread.joinable()) {
		s_testRunnerThread.join();
	}

	*pIsRunning = s_testsRunning = 1;

	s_testRunnerThread = std::thread([pIsRunning]() {
		runTests(s_packageName);
		*pIsRunning = s_testsRunning = 0;
	});

	return true;
}
