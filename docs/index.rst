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
from the iocShell. See Section Installation, below.


Download
--------

You can download a release from the links in the table below, or get the
latest development version via `darcs`_::

   darcs get http://www-csr.bessy.de/control/SoftDist/caPutLog/repo/caPutLog

+---------+---------------+---------------------------------+---------------+
| Version | EPICS Release | Filename                        | Release Notes |
+=========+===============+=================================+===============+
|   3.0   |   3.14.8.2    | :download:`caPutLog-3.0.tar.gz` | n/a           |
+---------+---------------+---------------------------------+---------------+
|   3.1   |   3.14.8.2    | :download:`caPutLog-3.1.tar.gz` | :ref:`R3-2`   |
+---------+---------------+---------------------------------+---------------+
|   3.2   |   3.14.11     | :download:`caPutLog-3.2.tar.gz` | :ref:`R3-1`   |
+---------+---------------+---------------------------------+---------------+

Setup
-----

Change the definition of ``EPICS_BASE`` in ``configure/RELEASE`` according to
the location of epics base on the host, then (gnu-)make. Add the install
directory to your IOC application's ``configure/RELEASE``, and in the
Makefile add ``caPutLog.dbd`` to your dbd includes and ``caPutLog`` to the
libraries to link.

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


``caPutLogInit`` expects access security to be already running, so must be
called *after* iocInit.

Other shell commands are:

``caPutLogReconf config`` Change configuration on-line. The argument is the
same as in ``caPutLogInit``. ``caPutLogShow level`` Show Here, level is the
usual interest level (0, 1, or 2).

For the server you can use the same executable as for the regular IOC log
server. You might want to start another instance with a dfferent port,
though. However, you can also use the same log server instance (so that caput
log messages and regular IOC log messages go into the same log file).


Problems
--------

If you have any problems with this module, send me (`Ben Franksen`_) a mail.


.. _Ben Franksen: mailto:benjamin.franksen@bessy.de
.. _darcs: http://www.darcs.net/
.. _caPutLog-3.0.tar.gz: caPutLog-3.0.tar.gz
.. _caPutLog-3.1.tar.gz: caPutLog-3.1.tar.gz
.. _caPutLog-3.2.tar.gz: caPutLog-3.2.tar.gz
.. _HZB: http://www.helmholtz-berlin.de/
.. _EPICS: http://www.aps.anl.goc/epics/