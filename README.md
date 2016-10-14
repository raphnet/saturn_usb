# Sega Saturn controller to USB adapter firmware

A firmware for Sega Saturn controller to USB adapters.

## Project homepage

Schematic and additional information are available on the project homepage:

English: [Sega Saturn controller to USB adapter](http://www.raphnet.net/electronique/saturn_usb/index_en.php)
French: [Adaptateur manettes Sega Saturn à USB](http://www.raphnet.net/electronique/saturn_usb/index.php)

## Supported micro-controller(s)

Currently supported micro-controller(s):

* Atmega8
* Atmega168

Adding support for other micro-controllers should be easy, as long as the target has enough
IO pins, enough memory (flash and SRAM) and is supported by V-USB.

## Built with

* [avr-gcc](https://gcc.gnu.org/wiki/avr-gcc)
* [avr-libc](http://www.nongnu.org/avr-libc/)
* [gnu make](https://www.gnu.org/software/make/manual/make.html)

## License

This project is licensed under the terms of the GNU General Public License, version 2.

## Acknowledgments

* Thank you to Objective development, author of [V-USB](https://www.obdev.at/products/vusb/index.html) for a wonderful software-only USB device implementation.
