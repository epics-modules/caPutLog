Release Notes
=============

R4-1: Changes since R4-0
------------------------

**This version has not yet been released**

* Prevent crashes by only allowing one set of commands to be registered.

* Fix an issue where waveform-type arrays would not send out monitors in certain
  cases. See https://epics.anl.gov/tech-talk/2024/msg00481.php and 
  https://github.com/epics-base/pva2pva/issues/60 for examples.


R4-0: Changes since R3-7
------------------------

* add new JSON log format

* quote and escape strings in non-JSON logs

* allow to change time format

* allow multiple receivers

* fix caPutLogReconf which was non-functional

* allow to switch on debug messages at run time


R3-5: Changes since R3-4
------------------------

* removed dead code for val_cmp

* prepare for DBR_INT64 & fixed default clauses

* support base-3.15

* remove missing (in base-3.15) and unneeded include tsDefs.h

R3-4: Changes since R3-3-3
--------------------------

* expose some internal APIs to support non-IOC servers

  This patch was provided by John Priller <priller@frib.msu.edu>
  in order to user caPutLog inside the CA-Gateway. The header files
  caPutLogTask.h and caPutLogAs.h are now installed, and export
  the additional function caPutLogDataCalloc, enabling user code to
  send their own data (of type LOGDATA) to the caPutLogTask.
  Apart from a few small fixes normal use on an IOC is unaffected.

R3-3-3: Changes since R3-3-2
----------------------------

* replace %T by %H:%M:%S for strftime

  Older VxWorks versions do not know the %T format specifier.

R3-3-2: Changes since R3-3-1
----------------------------

* base -> 3-14-12-2-1

* fixed tarball generation rules

* suppress darcs dist output if -s flag is given to make

R3-3-1: Changes since R3-3
--------------------------

* base release -> 3-14-12-1-1

R3-3: Changes since R3-2
------------------------

* create documentation with sphinx from rst files
* minor changes to source code organisation
* bump EPICS base default release to 3.14.12
* add configuratuion switch to disable filtering
  (thanks to a patch from David Morris)

R3-2: Changes since R3-1
------------------------

* fix: do not return void (some compilers dont seem to like this)
* added some defines that have been removed from base-3.14.11

R3-1: Changes since R3-0
------------------------

* fixed behavior if field type is not a valid db request type
* added a web page for distribution and minimal docs
* fixed for little-endian architectures
* fixed environment variable chaos

  - removed defunct EPICS_CA_PUT_LOG_INET and EPICS_CA_PUT_LOG_PORT and
    replaced by EPICS_CA_PUT_LOG_ADDR (host[:port] notation)
  - environ var gets used if first parameter of caPutLogInit is NULL or empty
