#pragma once

#include <functional>
#include <vector>

using TestFunction = std::function<bool()>;

void registerTests(std::vector<TestFunction>& tests);
