#pragma once

#ifdef _WIN32
#ifdef PROJECT_DATA_LIB_BUILD
#define PROJECT_DATA_API __declspec(dllexport)
#else
#define PROJECT_DATA_API __declspec(dllimport)
#endif
#else
#define PROJECT_DATA_API
#endif