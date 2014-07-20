#!/bin/sh
gnuplot <<EOF
set terminal svg size 800,600
set output '/var/www/img/gas-week.svg'
set title "One week gas consumption"
set xlabel "Time (UTC)"
set ylabel "Consumption in m3"
set xdata time
set timefmt "%s"
set xtics 86400
set format x "%d/%m"
`cat /var/meterd/gas.week.ranges`
plot "/var/meterd/gas.week" using 1:2 title "consumption" with lines
EOF
