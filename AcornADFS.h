#ifndef ACORN_ADFS_INC
#define ACORN_ADFS_INC

#include "AcornFS.h"
#include "DiskImgIO.h"

#define ADFS_MAX_NAME 10

class AcornADFS: public AcornFS {
    public:
        AcornADFS(DiskImgIO *dio);
        static const char *afs_error(afs_status status);
        afs_status find(const char *adfs_name, afs_object *obj);
        afs_status load(afs_object *obj);
        afs_status save(afs_object *obj, const char *dest_dir);
        void obj_free(afs_object *obj);
        static afs_status parse_attr(afs_object *obj, FILE *fp);
        static void print_attr(afs_object *obj, FILE *fp);
        static int host_load(afs_object *obj, const char *host_name);
        static int host_save(afs_object *obj, const char *host_name);
    private:
        afs_status search(afs_object *parent, afs_object *child, const char *name, int name_len, unsigned char **next_ent);
        afs_status load_fsmap();
        afs_status save_fsmap();
        afs_status map_free(afs_object *obj);
        afs_status alloc_write(afs_object *obj);
        afs_status dir_update(afs_object *parent, afs_object *child, unsigned char *ent);
        void dir_makeslot(afs_object *parent, unsigned char *ent);
        DiskImgIO *discio;
        unsigned char *fsmap;
};

#endif
