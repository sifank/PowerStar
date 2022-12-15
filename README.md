# Power*Star
INDI driver for Wa_Chu_Ed Power*Star, also known as FeatherTouch Power*Star

DEPENDENCIES:
- sudo apt install build-essential devscripts debhelper fakeroot cdbs software-properties-common cmake pkg-config libhidapi-hidraw0 libhidapi-libusb0 git 
- sudo add-apt-repository ppa:mutlaqja/ppa 
- sudo apt install libindi-dev libnova-dev libz-dev libgsl-dev

ISSUES:
- Latest INDI release has an orphaned link:  /usr/lib/libindidriver.so points to nowhere!
  - To fix: remove it, use 'locate libindidriver.so' to find the most recent version and create a link to it.
  - Example: 
    - locate libindidriver.so 
    - sudo rm /usr/lib/libindidriver.so
    - sudo ln -s /usr/lib/x86_64-linux-gnu/libindidriver.so   /usr/lib/libindidriver.so

CONTENTS:
- INDI driver and xml files

INSTALLING:

In a work directory of your choosing on the RPI 
or (linux) system that the Power*Star is plugged into:

- git clone https://github.com/sifank/PowerStar.git
- cd PowerStar
- mkdir build; cd build
- cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug ../
- make
- sudo make install
- (Note: you will need to restart indiserver (or indiwebmanager)

NOTES:

- !NOTE! devices names 'must' be unique!
- Initial configuration is set for a Unipolar motor, if you have a bipolar motor or not sure of your Unipolar, then do not connect it to your focus motor until after you set the motor type under the 'Options' tab.


