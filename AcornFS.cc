#include "AcornFS.h"

#include <alloca.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static const char *afs_errors[] = {
    "No error",
    "Bad command",
    "Host error",
    "Not found",
    "Name too long",
    "Not a directory",
    "Read error",
    "Write error",
    "Broken directory",
    "Directory full",
    "Free space map full",
    "Bad free space map",
    "Not enough space",
    "Bad attribute string",
    "Internal inconsitency",
    "Not implemented"
};

const char *AcornFS::afs_error(afs_status status) {
    if (status < AFS_MAX_ERROR)
        return afs_errors[status];
    return "Unknown error";
}

static int get_nonsp(FILE *fp) {
    int ch;

    do
        ch = getc(fp);
    while (isspace(ch));
    return ch;
}

afs_status AcornFS::parse_attr(afs_object *obj, FILE *fp) {
    int ch;

    memset(obj, 0, sizeof(*obj));
    if (fscanf(fp, "%12s %x %x %x", obj->name, &obj->load_addr, &obj->exec_addr, &obj->length) == 4) {
        ch = get_nonsp(fp);
        if (ch == 'L') {
            obj->locked = 1;
            ch = get_nonsp(fp);
        }
        obj->is_dir     = (ch == 'D');
        obj->is_dir     = (get_nonsp(fp) == 'D');
        obj->user_read  = (get_nonsp(fp) == 'R');
        obj->user_write = (get_nonsp(fp) == 'W');
        obj->user_exec  = (get_nonsp(fp) == 'E');
        obj->pub_read   = (get_nonsp(fp) == 'R');
        obj->pub_write  = (get_nonsp(fp) == 'W');
        obj->pub_exec   = (get_nonsp(fp) == 'E');
        obj->priv       = (get_nonsp(fp) == 'P');
        return AFS_OK;
    }
    return AFS_BAD_ATTR;
}

int AcornFS::host_load(afs_object *obj, const char *host_name) {
    int  status = 0;
    FILE *fp;
    char *inf_fn;
    int len;

    inf_fn = (char *)alloca(strlen(host_name) + 5);
    sprintf(inf_fn, "%s.inf", host_name);
    if ((fp = fopen(inf_fn, "rt"))) {
        parse_attr(obj, fp);
        fclose(fp);
    }
    if ((fp = fopen(host_name, "rb"))) {
        fseek(fp, 0, SEEK_END);
        if ((len = ftell(fp)) >= 0) {
            if (len == 0) {
                fclose(fp);
                obj->data = NULL;
                return 0;
            }
            else {
                obj->length = len;
                obj->data = (unsigned char *)malloc(len);
                if (fseek(fp, 0, SEEK_SET) == 0) {
                    if ((int)fread(obj->data, len, 1, fp) == len) {
                        fclose(fp);
                        return 0;
                    }
                }
            }
        }
        status = errno;
        fclose(fp);
    } else
        status = errno;
    return status;
}

void AcornFS::print_attr(afs_object *obj, FILE *fp) {
    char attr[12], *ap;

    fprintf(fp, "%-12s %08X %08X %08X", obj->name, obj->load_addr, obj->exec_addr, obj->length);
    ap = attr;
    if (obj->locked) {
        *ap++ = ' ';
        *ap++ = 'L';
    }
    *ap++ = obj->is_dir     ? 'D' : '-';
    *ap++ = obj->user_read  ? 'R' : '-';
    *ap++ = obj->user_write ? 'W' : '-';
    *ap++ = obj->user_exec  ? 'E' : '-';
    *ap++ = obj->pub_read   ? 'R' : '-';
    *ap++ = obj->pub_write  ? 'W' : '-';
    *ap++ = obj->pub_exec   ? 'E' : '-';
    *ap++ = obj->priv       ? 'P' : '-';
    *ap++ = '\n';
    fwrite(attr, ap-attr, 1, fp);
}


int AcornFS::host_save(afs_object *obj, const char *host_name) {
    int  status = 0;
    FILE *fp;
    char *inf_fn;

    if ((fp = fopen(host_name, "wb"))) {
        if (fwrite(obj->data, obj->length, 1, fp) == 1) {
            fclose(fp);
            inf_fn = (char *)alloca(strlen(host_name) + 5);
            sprintf(inf_fn, "%s.inf", host_name);
            if ((fp = fopen(inf_fn, "wt"))) {
                print_attr(obj, fp);
                fclose(fp);
                status = 0;
            } else
                status = errno;
        }
        else {
            status = errno;
            fclose(fp);
        }
    } else
        status = errno;
    return status;
}
