#ifndef SSE_SHIM_H
#define SSE_SHIM_H

  #ifdef _WIN32
      #if defined(_M_AMD64) || defined(_M_X64)
          #define __SSE2__ 1
      #elif _M_IX86_FP == 2
          #define __SSE2__ 1
      #elif _M_IX86_FP == 1
          #define __SSE__ 1
      #endif
  #endif
#endif