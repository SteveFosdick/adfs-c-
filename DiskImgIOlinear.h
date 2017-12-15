#ifndef DiskImgIOlinear_INC
#define DiskImgIOlinear_INC

#include "DiskImgIO.h"

class DiskImgIOlinear: public DiskImgIO {
    public:
        DiskImgIOlinear(FILE *fp) : DiskImgIO(fp) {};
        unsigned char *read(unsigned sector, unsigned bytes);
        int write(unsigned sector, unsigned bytes, const unsigned char *data);
};

#endif
