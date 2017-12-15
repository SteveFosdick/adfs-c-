#include "adfs.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define ADFS_ENTRY_SIZE 0x1a
#define SECTOR_SIZE     256
#define FSMAP_SIZE      512

struct _adfs {
    FILE          *fp;
    unsigned char *fsmap;
    adfs_object   *root_dir;
};

typedef struct {
    unsigned char sects[0xf6];
    unsigned char part[3];
    unsigned char reserved[2];
    unsigned char tot_sectors[3];
    unsigned char checksum;
} adfs_fsmap_s1;

typedef struct {
    unsigned char sizes[0xf6];
    unsigned char part[4];
    unsigned char discid[2];
    unsigned char boot_opt;
    unsigned char last_entry;
    unsigned char checksum;
} adfs_fsmap_s2;

typedef struct {
    unsigned char mseq;
    unsigned char hugo[4];
} adfs_dir_hdr;

typedef struct {
    unsigned char name[10];
    unsigned char load_a[4];
    unsigned char exec_a[4];
    unsigned char length[4];
    unsigned char sector[3];
    unsigned char seq;
} adfs_dir_entry;

typedef struct {
    unsigned char zero;
    unsigned char dir_name[10];
    unsigned char parent_sect[3];
    unsigned char title[19];
    unsigned char reserved[14];
    unsigned char hugo[4];
    unsigned char checksum; // not used
} adfs_dir_ftr;

typedef struct {
    uint32_t       size;
    uint32_t       used;
    adfs_dir_ftr   *ftr;
    adfs_dir_hdr   hdr;
    adfs_dir_entry entries[];
} adfs_directory;

static const char *adfs_errors[] = {
    "No error",
    "Bad command",
    "Host error",
    "Not found",
    "Name too long",
    "Not a directory",
    "Seek error",
    "Read error",
    "Write error",
    "Broken directory",
    "Bad free space map",
    "Not enough space",
    "Not implemented"
};

const char *adfs_error(adfs_status status) {
    if (status < ADFS_MAX_ERROR)
        return adfs_errors[status];
    return "Unknown error";
}

ADFS *adfs_open(const char *name, uint8_t writable) {
    ADFS *adfs;
    FILE *fp;
    const char *mode = writable ? "rb+" : "rb";

    if ((adfs = malloc(sizeof(ADFS)))) {
        if ((fp = fopen(name, mode))) {
            adfs->fp = fp;
            adfs->fsmap = NULL;
            adfs->root_dir = NULL;
            return adfs;
        }
        free(adfs);
    }
    return NULL;
}

adfs_status adfs_close(ADFS *adfs) {
    adfs_status status;

    status = ADFS_OK;
    if (fclose(adfs->fp) != 0)
        status = ADFS_WRITE_ERR;
    if (adfs->root_dir)
        free(adfs->root_dir);
    if (adfs->fsmap)
        free(adfs->fsmap);
    free(adfs);
    return status;
}

adfs_status adfs_load(ADFS *adfs, adfs_object *obj) {
    unsigned byte_posn = obj->sector * SECTOR_SIZE;
    if (fseek(adfs->fp, byte_posn, SEEK_SET) != byte_posn)
        return ADFS_SEEK_ERROR;
    if ((obj->data = malloc(obj->length)) == NULL)
        return ADFS_NO_MEMORY;
    if (fread(obj->data, obj->length, 1, adfs->fp) == 1)
        return ADFS_OK;
    free(obj->data);
    obj->data = NULL;
    return ADFS_READ_ERR;
}

static inline uint32_t adfs_get32(const unsigned char *base) {
    return base[0] | (base[1] << 8) | (base[2] << 16) | (base[3] << 24);
}

static inline uint32_t adfs_get24(const unsigned char *base) {
    return base[0] | (base[1] << 8) | (base[2] << 16);
}

static inline void adfs_put32(unsigned char *base, uint32_t value) {
    base[0] = value & 0xff;
    base[1] = (value >> 8) & 0xff;
    base[2] = (value >> 16) & 0xff;
    base[3] = (value >> 24) & 0xff;
}

static inline void adfs_put24(unsigned char *base, uint32_t value) {
    base[0] = value & 0xff;
    base[1] = (value >> 8) & 0xff;
    base[2] = (value >> 16) & 0xff;
}

static uint8_t checksum(uint8_t *base) {
    int i = 255, c = 0;
    unsigned sum = 255;
    while (--i >= 0) {
        sum += base[i] + c;
        c = 0;
        if (sum >= 256) {
            sum &= 0xff;
            c = 1;
        }
    }
    return sum;
}

static adfs_status adfs_write_map(ADFS *adfs) {
    if (adfs->map_dirty) {
        adfs->fsmap1.checksum = checksum(adfs->fsmap1);
        adfs->fsmap2.checksum = checksum(adfs->fsmap2);
        if (fseek(adfs->fp, 0, SEEK_SET) != 0)
            return ADFS_SEEK_ERROR;
        if (fwrite(adfs->fsmap1, sizeof(fsmap1)+sizeof(fsmap2), adfs_fp) != 1)
            return ADFS_WRITE_ERR;
    }
    return ADFS_OK;
}

static adfs_status adfs_write_dir(ADFS *adfs) {
    if (adfs->dir_dirty) {
        if (fseek(adfs->fp, adfs->dir_posn, SEEK_SET) != adfs->dir_posn)
            return ADFS_SEEK_ERROR;
        if (fwrite(adfs->dir_hdr, adfs->dir_size, 1, adfs-.fp) != 1)
            return ADFS_WRITE_ERR;
    }
    return ADFS_OK;
}

static adfs_status adfs_search(FILE *adfs, adfs_object *parent, adfs_object *child, const char *name, int name_len) {
    unsigned char entry[ADFS_ENTRY_SIZE];
    unsigned byte_start, bytes_done;
    int i;

    if (name_len > ADFS_NAME_MAX)
        return ADFS_NAME_TOO_LONG;
    if (!parent->is_dir)
        return ADFS_NOT_A_DIR;
    byte_start = parent->sector * SECTOR_SIZE;
    if (fseek(adfs, byte_start, SEEK_SET) == -1)
        return ADFS_SEEK_ERROR;
    if (fread(entry, 5, 1, adfs) != 1)
        return ADFS_READ_ERR;
    if (entry[1] != 'H' || entry[2] != 'u' || entry[3] != 'g' || entry[4] != 'o')
        return ADFS_BROKEN_DIR;
    for (bytes_done = 0; bytes_done < parent->length; bytes_done += ADFS_ENTRY_SIZE) {
        if (fread(entry, ADFS_ENTRY_SIZE, 1, adfs) != 1)
            return ADFS_READ_ERR;
        int found = 1;
        for (i = 0; i < name_len; i++) {
            if ((name[i] & 0xdf) != (entry[i] & 0x5f)) {
                found = 0;
                break;
            }
        }
        if (found) {
            for (i = 0; i < name_len; i++)
                child->name[i] = (entry[i] & 0x7f);
            child->name[i]    = '\0';
            child->user_read  = ((entry[0] & 0x80) == 0x80);
            child->user_write = ((entry[1] & 0x80) == 0x80);
            child->locked     = ((entry[2] & 0x80) == 0x80);
            child->is_dir     = ((entry[3] & 0x80) == 0x80);
            child->user_exec  = ((entry[4] & 0x80) == 0x80);
            child->pub_read   = ((entry[5] & 0x80) == 0x80);
            child->pub_write  = ((entry[6] & 0x80) == 0x80);
            child->pub_exec   = ((entry[7] & 0x80) == 0x80);
            child->pub_exec   = ((entry[8] & 0x80) == 0x80);
            child->load_addr  = adfs_get32(entry + 0x0a);
            child->exec_addr  = adfs_get32(entry + 0x0e);
            child->length     = adfs_get32(entry + 0x12);
            child->sector     = adfs_get24(entry + 0x16);
            return ADFS_OK;
        }
    }
    return ADFS_NOT_FOUND;
}

static adfs_status adfs_find(FILE *adfs, const char *adfs_name, adfs_object *obj) {
    adfs_status status;
    const char  *ptr;
    adfs_object *parent, *child, *temp, a, b;

    if (adfs_name[0] == '$' && adfs_name[1] == '.')
        adfs_name += 2;
    memset(&a, 0, sizeof(a));
    a.is_dir = 1;
    a.length = 1280;
    a.sector = 2;
    parent = &a;
    child = &b;
    while ((ptr = strchr(adfs_name, '.'))) {
        if ((status = adfs_search(adfs, parent, child, adfs_name, ptr - adfs_name)) != ADFS_OK)
            return status;
        temp = parent;
        parent = child;
        child = temp;
        adfs_name = ptr + 1;
    }
    if ((status = adfs_search(adfs, parent, child, adfs_name, strlen(adfs_name))) != ADFS_OK)
        return status;
    *obj = *child;
    return ADFS_OK;
}

adfs_status adfs_info(FILE *adfs, const char *adfs_name, FILE *out) {
    adfs_status status;
    adfs_object obj;

    if ((status = adfs_find(adfs, adfs_name, &obj)) == ADFS_OK) {
        printf("%-10s %08X %08X %08X %06X %c%c%c%c%c%c%c%c%c\n",
                obj.name, obj.load_addr, obj.exec_addr, obj.length, obj.sector,
                obj.user_read  ? 'R' : '-',
                obj.user_write ? 'W' : '-',
                obj.locked     ? 'L' : '-',
                obj.is_dir     ? 'D' : '-',
                obj.user_exec  ? 'X' : '-',
                obj.pub_read   ? 'R' : '-',
                obj.pub_write  ? 'W' : '-',
                obj.pub_exec   ? 'X' : '-',
                obj.private    ? 'P' : '-');
    }
    return status;
}

adfs_status adfs_copyout(FILE *adfs, const char *adfs_name, const char *host_name) {
    adfs_status status;
    adfs_object obj;
    char *inf_name;
    FILE *dat_fp, *inf_fp;
    unsigned byte_start;

    if ((status = adfs_find(adfs, adfs_name, &obj)) == ADFS_OK) {
        inf_name = alloca(strlen(host_name) + 4);
        sprintf(inf_name, "%s.inf", host_name);
        if ((dat_fp = fopen(host_name, "wb"))) {
            if ((inf_fp = fopen(inf_name, "wt"))) {
                fprintf(inf_fp, "%s %08x %08x %08x\n", obj.name, obj.load_addr, obj.exec_addr, obj.length);
                fclose(inf_fp);
                byte_start = obj.sector * SECTOR_SIZE;
                if (fseek(adfs, byte_start, SEEK_SET) == -1)
                    status = ADFS_SEEK_ERROR;
                else {
                    for (int len = obj.length; len--; )
                        putc(getc(adfs), dat_fp);
                }
            } else {
                fprintf(stderr, "unable to open host INF file '%s' - %s\n", inf_name, strerror(errno));
                status = ADFS_HOST_ERROR;
            }
            fclose(dat_fp);
        } else {
            fprintf(stderr, "unable to open host data file '%s' - %s\n", host_name, strerror(errno));
            status = ADFS_HOST_ERROR;
        }
    } else
        fprintf(stderr, "unable to find ADFS file '%s' - %s\n", adfs_name, adfs_errors[status]);
    return status;
}

static adfs_status adfs_read_fsmap(FILE *adfs, unsigned char *fsmap) {
    if (fseek(adfs, 0, SEEK_SET) != 0)
        return ADFS_SEEK_ERROR;
    if (fread(fsmap, FSMAP_SIZE, 1, adfs) != FSMAP_SIZE)
        return ADFS_READ_ERR;
    if (checksum(fsmap) != fsmap[0xff] || checksum(fsmap+0x100) != fsmap[0x1f])
        return ADFS_BAD_FSMAP;
    return ADFS_OK;
}

static adfs_status adfs_find_space(FILE *adfs, unsigned bytes_reqd, int *entry, int *sector) {
    adfs_status status;
    unsigned char fsmap[512];
    unsigned i, end, sects_reqd, entry_sects, sects_found;

    if ((status = adfs_read_fsmap(adfs, fsmap)) == ADFS_OK) {
        sects_reqd = ((bytes_reqd-1)/SECTOR_SIZE)+1;
        sects_found = 0;
        end = fsmap[0x1e];
        for (i = 0; i < end; i += 3) {
            entry_sects = adfs_get24(fsmap + 0x100 + i);
            if (entry_sects >= sects_reqd) {
                if (sects_found == 0 || entry_sects < sects_found) {
                    sects_found = entry_sects;
                    *entry = i;
                }
            }
        }
        status = ADFS_NO_SPACE;
        if (sects_found > 0) {
            *sector = adfs_get24(fsmap + *entry);
            return ADFS_OK;
        }
    }
    return status;
}

static adfs_status adfs_claim_space(FILE *adfs, unsigned bytes_reqd, int entry) {
    adfs_status status;
    unsigned char fsmap[512];
    unsigned sects_reqd;

    if ((status = adfs_read_fsmap(adfs, fsmap)) == ADFS_OK) {
        sects_reqd = ((bytes_reqd-1)/SECTOR_SIZE)+1;
        adfs_put24(fsmap+entry, adfs_get24(fsmap+entry) + sects_reqd);
        adfs_put24(fsmap+0x100+entry, adfs_get24(fsmap+0x100+entry) - sects_reqd);
        fsmap[0xff] = checksum(fsmap);
        fsmap[0x1ff] = checksum(fsmap+0x100);
        if (fseek(adfs, 0, SEEK_SET) != 0)
            status = ADFS_SEEK_ERROR;
        else if (fwrite(fsmap, FSMAP_SIZE, 1, adfs) != FSMAP_SIZE)
            status = ADFS_WRITE_ERR;
    }
    return status;
}

adfs_status adfs_copyin(FILE *adfs, const char *adfs_name, const char *host_name) {
    return ADFS_NOT_IMPLEMENTED;
}

int main(int argc, char **argv) {
    adfs_status status;
    const char *cmd;
    FILE *adfs;

    printf("%d\n", (1280-sizeof(adfs_dir_hdr) - sizeof(adfs_dir_ftr)) / sizeof(adfs_dir_entry));
    return 0;
    status = ADFS_BAD_COMMAND;
    if (argc >= 2) {
        cmd = argv[1];
        if (argc == 4 && strcasecmp(cmd, "info") == 0) {
            if ((adfs = fopen(argv[2], "rb"))) {
                status = adfs_info(adfs, argv[3], stdout);
                fclose(adfs);
            } else {
                fprintf(stderr, "adfscp: unable to open ADFS disc '%s' for reading: %s\n", argv[2], strerror(errno));
                status = ADFS_HOST_ERROR;
            }
        } else if (argc == 5) {
            if (strcasecmp(cmd, "in") == 0) {
                if ((adfs = fopen(argv[2], "rb+"))) {
                    status = adfs_copyin(adfs, argv[3], argv[4]);
                    fclose(adfs);
                } else {
                    fprintf(stderr, "adfscp: unable to open ADFS disc '%s' for update: %s\n", argv[2], strerror(errno));
                    status = ADFS_HOST_ERROR;
                }
            } else if (strcasecmp(cmd, "out") == 0) {
                if ((adfs = fopen(argv[2], "rb"))) {
                    status = adfs_copyout(adfs, argv[3], argv[4]);
                    fclose(adfs);
                } else {
                    fprintf(stderr, "adfscp: unable to open ADFS disc '%s' for reading: %s\n", argv[2], strerror(errno));
                    status = ADFS_HOST_ERROR;
                }
            }
        }
    }
    if (status == ADFS_BAD_COMMAND)
        fputs("Usage: adfscp <in|out> <adfs-disc> <adfs-file> <host-file>\n", stderr);
    return status;
}
