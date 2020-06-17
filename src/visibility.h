#ifndef NM_VISIBILITY_H
#define NM_VISIBILITY_H
#ifdef __cplusplus
extern "C" {
#endif

// Symbol visibility
#define NM_PUBLIC  __attribute__((visibility("default")))
#define NM_PRIVATE __attribute__((visibility("hidden")))

#ifdef __cplusplus
}
#endif
#endif
