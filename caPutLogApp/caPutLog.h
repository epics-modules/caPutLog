/*
 *	Author:	Korobov V.
 *	Date:	5-98
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *		MKS-2 Group, DESY, Hamburg
 *
 * 	Modification Log:
 * 	-----------------
 *	03/09/00	kor	added FREE_LIST_SIZE definition
 *	01/03/01	kor	change value to union
 */

#ifndef INCLasLogPutH
#define INCLasLogPutH

#define MAX_USR_ID	10	/* Length of user ID */
#define MAX_HOST_ID	15	/* Length of host ID */
#define FREE_LIST_SIZE	100 	/* Size of free list */

#define CHANGING_PUTS	0	/* Log CA Puts that change values */
#define ALL_PUTS	1	/* Log all CA Puts */

typedef    union {
      char	v_char;
      uchar_t 	v_uchar;
      short	v_short;
      ushort_t	v_ushort;
      long	v_long;
      ulong_t	v_ulong;
      float	v_float;
      double	v_double;
      char	v_string[MAX_STRING_SIZE];
   } VALUE;


struct tim_val {
   TS_STAMP time;
   VALUE value;
}; 

typedef struct asPutLog {
   char userid[MAX_USR_ID+1];		/* User ID */
   char hostid[MAX_HOST_ID+1];		/* Host ID */
   char pv_name[PVNAME_STRINGSZ+5];	/* PV name.field name */
   short type;				/* Field type */
   VALUE old_value;			/* Value  before Put */
   struct tim_val new_value;		/* Value & time after Put */
}LOGDATA;

#endif
