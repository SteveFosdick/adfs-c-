#ifndef ACORN_FS_INC
#define ACORN_FS_INC

#include <stdint.h>
#include <stdio.h>

#define ACORN_FS_MAX_NAME 12

typedef enum {
    AFS_OK,
    AFS_BAD_COMMAND,
    AFS_HOST_ERROR,
    AFS_NOT_FOUND,
    AFS_NAME_TOO_LONG,
    AFS_NOT_A_DIR,
    AFS_READ_ERR,
    AFS_WRITE_ERR,
    AFS_BROKEN_DIR,
    AFS_DIR_FULL,
    AFS_MAP_FULL,
    AFS_BAD_FSMAP,
    AFS_NO_SPACE,
    AFS_NO_MEMORY,
    AFS_BAD_ATTR,
    AFS_BUG,
    AFS_NOT_IMPLEMENTED,
    AFS_MAX_ERROR
} afs_status;

typedef struct {
    char          name[ACORN_FS_MAX_NAME];
    unsigned      user_read:1;
    unsigned      user_write:1;
    unsigned      locked:1;
    unsigned      is_dir:1;
    unsigned      user_exec:1;
    unsigned      pub_read:1;
    unsigned      pub_write:1;
    unsigned      pub_exec:1;
    unsigned      priv:1;
    unsigned      load_addr;
    unsigned      exec_addr;
    unsigned      length;
    unsigned      sector;
    unsigned char *data;
} afs_object;

class AcornFS {
    public:
        static const char *afs_error(afs_status status);
        virtual afs_status find(const char *adfs_name, afs_object *obj) = 0;
        virtual afs_status load(afs_object *obj) = 0;
        virtual afs_status save(afs_object *obj, const char *dest_dir) = 0;
        static afs_status parse_attr(afs_object *obj, FILE *fp);
        static void print_attr(afs_object *obj, FILE *fp);
        static int host_load(afs_object *obj, const char *host_name);
        static int host_save(afs_object *obj, const char *host_name);
};

#endif
