
#inscripcio.pdf: inscripcio.tex inscripcio.glg inscripcio.gls

pdf: projecte.pdf

projecte.pdf: *tex
	pdflatex projecte.tex 	
	makeindex projecte.nlo -s nomencl.ist -o projecte.nls
	pdflatex projecte.tex 	

clean:
	rm -f *.aux *.dvi *.log *.toc *.glo *.idx *.ist *.out *.pdf *.nlo *.ilg *.nls
