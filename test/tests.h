#pragma once

#include <functional>
#include <vector>

namespace Lagarith {
using TestFunction = std::function<bool()>;
void registerTests(std::vector<TestFunction>& tests);
int  runTests();
}
