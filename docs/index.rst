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
server. Currently it supports two output formats, **Original** and **JSON**.
The JSON format is only supported with `EPICS`_ base versions 7.0.1 and later.


Download
--------

You can download a release from the `Github releases`_ page or get the
latest development version via `Github`_::

   git clone https://github.com/epics-modules/caPutLog.git

You can also `browse`_ through the latest changes in the repo.

Compatibility Matrix and Release Notes
++++++++++++++++++++++++++++++++++++++

+---------+------------------+------------------+
| Version | EPICS Release    | Release Notes    |
+=========+==================+==================+
|   4.1   | 3.14.12 .. 7.0.x | `R4-1`_          |
+---------+------------------+------------------+
|   4.0   | 3.14.12 .. 7.0.x | `R4-0`_          |
+---------+------------------+------------------+
|   3.5   | 3.14.12 .. 3.15  | `R3-5`_          |
+---------+------------------+------------------+
|   3.4   | 3.14.12          | `R3-4`_          |
+---------+------------------+------------------+
|  3.3.3  | 3.14.12          | `R3-3-3`_        |
+---------+------------------+------------------+
|  3.3.2  | 3.14.12          | `R3-3-2`_        |
+---------+------------------+------------------+
|  3.3.1  | 3.14.12          | `R3-3-1`_        |
+---------+------------------+------------------+
|   3.3   | 3.14.12          | `R3-3`_          |
+---------+------------------+------------------+
|   3.2   | 3.14.11          | `R3-2`_          |
+---------+------------------+------------------+
|   3.1   | 3.14.8.2         | `R3-1`_          |
+---------+------------------+------------------+
|   3.0   | 3.14.8.2         |                  |
+---------+------------------+------------------+


Setup
-----

Build caPutLog
++++++++++++++

Set the definition of ``EPICS_BASE`` in ``configure/RELEASE`` to point to
the Base installation to be used, then run ``make`` or ``gmake`` as normal to
compile the module.

Add it to an EPICS application
++++++++++++++++++++++++++++++

To build the module into an IOC application, add the install directory to the
application's ``configure/RELEASE`` file::

    CAPUTLOG = /path/to/caPutLog

Then in the IOC's ``src/Makefile`` choose one (but not both) of the two
published ``.dbd`` files to pick the logging format to use::

    <IOC app name>_DBD += caPutLog.dbd  # For original format
    <IOC app name>_DBD += caPutJsonLog.dbd  # For JSON format

The ``caPutJsonLog.dbd`` file will only be available if the module was compiled
with a version of Base that supports it, 7.0.1 or later. IOCs should only be
built with one of the ``.dbd`` files, if both are included a warning will be
shown at registration time and only one set of commands for either the original
or JSON format logs will be registered with the IOC shell.

Also in the IOC's ``src/Makefile`` add the module's library which the IOC must
be linked against::

    <IOC app name>_LIBS += caPutLog  # Required for either output format


Configure the IOC
+++++++++++++++++

.. note::  Only one logger instance may be running at a time (although it can
    log to more than one log server at once), and the output format is fixed by
    which of the two DBD files the IOC loaded. The format cannot be changed
    without restarting and possibly rebuilding the IOC.

In your IOC startup file add this command for logging using the original output
format::

   caPutLogInit "host[:port]" [config]

or for the JSON output format::

   caPutJsonLogInit "host[:port]" [config]

In both cases ``host`` is the IP address or host name of the log server and
``port`` is optional (the default is 7011).

To log to multiple hosts, either provide the ``LogInit`` command with a space
separated list inside quotes like ``"host1[:port] host2[:port]"`` or run the
``LogInit`` command multiple times with a different host/port each time.

The environment variable ``EPICS_CA_PUT_LOG_ADDR`` /
``EPICS_CA_JSON_PUT_LOG_ADDR`` is used if the first parameter to
``caPutLogInit`` / ``caPutJsonLogInit`` is ``NULL``, an empty string ``""``, or
blank.

The second (optional, default=0) argument should be one of:

- ``-1`` - No logging (disabled)
- ``0``  - Log only value changes (ignore puts of the samr value)
- ``1``  - Log all puts with a burst filter
- ``2``  - Log all puts without any filters


Access security must be enabled in the IOC by creating a suitable configuration
file and loading it with a call to ``asSetFilename(<filename>)`` before
``iocInit``. The configuration file must contain a ``TRAPWRITE`` rule that will
match all records that are to have puts logged. The following snippet can be
used as an access security file that enables read/write access and put logging
for everyone to all records that don't have their ``ASG`` field set. This gives
unrestricted access to those records, and all puts to them will be logged::

   ASG(DEFAULT) {
      RULE(1,READ)
      RULE(1,WRITE,TRAPWRITE)
   }

.. note:: ``caPutLogInit`` or ``caPutJsonLogInit`` expect access security to be
    active when they are executed, so they must be run *after* iocInit.

Other shell commands for controlling the logger are:

``caPutLogReconf config`` / ``caPutJsonLogReconf config``

   Change the logger configuration while it's already active. The ``config``
   argument has the same meaning as described for ``caPutLogInit`` /
   ``caPutJsonLogInit`` above.

``caPutLogShow level`` / ``caPutJsonLogShow level``

   Show information about an active logger, including its current ``config``
   setting. ``level`` is the usual interest level (0, 1, or 2).


Set up a Log Server
+++++++++++++++++++

For the server you can use the basic IOC log server that comes with EPICS Base,
but many newer log servers can also be configured to listen for connections to a
TCP socket and then accept text messages through that. A most standard server
will probably also be more convenient for viewing the log messages.

If your IOCs are already sending their error log messages to a log server you
might want to run another instance of it on a different port number to easily
distinguish the caput log messages. However you can use a single log server
instance and have caput log messages and regular IOC log messages all go into
the same log file or database.


Original Log Format
+++++++++++++++++++

The EPICS iocLogServer starts each line in its log files with these data::

   <host:port of log client> <date and time of log message reception>

Other log servers may handle client and timestamp identification differently.

The actual log message sent from the IOC has this format::

   <iocLogPrefix><date> <time> <host> <user> <pv> <change>

where <iocLogPrefix> is an optional string set using the IOC's ``iocLogPrefix``
command; <date> and <time> refer to the time of the caput request; <host> and
<user> identify the agent that requested the caput; <pv> is the record or
record.field name, and <change> is either::

   new=<value> old=<value>

or::

   new=<value> old=<value> min=<value> max=<value>

The latter format means that several puts for the same PV were received in quick
succession; in this case only the original and final values of the burst as well
as the minimum and maximum values seen are logged. This burst filtering can be
disabled by selecting the ``caPutLogAllNoFilter`` (``2``) configuration value.

From release 4.0 on, string values are placed inside quotes, and special
characters within the string are escaped. The default date/time format
``%d-%b-%y %H:%M:%S`` can be changed at compile time with the macro
DEFAULT_TIME_FMT and/or modified at run time using the shell function
``caPutLogSetTimeFmt "<date_time_format>"``.

JSON Log Format
+++++++++++++++

``caPutJsonLogger`` writes its output in JSON (JavaScript Object Notation)
format. The output contains no newline characters until the very end of each log
message, but has been broken up here and in the examples below for readability.
It looks like this::

    <iocLogPrefix>{"date":"<date>","time":"<time>",
        "host":"<client hostname>","user":"<client username>",
        "pv":"<pv name>",
        "new":<new value>,["new-size":<new array size>,]
        "old":<new value>[,"old-size":<old array size>]
        [,"min":<minimum value>][,"max":<maximum value>]}<LF>

The JSON properties are:

    * **iocLogPrefix** is an optional string set using the IOC's
      ``iocLogPrefix`` command. It is prefixed to all messages sent through the
      log client software provided by Base and cannot be controlled or removed
      by the caPutLog module.

    * **date** date when the caput was made in the following format: yyyy-mm-dd.

    * **time**  time of the day when the caput was made in the following format:
      hh-mm-ss.sss (24h format).

    * **client hostname** server/workstation's hostname from which the value was
      changed.

    * **client username** username of the user who changed the value.

    * **pv name** name of the changed PV.

    * **new value** new value of the PV. This can either be a scalar value
      (number or a string), or an array.

    * **old value** new value of the PV. This can either be a scalar value
      (number or a string), or an array.

    * **new array size** is included only if **new value** is an array and
      provides the actual array size on the IOC.

    * **old array size** is included only if **old value** is an array and
      provides the actual array size on the IOC.

    * **min** value is included only if the burst filtering was applied and
      gives the minimum value of the puts received within the burst period.

    * **max** value is included only if the burst filtering was applied and
      gives the maximum value of the puts received within the burst period.

The JSON implementation of the logger added support for arrays and long string
fields. As these values can get very large, there is a limit to how long the
**new value** and **old value** properties can be. Each value can use up to 400
bytes of internal storage, which translates to:

    * long string: 400 characters
    * array of strings: 10 40-character strings
    * array of chars: 400 characters
    * array of longs: 100 32-bit integers
    * array of doubles: 50 doubles
    * array of int64: 50 64-bit integers

Nan (not a number) and both infinity values are also supported. In JSON they are represented
as string properties: "Nan", "-Infinity" and "Infinity" respectively.

Examples
^^^^^^^^

Scalar value::

    testIOC{"date":"2020-08-10","time":"13:02:08.124",
        "host":"devWs","user":"devman",
        "pv":"ao",
        "new":77.5,"old":1}<LF>

Burst of scalar values::

    testIOC{"date":"2020-08-10","time":"13:08:44.144",
        "host":"devWs","user":"devman",
        "pv":"ao",
        "new":8,"old":77.5,
        "min":7.5,"max":870.5}<LF>

String value::

    testIOC{"date":"2020-08-10","time":"13:09:43.741",
        "host":"devWs","user":"devman",
        "pv":"stringout",
        "new":"Example put on stringout","old":"so1"}<LF>

Long string value::

    testIOC{"date":"2020-08-10","time":"13:11:07.100",
        "host":"devWs","user":"devman",
        "pv":"lso.$",
        "new":["Some very long string in lso record 123456789012345678901234567890"],"new-size":67,
        "old":[""],"old-size":0}<LF>

Array of doubles::

    testIOC{"date":"2020-08-10","time":"13:13:06.544",
        "host":"devWs","user":"devman",
        "pv":"wfd",
        "new":[4.5,5,10,11],"new-size":4,
        "old":[],"old-size":0}<LF>

Nan value::

    testIOC{"date":"2020-08-10","time":"13:14:31.187",
        "host":"devWs","user":"devman",
        "pv":"ao",
        "new":"Nan","old":8}<LF>

Minus infinity::

    testIOC{"date":"2020-08-10","time":"13:15:22.189",
        "host":"devWs","user":"devman",
        "pv":"ao",
        "new":"-Infinity","old":"Nan"}<LF>


Logging to a PV
+++++++++++++++

Logs can be also written to a PV (waveform of chars or an lso/lsi record). This
functionality is activated by setting the ``EPICS_AS_PUT_LOG_PV`` /
``EPICS_AS_PUT_JSON_LOG_PV`` environment variable to a PV name, which must be
local to the IOC. If the PV is found in the IOC, logs will be written to it. If
a log is too long for the record it will be truncated.

.. note:: When logging to an lsi/lso record the log will be truncated at 40
    characters unless a long-string field modifier ``.$`` or ``.VAL$`` is added
    to the record name in the appropriate environment variable.

Debugging
+++++++++

To switch on debug messages, use the IOC shell command ``var caPutLogDebug, 1``.


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


Problems
--------

If you have any problems with this module, send me (`Ben Franksen`_) a mail.


.. _Ben Franksen: mailto:benjamin.franksen@bessy.de
.. _Github: https://github.com/epics-modules/caPutLog
.. _Github releases: https://github.com/epics-modules/caPutLog/releases
.. _HZB: http://www.helmholtz-berlin.de/
.. _EPICS: https://epics.anl.gov/
.. _browse: https://github.com/epics-modules/caPutLog/commits/master
.. _R4-1: releasenotes.rst#r4-1-changes-since-r4-0
.. _R4-0: releasenotes.rst#r4-0-changes-since-r3-7
.. _R3-5: releasenotes.rst#r3-5-changes-since-r3-4
.. _R3-4: releasenotes.rst#r3-4-changes-since-r3-3-3
.. _R3-3-3: releasenotes.rst#r3-3-3-changes-since-r3-3-2
.. _R3-3-2: releasenotes.rst#r3-3-2-changes-since-r3-3-1
.. _R3-3-1: releasenotes.rst#r3-3-1-changes-since-r3-3
.. _R3-3: releasenotes.rst#r3-3-changes-since-r3-2
.. _R3-2: releasenotes.rst#r3-2-changes-since-r3-1
.. _R3-1: releasenotes.rst#r3-1-changes-since-r3-0

