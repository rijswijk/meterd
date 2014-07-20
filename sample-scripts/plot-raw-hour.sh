#!/bin/sh
gnuplot <<EOF
set terminal svg size 800,600
set output '/var/www/img/raw-hour.svg'
set title "One hour actual consumption/production"
set xlabel "Time (UTC)"
set ylabel "Power in kW"
set xdata time
set timefmt "%s"
set xtics 300
set format x "%H:%M"
`cat /var/meterd/raw.hour.ranges`
plot "/var/meterd/raw.hour" using 1:2 title "consumption" with lines, \
     "/var/meterd/raw.hour" using 1:3 title "production" with lines
EOF
