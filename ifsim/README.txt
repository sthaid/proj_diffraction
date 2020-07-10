==============================
OVERVIEW
==============================

This program calculates the diffraction pattern from various optical
systems that are defined by the user in a configurtion file.

The optical elements supported include:
- Sources: SingleSlit, DoubleSlit, RoundHole
- Mirror
- Beam Splitter
- Beam Blocker
- Detection Screen

This program simulates the diffraction pattern by generating random photons
from the Source and tracing their path through the system to the Screen.
This is repeated for many photons, and the user can observe the diffraction
pattern as it gradually appears on the Screen.

The optical element positions and the photon ray tracing is performed in
three dimensions.

Program command line: 
  $ ifsim [-x] [config_file]

  -x: swap white and black colors, a white background is better for printing
  config_file: defines one or more optical systems, the default config_file
               is ifsim.config which is provided

==============================
PANES & CONTROLS
==============================

Global Controls
- Ctrl-P or Alt-P:  print screen 
- Alt-Q:            exit program

There are 4 display panes:
- Interferometer_diagram_pane: located on the left.
  This pane displays a diagram of the optical system, and the photon
  ray tracing. A major function of this pane is to simulate the adjustment
  of interferometer components to create a fringe pattern.
- Interference_pattern_graph_pane: located on the left, and initially obscured.
  This pane displays the sensor intensity graph in a horizontal format, similar
  to the ds program's format. The purpose of this display format is to facilitate
  comparison between the measured diffraction pattern, the diffraction pattern
  calculated by this program, and the diffraction pattern calculated by the 
  ds program.
- Interference_pattern_pane:  located top right.
  The top 3/4 of this pane shows the buildup of photons on the detector Screen.
  The bottom 1/4 of this pane is a graph of what a sensor, which is located 
  halfway down the screen, would detect as it traverses the screen. The size of the
  sensor is adjustable. As the sensor width is increased the sensor graph loses 
  resolution.
- Control_pane: located bottom right.
  This pane is used to select a system configuration that is defined in the 
  config file. The default ifsim.config file is supplied.

The pane controls are described in the following ...

Interferometer_diagram_pane general controls:
- Mouse Wheel:         Zoom
- Mouse Click & Drag:  Pan
- RESET_PAN_ZOOM:      Resets Pan/Zoom
- STOP / RUN:          Stops and runs the simulation
- Mouse Right CLick:   Puts this pane in background, so that the 
                       Interference_pattern_graph_pane will become visible

Interferometer_diagram_pane optical element adjustment controls:
- Mouse Click:         Selects an optical element. Once selected the element
                       is displayed in flashing orange. Either the element or 
                       the table can be clicked on.
- '<ESC>':             Deselect optical element
- RANDOMIZE:           Randomize all optical elements
- 'R':                 Randomize the location and orientation of the selected element
- RESET_RANDOM:        Resets all of the optical element randomizations
- 'r':                 Resets the randomization of the selected element
- INSERT/HOME:         Adjust pan orientation of the selected element, 
                       combine with Ctrl for fine adjustment
- PGUP/PGDN:           Adjust tilt orientation of the selected element, 
                       combine with Ctrl for fine adjustment
- ARRORS:              Adjust position (x,y only) of the selected element,
                       combine with Ctrl for fine adjustment
- 0 .. 9:              Adjust selected element bit flags, where (0 refers to bit0)
                       - bit0 for source elements = BEAMFINDER (source becomes a point)
                       - bit0 for mirror element  = DISCARD (mirror does not reflect)

Interference_pattern_graph_pane controls:
- Mouse Wheel:         Change scale
- Mouse Right CLick:   Puts this pane in background, so that the 
                       Interference_diagram_graph_pane will become visible

Interference_pattern_pane_hndlr controls:
- ALG:                 Used to choose an algorithm for mapping the calculated Screen
                       intensity to display pixel brightness:
                         0:  sqrt
                         1:  logarithmic
                         2:  linear
- W=N.N  or  H=N.N:    Position mouse over these and then use the mouse wheel to 
                       adjust the sensor's width and height.

Control_pane_hndlr controls:
- Mouse Click:         Selects one of the opticial systems that is defined in the
                       config file.

==============================
CONFIG FILE FORMAT
==============================

Refer to the included config file: ifsim.config.

General notes on the config file format:
- Blank lines are ignored.
- Lines begining with '#' are comment lines.
- The first optical element must be a source element.
- There can be just one source element and one screen element.
- A 'next' param value of -1 discards the photon.
- Units of distance are millimeters, and angles are degrees.
- The Interferometer_diagram_pane shows a top view of the Optical System.
  The Z axes is not depicted.

Format of an Optical System definition:
  <OpticalSystemName> <WavelenghInMillimeters>
  0 <element_type> <param1> <param2> ...
  1 <element_type> <param1> <param2> ...
     ...
  N <element_type> <param1> <param2> ...
  .

The supported <element_type> are:
  src_ss:   single slit source
  src_ds:   double slit source
  src_rh:   round hole source
  mirror:   mirror
  beam_spl: beam splitter
  screen:   detection screen
  discard:  opaque

Examples of optical element definitions
  0 src_ss   ctr=0,0,0 nrml=0,1,0 w=0.1 h=1.6 wspread=2.00 hspread=0.01 next=1
  0 src_ds   ctr=0,0,0 nrml=1,0,0 w=0.15 h=1.6 wspread=2.00 hspread=0.01 ctrsep=0.50 next=1
  0 src_rh   ctr=0,0,0 nrml=0,1,0 diam=0.1 spread=2.00 next=1
  2 mirror   ctr=-100,100,0 nrml=1,0,0 next=1
  1 beam_spl ctr=100,0,0 nrml=-1,1,0 next1=-1 next2=-1 next3=2 next4=3
  6 screen   ctr=1900,100,0 nrml=-1,0,0
  3 discard  ctr=1100,0,0 nrml=-1,0,0

  All elements contain params 'ctr' and 'nrml', which define the location (ctr) and
  orientation normal vector (nrml). It is often convenient to define the location of
  the source element as 0,0,0; but this is not a requirement. The magnitude of the
  orientation normal vector (nrml) is arbitrary and is not required to be 1.

  Additional parameters for the elements are described below:

  src_ss: Simulated photons originate at a random location within the slit's
          width and height; with a direction that is normal to the slit plus a random spread.
  - w:       width of the slit (mm)
  - h:       height of the slit (mm), this should usually be the diameter of the laser beam
  - wspread: should be set such that the beam's spread encompasses the entire 
             detector screen; setting larger wspread will increase the program's run time;
             for example: 
              . 50 = wspread / 57.3 * 1800;
              . solving for wspread yields 1.59 degrees
              . a wspread value of 1.59 degrees will cover a 50mm screen at 1800mm distance
  - hspread: this should usually be the spread of the laser beam
  - next:    which optical element the beam will impact next

  src_ds: Double slit source, this is similar to the single slit source described 
          above, except for the addition of the ctrsep param.
  - ctrsep: distance between the centers of the two slits.

  src_rh: Round hole source, also similar to the single slit.

  mirror: The mirror element assumes the beam always proceeds to the same element
          after departing the mirror.
  - next: which optical element the beam will impact next

  beam_spl: The beam splitter will either reflect or transmit the photon with
            equal probability. The four next parameters define which 
            optical element the photon will impact next based upon the direction
            that the photon is departing the beam splitter. A next value of -1
            will discard the photon.
  - next1: Assuming the nrml vector is a positive X axis, this defines 4 quadrants.
           If the photon is leaving the beam splitter via quadrant 1 then next1 
           defines the next optical element.
  - next2: If photon is leaving via quadrant 2 then the next2 element is next.
  - next3, next4: continue this pattern

=============================================
APPENDIX - ALIGNING THE MICHELSON INTERFEROMETER
=============================================

turn on beamfinder

center the dots
- use laser to center 2 dots as best as possible on the screen
- then mirror to put dots on top of each other
- and back to laser to re-center on screen

turn off beamfinder

make fringes
- use mirror pan adjust until fringes are seen
- maybe also use tilt adjust to make the fringes vertical

NOTE - the x/y positionof he mirrors doesn't seem to matter very much

=============================================
APPENDIX - ALIGNING THE MACH ZEHNDER INTERFEROMETER
=============================================

turn on beamfinder

center the dots
- use laser to center 2 dots as best as possible on the screen
- then mirror to put dots on top of each other
- and back to laser to re-center on screen

turn off beamfinder

make fringes
- use mirror x/y adjust until fringes are seen
- use mirror tilt to make the fringes vertical

