# U-Boot-Typewriter for Pomera DM250 

WIP

DEMO: <https://www.youtube.com/watch?v=r5F_BPKYIpU>

<img width="5712" height="4284" alt="IMG_3247" src="https://github.com/user-attachments/assets/d23f91c6-44ba-4057-a4c7-375a81b196ac" />

based on:
 - ichinomoto's DM250 SD card Debian image: <https://www.ekesete.net/log/?p=9565>
 - icninomoto's Pomera U-Boot <https://github.com/ichinomoto/u-boot>
 - jcs's Pomera U-Boot <https://github.com/jcs/u-boot>
 - rockchip's boot from SD <https://github.com/mfkiwl/rk-open-docs/blob/master/MMC/Rockchip_Developer_Guide_SD_Boot_CN.md>
 - a successful one on Chromebook Samsung Plus RK3399: <https://github.com/scateu/u-boot-typewriter>

## BUILD U-boot, Generate Image & Write to SD Card


```
# 1. write sd_dm250.img from ichinomoto to a SD Card

# 2. build u-boot

sudo apt update
sudo apt install -y git make gcc bison flex libssl-dev bc ncurses-dev
sudo apt install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
sudo apt install -y libgnutls28-dev

git clone https://github.com/ichinomoto/u-boot


# 3. Wire typewriter in...

git clone https://github.com/scateu/u-boot-typewriter

cp ../../u-boot-typewriter/{*.c,*.h} .

# 3a Generate wubi and get unifont.hex ...  FIXME currently
vi configs/pomera-dm250_defconfig
vi cmd/Makefile cmd/Kconfig

## 3b drop all powersave function

# 4. build and write

## 4a. Drop the 1MB limit from /build/tools/rockchip/loaderimage.c and write_uboot.sh

./make_uboot_image.sh dm250
sudo ./write_uboot.sh dm250 /dev/mmcblk0

```

## TODO
 x LIMIT: 1MB at most - FIXME. override.
 - powersave, brightness
 - power button
