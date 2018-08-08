#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memory>
#include <vector>
#include <algorithm>
#include <functional>
#include <vector>

#if defined(_WINDOWS)
#	include <direct.h>
#	define PATH_SEP '\\'
#	define chdir _chdir
#else
#	include <unistd.h>
#	define PATH_SEP '/'
#endif

#if !defined(_WINDOWS)
#	define sprintf_s sprintf
#endif

using TestFunction = std::function<bool()>;

void registerTests(std::vector<TestFunction>& tests);
