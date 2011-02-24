Release Notes
=============

.. _R3-2:

Changes since R3-1
------------------

* fix: do not return void (some compilers dont seem to like this)
* added some defines that have been removed from base-3.14.11

.. _R3-1:

Changes since R3-0
------------------

* fixed behavior if field type is not a valid db request type
* added a web page for distribution and minimal docs
* fixed for little-endian architectures
* fixed environment variable chaos

  - removed defunct EPICS_CA_PUT_LOG_INET and EPICS_CA_PUT_LOG_PORT and
    replaced by EPICS_CA_PUT_LOG_ADDR (host[:port] notation)
  - environ var gets used if first parameter of caPutLogInit is NULL or empty