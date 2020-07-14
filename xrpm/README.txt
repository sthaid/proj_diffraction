==================================
OVERVIEW
==================================

The purpose of the software in this directory is to position the
SiPM sensor using the Translation Rail, in 0.1 mm increments; and
to capture the SiPM count rate at each position. Thus obtaining data 
that can later be plotted to make a graph of the diffraction pattern.

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
with electrical tape covering all LED light sources. Fortunately I live in a rural area
and there is not much traffic or other exterior sources of light. The main problem with
this approach (for me) is finding the motivation to stay up late, especially in the 
Summer months.

Summary of Results
------------------

A diffraction pattern was observed, and the diffraction pattern obtained corresponds
very closely to the diffraction pattern computed by the ifsim program. I had not expected
such a close match between the measured and predicted diffraction patterns.

The detailed results are included in the xrpm/results directories.

The results/compare_data_with_ifsim.jpg file shows the measured diffraction pattern
(graph on the left) with the diffraction pattern computed by the ifsim program (the graph
on the right), so that these two graphs can easily be compared.

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

