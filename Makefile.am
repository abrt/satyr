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
	rpmbuild $(RPM_DIRS) $(RPM_FLAGS) -ba satyr.spec
srpm: dist-xz satyr.spec
	rpmbuild $(RPM_DIRS) -bs satyr.spec

.PHONY: release-minor
release-minor:
	OLD_VER=$$(git describe --tags --match "[0-9]*" --abbrev=0 HEAD 2>/dev/null); \
	MAJOR_VER=$$(echo $$OLD_VER | cut -d. -f 1); \
	MINOR_VER=$$(echo $$OLD_VER | cut -d. -f 2); \
	$(MAKE) release OLD_VER=$$OLD_VER NEW_VER="$$MAJOR_VER.$$((MINOR_VER+1))"

.PHONY: release-major
release-major:
	OLD_VER=$$(git describe --tags --match "[0-9]*" --abbrev=0 HEAD 2>/dev/null); \
	MAJOR_VER=$$(echo $$OLD_VER | cut -d. -f 1); \
	$(MAKE) release OLD_VER=$$OLD_VER NEW_VER="$$((MAJOR_VER+1)).0"

.PHONY: release
release:
	echo "* $$(LANG='en_US.UTF-8' date +'%a %b %d %Y') $$(git config --get user.name) <$$(git config --get user.email)> $$NEW_VER-1" | sort > /tmp/changelog.tmp; \
	git log --oneline $$OLD_VER..HEAD | awk '{$$1=""; if (a[$$0]++ == 0) print "-" $$0} END {print ""}' | grep -v -e "- Merge" -e "- testsuite:" -e "- make:" >> /tmp/changelog.tmp; \
	sed "$$(grep -n %changelog satyr.spec.in | cut -f1 -d: | head -1)"'r /tmp/changelog.tmp' -i satyr.spec.in; \
	sed 's|DEF_VER=.*$$|DEF_VER='$$NEW_VER'|' -i gen-version; \
	git add gen-version satyr.spec.in; \
	git commit -s -m "Release $$NEW_VER"; \
	git tag "$$NEW_VER"; \
	echo -n "$$NEW_VER" > satyr-version
	autoconf --force
	$(MAKE) dist-xz
