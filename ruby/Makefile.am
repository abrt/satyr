EXTRA_DIST = satyr.gemspec Rakefile lib test rubygem-satyr.spec.in

GEM_VERSION = `fgrep "s.version =" satyr.gemspec | sed -r "s/^[^']*'([^']*)'.*$$/\\1/"`
rubygem-satyr.spec: rubygem-satyr.spec.in
	sed "s/@@GEM_VERSION@@/$(GEM_VERSION)/" rubygem-satyr.spec.in > rubygem-satyr.spec

gem:
	gem build satyr.gemspec


RPM_DIRS = --define "_sourcedir `pwd`" \
           --define "_rpmdir `pwd`/build" \
           --define "_specdir `pwd`" \
           --define "_builddir `pwd`/build" \
           --define "_srcrpmdir `pwd`/build"

rpm: gem rubygem-satyr.spec
	rpmbuild $(RPM_DIRS) -ba rubygem-satyr.spec
srpm: gem rubygem-satyr.spec
	rpmbuild $(RPM_DIRS) -bs rubygem-satyr.spec
