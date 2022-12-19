nrfjprog --eraseall -f NRF52
pause
nrfjprog --program zazu-Atel-Generic-1.0.255.31_whole.hex --verify -f NRF52
pause
nrfjprog --reset -f NRF52
pause
