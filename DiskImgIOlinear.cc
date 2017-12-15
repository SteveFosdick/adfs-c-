#include "DiskImgIOlinear.h"

#include <errno.h>
#include <stdlib.h>

unsigned char *DiskImgIOlinear::read(unsigned sector, unsigned bytes) {
    int byte_posn = sector * sect_size;
    unsigned char *data;

    if (fseek(fp, byte_posn, SEEK_SET) == 0) {
        if ((data = (unsigned char *)malloc(bytes))) {
            if (fread(data, bytes, 1, fp) == 1)
                return data;
            free(data);
        }
    }
    return NULL;
}

int DiskImgIOlinear::write(unsigned sector, unsigned bytes, const unsigned char *data) {
    int byte_posn = sector * sect_size;
    if (fseek(fp, byte_posn, SEEK_SET) == 0)
        if (fwrite(data, bytes, 1, fp) == 1)
            return 0;
    return errno;
}
