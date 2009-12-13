
#inscripcio.pdf: inscripcio.tex inscripcio.glg inscripcio.gls

pdf: projecte.pdf

projecte.pdf: *tex
	groff -man -Dutf8 -Tps -rS12 skd-launcher.1 > skd-launcher.ps
	ps2pdf skd-launcher.ps
	groff -man -Dutf8 -Tps -rS12 skd-config.7 > skd-config.ps 
	ps2pdf skd-config.ps
	groff -man -Dutf8 -Tps -rS12 skd-client.1 > skd-client.ps 
	ps2pdf skd-client.ps
	pdflatex projecte.tex 	
	makeindex projecte.nlo -s nomencl.ist -o projecte.nls
	pdflatex projecte.tex 	

clean:
	rm -f *.aux *.dvi *.log *.toc *.glo *.idx *.ist *.out *.pdf *.nlo *.ilg *.nls *.ps
