#include <stdio.h>
#include <math.h>

// References:
//
// https://en.wikipedia.org/wiki/Laser_pointer
// - Laser pointers are Class II or Class IIIa devices, with output beam power 
//   less than 5 milliwatts (<5 mW). According to U.S. Food and Drug Administration 
//   (FDA) regulations, more powerful lasers may not be sold or promoted as laser 
//   pointers. Also, any laser with class higher than IIIa (more than 5 milliwatts) 
//   requires a key-switch interlock and other safety features.[
//
// https://www.edmundoptics.com/knowledge-center/application-notes/optics/understanding-neutral-density-filters/
//

#define RED    0
#define GREEN  1
#define BLUE   2

struct {
    char *name;
    double wavelength;
    double detector_effeciency;
} color_tbl[3] = {
    { "RED",   635e-9,   0.13 },  // effeciency is for overvoltage=5.0v
    { "GREEN", 520e-9,   0.27 },
    { "BLUE",  445e-9,   0.40 },
                    };

const double h                = 6.626e-34;          // planck constant
const double c                = 2.998e8;            // speed of light
const double detector_area    = (.1e-3 * 1e-3);     // .1mm x 1mm
const double pattern_area     = (100e-3 * 10e-3);   // 100mm x 10mm
const double laser_power      = 5e-3;               // 5 milliwatt output beam power
const double filter_od        = 7;                  // filter optical density

void calculate_detector_count_rate(int color);

// -----------------------------------------------------------------

int main(int argc, char **argv)
{
    calculate_detector_count_rate(RED);
    calculate_detector_count_rate(GREEN);
    calculate_detector_count_rate(BLUE);

    return 0;
}

void calculate_detector_count_rate(int color)
{
    char *name;
    double wavelength, detector_effeciency;
    double photon_rate_before_filter, photon_rate_after_filter;
    double detector_count_rate, intvl_ns;

    name = color_tbl[color].name;
    wavelength = color_tbl[color].wavelength;
    detector_effeciency = color_tbl[color].detector_effeciency;

    photon_rate_before_filter = laser_power * wavelength / (h * c);
    photon_rate_after_filter = photon_rate_before_filter * pow(10, -filter_od);
    detector_count_rate = photon_rate_after_filter * (detector_area / pattern_area) * detector_effeciency;
    intvl_ns = 1 / photon_rate_after_filter * 1e9;

    // notes:
    // - intvl_ns is the average interval between photons after the filter
    // - an intvl_ns of 1 would mean the photons are on average seperated by 1 foot

    printf("%8s : detctor_count_rate=%.0f  intvl_ns=%0.2f  (%e %e)\n", 
          name, 
          detector_count_rate, 
          intvl_ns,
          photon_rate_before_filter, 
          photon_rate_after_filter);
}
