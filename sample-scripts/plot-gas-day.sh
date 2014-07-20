#!/bin/sh
gnuplot <<EOF
set terminal svg size 800,600
set output '/var/www/img/gas-day.svg'
set title "One day gas consumption"
set xlabel "Time (UTC)"
set ylabel "Consumption in m3"
set xdata time
set timefmt "%s"
set xtics 3600
set format x "%H"
`cat /var/meterd/gas.day.ranges`
plot "/var/meterd/gas.day" using 1:2 title "consumption" with lines
EOF
