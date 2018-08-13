#pragma once

#include "lagarith_platform.h"

#include <list>
#include <functional>

using TestFunction = std::function<bool()>;

class Test {
public:
	static int runTests();

public:
	Test(const char* testName, const TestFunction& testFunction);

private:
	static std::list<Test*>& s_tests() {
		static std::list<Test*> s_testVec;
		return s_testVec;
	}

private:
	TestFunction function;
	std::string  name;
	size_t       index = ~0ull;
};


#	define DECLARE_TEST(T)                                                                          \
		bool T();                                                                                      \
		Test test_##T(#T, &T);                                                                         \
		bool T()
