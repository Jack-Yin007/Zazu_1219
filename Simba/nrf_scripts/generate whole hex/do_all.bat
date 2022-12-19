DEL bootloader.hex
DEL nrf52832_xxaa.hex
DEL bootloader_setting.hex
DEL nrf52832_xxaa_mbr.hex
DEL whole.hex

pause

COPY ..\..\application\pca10040\s132\armgcc\_build\nrf52832_xxaa.hex
COPY ..\..\bootloader\pca10040_uart\armgcc\_build\nrf52832_xxaa_mbr.hex
pause

nrfutil settings generate --family NRF52 --application nrf52832_xxaa.hex --application-version 0 --bootloader-version 0 --bl-settings-version 2 bootloader_setting.hex

pause

mergehex --merge nrf52832_xxaa_mbr.hex bootloader_setting.hex --output bootloader.hex
mergehex --merge bootloader.hex nrf52832_xxaa.hex s132_nrf52_7.0.1_softdevice.hex --output whole.hex

nrfjprog --eraseall -f NRF52
pause

nrfjprog --program whole.hex --verify -f NRF52

pause

nrfjprog --reset -f NRF52
pause
