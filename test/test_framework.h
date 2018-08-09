#pragma once
#include <string.h>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <vector>
#include <functional>
#include <string>
#include <list>

#if defined(_WINDOWS)
#	include <direct.h>
#	define PATH_SEP '\\'
#	define chdir _chdir
#else
#	include <unistd.h>
#	define PATH_SEP '/'

#	define sprintf_s sprintf
inline int fopen_s(FILE** pfp, const char* path, const char* mode) {
	*pfp = fopen(path, mode);
	return *pfp ? 0 : -1;
}
#endif

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
