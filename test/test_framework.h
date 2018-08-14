#pragma once

#include "lagarith_platform.h"

#include <list>
#include <functional>

using TestFunction = std::function<bool()>;

class Test {
public:
	static int runTests(bool breakOnFailure);

public:
	Test(const char* testName, const TestFunction& testFunction);

	static bool GetBreakOnFailure() { return !!s_breakOnFailure(); }

	static bool OnFail();

private:
	static int& s_breakOnFailure() {
		static int s_bof = 0;
		return s_bof;
	}

	static std::list<Test*>& s_tests() {
		static std::list<Test*> s_testVec;
		return s_testVec;
	}

private:
	TestFunction function;
	std::string  name;
	size_t       index = ~0ull;
};


#define DECLARE_TEST(T)                                                                            \
	bool T();                                                                                        \
	Test test_##T(#T, &T);                                                                           \
	bool T()

#define FAIL() return Test::OnFail();

#define FAIL_MSG(FMT, ...)                                                                         \
	{                                                                                                \
		fprintf(stderr, FMT "\n", ##__VA_ARGS__);                                                        \
		return Test::OnFail();                                                                         \
	}

#define CHECK(T)                                                                                   \
	{                                                                                                \
		if (!(T)) {                                                                                    \
			fprintf(stderr, "%s failed!\n", #T);                                                         \
			FAIL();                                                                                      \
		}                                                                                              \
	}

#define CHECK_MSG(T, FMT, ...)                                                                     \
	{                                                                                                \
		if (!(T)) {                                                                                    \
			fprintf(stderr, "%s failed!\n", #T);                                                         \
			FAIL_MSG(FMT, ##__VA_ARGS__);                                                                  \
		}                                                                                              \
	}

#define CHECK_EQUAL(EXP, ACT)                                                                      \
	{                                                                                                \
		if (!((EXP) == (ACT))) {                                                                       \
			fprintf(stderr, "%s == %s failed!\n", #EXP, #ACT);                                           \
			FAIL();                                                                                      \
		}                                                                                              \
	}

#define CHECK_EQUAL_MSG(EXP, ACT, FMT, ...)                                                        \
	{                                                                                                \
		if (!((EXP) == (ACT))) {                                                                       \
			fprintf(stderr, "%s == %s failed!\n", #EXP, #ACT);                                           \
			FAIL_MSG(FMT, ##__VA_ARGS__);                                                                  \
		}                                                                                              \
	}
