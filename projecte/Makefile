
#inscripcio.pdf: inscripcio.tex inscripcio.glg inscripcio.gls

pdf: projecte.pdf

#projecte.pdf: *.tex
projecte.pdf: *tex
	pdflatex projecte.tex 	
	makeindex projecte.nlo -s nomencl.ist -o projecte.nls
	pdflatex projecte.tex 	

#inscripcio.glo inscripcio.ist: inscripcio.tex
#	pdflatex inscripcio.tex

#index inscripcio.glg inscripcio.gls: inscripcio.ist inscripcio.glo
#	makeindex 'inscripcio.glo' -s 'inscripcio.ist' -t 'inscripcio.glg' -o 'inscripcio.gls'
	
clean:
	rm -f *.aux *.dvi *.log *.toc *.glo *.idx *.ist *.out *.pdf *.nlo
