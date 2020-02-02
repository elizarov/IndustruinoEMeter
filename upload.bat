@echo off
set host=%1
if [%host%] == [] goto done
prepareFile password ..\build\IndustruinoEMeter.ino.bin upload.bin
curl http://%host%/reset
rem curl --tftp-no-options -v -T upload.bin tftp://%host%
rem tftp -i %host% PUT upload.bin
:done
