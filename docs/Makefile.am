doc_DATA = btparser.tex

CLEANFILES = *.aux btparser.log btparser.out btparser.toc btparser.idx btparser.ilg btparser.ind


if HAVE_PDFLATEX
if HAVE_DOXYGEN
if HAVE_DOT
doc_DATA += btparser.pdf

btparser.pdf: btparser.tex doxygen/refman.tex
	echo Calling pdflatex twice to handle all references.
	$(PDFLATEX) btparser.tex
	makeindex btparser.idx
	$(PDFLATEX) btparser.tex
	latex_count=5 ; \
	while egrep -s 'Rerun (LaTeX|to get cross-references right)' btparser.log && [ $$latex_count -gt 0 ] ;\
	    do \
	      echo "Rerunning latex...." ;\
	      $(PDFLATEX) btparser.tex ;\
	      latex_count=`expr $$latex_count - 1` ;\
	    done

doxygen/refman.tex: doxygen.conf ../lib/*.h
	$(DOXYGEN) doxygen.conf
	sed -i'~' '4d' doxygen/refman.tex
	sed -i'~' '/\\printindex$$/d' doxygen/refman.tex
	sed -i'~' '/\\end{document}$$/d' doxygen/refman.tex
endif
endif
endif

clean-local:
	-rm -rf doxygen
