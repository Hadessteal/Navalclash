#include "nc_platform_api.h"

#include <cstring>

NcPlatformVersion nc_platform_version(void)
{
    return {1, 0};
}

int nc_platform_initialize(const char *app_id)
{
    (void)app_id;
    return 1;
}

void nc_platform_tick(void) {}
void nc_platform_shutdown(void) {}

NcPlatformUser nc_platform_current_user(void)
{
    NcPlatformUser user{};
    std::strncpy(user.stable_id, "offline", sizeof(user.stable_id) - 1);
    std::strncpy(user.display_name, "Offline Player", sizeof(user.display_name) - 1);
    user.authenticated = 0;
    return user;
}

int nc_platform_unlock_achievement(const char *achievement_id)
{
    (void)achievement_id;
    return 0;
}

int nc_platform_cloud_supported(void)
{
    return 0;
}
