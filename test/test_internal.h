#pragma once
#include <string.h>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <vector>
#include <functional>
#include <string>

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


class TestRegistrar {
public:
	struct Test {
		TestFunction function;
		std::string  name;
	};

	static const std::vector<Test>& GetTests() { return s_tests(); }

public:
	TestRegistrar() = default;

	TestRegistrar(const char* testName, const TestFunction& testFunction) : m_registered(true) {
		registerTest(testName, testFunction);
	}

	~TestRegistrar() { m_registered = false; }

private:
	static int registerTest(const char* testName, const TestFunction& testFunction) {
		s_tests().push_back({testFunction, testName});
		return (int)(s_tests().size() - 1);
	}

	static std::vector<Test>& s_tests() {
		static std::vector<Test> s_testVec;
		return s_testVec;
	}

public:
	bool m_registered = false;
};

#define DECLARE_TEST(T)                                                                            \
	bool                 T();                                                                        \
	static TestRegistrar s_##T##Reg(#T, &T);                                                         \
	bool                 T()