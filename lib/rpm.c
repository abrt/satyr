
struct btp_rpm_info *
rpm_get_package_info(const char *name)
{
    int rpmdbOpen(char * root,
               rpmdb * dbp,
               int mode,
               int perms);
          dbiIndexSet matches;
int rpmdbFindPackage(rpmdb db,
                     char * name,
                     dbiIndexSet * matches);

          int rpmdbFindByProvides(rpmdb db,
                        char * provides,
                        dbiIndexSet * matches);

    if (stat == 0) {
      if (matches.count) {
        printf("Number of matches: %d\n", matches.count);
        h = rpmdbGetRecord(db, matches.recs[0].recOffset);
        if (h) headerDump(h, stdout, 1);
        headerFree(h);
        dbiFreeIndexRecord(matches);
      }
    }

RPMTAG_INSTALLTIME
RPMTAG_NAME
RPMTAG_VERSION
RPMTAG_RELEASE
RPMTAG_EPOCH
RPMTAG_VENDOR

/*
Check require updates.

*/
void rpmdbClose(rpmdb db);
}
