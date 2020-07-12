=======================
THE RESULTS ...
=======================

ctlr_screenshot_07-03_23-53-06.jpg:  Screen shot taken of the ctlr program while capturing data.

gnuplot_2020_07_03_23_47_31.dat:     The raw photon detection rate vs sensor position data, 
                                     obtained by the ctlr program.

gnuplot_2020_07_03_23_47_31.jpg:     Photon rate vs sensor postion data plotted using gnuplot.
                                     The script used to invoke gnuplot is in bin/run_gnuplot.
                                     The gnuplot program generated this graph from the data in
                                     the gnuplot_2020_07_03_23_47_31.dat file.

ifsim_screenshot.jpg:                Simulation model of the same double slit configuration as 
                                     captured in gnuplot_2020_07_03_23_47_31.dat. This simulation 
                                     model program is in the proj_diffraction/ifsim directory.
                                     This simulation takes into account the size of the detector,
                                     and is intended to be an accurate simulation.

ds_screenshot.jpg:                   Simulation of the same double slit configuration captured
                                     in the gnuplot_2020_07_03_23_47_31.dat. This simulation
                                     model program is in the proj_diffraction/ds directory. 
                                     This simulation is not as good as the ifsim simulation, 
                                     because this simulation assumes an ideal detector of 
                                     close to zero size.

compare_data_with_ifsim.jpg:         MOST IMPORTANT RESULT FILE
                                     A side by side comparison of the measured data and the 
                                     ifsim simulation model; this file contains a comparison of files:
                                     - gnuplot_2020_07_03_23_47_31.jpg
                                     - ifsim_screenshot.jpg
                                     I have adjusted the magnitude of the central pulse to be the 
                                     same in these two graphs. However, the close agreement between 
                                     the measured data and the simulation model exceeded my 
                                     expectations. The measured data is the left graph (purple); and 
                                     the simulation model is the right graph (black).

bin/create_compare_data_with_ifsim:  This script is used to create the compare_data_with_ifsim.jpg 
                                     file. The proj_jpeg_merge/image merge program was used to 
                                     combine the 2 jpg images into this one.

bin/run_gnuplot:                     Used to run the gnuplot program (from package gnuplot), to
                                     convert the raw sensor position vs count rate to a graph.


