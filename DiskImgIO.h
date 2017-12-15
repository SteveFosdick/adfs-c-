#ifndef DiskImgIO_INC
#define DiskImgIO_INC

#include <stdio.h>

class DiskImgIO {
    public:
        static DiskImgIO *openImg(const char *filename, int writable);
        DiskImgIO(FILE *fp);
        virtual unsigned char *read(unsigned sector, unsigned bytes) = 0;
        void dio_free(unsigned char *data);
        virtual int write(unsigned sector, unsigned bytes, const unsigned char *data) = 0;
        int close();
        unsigned sectors(unsigned bytes);
    protected:
        FILE     *fp;
        unsigned sect_size;
};

#endif
