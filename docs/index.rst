EPICS CaPutLog Module
=====================

Author: `Ben Franksen`_ (`HZB`_)


About
-----

CaPutLog is an EPICS support module that provides logging for Channel Access
put operations. The current version works with EPICS 3.14.8.2 and later and
is almost a complete re-write of the original (3.13 compatible) version that
was written by V. Korobov (DESY). It is based on the generic iocLogClient
that is part of the EPICS libCom. Starting a log server requires exactly the
same steps as for the regular iocLogServer, except that you probably want to
use a different port. On the IOC side there are three routines to be called
from the iocShell. See section `Setup`_ below for details.

This module will send logs of CA puts in a text format to a remote logging
server. Currently, supports two output formats, ``standard`` and ``JSON``.
The Json format is only supported with `EPICS`_ base versions 7.0.1+.


Download
--------

You can download a release from the `Github releases`_:: page or get the
latest development version via `Github`_::

   git clone https://github.com/epics-modules/caPutLog.git

You can also `browse`_ through the latest changes in the repo.

Compatibility matrix and release notes
++++++++++++++++++++++++++++++++++++++

+---------+------------------+------------------+
| Version | EPICS Release    | Release Notes    |
+=========+==================+==================+
|   4.0   | 3.14.12..7       | :ref:`R4-0`      |
+---------+------------------+------------------+
|   3.7   |                  |                  |
+---------+------------------+------------------+
|   3.6   |                  |                  |
+---------+------------------+------------------+
|   3.5   | 3.14.12..3.15    | :ref:`R3-5`      |
+---------+------------------+------------------+
|   3.4   |   3.14.12        | :ref:`R3-4`      |
+---------+------------------+------------------+
|  3.3.3  |   3.14.12        | :ref:`R3-3-3`    |
+---------+------------------+------------------+
|  3.3.2  |   3.14.12        | :ref:`R3-3-2`    |
+---------+------------------+------------------+
|  3.3.1  |   3.14.12        | :ref:`R3-3-1`    |
+---------+------------------+------------------+
|   3.3   |   3.14.12        | :ref:`R3-3`      |
+---------+------------------+------------------+
|   3.2   |   3.14.11        | :ref:`R3-2`      |
+---------+------------------+------------------+
|   3.1   |   3.14.8.2       | :ref:`R3-1`      |
+---------+------------------+------------------+
|   3.0   |   3.14.8.2       |                  |
+---------+------------------+------------------+


Setup
-----

Build
+++++

Change the definition of ``EPICS_BASE`` in ``configure/RELEASE`` according to
the location of epics base on the host, then (gnu-)make.

Include to your EPICS application
+++++++++++++++++++++++++++++++++

To include the module to your IOC application, add the install directory to your
application's ``configure/RELEASE``, and include ``dbd`` and ``lib`` in the
``src`` Makefile: ::

    <IOC app name>_DBD += caPutLog.dbd  # For standard format
    <IOC app name>_DBD += caPutJsonLog.dbd  # For JSON format (Exists only if module is compiled with supported version of base)
    <IOC app name>_LIBS += caPutLog  # Required for both output formats

for libraries to link.

Configure
+++++++++

.. note::  Only one instance of the logger is supported to run at the time.

In your IOC startup file add the following command for the standard output format::

   caPutLogInit "host[:port]" [config]

or for JSON output format::

   caPutJsonLogInit "host[:port]" [config]

where ``host`` (mandatory argument) is the IP address or host name of the log
server and ``port`` is optional (the default is 7011).
To log to multiple hosts, either call the funtion with a space separated list like
``"host1[:port] host2[:port]"`` or call the function multiple times with different
hosts.

The environment variable ``EPICS_CA_PUT_LOG_ADDR`` / ``EPICS_CA_PUT_JSON_LOG_ADDR``
is used if the first parameter to ``caPutLogInit`` / ``caPutJsonLogInit`` is ``NULL``
or the empty string, respectively.

The second (optional, default=0) argument should be one of:

- ``-1`` - No logging (disabled)
- ``0``  - Log only on value change (ignore if old and new values are equal)
- ``1``  - Log all puts with burst filter
- ``2``  - Log all puts without any filters


Make sure access security is enabled on the IOC by providing a
suitable configuration file and load it with a call to
``asSetFilename(<filename>)`` before iocInit. Your configuration file
should contain a TRAPWRITE rule. The following snippet can be used to
enable read/write access and write trapping for everyone (i.e.
unrestricted access)::

   ASG(DEFAULT) {
      RULE(1,READ)
      RULE(1,WRITE,TRAPWRITE)
   }

.. note::  ``caPutLogInit`` or ``caPutJsonLogInit`` are expecting access security
            to be already running, so they must be called *after* iocInit.

Other shell commands for logger are:

``caPutLogReconf config`` / ``caPutJsonLogReconf config``
   Change configuration on-line. The argument is the same as in
   ``caPutLogInit`` / ``caPutJsonLogInit``.

``caPutLogShow level`` / ``caPutJsonLogShow level``
   Show information about a running caPutLog,
   level is the usual interest level (0, 1, or 2).

Server
++++++

For the server you can use the same executable as for the regular IOC log
server. You might want to start another instance with a different port,
though. However, you can also use the same log server instance (so that caput
log messages and regular IOC log messages go into the same log file).


Standard Log Format
+++++++++++++++++++

The iocLogServer precedes each line with these data::

   <host:port of log client> <date and time of log message reception>

After this comes the actual log message, which has this format::

   <date> <time> <host> <user> <pv> <change>

where <date> and <time> refer to the time of the caput request, <host> and
<user> identify the agent that requested the caput, <pv> is the record or
record.field name and <change> is one of ::

   new=<value> old=<value>

or ::

   new=<value> old=<value> min=<value> max=<value>

The latter format means that several puts for the same PV have been received
in rapid succession; in this case only the original and the final value as
well as the minimum and maximum value are logged. This filtering can be
disabled by specifying the ``caPutLogAllNoFilter`` (``2``) configuration option.

From release 4 on, string values are quoted and special characters are escaped.
The default date/time format ``%d-%b-%y %H:%M:%S`` may be changed at compile time
with the macro DEFAULT_TIME_FMT and/or modified at run time using the shell function
``caPutLogSetTimeFmt "<date_time_format>"``.

Json Log Format
+++++++++++++++

``caPutJsonLogger`` is using Json as the output format. General format looks like ::

    <iocLogPrefix>{"date":"<date>","time":"<time>","host":"<client hostname>","user":"<client username>","pv":"<pv name>","new":<new value>,["new-size":<new array size>,]"old":<new value>[,"old-size":<old array size>][,"min":<minimum value>][,"max":<maximum value>]}<LF>

Where Json properties are:
    * **iocLogPrefix** is an optional prefix defined with a ``iocLogPrefix`` command in the IOC
                        startup script. This command is part of the log client build into the EPICS
                        base (and used by this module). As this value is
                        appended at the lower level, currently can not be part of the JSON structure.
    * **date** date when the caput was made in the following format: yyyy-mm-dd.
    * **time**  time of the day when the caput was made in the following format: hh-mm-ss.sss (24h format).
    * **client hostname** server/workstation's hostname from which the value was changed.
    * **client username** username of the user who changed the value.
    * **pv name** name of the changed PV.
    * **new value** new value of the PV. This can either be a scalar value (number or a string) or an array.
    * **old value** new value of the PV. This can either be a scalar value (number or a string) or an array.
    * **new array size** is included only if **new value** is an array and contains information of the actual array size on the IOC.
    * **old array size** is included only if **old value** is an array and contains information of the actual array size on the IOC.
    * **min** value is included only if the burst filtering was applied and tells the minimum value of the puts in the burst period.
    * **max** value is included only if the burst filtering was applied and tells the maximum value of the puts in the burst period.

Json implementation of the logger also supports arrays and lso/lsi PVs. As these values
can get very long, there is currently a limit how long **new value** and **old value** properties can be.
Internally value can take up to 400 bytes, which translates to:

    * lso/lsi: 400 characters
    * waveform of strings: 10 string
    * waveform of chars: 400 characters
    * waveform of doubles: 50 doubles
    * waveform of longs: 50 doubles

Nan (not a number) and both infinity values are also supported. In JSON they are represented
as string properties: "Nan", "-Infinity" and "Infinity" respectively.

Examples
^^^^^^^^

Scalar value: ::

    testIOC{"date":"2020-08-10","time":"13:02:08.124","host":"devWs","user":"devman","pv":"ao","new":77.5,"old":1}<LF>

Burst of scalar values: ::

    testIOC{"date":"2020-08-10","time":"13:08:44.144","host":"devWs","user":"devman","pv":"ao","new":8,"old":77.5,"min":7.5,"max":870.5}<LF>

String value: ::

    testIOC{"date":"2020-08-10","time":"13:09:43.741","host":"devWs","user":"devman","pv":"stringout","new":"Example put on stringout","old":"so1"}<LF>

Lso/lsi value: ::

    testIOC{"date":"2020-08-10","time":"13:11:07.100","host":"devWs","user":"devman","pv":"lso.$","new":["Some very long string in lso record 123456789012345678901234567890"],"new-size":67,"old":[""],"old-size":0}<LF>

Waveform of doubles: ::

    testIOC{"date":"2020-08-10","time":"13:13:06.544","host":"devWs","user":"devman","pv":"wfd","new":[4.5,5,10,11],"new-size":4,"old":[],"old-size":0}<LF>

Nan value: ::

    testIOC{"date":"2020-08-10","time":"13:14:31.187","host":"devWs","user":"devman","pv":"ao","new":"Nan","old":8}<LF>

Minus infinity: ::

    testIOC{"date":"2020-08-10","time":"13:15:22.189","host":"devWs","user":"devman","pv":"ao","new":"-Infinity","old":"Nan"}<LF>



Logging to a PV
+++++++++++++++

Logs can be also written to a PV (Waveform of chars or lso/lsi records).
This functionality can be activated by setting the ``EPICS_AS_PUT_LOG_PV`` /
``EPICS_AS_PUT_JSON_LOG_PV`` environment variable to a PV that should be local
to the IOC. If the PV is found on the IOC logs will be written to it. If a log is
too long for the record it will be truncated.

.. note::  As of EPICS base 7.0.1 ``lso``/``lsi`` records will be truncate a message at
    40 character. As workaround add ``.$`` or ``.VAL$`` to a PV name.

Acknowledgments
----------------


V\. Korobov (DESY)
   created the original version for the EPICS base 3.13 series

Jeff Hill (LANL)
   wrote the iocLog code in base on which much of the implementation
   was based on

David Morris (TRIUMF)
   suggested an option to disable filtering and wrote a patch to implemented it

John Priller <priller@frib.msu.edu>
   provided a patch to allow non-IOC servers to use (parts of) caPutLog
   by exposing some previously internal APIs

Matic Pogacnik (Implementation), Andrew Johnson (Requirements)
   add new JSON logger

If I forgot to mention anyone, please drop me a note and I'll add them.


Problems
--------

If you have any problems with this module, send me (`Ben Franksen`_) a mail.


.. _Ben Franksen: mailto:benjamin.franksen@bessy.de
.. _Github: https://github.com/epics-modules/caPutLog
.. _Github releases: https://github.com/epics-modules/caPutLog/releases
.. _HZB: http://www.helmholtz-berlin.de/
.. _EPICS: http://www.aps.anl.goc/epics/
.. _browse: https://github.com/epics-modules/caPutLog/commits/master
