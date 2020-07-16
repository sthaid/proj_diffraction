### Overview

This project demonstrates single photon quantum interference, using a double slit.

The sub-directories are described below. The most important are xrpm, xrpm/results,
ifsim, and apparatus. Each sub-directory contains a README.

### Directories

* apparatus: Pictures of the system, and a list of the major components.
* ds: Simulation of photons passing through one or two slits, simulating the 
diffraction pattern on a screen. This is a 2D simulation. The ifsim directory
contains a better program for this.
* gpio: Evaluation of different techniques of reading Raspberry PI GPIO registers. A simple
direct register access API provided the best result of 64 million reads per second.
* ifsim: 3D simulation of photons traveling from a source, through beam-splitters and
mirrors and displaying simulated intensity on a screen. The position and orientation of the
mirrors and beam-splitters can be adjusted to learn what is involved in setting up
an interferometer. This program can also be configured to simulate a double-slit source
and a screen.
* photons: This program calculates the detector count rate and interval between photons 
based on factors; which include laser power, filter optical density, the percentage
of photons that pass through a double slit, pattern area, and detector area.
* poisson: The Silicon Photomultiplier sensor had a dark count rate. The
intent of this program is to determine how long the sensor will need to be sampled
for the dark count rate to stabilize enough so that a small photon count rate can be detected.
* pololu: A Linear Rail Actuator, controlled by the Pololu TIC T825 stepper motor controller,
is used to position the SiPM. The software in this directory is used to test the
stepper motor and linear rail actuator.
* util: general support routines
* xrpm: This directory contains the main software for this project. This software is used
to position the SiPM, using the Translation Rail, and collect photon count rates at each
position. The data obtained is later plotted, and shows the double slit diffraction pattern
even though there is usually just one photon in the system at a time.
