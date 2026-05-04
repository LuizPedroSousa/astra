#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ASTRA_MODULE_API_VERSION 1

struct AstraModuleAPI {
  uint32_t api_version;
  void (*load)(void *config, uint32_t config_size);
  void (*unload)();
};

typedef const AstraModuleAPI *(*AstraModuleGetAPIFn)();

#ifdef __cplusplus
}
#endif
