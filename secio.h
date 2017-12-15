#ifndef SECIO_INC
#define SECIO_INC

typedef struct _secio secio;

extern secio *secio_open(const char *filename, int writable);
extern int secio_close(secio *sio);
extern unsigned char *secio_read(secio *sio, unsigned sector, unsigned bytes);
extern void secio_free(secio *sio, unsigned char *data);
extern int secio_write(secio *sio, unsigned sector, unsigned bytes, const unsigned char *data);
extern unsigned secio_sectors(secio *sio, unsigned bytes);

#endif
