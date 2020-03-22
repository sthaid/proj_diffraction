=====================
OVERVIEW
=====================

This program calculates the diffraction pattern that results from
monochromatic light passing through one or more slits and illuminating
a screen.

The distance-to-screen, wavelength, and slit geometry are defined in 
the 'dsparam' file which provides the input parameters to this program.
A sample 'dsparam' file is included in this repository.

Refer to sample output in screenshot.jpg.

=====================
BUILDING THE SOFTWARE
=====================

To build: install the required packages and run 'make'.

The following packages are required:
* SDL2-devel
* SDL2_ttf-devel
* SDL2_mixer-devel
* libpng-devel
* libjpeg-turbo-devel

=====================
DESIGN
=====================

The program calculates the diffraction pattern on the screen by assuming that every point 
along each slit emits a circular wave. The amplitude of this circular wave is
calculated for each point along the screen. This process is repeated for each of
the points of every slit. The amplitude computed on the screen for each 
point along the slits is summed to determine the total amplitude, which is
stored in array 'screen1_amp'.

The above is repeated using circular waves that are 90 degrees out of phase
compared to the above. This yields a different amplitude profile, stored in
array 'screen2_amp'

Finally the screen intensity is determined as 

    screen_inten[idx] = sqrt( screen1_amp[idx]^2 + screen2_amp[idx}^2 )

The above approach was inspired by 
https://en.wikipedia.org/wiki/Huygens%E2%80%93Fresnel_principle.
However, simplifying assumptions are used in this software.

=====================
TESTING     
=====================

Tested by confirming that the Distance Between Adjacent Fringes predicted
by this software agrees with this equation:

                                     DistanceToScreen * Wavelength
    DistanceBetweenAdjacentFringes = -----------------------------
                                      DistanceBetweenSLitCenters


=====================
REFERENCES
=====================

* https://en.wikipedia.org/wiki/Huygens%E2%80%93Fresnel_principle
* https://courses.lumenlearning.com/physics/chapter/27-3-youngs-double-slit-experiment/

