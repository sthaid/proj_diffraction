The purpose of the program in this directory is to determine how long
a time interval is needed to determine the mean rate of time independent
events. Refer to "Poisson Distribution".

This program simulates the generation of time indepenent events at a
rate of 20000 per second. And calculates an observed mean rate for 
an ever increasing time interval.

For example, given the true rate of events is 20000/sec, and you count
the number of events over a 0.1 second interval you might count 
2032 events, and thus conclude the true rate is approxiamately 20320/sec.

Observing events for 1 second, you might conclude the approximate 
true rate is 20074/sec.

And observing for 10 seconds, you might conclude the approximate 
true rate is 20002.3/sec.

--- output of the rate program ---

time =  0.100   observed_rate = 20320.000   percent_deviation = 1.600
time =  0.200   observed_rate = 20130.000   percent_deviation = 0.650
time =  0.300   observed_rate = 20006.667   percent_deviation = 0.033
time =  0.400   observed_rate = 20082.500   percent_deviation = 0.413
time =  0.500   observed_rate = 20142.000   percent_deviation = 0.710
time =  0.600   observed_rate = 20065.000   percent_deviation = 0.325
time =  0.700   observed_rate = 20034.286   percent_deviation = 0.171
time =  0.800   observed_rate = 20136.250   percent_deviation = 0.681
time =  0.900   observed_rate = 20144.444   percent_deviation = 0.722
time =  1.000   observed_rate = 20074.000   percent_deviation = 0.370
time =  1.100   observed_rate = 20114.545   percent_deviation = 0.573
time =  1.200   observed_rate = 20029.167   percent_deviation = 0.146
time =  1.300   observed_rate = 20071.538   percent_deviation = 0.358
time =  1.400   observed_rate = 20052.857   percent_deviation = 0.264
   ....
time = 10.000   observed_rate = 20002.300   percent_deviation = 0.011

