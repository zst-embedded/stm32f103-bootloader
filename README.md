# STM32F103 DFU Bootloader

This is a DFU bootloader for STM32F103 that fits in 8K of flash space, with several activation modes. I don't know how DFU-compliant it is, but it works with certain versions of dfu_util. The end goal is to provide a DFU-compliant bootloader that is both well written and has configurable entry options to cater to the target. This project is very far from the former and only has small amounts of the latter, but it does work today.

Send code to AltID 2 for proper operation.

The long term goal is to have a well written, well documented bootloader that works on other targets as well (particularly other STM32, and EFM32). I don't know if this goal will be achieved.

*This fork has been tested to build and function with GCC4.8, GCC4.9 and GCC5.3.*

Some useless information:

- The bootloader has a DFU - AltID for RAM uploads, but the code for it has mostly been removed. The AltID will probably be removed in the future.
- On "generic" boards, the USB reset (to force re-enumeration by the host) is triggered by reconfiguring USB line D+ (typically on PA12) into GPIO mode, and driving PA12 low for a short period, before setting the pin back to its USB operational mode. This is hacky as all hell, and this explanation may no longer be accurate, either. This system to reset the USB was written by @Victor_pv.
- There are multiple build targets for "generic" STM32F103 boards, because each vendor seems to have the "LED" on a different port/pin, and the bootloader flashes the LED to indicate the current operation / state. Bootloader entry buttons are also on various pins, or sometimes are switches, or aren't present at all.
-- You can add your own configurations in config.h. The Makefile sets the board name as a #define e.g. #define TARGET_GENERIC_F103_PD2, and config.h contains a block of config defines for each board.
-- Boards which have the Maple USB reset hardware need to defined HAS_MAPLE_HARDWARE


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
