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
Repo: https://github.com/David-Paredes/simtemp
Video demo: https://github.com/David-Paredes/simtemp/blob/main/docs/unknown_2025.10.16-23.12.mp4
