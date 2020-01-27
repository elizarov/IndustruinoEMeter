@echo off
set host=%1
if [%host%] == [] goto done
prepareFile password ..\build\IndustruinoEMeter.ino.bin IndustruinoEMeter.bin
curl http://%host%/reset
tftp -i %host% PUT IndustruinoEMeter.bin
:done
