make clean
make
sudo ./gpt /dev/sde -w

#MLO,uboot
sudo dd if=/git/uevm/u-boot/MLO        of=/dev/sde  bs=1k seek=128
sudo dd if=/git/uevm/u-boot/u-boot.img of=/dev/sde  bs=1k seek=256

#kernel
sudo dd if=/second/chromeos-release-R18-1660.B/chroot/home/a0876558local/trunk/chroot/kernel/arch/arm/boot/uImage          of=/dev/sde2
#sudo dd if=/git/uevm/kernel/arch/arm/boot/omap5-uevm-chrome.dtb  of=/dev/sde10

cd /second/chromeos-release-R18-1660.B/chroot/home/a0876558local/trunk/src/build/images/arm-generic/latest
sudo dd if=part_3 of=/dev/sde3
sudo dd if=part_1 of=/dev/sde1

umount /media/*
sync

