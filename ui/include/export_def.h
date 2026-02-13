#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #ifdef UI_LIBRARY
        #define UI_API __declspec(dllexport)
    #else
        #define UI_API __declspec(dllimport)
    #endif
#else
    #define UI_API
#endif
