### Overview

UNDER CONSTRUCTION

In this project I hope to demonstrate single photon quantum interference, 
and quantum eraser.

### Directories

* ds: Simulation of photons passing through one or two slits, showing the 
diffraction pattern on a screen. This is a 2D simulation. The ifsim directory
contains a better program for this
* gpio: Evaluation of different techniques of reading Raspberry PI GPIO registers. A simple
direct register access API provided the best result of 64 million reads per second.
* ifsim: 3D Simulation of photons travelling from a source, through beam-splitters and
mirrors and showing the intensity on the screen. The position and orientation of the
mirrors and beam-splitters can be adjusted to learn what is involved in setting up
an interferometer.
* photons: This program calculates the detector count rate and interval between photons 
based on various factors; including laser power, filter optical density, the percentage
of photons that pass through a double slit, pattern area, and detector area.
* poisson: The Silicon Photo Multiplier sensor I plan to use has a dark count rate. The
intent of this program is to determine how long the sensor will have to remain in one
position for the dark count rate to stabilize enough to detect a smaller photon count rate.
* pololu: I will be using the Pololu TIC T825 stepper motor controller to position the
SiPM using a Linear Rail Actuator. The software in this directory is a test of the
stepper motor and linear rail actuator.
* util: utilities used by programs in the other directories

