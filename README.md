# Power*Star-Focus
INDI driver for Greg's Power*Star Focuser, also known as FeatherTouch Power*Star

This is one of three INDI drivers (Focuser, Power, Weather)

DEPENDENCIES:

- sudo apt install build-essential devscripts debhelper fakeroot cdbs software-properties-common cmake pkg-config 
- sudo add-apt-repository ppa:mutlaqja/ppa 
- sudo apt install libindi-dev libnova-dev libz-dev libgsl-dev

CONTENTS:
- INDI driver and xml files

INSTALLING:
In a work directory of your choosing on the RPI 
or (linux) system that the Power*Star is plugged into:

git clone https://github.com/sifank/PowerStar.git

Binary files for the Raspberry Pi are included:
- cd [install path/]PowerStar
- sudo make install

To compile from scratch:
- cd [install_path/]PowerStar
- make clean; make
- sudo make install

NOTES:
- Initial configuration is set for a Unipolar motor, if you have a bipolar motor or not sure of your Unipolar, then do not connect it to your focus motor until after you set the motor type under the 'Options' tab.


