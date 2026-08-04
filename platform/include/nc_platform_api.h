#pragma once
#include <stdint.h>

#if defined(_WIN32)
  #if defined(NC_PLATFORM_BUILD)
    #define NC_PLATFORM_EXPORT __declspec(dllexport)
  #else
    #define NC_PLATFORM_EXPORT __declspec(dllimport)
  #endif
#else
  #define NC_PLATFORM_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NcPlatformVersion {
    uint32_t abi_major;
    uint32_t abi_minor;
} NcPlatformVersion;

typedef struct NcPlatformUser {
    char stable_id[128];
    char display_name[128];
    int authenticated;
} NcPlatformUser;

NC_PLATFORM_EXPORT NcPlatformVersion nc_platform_version(void);
NC_PLATFORM_EXPORT int nc_platform_initialize(const char *app_id);
NC_PLATFORM_EXPORT void nc_platform_tick(void);
NC_PLATFORM_EXPORT void nc_platform_shutdown(void);
NC_PLATFORM_EXPORT NcPlatformUser nc_platform_current_user(void);
NC_PLATFORM_EXPORT int nc_platform_unlock_achievement(const char *achievement_id);
NC_PLATFORM_EXPORT int nc_platform_cloud_supported(void);

#ifdef __cplusplus
}
#endif
