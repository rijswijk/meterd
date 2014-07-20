#!/bin/sh
gnuplot <<EOF
set terminal svg size 800,600
set output '/var/www/img/consumed-week.svg'
set title "One week consumption/production"
set xlabel "Time (UTC)"
set ylabel "Consumption/production in kWh"
set xdata time
set timefmt "%s"
set xtics 86400
set format x "%d/%m"
`cat /var/meterd/consumed.week.ranges`
plot "/var/meterd/consumed.week" using 1:2 title "consumption" with lines
EOF
