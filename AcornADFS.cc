#include "AcornADFS.h"

#include <stdint.h>
#include <string.h>

#define FSMAP_MAX_ENT 82
#define DIR_HDR_SIZE  0x05
#define DIR_ENT_SIZE  0x1A
#define DIR_FTR_SIZE  0x35

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

AcornADFS::AcornADFS(DiskImgIO *dio) {
    discio = dio;
    fsmap = NULL;
}

void AcornADFS::obj_free(afs_object *obj) {
    if (obj->data != NULL)
        discio->dio_free(obj->data);
}

afs_status AcornADFS::load(afs_object *obj) {
    if ((obj->data = discio->read(obj->sector, obj->length)) == NULL)
        return AFS_READ_ERR;
    return AFS_OK;
}

afs_status AcornADFS::search(afs_object *parent, afs_object *child, const char *name, int name_len, unsigned char **ent_ptr) {
    afs_status status;
    unsigned char *hdr, *ent, *ftr;
    int i, c, found;

    if (name_len > ADFS_MAX_NAME)
        return AFS_NAME_TOO_LONG;
    if (!parent->is_dir)
        return AFS_NOT_A_DIR;
    if ((status = load(parent)) == AFS_OK) {
        hdr = parent->data;
        if (hdr[1] == 'H' && hdr[2] == 'u' && hdr[3] == 'g' && hdr[4] == 'o') {
            ftr = hdr + parent->length - DIR_FTR_SIZE;
            if (ftr[47] == hdr[0] && ftr[48] == 'H' && ftr[49] == 'u' && ftr[50] == 'g' && ftr[51] == 'o') {
                for (ent = hdr + DIR_HDR_SIZE; ent < ftr; ent += DIR_ENT_SIZE) {
                    if (*ent == 0) {
                        *ent_ptr = ent;
                        return AFS_NOT_FOUND;
                    }
                    found = 1;
                    for (i = 0; i < name_len; i++) {
                        c = (name[i] & 0xdf) - (ent[i] & 0x5f);
                        if (c < 0) {
                            *ent_ptr = ent;
                            return AFS_NOT_FOUND;
                        }
                        if (c > 0) {
                            found = 0;
                            break;
                        }
                    }
                    if (found) {
                        for (i = 0; i < name_len; i++)
                            child->name[i] = (ent[i] & 0x7f);
                        child->name[i]    = '\0';
                        child->user_read  = ((ent[0] & 0x80) == 0x80);
                        child->user_write = ((ent[1] & 0x80) == 0x80);
                        child->locked     = ((ent[2] & 0x80) == 0x80);
                        child->is_dir     = ((ent[3] & 0x80) == 0x80);
                        child->user_exec  = ((ent[4] & 0x80) == 0x80);
                        child->pub_read   = ((ent[5] & 0x80) == 0x80);
                        child->pub_write  = ((ent[6] & 0x80) == 0x80);
                        child->pub_exec   = ((ent[7] & 0x80) == 0x80);
                        child->pub_exec   = ((ent[8] & 0x80) == 0x80);
                        child->priv       = ((ent[9] & 0x80) == 0x80);
                        child->load_addr  = adfs_get32(ent + 0x0a);
                        child->exec_addr  = adfs_get32(ent + 0x0e);
                        child->length     = adfs_get32(ent + 0x12);
                        child->sector     = adfs_get24(ent + 0x16);
                        *ent_ptr = ent;
                        return AFS_OK;
                    }
                }
                *ent_ptr = NULL;
                return AFS_NOT_FOUND;
            }
        }
        status = AFS_BROKEN_DIR;
        obj_free(parent);
    }
    return status;
}

static void make_root(afs_object *obj) {
    memset(obj, 0, sizeof(afs_object));
    obj->is_dir = 1;
    obj->length = 1280;
    obj->sector = 2;
}

afs_status AcornADFS::find(const char *adfs_name, afs_object *obj) {
    afs_status status;
    const char  *ptr;
    afs_object *parent, *child, *temp, a, b;
    unsigned char *ent;

    if (adfs_name[0] == '$') {
        if (adfs_name[1] == '\0') {
            make_root(obj);
            return AFS_OK;
        } else if (adfs_name[1] == '.')
            adfs_name += 2;
    }
    make_root(&a);
    parent = &a;
    child  = &b;
    while ((ptr = strchr(adfs_name, '.'))) {
        if ((status = search(parent, child, adfs_name, ptr - adfs_name, &ent)) != AFS_OK) {
            obj_free(parent);
            return status;
        }
        temp = parent;
        parent = child;
        child = temp;
        adfs_name = ptr + 1;
    }
    status = search(parent, obj, adfs_name, strlen(adfs_name), &ent);
    obj_free(parent);
    return status;
}

afs_status AcornADFS::load_fsmap() {
    afs_status status = AFS_OK;
    if (!fsmap) {
        if ((fsmap = discio->read(0, 512)) == NULL)
            status = AFS_READ_ERR;
        else {
            if (checksum(fsmap) != fsmap[0xff] || checksum(fsmap + 0x100) != fsmap[0x1ff]) {
                discio->dio_free(fsmap);
                fsmap = NULL;
                status = AFS_BAD_FSMAP;
            }
        }
    }
    return status;
}

afs_status AcornADFS::save_fsmap() {
    int err;

    if (fsmap) {
        fsmap[0x0ff] = checksum(fsmap);
        fsmap[0x1ff] = checksum(fsmap + 0x100);
        if ((err = discio->write(0, 512, fsmap)) != 0)
            return AFS_WRITE_ERR;
        return AFS_OK;
    }
    return AFS_BUG;
}

afs_status AcornADFS::save(afs_object *obj, const char *dest_dir) {
    afs_status status;
    afs_object parent, child;
    unsigned char *ent;

    if ((status = find(dest_dir, &parent)) == AFS_OK) {
        if (!parent.is_dir)
            status = AFS_NOT_A_DIR;
        else if ((status = load_fsmap()) == AFS_OK) {
            if ((status = search(&parent, &child, obj->name, strlen(obj->name), &ent)) == AFS_OK) {
                if ((status = map_free(&child)) == AFS_OK)
                    if ((status = alloc_write(obj)) == AFS_OK)
                        status = dir_update(&parent, obj, ent);
            } else if (status == AFS_NOT_FOUND) {
                if (ent == NULL)
                    status = AFS_DIR_FULL;
                else {
                    if ((status = alloc_write(obj)) == AFS_OK) {
                        dir_makeslot(&parent, ent);
                        status = dir_update(&parent, obj, ent);
                    }
                }
            }
            if (status == AFS_OK)
                status = save_fsmap();
            else {
                discio->dio_free(fsmap);
                fsmap = NULL;
            }
        }
    }
    return status;
}

afs_status AcornADFS::map_free(afs_object *obj) {
    unsigned char *sizes = fsmap + 0x100;
    int end = fsmap[0x1fe];
    int ent, bytes;
    uint32_t posn, size, obj_size;

    obj_size = discio->sectors(obj->length);
    for (ent = 0; ent < end; ent += 3) {
        posn = adfs_get24(fsmap + ent);
        size = adfs_get24(sizes + ent);
        if ((posn + size) == obj->sector) { // coallesce
            size += obj_size;
            adfs_put24(sizes + ent, size);
            return AFS_OK;
        }
        if (posn > obj->sector) {
            if (end >= 82)
                return AFS_MAP_FULL;
            bytes = (end - ent) * 3;
            memmove(fsmap + ent + 3, fsmap + ent, bytes);
            memmove(sizes + ent + 3, sizes + ent, bytes);
            break;
        }
    }
    if (end >= 82)
        return AFS_MAP_FULL;
    adfs_put24(fsmap + ent, obj->sector);
    adfs_put24(sizes + ent, obj_size);
    fsmap[0x1fe]++;
    return AFS_OK;
}

afs_status AcornADFS::alloc_write(afs_object *obj) {
    unsigned char *sizes = fsmap + 0x100;
    int end = fsmap[0x1fe];
    int ent, bytes, err;
    uint32_t posn, size, obj_size;

    obj_size = discio->sectors(obj->length);
    for (ent = 0; ent < end; ent += 3) {
        size = adfs_get24(sizes + ent);
        if (size >= obj_size) {
            posn = adfs_get24(fsmap + ent);
            obj->sector = posn;
            if (size == obj_size) { // uses exact space so kill entry.
                bytes = (end - ent) * 3;
                memmove(fsmap + ent, fsmap + ent + 3, bytes);
                memmove(sizes + ent, sizes + ent + 3, bytes);
                fsmap[0x1fe]--;
            } else {
                adfs_put24(fsmap + ent, posn + obj_size);
                adfs_put24(sizes + ent, size - obj_size);
            }
            if ((err = discio->write(posn, obj->length, obj->data)) == 0)
                return AFS_OK;
            return AFS_WRITE_ERR;
        }
    }
    return AFS_NO_SPACE;
}

afs_status AcornADFS::dir_update(afs_object *parent, afs_object *child, unsigned char *ent) {
    int i, ch, err;

    for (i = 0; i < ADFS_MAX_NAME; i++) {
        ch = child->name[i];
        ent[i] = ch & 0x7f;
        if (ch == 0)
            break;
    }
    if (child->user_read)  ent[0] |= 0x80;
    if (child->user_write) ent[1] |= 0x80;
    if (child->locked)     ent[2] |= 0x80;
    if (child->is_dir)     ent[3] |= 0x80;
    if (child->user_exec)  ent[4] |= 0x80;
    if (child->pub_read)   ent[5] |= 0x80;
    if (child->pub_write)  ent[6] |= 0x80;
    if (child->pub_exec)   ent[7] |= 0x80;
    if (child->pub_exec)   ent[8] |= 0x80;
    if (child->priv)       ent[9] |= 0x80;
    adfs_put24(ent + 0x0a, child->load_addr);
    adfs_put24(ent + 0x0e, child->exec_addr);
    adfs_put24(ent + 0x12, child->length);
    adfs_put24(ent + 0x16, child->sector);
    if ((err = discio->write(parent->sector, parent->length, parent->data)) == 0)
        return AFS_OK;
    return AFS_WRITE_ERR;
}

void AcornADFS::dir_makeslot(afs_object *parent, unsigned char *ent) {
    unsigned char *ftr = parent->data + parent->length - DIR_FTR_SIZE;
    unsigned bytes = ftr - ent - DIR_ENT_SIZE;
    memmove(ent + DIR_ENT_SIZE, ent, bytes);
}
