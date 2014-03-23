default:
	nmake PYTHON=c:\python27 install
	c:\python27\python setup.py py2exe
	copy dist\*.* ..\..\bin

justinstall:
	nmake PYTHON=c:\python27 install

FINALDIR = $(PYTHON)\lib\site-packages\nosuch

install : \
	$(FINALDIR) \
	$(FINALDIR)\__init__.py \
	$(FINALDIR)\oscutil.py

$(FINALDIR):
	mkdir $(FINALDIR)

$(FINALDIR)\__init__.py: __init__.py
	copy __init__.py $(FINALDIR)\__init__.py

$(FINALDIR)\oscutil.py: oscutil.py
	copy oscutil.py $(FINALDIR)\oscutil.py

clean :
	rm -f *~ *.bak *.pyc

dist :
	rm -f *~ *.bak *.pyc
	cd ..
	del /q python_nosuch_oscutil.zip
	zipdir nosuch_oscutil
	mv nosuch_oscutil.zip python_nosuch_oscutil.zip
	tosoft python_nosuch_oscutil.zip
	cd nosuch_oscutil
