if DOXYGEN_DOCS_ENABLED
SUBDIRS = html

EXTRA_DIST = Doxygen.conf

noinst_PROGRAMS = index.html

index_html_SOURCES = Doxygen.conf

index.html: Doxygen.conf
	doxygen Doxygen.conf

endif

MAINTAINERCLEANFILES = \
	Makefile.in
