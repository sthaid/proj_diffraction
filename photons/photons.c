#include <stdio.h>
#include <math.h>

//
// REFERENCES
// ----------
// https://en.wikipedia.org/wiki/Laser_pointer
// - Laser pointers are Class II or Class IIIa devices, with output beam power 
//   less than 5 milliwatts (<5 mW). According to U.S. Food and Drug Administration 
//   (FDA) regulations, more powerful lasers may not be sold or promoted as laser 
//   pointers. Also, any laser with class higher than IIIa (more than 5 milliwatts) 
//   requires a key-switch interlock and other safety features.[
//
// https://www.edmundoptics.com/knowledge-center/application-notes/optics/understanding-neutral-density-filters/
//
// NOTES 
// -----
//                          Detector (*)
// Color    Wavelength      Effeciency
// RED      635e-9          0.10
// GREEN    520e-9          0.22
// BLUE     445e-9          0.33
// (*) effeciency is for overvoltage=3.5v;
//     based on 3 9v batteries providing 28v, 
//     which is 3.5v greater than Vbr 24.5
//

const double h                   = 6.626e-34;      // planck constant
const double c                   = 2.998e8;        // speed of light  m/sec
const double c_ft_per_ns         = 0.983571;       // speed of light ft/ns

const double wavelength          = 520e-9;         // 520 nm, Green
const double laser_power         = 5e-3;           // 5 milliwatt output beam power  XXX output power <5mW
const double double_slit_pass    = 0.20;           // fraction of laser photons which pass through double slit
                                                   // - based on 1.6mm dia beam and using the slide b=.15 g=.50
const double pattern_area        = (38e-3 * 2e-3); // 38mm x 2mm  
                                                   // - estimated using slide b=.15 g=.50
const double detector_area       = (1e-3 * 1e-3);  // 1mm x 1mm
const double detector_effeciency = 0.22;            

// -----------------------------------------------------------------

int main(int argc, char **argv)
{
    double photon_rate_at_laser;
    double photon_rate_after_filter; 
    double photon_rate_after_double_slit;
    double detector_count_rate; 
    double filter_od;
    double photon_seperation_after_filter;

    printf("wavelength          = %.0f nanometers\n",           wavelength * 1e9);
    printf("laser_power         = %.4f watts (joules/sec)\n",   laser_power);
    printf("double_slit_pass    = %.0f percent\n",              double_slit_pass * 100);
    printf("pattern_area        = %.1f mm^2\n",                 pattern_area * 1e6);
    printf("detector_area       = %.1f mm^2\n",                 detector_area * 1e6);
    printf("detector_effeciency = %.1f percent\n",              detector_effeciency * 100);
    printf("\n");

    printf("      Rate                  Rate       Rate   Detector Seperation\n");
    printf("   Leaving     Filter      After      After      Count      After\n");
    printf("     Laser         OD     Filter    DblSlit     Rate K     Filter\n");
    printf("---------- ---------- ---------- ---------- ---------- ----------\n");

    for (filter_od = 6; filter_od <= 8; filter_od += .1) {
        photon_rate_at_laser          = laser_power * wavelength / (h * c);
        photon_rate_after_filter      = photon_rate_at_laser * pow(10, -filter_od);
        photon_rate_after_double_slit = photon_rate_after_filter * double_slit_pass;
        detector_count_rate           = photon_rate_after_double_slit * 
                                        (detector_area / pattern_area) * 
                                        detector_effeciency;

        photon_seperation_after_filter = (1e9 / photon_rate_after_filter) * c_ft_per_ns;

        printf("%10.1e %10.1f %10.1e %10.1e %9.0fK %8.2fft\n",
               photon_rate_at_laser,
               filter_od,
               photon_rate_after_filter,
               photon_rate_after_double_slit,
               detector_count_rate/1000,
               photon_seperation_after_filter);
    }

    return 0;
}
