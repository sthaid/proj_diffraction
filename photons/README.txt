===============================================
TITLE: PHOTONS PROGRAM
===============================================

The purpose of the photons program is to calculate the detector
count rate based on the Optical Density (OD) value of the 
Neutral Density Filter. These results help decide the ND Filter
OD value to purchase.

System Diagram:

                                                            Pattern
                                                              |
                                                       /      |
                            -----------    ---------  /       |
      -----------------     | Neutral |    | Double |/        | ------------
      | Laser Pointer |---->| Density |--->| Slit   |- ... -> | | Detector |
      -----------------     | Filter  |    ----------\        | ------------
                            -----------               \       |
                                                       \      |
                                                              |

The photons/photons.c program generated the attached below.

Description of columns:
- RateLeavingLaser: The rate of the photons leaving the laser. This is
  based on 5 milliwatt laser output (laser pointers have output < 5 mW).
- FilterOD: The Filter OD is varied from 6.0 to 8.0.
- RateAfterFilter: This equals (RateLeavingLaster * 10 ^ -FilterOD)
- RateAfterDblSlit: For the double slit I plan to use, I've estimated that
  20 percent of the photons pass through the double slit. The other 80% are
  absorbed or reflected by the opaque material surrounding the slits.
- DetectorCountRate: This is the RateAfterDblSlit adjusted by 2 factors:
  (1) the area of the detector divided by the area that the diffraction 
      pattern encompasses; this is rough estimate, and does not take into
      account that the intensity of the diffraction pattern varies;
      value used = (1 * 1) / (38 * 2) = .013
  (2) the detector effeciency, this is estimated from information contained
      in the SiPM datasheet;
      value used = .22
- SeperationAfterDblSlit: This is the average distance between photons, 
  assuming a large apparatus size.

The major uncertainties are:
- Laser power: is required to be less than 5 mV. My green laser appears
  very bright, so I guess that it's output is near 5 mW.
- Area of Diffraction Pattern: 
- Detector Effeciency: This is based on the manufacturer's datasheet. But
  is influenced by how well the pulse stretcher circuit works, as well as
  how well the Raspberry Pi software that reads the GPIO works.

Based on the program output shown below, I chose OD 7.6;  and purchased 
2 neutral densisty filters with OD=4 and OD=3.6.

--- Photons Program Output Follows ---

wavelength          = 520 nanometers
laser_power         = 0.0050 watts (joules/sec)
double_slit_pass    = 20 percent
pattern_area        = 76.0 mm^2
detector_area       = 1.0 mm^2
detector_effeciency = 22.0 percent

      Rate                  Rate       Rate   Detector Seperation
   Leaving     Filter      After      After      Count      After
     Laser         OD     Filter    DblSlit     Rate K    DblSlit
---------- ---------- ---------- ---------- ---------- ----------
   1.3e+16        6.0    1.3e+10    2.6e+09      7578K     0.38ft
   1.3e+16        6.1    1.0e+10    2.1e+09      6019K     0.47ft
   1.3e+16        6.2    8.3e+09    1.7e+09      4781K     0.60ft
   1.3e+16        6.3    6.6e+09    1.3e+09      3798K     0.75ft
   1.3e+16        6.4    5.2e+09    1.0e+09      3017K     0.94ft
   1.3e+16        6.5    4.1e+09    8.3e+08      2396K     1.19ft
   1.3e+16        6.6    3.3e+09    6.6e+08      1903K     1.50ft
   1.3e+16        6.7    2.6e+09    5.2e+08      1512K     1.88ft
   1.3e+16        6.8    2.1e+09    4.1e+08      1201K     2.37ft
   1.3e+16        6.9    1.6e+09    3.3e+08       954K     2.98ft
   1.3e+16        7.0    1.3e+09    2.6e+08       758K     3.76ft
   1.3e+16        7.1    1.0e+09    2.1e+08       602K     4.73ft
   1.3e+16        7.2    8.3e+08    1.7e+08       478K     5.96ft
   1.3e+16        7.3    6.6e+08    1.3e+08       380K     7.50ft
   1.3e+16        7.4    5.2e+08    1.0e+08       302K     9.44ft
   1.3e+16        7.5    4.1e+08    8.3e+07       240K    11.88ft
   1.3e+16        7.6    3.3e+08    6.6e+07       190K    14.96ft     <====  4.0 + 3.6 OD
   1.3e+16        7.7    2.6e+08    5.2e+07       151K    18.83ft
   1.3e+16        7.8    2.1e+08    4.1e+07       120K    23.71ft
   1.3e+16        7.9    1.6e+08    3.3e+07        95K    29.85ft
   1.3e+16        8.0    1.3e+08    2.6e+07        76K    37.57ft

