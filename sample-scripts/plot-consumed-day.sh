#!/bin/sh
gnuplot <<EOF
set terminal svg size 800,600
set output '/var/www/img/consumed-day.svg'
set title "One day consumption/production"
set xlabel "Time (UTC)"
set ylabel "Consumption/production in kWh"
set xdata time
set timefmt "%s"
set xtics 3600
set format x "%H"
`cat /var/meterd/consumed.day.ranges`
plot "/var/meterd/consumed.day" using 1:2 title "consumption" with lines
EOF
