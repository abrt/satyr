if BUILD_DOCUMENTATION
  MAYBE_DOCUMENTATION = docs
endif

SUBDIRS = lib python tests $(MAYBE_DOCUMENTATION)

# Automatic update of libtool.
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status libtool

bin_PROGRAMS = satyr
satyr_CFLAGS = -Wall
satyr_SOURCES = satyr.c
satyr_LDADD = lib/libsatyr.la

man_MANS = satyr.1
EXTRA_DIST = $(man_MANS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = satyr.pc
DISTCLEANFILES = $(pkgconfig_DATA)
EXTRA_DIST += satyr.pc.in

EXTRA_DIST += autogen.sh RELEASE

UPLOAD_URI = kklic@fedorahosted.org:btparser
.PHONY: upload
upload:
	make dist-gzip
	scp $(distdir).tar.gz $(UPLOAD_URI)
	make dist-zip
	scp $(distdir).zip $(UPLOAD_URI)
	make dist-xz
	scp $(distdir).tar.xz $(UPLOAD_URI)

RPM_DIRS = --define "_sourcedir `pwd`" \
           --define "_rpmdir `pwd`" \
           --define "_specdir `pwd`" \
           --define "_builddir `pwd`" \
           --define "_srcrpmdir `pwd`"

EXTRA_DIST += satyr.spec.in
rpm: dist-xz satyr.spec
	rpmbuild $(RPM_DIRS) -ba satyr.spec
srpm: dist-xz satyr.spec
	rpmbuild $(RPM_DIRS) -bs satyr.spec
