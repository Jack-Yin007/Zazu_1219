@REM exe combinedFilename NumberOfHWid nHWid nHWVer nIndividualFilename nIndividualFileType nIndividualFileReset nIndividualFileAttr
@REM 方法：在makezip下，分别mcu，app，cfg 共3个文件夹 ，对应文件放入各个文件夹内
@makeAzip.exe zipmcu00.bin 6 3035 2 3035 1 3101 1 3031 2 3031 1 3100 1 .\mcu\nrf52832_xxaa.dat I 0 0 .\mcu\nrf52832_xxaa.bin I 0 0
makeAzip.exe zipmcu00.bin 6 3035 2 3035 1 3101 1 3031 2 3031 1 5100 1 .\mcu\nrf52833_xxaa.dat I 0 0 .\mcu\nrf52833_xxaa.bin I 0 0