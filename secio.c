#include "secio.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

struct _secio {
    FILE  *fp;
    off_t disc_size;
    int   sect_size;
};

secio *secio_open(const char *filename, int writable) {
    secio *sio;
    const char *mode;

    if ((sio = malloc(sizeof(secio)))) {
        mode = writable ? "rb+" : "rb";
        if ((sio->fp = fopen(filename, mode))) {
            sio->sect_size = 256;
            if ((sio->disc_size = fseek(sio->fp, 0, SEEK_END)) > 0)
                return sio;
            fclose(sio->fp);
        }
        free(sio);
    }
    return NULL;
}

int secio_close(secio *sio) {
    int res;
    res = fclose(sio->fp);
    free(sio);
    return res;
}

unsigned char *secio_read(secio *sio, unsigned sector, unsigned bytes) {
    unsigned byte_posn = sector * sio->sect_size;
    unsigned char *data;

    if (fseek(sio->fp, byte_posn, SEEK_SET) == byte_posn) {
        if ((data = malloc(bytes))) {
            if (fread(data, bytes, 1, sio->fp) == 1)
                return data;
            free(data);
        }
    }
    return NULL;
}

void secio_free(secio *sio, unsigned char *data) {
    free(data);
}

int secio_write(secio *sio, unsigned sector, unsigned bytes, const unsigned char *data) {
    unsigned byte_posn = sector * sio->sect_size;
    if (fseek(sio->fp, byte_posn, SEEK_SET) == byte_posn)
        if (fwrite(data, bytes, 1, sio->fp) == 1)
            return 0;
    return errno;
}

unsigned secio_sectors(secio *sio, unsigned bytes) {
    return ((bytes-1) / sio->sect_size) + 1;
}


