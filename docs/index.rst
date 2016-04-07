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


Download
--------

You can download a release from the links in the table below, or get the
latest development version via `darcs`_::

   darcs get http://www-csr.bessy.de/control/SoftDist/caPutLog/repo/caPutLog

You can also `browse`_ through the latest changes in the repo.

+---------+---------------+-----------------------------------+---------------+
| Version | EPICS Release | Filename                          | Release Notes |
+=========+===============+===================================+===============+
|   3.5   | 3.14.12..3.15 | :download:`caPutLog-3.5.tar.gz`   | :ref:`R3-5`   |
+---------+---------------+-----------------------------------+---------------+
|   3.4   |   3.14.12     | :download:`caPutLog-3.4.tar.gz`   | :ref:`R3-4`   |
+---------+---------------+-----------------------------------+---------------+
|  3.3.3  |   3.14.12     | :download:`caPutLog-3.3.3.tar.gz` | :ref:`R3-3-3` |
+---------+---------------+-----------------------------------+---------------+
|  3.3.2  |   3.14.12     | :download:`caPutLog-3.3.2.tar.gz` | :ref:`R3-3-2` |
+---------+---------------+-----------------------------------+---------------+
|  3.3.1  |   3.14.12     | :download:`caPutLog-3.3.1.tar.gz` | :ref:`R3-3-1` |
+---------+---------------+-----------------------------------+---------------+
|   3.3   |   3.14.12     | :download:`caPutLog-3.3.tar.gz`   | :ref:`R3-3`   |
+---------+---------------+-----------------------------------+---------------+
|   3.2   |   3.14.11     | :download:`caPutLog-3.2.tar.gz`   | :ref:`R3-2`   |
+---------+---------------+-----------------------------------+---------------+
|   3.1   |   3.14.8.2    | :download:`caPutLog-3.1.tar.gz`   | :ref:`R3-1`   |
+---------+---------------+-----------------------------------+---------------+
|   3.0   |   3.14.8.2    | :download:`caPutLog-3.0.tar.gz`   | n/a           |
+---------+---------------+-----------------------------------+---------------+


Setup
-----

Build
+++++

Change the definition of ``EPICS_BASE`` in ``configure/RELEASE`` according to
the location of epics base on the host, then (gnu-)make. Add the install
directory to your IOC application's ``configure/RELEASE``, and in the
Makefile add ``caPutLog.dbd`` to your dbd includes and ``caPutLog`` to the
libraries to link.

Configure
+++++++++

In your IOC startup file add the command::

   caPutLogInit "host[:port]" [config]

where ``host`` (mandatory argument) is the IP address or host name of the log
server and ``port`` is optional (the default is 7011). The environment
variable ``EPICS_CA_PUT_LOG_ADDR`` is used if the first parameter to
``caPutLogInit`` is ``NULL`` or the empty string.

The second (optional, default=0) argument should be one of ::

   #define caPutLogNone        -1  /* no logging (disable) */
   #define caPutLogOnChange    0   /* log only on value change */
   #define caPutLogAll         1   /* log all puts */
   #define caPutLogAllNoFilter 2   /* log all puts no filtering on same PV*/

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


Note that ``caPutLogInit`` expects access security to be already running, so
must be called *after* iocInit.

Other shell commands are:

``caPutLogReconf config``
   Change configuration on-line. The argument is the same as in
   ``caPutLogInit``.

``caPutLogShow level``
   Show information about a running caPutLog,
   level is the usual interest level (0, 1, or 2).

Server
++++++

For the server you can use the same executable as for the regular IOC log
server. You might want to start another instance with a different port,
though. However, you can also use the same log server instance (so that caput
log messages and regular IOC log messages go into the same log file).


Log Format
----------

The iocLogServer precedes each line with these data::

   <host:port of log client> <date and time of log message reception>

After this comes the actual log message, which has this format::

   <date> <time> <host> <user> <change>

where <date> and <time> refer to the time of the caput request, <host> and
<user> identify the agent that requested the caput, and <change> is one of ::

   new=<value> old=<value>

or ::

   new=<value> old=<value> min=<value> max=<value>

The latter format means that several puts for the same PV have been received
in rapid succession; in this case only the original and the final value as
well as the minimum and maximum value are logged. This filtering can be
disabled by specifying the ``caPutLogAllNoFilter`` configuration option.


Acknowledgements
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

If I forgot to mention anyone, please drop me a note and I'll add them.


Problems
--------

If you have any problems with this module, send me (`Ben Franksen`_) a mail.


.. _Ben Franksen: mailto:benjamin.franksen@bessy.de
.. _darcs: http://www.darcs.net/
.. _HZB: http://www.helmholtz-berlin.de/
.. _EPICS: http://www.aps.anl.goc/epics/
.. _browse: http://www-csr.bessy.de/cgi-bin/darcsweb.cgi?r=caPutLog;a=summary
