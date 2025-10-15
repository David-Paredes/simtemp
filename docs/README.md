Build steps:
- cd kernel
- make
- cd ..

Run steps:
- sudo insmod kernel/nxp_simtemp.ko
- sudo python3 user/cli/main.py
- sudo rmmod simtemp
- cd kernel
- make clean

Links: