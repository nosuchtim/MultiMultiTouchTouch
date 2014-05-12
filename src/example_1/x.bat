set dir=..\..\..\src\example_1

rem Someday I should figure out how to get processing to use ONLY what's in
rem local directories (not Documents\Processing), so this is more standalone.
rem set core=..\..\other\processing\core\library\core.jar
rem set midibus=..\..\other\themidibus\library\themidibus.jar
rem
rem
..\..\other\processing-2.1.2-windows32\Processing\processing-java.exe --run --force --sketch=%dir% --output=%dir%\output
