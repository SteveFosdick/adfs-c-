#include "DiskImgIO.h"
#include "AcornADFS.h"

#include <errno.h>
#include <string.h>

static const char usage[] = "Usage: adfscp: <in|out> <adfs-disc> <from-name> <to-name>\n";

int main(int argc, char **argv) {
    const char *cmd, *disc, *aname, *hname;
    AcornADFS *adfs;
    afs_status status;
    afs_object obj;
    int err, copyin;

    if (argc != 5) {
        fputs(usage, stderr);
        return 1;
    }
    cmd = argv[1];
    if (strcasecmp(cmd, "in") == 0)
        copyin = 1;
    else if (strcasecmp(cmd, "out") == 0)
        copyin = 0;
    else {
        fputs(usage, stderr);
        return 1;
    }
    disc = argv[2];
    DiskImgIO *dio = DiskImgIO::openImg(disc, copyin);
    if (dio == NULL) {
        fprintf(stderr, "adfscp: unable to open ADFS disc '%s': %s\n", disc, strerror(errno));
        return 2;
    }
    adfs = new AcornADFS(dio);
    if (copyin) {
        hname = argv[3];
        aname = argv[4];
        if ((err = AcornFS::host_load(&obj, hname)) == 0)
            status = adfs->save(&obj, aname);
        else {
            fprintf(stderr, "adfscp: error loading host file '%s': %s\n", hname, strerror(err));
            err = 5;
        }
    } else {
        aname = argv[3];
        hname = argv[4];
        if ((status = adfs->find(aname, &obj)) == AFS_OK) {
            if ((status = adfs->load(&obj)) == AFS_OK) {
                if ((err = AcornFS::host_save(&obj, hname)) != 0) {
                    fprintf(stderr, "adfscp: error saving host file '%s': %s\n", hname, strerror(err));
                    err = 5;
                }
            }
        }
    }
    dio->close();
    if (status != AFS_OK) {
        fprintf(stderr, "adfscp: error loading ADFS file '%s': %s\n", aname, AcornFS::afs_error(status));
        err = 4;
    }
    return err;
}
