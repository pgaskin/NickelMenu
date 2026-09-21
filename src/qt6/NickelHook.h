#pragma once
// Logging-only compatibility surface for shared configuration/action sources.
// The Qt 6 loader uses the validated, build-pinned interception implementation;
// this header does NOT implement the Qt 5 NickelHook initialization macros.
#ifdef __cplusplus
extern "C" {
#endif
void nh_log(const char *format, ...) __attribute__((format(printf, 1, 2)));
#ifdef __cplusplus
}
#endif
