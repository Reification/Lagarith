#include "pch.h"
#include "TestRunner.hpp"
#include <thread>
//#include <dlfcn.h>
#include <stdio.h>
//#include <chrono>
//#include <ctime>

#include <string>
//#include <fstream>
//#include <streambuf>

#include "test/tests.h"

static std::string getDateTime() {
	time_t     rawtime;
	struct tm* timeinfo;
	char       buffer[80];

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	strftime(buffer, sizeof(buffer), "%Y-%m-%d %I:%M:%S", timeinfo);
	return std::string(buffer);
}

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

	remove(testErrLog);
	remove(testOutLog);

	freopen(testOutLog, "a", stdout);
	freopen(testErrLog, "a", stderr);

	std::string curTimeStr = getDateTime();
	printf("tests starting: %s\n", curTimeStr.c_str());
	
	Lagarith::runTests();
	
	fclose(stdout);
	fclose(stderr);	
}

bool LaunchTestRunner(int* pIsRunning, const char* pPackageName) {
	static std::thread s_testRunnerThread;
	static int         s_testsRunning = 0;

	static const std::string s_packageName(pPackageName);

	using namespace std::chrono_literals;

	// must match value in ReifyCommon/src/android/PlatformAndroid.cpp
	const char* packageNameEnvar = "REIFY_PACKAGE_NAME";
	setenv(packageNameEnvar, pPackageName, false);

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