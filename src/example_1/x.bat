
rem Someday I should figure out how to get processing to use ONLY what's in
rem local directories (not Documents\Processing), so this is more standalone.
rem set core=..\..\other\processing\core\library\core.jar
rem set midibus=..\..\other\themidibus\library\themidibus.jar
rem
rem

set dir="c:\users\tjt\documents\github\multimultitouchtouch\src\example_1"
set p=c:\users\tjt\documents\processing

%p%\processing-java.exe --run --force --sketch=%dir% --output=%dir%\output
