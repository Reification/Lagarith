#pragma once

#define LOGI(...)                                                                                  \
	((void)__android_log_print(ANDROID_LOG_INFO, "Lagarith.test_runner", __VA_ARGS__))
#define LOGW(...)                                                                                  \
	((void)__android_log_print(ANDROID_LOG_WARN, "Lagarith.test_runner", __VA_ARGS__))
#define LOGE(...)                                                                                  \
	((void)__android_log_print(ANDROID_LOG_ERROR, "Lagarith.test_runner", __VA_ARGS__))

/* For debug builds, always enable the debug traces in this library */
#if defined(REIFY_DEBUG)
#define LOGV(...)                                                                                  \
	((void)__android_log_print(ANDROID_LOG_VERBOSE, "Lagarith.test_runner", __VA_ARGS__))
#else
#define LOGV(...) ((void)0)
#endif

extern bool LaunchTestRunner(int* pIsRunning, const char* packageName);
