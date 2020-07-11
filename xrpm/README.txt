==================================
OVERVIEW
==================================

The purpose of the software in this directory is to position the
SiPM sensor using the Translation Rail, in 0.1 mm increments; and
to capture the SiPM count rate at each position.

The sensor position & count rate data is stored in a file so that a 
graphing program, such as gnuplot, can later be used to plot the graph 
of count rate vs. sensor position.

There are 2 programs, and I run each program on a separate Raspberry Pi. 

Sipm_server Program: This program reads the GPIO signal from the 
SiPM sensor, and analyzes the values read to determine the 
number of pulses per second (photons detected per second).
The process of reading and analyzing the GPIO values is
cpu intensive, and to obtain accurate results it is best to not
interrupt this processing. This is the reason why this program is
run on a dedicated Raspberry Pi Model 4.

Ctlr Program: This program connects to the sipm_server using a 
TCP socket. I used WiFi, but a wired ethernet to the sipm_server 
might have been a better choice. 
The ctlr program controls the position of the translation rail 
utilizing a USB connection to a stepper motor controller. The SiPM sensor 
is mounted on the translation rail
The ctlr program's positions the SiPM sensor in 0.1 mm increments,
and requests SiPM count rate from the sipm_server at each position.

The position/count_rate data is stored to a file, so it can later
be plotted. A crude plot of the data is also displayed in real time
on the terminal.

Electronics Block Diagram 
-------------------------

                                                    ----------------
               --------------     -------------     | Raspberry Pi |
  --------     | Broadband  |     | Pulse     |     | running      |
  | SiPM |---->| Amplifiers |---->| Stretcher |---->| sipm_server  |
  --------     --------------     -------------     ----------------
                                                           |
                                                           | WiFi TCP
                                                           | Connection
                                                           |
                            -------------           ----------------
                            | User's    |   WiFi    | Raspberry Pi |
                            | Ssh Login |-----------| running ctlr |
                            -------------           ----------------
                                                      |    |
                                            ---------/     | USB
                                           /               |
                                  -------------      ----------------
                                  | Speaker   |      | Translation  |
                                  -------------      | Rail Stepper |
                                                     | Motor Ctlr   |
                                                     ----------------

Optics Block Diagram
--------------------

  -----------------    ---------------    ----------------     ---------------    ---------------
  | Laser Pointer |--->| OD 4 Filter |--->| OD 3.6 Filter |--->| Double SLit |--->| SiPM Sensor |
  -----------------    ---------------    ----------------     ---------------    ---------------

Dark Environment Needed
-----------------------

The OD 4+3.6 filters reduce the number of photons from the laser pointer by a
factor of 10^7.6. The reason for this is so that there is usually just one photon 
at a time in the region between the Filters and the SiPM detector. 

In Classical Physics one might expect that a single photon would travel straight from 
the laser to the detector, and that there would not be an interference pattern. 
However an interference pattern does occur, even with a single photon. Quantum Mechanics 
attempts to explain why that is so.

The SiPM is very sensitive to photons and this experiment must be performed in a
very dark environment. I had originally planned to put the apparatus in a light proof
box; however making even a small box that keeps out daylight proved to be quite difficult.

Instead I opted to run the experiment at night, with the curtains taped closed, and
with electrical tape covering LED light sources. Fortunately I live in a rural area
and there is not much traffic or other outside light sources. The main problem with
this approach (for me) is finding the motivation to stay up late, especially in the 
Summer months.

Summary of Results
------------------

A diffraction pattern was observed, and the diffraction pattern obtained corresponds
very closely to the diffraction pattern computed by the ifsim program. I had not expected
such a close match between the measured and predicted diffraction patterns.

The detailed results are included in the xrpm/results directories.

Misc General Info
-----------------

Distance from Double Slit to Detector    1.8 m
Translation Rail Range Used              20 mm
Translation Rail Step Distance           0.1 mm
SiPM Sensor Size                         1 mm x 1 mm

==================================
SIPM_SERVER PROGRAM
==================================

The primary function of this program is to read the raw SiPM pulse data
from a GPIO pin and analyze the data to determine the pulse rate.
The pulse rate is the number of photons detected per second.

This primary function is implemented in the sipm_thread:
    while (true) {
        read_sipm();
        analyze_sipm();
    }

Read_sipm is an optimized loop that reads the GPIO pin and saves the
values in a bit array. On my Raspberry Pi 4 this routine is able to read the
GPIO pin and save the value at a rate of 75 million per second. This routine
reads the GPIO values for 100 milliseconds and then returns.

Analyze_sipm inspects the GPIO values that were collected by read_sipm and 
determines the pulse_count for the 100 ms interval of data that had just been
acquired.

When a GET_RATE request is received from the ctlr program, the sipm_get_rate routine
is called. This routine combines the pulse_count info for the most recent one second
of analyze_sipm results (approximately the last 9 results saved by analyze_sipm). 
The pulse_rate for the past one second is calculated and sent back to the 
ctlr program.

Add the following to /etc/rc.local to auto start this program when the Raspberry Pi starts:
  # start sipm_server
  /PATH/sipm_server >/dev/null 2>&1

The sipm_server program saves debug info to sipm_server.log.

==================================
CTLR PROGRAM
==================================

This program uses the ncurses library. Controls are single key strokes.

Display Layout:
- top left: provides HELP, summarizing the program usage.
- top right: provides STATUS of Audio, Translation Rail and Photomultiplier sensor
- bottom: provides either a graph of SiPM sensor pulse_rate vs time, or vs
  SiPM sensor position

Audio Commands:
- v:  volume down
- V:  volume up
- m:  toggle mute
- 1:  say audio test phrase

Translation Rail (XRAIL) Commands:
- to calibrate the xrail
  - F7: move 1 mm to the left, to center the xrail; or
  - F8: move 1 mm to the right, to center the xrail
  - c:  calibrate done
- commands valid after calibration
  - h:  goto home
  - g:  position xrail in range -10mm to +10mm in 0.1mm steps, and collect SiPM count rate;
        the plot of this data can be viewed in real time by selecting Graph Command F2;
        the data is also stored in gnuplot_yyyy_mm_dd_hh_mm_ss.dat
  - 2:  test by moving xrail to -25mm, +25mm, and to home
  - <ESC>: cancel 'g' or '2' 

Graph Commands
- F1:     select the PULSE_RATE VS TIME graph
- F2:     select the PULSE_RATE VS SENSOR_LOC graph
- ARROWS: position graph horizontally and vertically
- + -:    adjust y span
- HOME:   position graph at beginning
- END:    position graph at the end
- r:      reset y offset and y span

Usage Example:
- ./ctlr:  starts the program
- F7 / F8: to position the xrail to home (center) position (usually not necessary)
- 'c':     to complete xrail calibration
- 'g':     starts collecting pulse_rate vs sensor_location data
- F2:      view graph of pulse_rate_vs sensor_loc data as it is being collected

To automatically start festival text to speech server when Raspberry Pi boots:
- add this to /etc/rc.local
    # start festival
    /usr/bin/festival --server >/var/log/festival.log 2>&1 &

Plotting the gnuplot_xxxx.dat file:
- This can be done on any Linux computer which has gnuplot available.
  To install gnuplot, on Fedora, 'yum install gnuplot'.
- Refer to results/run_gnuplot shell script for an example of how to
  run gnuplot.

Debugging:
- Check the ctlr.log file.
- Ensure festival text to speech server is running. It should have been 
  started by the additon to /etc/rc.local described above.
- Login to the Raspberry Pi which should be running the sipm_server, 'ssh ds', and
  confirm sipm_server is running.

==================================
PARTS LIST
==================================

Summary of major parts used in this project , and their prices.

SiPM
- https://www.digikey.com/product-detail/en/on-semiconductor/MICROFC-SMA-10035-GEVB/MICROFC-SMA-10035-GEVBOS-ND/9744734
    MICROFC-SMA-10035-GEVB
    Digi-Key Part Number    MICROFC-SMA-10035-GEVBOS-ND
    Manufacturer            ON Semiconductor
    Manufacturer            Part Number MICROFC-SMA-10035-GEVB
    Description             C-SERIES 1MM 35U SMA
    Detailed Description    MicroFC-10035 C Light, Silicon Photomultiplier (SiPM) Sensor Evaluation Board
    Price: $181     SiPM
- 3 9V batteries and cases connected in series
    Output voltage = 28.5V with new Duracell 9V batteries
    Price:  $20     SiPM - Power Supply

Boradband Amplifiers (Minicircuits suggested in ON Semiconductor document)
- https://www.minicircuits.com/WebStore/dashboard.html?model=ZX60-43-S%2B
    ZX60−43-S+
    Connector Type: SMA
    Price: $140     Broadband Amplifiers
- 5 V rechargeable Lithium battery, with USB connector
    Price:  $30     Broadband Amplifiers - Power Supply

Pulse Stretcher
- https://www.digikey.com/en/datasheets/linear-technologyanalog-devices/linear-technologyanalog-devices-45780
    Datasheet, Pulse Stretcher circuit on page 23
- http://www.proto-advantage.com/store/
    Ordered 2x of SC70-6  LTC6752ISC6-4#TRMPBFCT-ND   Assembled with 0.65 pins
    Price:  $30     Pulse Stretcher - Proto-Advantage order
- https://www.amazon.com/gp/product/B01LYN4J3B/ref=ox_sc_act_title_1?smid=A3IRH1M32QHQ71&psc=1
    Raspberry Pi T-Type GPIO Expander
    Price:  $13     Pulse Stretcher - T-Type GPIO Expander
- https://www.amazon.com/gp/product/B016D5LB8U/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1
    Kuman Solderless Breadboard 830 MB-102 Tie Points, Jump Wires 65pcs, 
    3.3V 5V Power Supply Module, Electronic Learning Kit Compatible with Arduino K3
    Price:   $8     Pulse Stretcher - Power Supply

Raspberry Pi Computers & Accessories
- https://www.amazon.com/gp/product/B07V5JTMV9/ref=ppx_yo_dt_b_asin_title_o00_s00?ie=UTF8&psc=1
    CanaKit Raspberry Pi 4 4GB Starter Kit - 4GB RAM
    Price: $100     Raspberry Pi - 4 Starter Kit
- https://www.amazon.com/gp/product/B01C6Q2GSY/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1
    CanaKit Raspberry Pi 3 Complete Starter Kit - 32 GB Edition
    Price:  $75     Raspberry Pi - 3 Starter Kit
- https://www.amazon.com/gp/product/B007OYAVLI/ref=ppx_yo_dt_b_asin_title_o03_s00?ie=UTF8&psc=1
    AYL Mini Speaker System, Portable Plug in Speaker with 3.5mm Aux Audio Input, 
    External Speaker for Laptop Computer, MP3 Player, iPhone, iPad, Cell Phone (Black)
    Price:  $15     Raspberry Pi - Speaker

Translation Rail and Stepper Motor Controller
- https://www.amazon.com/gp/product/B07DC42DLW/ref=ppx_yo_dt_b_asin_title_o01_s01?ie=UTF8&psc=1
    100mm Travel Length Linear Rail Guide Ballscrew Sfu1605 
    DIY CNC Router Parts X Y Z Linear Stage Actuator with NEMA17 Stepper Motor
    Price:  $75     Translation Rail
- https://www.pololu.com/product/3130
    Tic T825 USB Multi-Interface Stepper Motor Controller (Connectors Soldered)
    Price:  $32     Translation Rail - Stepper Motor Controller
- https://www.amazon.com/gp/product/B07KB6HKS1/ref=ppx_yo_dt_b_asin_title_o00_s02?ie=UTF8&psc=1
    20V 4.5A AC Adapter Laptop Charger
    Price:  $12     Translation Rail - Power Supply

Laser Pointer
- https://www.apinex.com/ret2/JLPS-20B.html
    JLPS-20B : Green Laser Pointer with ON/OFF switch
    Price:  $35     Laser Pointer

Neutral Density Filters
- https://www.edmundoptics.com/p/schott-ng9-40-od-50mm-sq-absorptive-nd-filter/42610/
    SCHOTT NG9, 4.0 OD, 50mm Sq., Absorptive ND Filter
    Price:  $80     Neutral Density Filter - 4.0 OD
- https://www.edmundoptics.com/p/schott-ng9-36-od-50mm-sq-absorptive-nd-filter/42609/
    SCHOTT NG9, 3.6 OD, 50mm Sq., Absorptive ND Filter
    Price:  $80     Neutral Density Filter - 3.6 OD

Double Slit
- https://www.amazon.com/gp/product/B00KWZ5WQ0/ref=ox_sc_act_title_1?smid=ATVPDKIKX0DER&psc=1
    Diaphragm with 4 Double Slits of Different Spacings
    Price:  $22     Double Slit Slide

3D Printed Parts
- various parts to mount laser, SiPM sensor, double slit slide
    Price:  $50     3D Printed Parts


Summary of Prices
-----------------

    Price: $181     SiPM
    Price:  $20     SiPM - Power Supply
    Price: $140     Broadband Amplifiers
    Price:  $30     Broadband Amplifiers - Power Supply
    Price:  $30     Pulse Stretcher - Proto-Advantage order
    Price:  $13     Pulse Stretcher - T-Type GPIO Expander
    Price:   $8     Pulse Stretcher - Power Supply
    Price: $100     Raspberry Pi - 4 Starter Kit
    Price:  $75     Raspberry Pi - 3 Starter Kit
    Price:  $15     Raspberry Pi - Speaker
    Price:  $75     Translation Rail
    Price:  $32     Translation Rail - Stepper Motor Controller
    Price:  $12     Translation Rail - Power Supply
    Price:  $35     Laser Pointer
    Price:  $80     Neutral Density Filter - 4.0 OD
    Price:  $80     Neutral Density Filter - 3.6 OD
    Price:  $22     Double Slit Slide
    Price:  $50     3D Printed Parts

    Total  $998

