#include "DiskImgIOlinear.h"

#include <stdlib.h>

DiskImgIO *DiskImgIO::openImg(const char *filename, int writable) {
    DiskImgIO *dio;
    const char *mode;
    FILE *fp;

    mode = writable ? "rb+" : "rb";
    if ((fp = fopen(filename, mode))) {
        dio = new DiskImgIOlinear(fp);
        return dio;
    }
    return NULL;
}

DiskImgIO::DiskImgIO(FILE *fp) {
    this->fp = fp;
    this->sect_size = 256;
}

int DiskImgIO::close() {
    return fclose(fp);
}

void DiskImgIO::dio_free(unsigned char *data) {
    free(data);
}

unsigned DiskImgIO::sectors(unsigned bytes) {
    return ((bytes-1) / sect_size) + 1;
}

