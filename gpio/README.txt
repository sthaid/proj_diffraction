===========================
TITLE: What's in this directory ?
===========================

Python Scripts from the Canakit Install Guide
- test_blink.py
- test_switch.py

WiringPi Sample Programs
- wpi_blink.c   - blink led on gpio-out 18
- wpi_switch.c  - switch led on gpio-out-18 based in gpio-in-25
- wpi_timing.c  - measure gpio-in-25 read rate

Bcm2835 Library Sample Programs
- bcm_blink.c
- bcm_switch.c
- bcm_timing.c

GPIO via Direct Register Access Sample Programs. 
These programs are based on info from:
    https://elinux.org/RPi_GPIO_Code_Samples#Direct_register_access
    https://www.raspberrypi.org/forums/viewtopic.php?t=243846   
    (for the RPI4 Peripherals Base)
- dra_gpio.h   - gpio via direct register access implementation
- dra_blink.c
- dra_switch.c
- dra_timing.c
- dra_pulse.c  - using Signal Generator for input - counts pulses (rising edges)
- dra_sipm.c   - using SiPM - refer to extensive comments in this file

===========================
TITLE: Results from the gpio input timing programs
===========================

[DS haid@ds gpio]$ sudo ./wpi_timing 
million_cnt/sec = 27.84
cnt=10000000  cnt_v_set=10000000

[DS haid@ds gpio]$ sudo ./bcm_timing 
million_cnt/sec = 11.09
cnt=10000000  cnt_v_set=10000000

[DS haid@ds gpio]$ sudo ./dra_timing 
million_cnt/sec = 64.45
cnt=10000000  cnt_v_set=10000000

The Direct Register Access (dra) is the winner for gpio read performance.

===========================
TITLE: Results from dra_pulse
===========================

  ------------- Signal Generator ----------          ------ DRA_PULSE ------
WAVE    VOLTAGE     OFFSET  DUTY    FREQUENCY     GPIO_READ_RATE    PULSE_RATE  (million/sec)

Square  3.3         100%    10%     1 MHZ              39           1.00
Square  3.3         100%    10%     1.3 MHZ            39           1.30
Square  3.3         100%    10%     1.5 MHZ            37           1.44 - 1.50

Summary: 
- The 1 MHZ frequency 10% duty cycle is a pulse width of 100 ns.
  This is the pulse width that needs to be measured; so the Raspberry Pi4 
  appears to meet my needs for detection of 100 ns pulses using GPIO.
- Start to have problems with the signal generator set to 1.5 MHZ. The 
  pulse width should be about 66 ns in this case; and the pulse rate should
  be 1.50 million/sec.

===========================
TITLE: RPi GPIO Code Samples - from the web
===========================

https://elinux.org/RPi_GPIO_Code_Samples#bcm2835_library
https://elinux.org/RPi_GPIO_Code_Samples#Direct_register_access

For C, the GPIO access options are:
- Director Register Access
- WiringPi
- sysfs
- bcm2835 library
- pigpio

===========================
TITLE: Wiring Pi
===========================

http://wiringpi.com/reference/

http://wiringpi.com/wiringpi-deprecated/
http://wiringpi.com/wiringpi-updated-to-2-52-for-the-raspberry-pi-4b/
https://projects.drogon.net/wiringpi-pin-numbering/

--- commands ---
gpio readall
gpio -v

--- to update wiringpi ---
cd /tmp
wget https://project-downloads.drogon.net/wiringpi-latest.deb
sudo dpkg -i wiringpi-latest.deb

--- wiringPi's "gpio readall" Examples ---
http://www.cpmspectrepi.uk/raspberry_pi/MoinMoinExport/WiringPiGpioReadall.html#Pi_4_B

===========================
TITLE: bcm2835 library
===========================

http://www.airspayce.com/mikem/bcm2835/
For documentation:
  MainPage -> Modules -> GPIO Register Access

--- comments at the above website for realtime ---
#define <sched.h>
#define <sys/mman.h>
struct sched_param sp;
memset(&sp, 0, sizeof(sp));
sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
sched_setscheduler(0, SCHED_FIFO, &sp);
mlockall(MCL_CURRENT | MCL_FUTURE);

--- installation ---
# download the latest version of the library, say bcm2835-1.xx.tar.gz, then:
tar zxvf bcm2835-1.xx.tar.gz
cd bcm2835-1.xx
./configure
make
sudo make check
sudo make install

