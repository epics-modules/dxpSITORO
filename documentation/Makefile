
doxygen:
	doxygen

areaDetectorDoc.pdf:
	-htmldoc --book --duplex --no-numbered --format pdf14 --title --size letter --outfile html/areaDetectorDoc.pdf ../ADCore/documentation/areaDetectorDoc.html

mappingDocs:
	./makeMappingDocs

install:
	cp -f *.html /home/epics/web_software/epics
	cp -f *.png /home/epics/web_software/epics
	cp -f *.jpg /home/epics/web_software/epics
	cp -f *.pdf /home/epics/web_software/epics
	cp -rfp dxpDoxygenHTML  /home/epics/web_software/epics/

clean:
	rm -rf dxpDoxygenHTML/*
	rm -f dxpDoc.pdf


realclean: clean
