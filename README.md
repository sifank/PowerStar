# Power*Star
INDI driver for Wa_Chu_Ed Power*Star, also known as FeatherTouch Power*Star

DEPENDENCIES:
- sudo apt install build-essential devscripts debhelper fakeroot cdbs software-properties-common cmake pkg-config apt install libhidapi-hidraw0 libhidapi-libusb0
- sudo add-apt-repository ppa:mutlaqja/ppa 
- sudo apt install libindi-dev libnova-dev libz-dev libgsl-dev
- I also found that the link /usr/lib/libindidriver.so points to nowhere!  So, remove it, use locate to find the most recent version and link to it: sudo ln -s /usr/lib/x86_64-linux-gnu/libindidriver.so.1.8.9 /usr/lib/libindidriver.so

CONTENTS:
- INDI driver and xml files

INSTALLING:

In a work directory of your choosing on the RPI 
or (linux) system that the Power*Star is plugged into:

git clone https://github.com/sifank/PowerStar.gitll

To compile from scratch:
- cd [install_path/]PowerStar
- mkdir build; cd bulid
- cmake ../
- make
- sudo make install
- (Note: you will need to restart indiserver (or indiwebmanager)

NOTES:

- Initial configuration is set for a Unipolar motor, if you have a bipolar motor or not sure of your Unipolar, then do not connect it to your focus motor until after you set the motor type under the 'Options' tab.


