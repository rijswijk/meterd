#!/bin/sh
gnuplot <<EOF
set terminal svg size 800,600
set output '/var/www/img/raw-day.svg'
set title "One day actual consumption/production"
set xlabel "Time (UTC)"
set ylabel "Power in kW"
set xdata time
set timefmt "%s"
set xtics 3600
set format x "%H"
`cat /var/meterd/raw.day.ranges`
plot "/var/meterd/raw.day" using 1:2 title "consumption" with lines, \
     "/var/meterd/raw.day" using 1:3 title "production" with lines
EOF
