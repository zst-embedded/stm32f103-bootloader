# STM32F103 DFU Bootloader

*Please Note: Must use GCC 4.8, as 4.9 and later have more aggressive optimisation which causes hardware registers not be read correctly and consequently the bootloader does not work.*


This is a DFU bootloader for STM32F103. I don't know how compliant it is, but it works with certain versions of dfu_util. The end goal is to provide a DFU-compliant bootloader that is both well written and has configurable entry options to cater to the target. This project is very far from that but it does work today.

This repo is a derivation of https://github.com/jonatanolofsson/maple-bootloader (mini-boot branch) which is in turn a derivation of the maple-bootloader created by Leaflabs http://github.com/leaflabs/maple-bootloader


Note: The bootloader has a DFU - AltID for RAM uploads, however this returns an error the host attempts to uplaod to this AltID. The AltID was kept, to allow backwards compatibility with anyone still using the old Maple IDE which has a upload to RAM option. Unless anyone cares to reimplement this, it will probably be removed in the future.


Note: On "generic" boards, the USB reset (to force re-enumeration by the host), is triggered by reconfiguring USB line D+ (typically on PA12) into GPIO mode, and driving PA12 low for a short period, before setting the pin back to its USB operational mode.

This system to reset the USB was written by @Victor_pv. It has preliminary fixes by @trueserve but really needs a closer look.

Note: There are multiple build targets for "generic" STM32F103 boards, because each vendor seems to have the "LED" on a different port/pin, and the bootloader flashes the LED to indicate the current operation / state. Bootloader entry buttons are also on various pins, or sometimes are switches, or aren't present at all.

You can add your own configurations in config.h. The Makefile sets the board name as a #define e.g. #define TARGET_GENERIC_F103_PD2, and config.h contains a block of config defines for each board.

Boards which have the Maple USB reset hardware need to defined HAS_MAPLE_HARDWARE


#####Other improvements on the original Maple bootloader

1. Smaller footprint - now fits within 8k
2. Additional DFU AltID upload type was added, which allows the sketch to be loaded at 0x8002000 instead of 0x8005000


##### Building on Linux

You probably know how to do this. Just make sure you have your ARM toolchain working.


##### Building on Windows

1. Make sure MinGW is installed, and has make, bash (Makefile depends on non-Windows shell)
2. Make sure your ARM toolchain is in your path
3. Make like you would on Linux

##### Debugging on Windows

I have found that using the ST-Link GDB server (stlink-20130324-win) works fine with gdb provided by MinGW, as for some reason the GDB provided with the Launchpad ARM toolchain wasn't working.

##### Problems with the bootloader

If you have questions about or issues with the bootloader, please submit an Issue.
