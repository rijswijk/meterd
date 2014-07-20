#!/bin/sh
gnuplot <<EOF
set terminal svg size 800,600
set output '/var/www/img/raw-week.svg'
set title "One week actual consumption/production"
set xlabel "Time (UTC)"
set ylabel "Power in kW"
set xdata time
set timefmt "%s"
set xtics 86400
set format x "%d/%m"
`cat /var/meterd/raw.week.ranges`
plot "/var/meterd/raw.week" using 1:2 title "consumption" with lines, \
     "/var/meterd/raw.week" using 1:3 title "production" with lines
EOF
