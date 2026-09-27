#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Adaptateur de stockage du cœur RACE vers la carte SD AZ-2.
const char *az2NgpSavePath(void);
void az2NgpSetSavePath(const char *path);
bool az2NgpReadFile(const char *path, void **data, size_t *size);
bool az2NgpWriteFile(const char *path, const void *data, size_t size);

#ifdef __cplusplus
}
#endif
