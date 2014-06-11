
##Smart Meter Monitoring Daemon (meterd)

Copyright (c) 2014 Roland van Rijswijk-Deij

All rights reserved. This software is distributed under a BSD-style
license. For more information, see LICENSE.

###1. Introduction

The Smart Meter Monitoring Daemon - or 'meterd' for short - allows you to
monitoring your current electricity consumption, to graph it using a variety
of predefined as well as custom methods and to keep track of your overall
energy expenditure over time. If you own your own means of producing
electricity, such as photovoltaic (PV) panels or a wind turbine, meterd also
allows you to monitor how much electricity you feed back in to the grid.

Meterd is compatible with all smart meters that have a so-called 'P1' port
that meets the Dutch Smart Meter Requirements (DSMR) 3.0 or up. Note that at
present the software has only been tested with a limited number of smart
meters; for more information see below under 'Testing' and 'Known Working'.

###2. Prerequisites

To build meterd you will need to have the following software installed:

 - libconfig (>= 1.3.2), [ C/C++ Configuration File Library ](http://www.hyperrealm.com/libconfig/)
 - SQLite (>= 3.7.0), [ A self-contained in-process database ](http://www.sqlite.org)

###3. Building

To build meterd, first clone the meterd repository:
```
$ git clone https://github.com/rijswijk/meterd
```
Then, in order to build meterd, execute the following commands:
```
$ cd meterd
$ ./autogen.sh
$ ./configure
$ make
```

###4. Installing 

To install meterd as a regular user, run:
```
$ sudo make install
```
If you are root (administrative user), run:
```
# make install
```

###5. Configuration

TBD

###6. Testing

TBD

###7. Known Working

TBD

###8. Contact

Questions/remarks/suggestions/praise on this tool can be sent to:

Roland van Rijswijk-Deij 	<roland@mazuki.nl>
