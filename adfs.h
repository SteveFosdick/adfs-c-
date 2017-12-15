#ifndef ADFS_INC
#define ADFS_INC

#include <stdint.h>
#include <stdio.h>

#define ADFS_NAME_MAX   10

typedef enum {
    ADFS_OK,
    ADFS_BAD_COMMAND,
    ADFS_HOST_ERROR,
    ADFS_NOT_FOUND,
    ADFS_NAME_TOO_LONG,
    ADFS_NOT_A_DIR,
    ADFS_READ_ERR,
    ADFS_WRITE_ERR,
    ADFS_BROKEN_DIR,
    ADFS_BAD_FSMAP,
    ADFS_NO_SPACE,
    ADFS_NO_MEMORY,
    ADFS_NOT_IMPLEMENTED,
    ADFS_MAX_ERROR
} adfs_status;

typedef struct _adfs ADFS;

typedef struct {
    char          name[ADFS_NAME_MAX];
    unsigned      user_read:1;
    unsigned      user_write:1;
    unsigned      locked:1;
    unsigned      is_dir:1;
    unsigned      user_exec:1;
    unsigned      pub_read:1;
    unsigned      pub_write:1;
    unsigned      pub_exec:1;
    unsigned      priv:1;
    uint32_t      load_addr;
    uint32_t      exec_addr;
    uint32_t      length;
    unsigned      sector;
    unsigned char *data;
} adfs_object;

extern const char *adfs_error(adfs_status status);
extern ADFS *adfs_open(const char *name, uint8_t writable);
extern adfs_status adfs_close(ADFS *adfs);
extern adfs_status adfs_find(ADFS *adfs, const char *adfs_name, adfs_object *obj);
extern adfs_status adfs_load(ADFS *adfs, adfs_object *obj);
extern adfs_status adfs_save(ADFS *adfs, adfs_object *obj, const char*path);

extern adfs_status adfs_host_load(adfs_object *obj, const char *host_name);
extern adfs_status adfs_host_save(adfs_object *obj, const char *host_name);

#endif
