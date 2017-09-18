SUBDIRS = lib python tests include ruby apidoc

# Automatic update of libtool.
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status libtool

bin_PROGRAMS = satyr
satyr_CFLAGS = -Wall -Iinclude -Ilib
satyr_SOURCES = satyr.c
satyr_LDADD = lib/libsatyr.la

man_MANS = satyr.1
EXTRA_DIST = satyr.1.in

satyr.1: satyr.1.in
	sed -e "s,\@PACKAGE_STRING\@,$(PACKAGE_STRING),g" $< > $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = satyr.pc
DISTCLEANFILES = $(pkgconfig_DATA)
EXTRA_DIST += satyr.pc.in

EXTRA_DIST += autogen.sh RELEASE

#UPLOAD_URI = user@fedorahosted.org:abrt
#.PHONY: upload
#upload:
#	make dist-xz
#	scp $(distdir).tar.xz $(UPLOAD_URI)

RPM_DIRS = --define "_sourcedir `pwd`" \
           --define "_rpmdir `pwd`/build" \
           --define "_specdir `pwd`" \
           --define "_builddir `pwd`/build" \
           --define "_srcrpmdir `pwd`/build"

EXTRA_DIST += satyr.spec.in
rpm: dist-xz satyr.spec
	rpmbuild $(RPM_DIRS) -ba satyr.spec
srpm: dist-xz satyr.spec
	rpmbuild $(RPM_DIRS) -bs satyr.spec
apidoc:
	doxygen --somepar
