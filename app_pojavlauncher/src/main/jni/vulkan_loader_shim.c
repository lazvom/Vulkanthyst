/*
 * Vulkan loader identity shim for Vulkanthyst.
 *
 * SDL3 and LWJGL must resolve vkGetInstanceProcAddr from the same shared
 * object. The selected Android Vulkan implementation remains the system
 * loader or the optional Turnip loader selected by Amethyst's existing code.
 */
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

extern void *maybe_load_vulkan(void);

#if defined(__GNUC__)
#define VTH_VK_EXPORT __attribute__((visibility("default"), used))
#else
#define VTH_VK_EXPORT
#endif

VTH_VK_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
vkGetInstanceProcAddr(VkInstance instance, const char *name) {
    if (name == NULL) return NULL;
    /* Preserve loader identity when a client asks for the loader entry point. */
    if (strcmp(name, "vkGetInstanceProcAddr") == 0) {
        return (PFN_vkVoidFunction)vkGetInstanceProcAddr;
    }
    void *driver = maybe_load_vulkan();
    if (driver == NULL) {
        fprintf(stderr, "Vulkanthyst: selected Vulkan loader is unavailable\n");
        return NULL;
    }
    PFN_vkGetInstanceProcAddr real_gipa =
        (PFN_vkGetInstanceProcAddr)dlsym(driver, "vkGetInstanceProcAddr");
    if (real_gipa == NULL) {
        fprintf(stderr, "Vulkanthyst: vkGetInstanceProcAddr missing: %s\n", dlerror());
        return NULL;
    }
    return real_gipa(instance, name);
}

/*
 * LWJGL's Android hook returns a dlopen handle. SDL3 is configured to load
 * this same library through SDL_VULKAN_LIBRARY, so both sides see the exact
 * same exported vkGetInstanceProcAddr symbol address.
 */
VTH_VK_EXPORT void *pojav_get_vulkan_shim_handle(void) {
    if (maybe_load_vulkan() == NULL) return NULL;

    const char *native_dir = getenv("POJAV_NATIVEDIR");
    char path[4096];
    void *handle = NULL;
    if (native_dir != NULL && native_dir[0] != '\0') {
        int length = snprintf(path, sizeof(path), "%s/libpojavexec.so", native_dir);
        if (length > 0 && (size_t)length < sizeof(path)) {
            handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
            if (handle == NULL) {
                fprintf(stderr, "Vulkanthyst: dlopen(%s) failed: %s\n", path, dlerror());
            }
        }
    }
    if (handle == NULL) handle = dlopen("libpojavexec.so", RTLD_NOW | RTLD_LOCAL);
    if (handle == NULL) {
        fprintf(stderr, "Vulkanthyst: cannot open libpojavexec.so: %s\n", dlerror());
        return NULL;
    }

    void *gipa = dlsym(handle, "vkGetInstanceProcAddr");
    if (gipa == NULL) {
        fprintf(stderr, "Vulkanthyst: shim symbol missing: %s\n", dlerror());
        dlclose(handle);
        return NULL;
    }
    fprintf(stderr, "Vulkanthyst: shared Vulkan loader handle=%p, GIPA=%p\n", handle, gipa);
    return handle;
}
