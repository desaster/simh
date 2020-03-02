// A rudimentary but fully functional hack:
// Add the BMP180 temperature and barometric pressure sensor as a Unibus device
//
// 1. Copied pdp11_pt.c to pdp11_i2c.c
// 2. Search & replace ptr_ into icr_, and ptp_ into icp_
//		(so that function names do not conflict with the original paper tape device)
//    Change PTR into ICR and PTP into ICP in the "DEVICE icr_dev = {" and "DEVICE icp_dev = {" structs
//		(so that the I2C devices have a proper name of their own, useable on the simh command line)
// 3. insert the "bmp180Setup (100);" initialisation call into icr_attach
//    (so to connect to the sensor, in simh's command line, do "attach icr junk.txt"). Rudimentary ;)
// 4. insert "*data = (int32) analogRead(100);" and (101) into icr_rd
//    (this is where the data from the i2c sensor get put into the PDP11)
// 5. add the 2 wiringPi includes
//
// To make simh know about its new unibus device:
//
// 6. in pdp11_io_lib.c, add:
//   { { "ICR" },         1,  1,  0, 0, 
//        {016100}, {0070} },                             /* I2C reader - fx CSR, fx VEC */
//    { { "ICP" },         1,  1,  0, 0, 
//        {016104}, {0074} },                             /* I2C punch - fx CSR, fx VEC */
// to the autoconfig device list, just before the last NULL, -1 item. So line 786 in my version.
// (this will handle where on the unibus the I2C devices will live)
//
// 7. in pdp11_sys.c, add the devices to the list of devices simh knows about, by 
//    adding to "DEVICE *sim_devices[] = {", line 204 in my version of the simh sources:
//	    	&icr_dev,
//  	    &icp_dev,
// (just above the NULL terminator of the list)
//
// 8. Lastly, declare the I2C deviced by inserting at line 120:
// 			extern DEVICE icr_dev;
// 			extern DEVICE icp_dev;
//
// 9. make sure you have an "i2c" directory inside the src/PDP11 directory, with the following files:
// bmp180.c  wiringPi.c  wiringPiI2C.c
// All taken more or less straight out of wiringPi, some minimal changes done.
// 
// That's it, for now! Type 'help icr' on the simh command line for help.




/* pdp11_pt.c: PC11 paper tape reader/punch simulator

   Copyright (c) 1993-2008, Robert M Supnik

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   ROBERT M SUPNIK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of Robert M Supnik shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert M Supnik.

   ptr          paper tape reader
   ptp          paper tape punch

   07-Jul-05    RMS     Removed extraneous externs
   19-May-03    RMS     Revised for new conditional compilation scheme
   25-Apr-03    RMS     Revised for extended file support
   12-Sep-02    RMS     Split off from pdp11_stddev.c
*/

#if defined (VM_PDP10)                                  /* PDP10 version */
#include "pdp10_defs.h"
#define PT_DIS          DEV_DIS

#elif defined (VM_VAX)                                  /* VAX version */
#include "vax_defs.h"
#define PT_DIS          DEV_DIS

#else                                                   /* PDP-11 version */
#include "pdp11_defs.h"
#define PT_DIS          0
#endif




// --------------------------------------------------------------------- PiDP-11 BMP180 hack
#include "i2c/bmp180.h"
#include "i2c/wiringPi.h"
// ---------------------------------------------------------------------




#define PTRCSR_IMP      (CSR_ERR+CSR_BUSY+CSR_DONE+CSR_IE) /* paper tape reader */
#define PTRCSR_RW       (CSR_IE)
#define PTPCSR_IMP      (CSR_ERR + CSR_DONE + CSR_IE)   /* paper tape punch */
#define PTPCSR_RW       (CSR_IE)

int32 icr_csr = 0;                                      /* control/status */
int32 icr_stopioe = 0;                                  /* stop on error */
int32 icp_csr = 0;                                      /* control/status */
int32 icp_stopioe = 0;                                  /* stop on error */

t_stat icr_rd (int32 *data, int32 PA, int32 access);
t_stat icr_wr (int32 data, int32 PA, int32 access);
t_stat icr_svc (UNIT *uptr);
t_stat icr_reset (DEVICE *dptr);
t_stat icr_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr);
const char *icr_description (DEVICE *dptr);
t_stat icr_attach (UNIT *uptr, CONST char *ptr);
t_stat icr_detach (UNIT *uptr);
t_stat icp_rd (int32 *data, int32 PA, int32 access);
t_stat icp_wr (int32 data, int32 PA, int32 access);
t_stat icp_svc (UNIT *uptr);
t_stat icp_reset (DEVICE *dptr);
t_stat icp_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr);
const char *icp_description (DEVICE *dptr);
t_stat icp_attach (UNIT *uptr, CONST char *ptr);
t_stat icp_detach (UNIT *uptr);

/* PTR data structures

   icr_dev      PTR device descriptor
   icr_unit     PTR unit descriptor
   icr_reg      PTR register list
*/

#define IOLN_PTR        004

DIB icr_dib = {
    IOBA_AUTO, IOLN_PTR, &icr_rd, &icr_wr,
    1, IVCL (PTR), VEC_AUTO, { NULL }
    };

UNIT icr_unit = {
    UDATA (&icr_svc, UNIT_SEQ+UNIT_ATTABLE+UNIT_ROABLE, 0),
           SERIAL_IN_WAIT
    };

extern DEVICE icr_dev;
REG icr_reg[] = {
    { GRDATAD (BUF,     icr_unit.buf, DEV_RDX,  8, 0, "last data item processed") },
    { GRDATAD (CSR,          icr_csr, DEV_RDX, 16, 0, "control/status register") },
    { FLDATAD (INT,          int_req, INT_V_PTR,      "interrupt pending flag") },
    { FLDATAD (ERR,          icr_csr, CSR_V_ERR,      "error flag (CSR<15>)") },
    { FLDATAD (BUSY,         icr_csr, CSR_V_BUSY,     "busy flag (CSR<11>)") },
    { FLDATAD (DONE,         icr_csr, CSR_V_DONE,     "device done flag (CSR<7>)") },
    { FLDATAD (IE,           icr_csr, CSR_V_IE,       "interrupt enable flag (CSR<6>)") },
    { DRDATAD (POS,     icr_unit.pos, T_ADDR_W,       "position in the input file"), PV_LEFT },
    { DRDATAD (TIME,   icr_unit.wait, 24,             "time from I/O initiation to interrupt"), PV_LEFT },
    { FLDATAD (STOP_IOE, icr_stopioe, 0,              "stop on I/O error") },
    { FLDATA  (DEVDIS, icr_dev.flags, DEV_V_DIS), REG_HRO },
    { GRDATA  (DEVADDR,   icr_dib.ba, DEV_RDX, 32, 0), REG_HRO },
    { GRDATA  (DEVVEC,   icr_dib.vec, DEV_RDX, 16, 0), REG_HRO },
    { NULL }
    };

MTAB icr_mod[] = {
    { MTAB_XTD|MTAB_VDV, 0, "ADDRESS", NULL,
      NULL, &show_addr, NULL },
    { MTAB_XTD|MTAB_VDV, 0, "VECTOR", NULL,
      NULL, &show_vec, NULL },
    { 0 }
    };

DEVICE icr_dev = {



// --------------------------------------------------------------------- PiDP-11 BMP180 hack
//    "PTR", &icr_unit, icr_reg, icr_mod,
    "ICR", &icr_unit, icr_reg, icr_mod,
// ---------------------------------------------------------------------



    1, 10, 31, 1, DEV_RDX, 8,
    NULL, NULL, &icr_reset,
    NULL, &icr_attach, &icr_detach,
    &icr_dib, DEV_DISABLE | PT_DIS | DEV_UBUS | DEV_QBUS, 0,
    NULL, NULL, NULL, &icr_help, NULL, NULL,
    &icr_description
    };

/* PTP data structures

   icp_dev      PTP device descriptor
   icp_unit     PTP unit descriptor
   icp_reg      PTP register list
*/

#define IOLN_PTP        004

DIB icp_dib = {
    IOBA_AUTO, IOLN_PTP, &icp_rd, &icp_wr,
    1, IVCL (PTP), VEC_AUTO, { NULL }
    };

UNIT icp_unit = {
    UDATA (&icp_svc, UNIT_SEQ+UNIT_ATTABLE, 0), SERIAL_OUT_WAIT
    };

REG icp_reg[] = {
    { GRDATAD (BUF,     icp_unit.buf, DEV_RDX,  8, 0, "last data item processed") },
    { GRDATAD (CSR,          icp_csr, DEV_RDX, 16, 0, "control/status register") },
    { FLDATAD (INT,          int_req, INT_V_PTP,      "interrupt pending flag") },
    { FLDATAD (ERR,          icp_csr, CSR_V_ERR,      "error flag (CSR<15>)") },
    { FLDATAD (DONE,         icp_csr, CSR_V_DONE,     "device done flag (CSR<7>)") },
    { FLDATAD (IE,           icp_csr, CSR_V_IE,       "interrupt enable flag (CSR<6>)") },
    { DRDATAD (POS,     icp_unit.pos, T_ADDR_W,       "position in the output file"), PV_LEFT },
    { DRDATAD (TIME,   icp_unit.wait, 24,             "time from I/O initiation to interrupt"), PV_LEFT },
    { FLDATAD (STOP_IOE, icp_stopioe, 0,              "stop on I/O error") },
    { GRDATA  (DEVADDR,   icp_dib.ba, DEV_RDX, 32, 0), REG_HRO },
    { GRDATA  (DEVVEC,   icp_dib.vec, DEV_RDX, 16, 0), REG_HRO },
    { NULL }
    };

MTAB icp_mod[] = {
    { MTAB_XTD|MTAB_VDV, 0, "ADDRESS", NULL,
      NULL, &show_addr, NULL },
    { MTAB_XTD|MTAB_VDV, 0, "VECTOR", NULL,
      NULL, &show_vec, NULL },
    { 0 }
    };

DEVICE icp_dev = {



// --------------------------------------------------------------------- PiDP-11 BMP180 hack
//    "PTP", &icp_unit, icp_reg, icp_mod,
    "ICP", &icp_unit, icp_reg, icp_mod,
// ---------------------------------------------------------------------



    1, 10, 31, 1, DEV_RDX, 8,
    NULL, NULL, &icp_reset,
    NULL, &icp_attach, &icp_detach,
    &icp_dib, DEV_DISABLE | PT_DIS | DEV_UBUS | DEV_QBUS, 0,
    NULL, NULL, NULL, &icp_help, NULL, NULL,
    &icp_description
    };

/* Paper tape reader I/O address routines */

t_stat icr_rd (int32 *data, int32 PA, int32 access)
{
switch ((PA >> 1) & 01) {                               /* decode PA<1> */

    case 0:                                             /* ptr csr */



// --------------------------------------------------------------------- PiDP-11 BMP180 hack
//        *data = icr_csr & PTRCSR_IMP;
*data = (int32) analogRead(100);
//		printf("Temp: %d C\n", analogRead(100));
// ---------------------------------------------------------------------



        return SCPE_OK;

    case 1:                                             /* ptr buf */
        icr_csr = icr_csr & ~CSR_DONE;
        CLR_INT (PTR);



// --------------------------------------------------------------------- PiDP-11 BMP180 hack
//        *data = icr_unit.buf & 0377;
*data = (int32) analogRead(101);
//		printf("Press: %d Pa\n", analogRead(101));
// ---------------------------------------------------------------------



        return SCPE_OK;
        }

return SCPE_NXM;                                        /* can't get here */
}

t_stat icr_wr (int32 data, int32 PA, int32 access)
{
switch ((PA >> 1) & 01) {                               /* decode PA<1> */

    case 0:                                             /* ptr csr */
        if (PA & 1)
            return SCPE_OK;
        if ((data & CSR_IE) == 0)
            CLR_INT (PTR);
        else if (((icr_csr & CSR_IE) == 0) && (icr_csr & (CSR_ERR | CSR_DONE)))
            SET_INT (PTR);
        if (data & CSR_GO) {
            icr_csr = (icr_csr & ~CSR_DONE) | CSR_BUSY;
            CLR_INT (PTR);
            if (icr_unit.flags & UNIT_ATT)              /* data to read? */
                sim_activate (&icr_unit, icr_unit.wait);  
            else sim_activate (&icr_unit, 0);           /* error if not */
            }
        icr_csr = (icr_csr & ~PTRCSR_RW) | (data & PTRCSR_RW);
        return SCPE_OK;

    case 1:                                             /* ptr buf */
        return SCPE_OK;
        }                                               /* end switch PA */

return SCPE_NXM;                                        /* can't get here */
}

/* Paper tape reader service */

t_stat icr_svc (UNIT *uptr)
{
int32 temp;

icr_csr = (icr_csr | CSR_ERR) & ~CSR_BUSY;
if (icr_csr & CSR_IE) SET_INT (PTR);
if ((icr_unit.flags & UNIT_ATT) == 0)
    return IORETURN (icr_stopioe, SCPE_UNATT);
if ((temp = getc (icr_unit.fileref)) == EOF) {
    if (feof (icr_unit.fileref)) {
        if (icr_stopioe)
            sim_printf ("PTR end of file\n");
        else return SCPE_OK;
        }
    else sim_perror ("PTR I/O error");
    clearerr (icr_unit.fileref);
    return SCPE_IOERR;
    }
icr_csr = (icr_csr | CSR_DONE) & ~CSR_ERR;
icr_unit.buf = temp & 0377;
icr_unit.pos = icr_unit.pos + 1;
return SCPE_OK;
}

/* Paper tape reader support routines */

t_stat icr_reset (DEVICE *dptr)
{
icr_unit.buf = 0;
icr_csr = 0;
if ((icr_unit.flags & UNIT_ATT) == 0)
    icr_csr = icr_csr | CSR_ERR;
CLR_INT (PTR);
sim_cancel (&icr_unit);
return auto_config (dptr->name, 1);
}

t_stat icr_attach (UNIT *uptr, CONST char *cptr)
{
t_stat reason;



// --------------------------------------------------------------------- PiDP-11 BMP180 hack
	bmp180Setup (100);
// ---------------------------------------------------------------------



reason = attach_unit (uptr, cptr);
if ((icr_unit.flags & UNIT_ATT) == 0)
    icr_csr = icr_csr | CSR_ERR;
else icr_csr = icr_csr & ~CSR_ERR;
return reason;
}

t_stat icr_detach (UNIT *uptr)
{
icr_csr = icr_csr | CSR_ERR;
return detach_unit (uptr);
}

/* Paper tape punch I/O address routines */

t_stat icp_rd (int32 *data, int32 PA, int32 access)
{
switch ((PA >> 1) & 01) {                               /* decode PA<1> */

    case 0:                                             /* ptp csr */
        *data = icp_csr & PTPCSR_IMP;
        return SCPE_OK;

    case 1:                                             /* ptp buf */
        *data = icp_unit.buf;
        return SCPE_OK;
        }

return SCPE_NXM;                                        /* can't get here */
}

t_stat icp_wr (int32 data, int32 PA, int32 access)
{
switch ((PA >> 1) & 01) {                               /* decode PA<1> */

    case 0:                                             /* ptp csr */
        if (PA & 1)
            return SCPE_OK;
        if ((data & CSR_IE) == 0)
            CLR_INT (PTP);
        else if (((icp_csr & CSR_IE) == 0) && (icp_csr & (CSR_ERR | CSR_DONE)))
            SET_INT (PTP);
        icp_csr = (icp_csr & ~PTPCSR_RW) | (data & PTPCSR_RW);
        return SCPE_OK;

    case 1:                                             /* ptp buf */
        if ((PA & 1) == 0)
            icp_unit.buf = data & 0377;
        icp_csr = icp_csr & ~CSR_DONE;
        CLR_INT (PTP);
        if (icp_unit.flags & UNIT_ATT)                  /* file to write? */
            sim_activate (&icp_unit, icp_unit.wait);
        else sim_activate (&icp_unit, 0);               /* error if not */
        return SCPE_OK;
        }                                               /* end switch PA */

return SCPE_NXM;                                        /* can't get here */
}

/* Paper tape punch service */

t_stat icp_svc (UNIT *uptr)
{
icp_csr = icp_csr | CSR_ERR | CSR_DONE;
if (icp_csr & CSR_IE)
    SET_INT (PTP);
if ((icp_unit.flags & UNIT_ATT) == 0)
    return IORETURN (icp_stopioe, SCPE_UNATT);
if (putc (icp_unit.buf, icp_unit.fileref) == EOF) {
    sim_perror ("PTP I/O error");
    clearerr (icp_unit.fileref);
    return SCPE_IOERR;
    }
icp_csr = icp_csr & ~CSR_ERR;
icp_unit.pos = icp_unit.pos + 1;
return SCPE_OK;
}

/* Paper tape punch support routines */

t_stat icp_reset (DEVICE *dptr)
{
icp_unit.buf = 0;
icp_csr = CSR_DONE;
if ((icp_unit.flags & UNIT_ATT) == 0)
    icp_csr = icp_csr | CSR_ERR;
CLR_INT (PTP);
sim_cancel (&icp_unit);                                 /* deactivate unit */
return auto_config (dptr->name, 1);
}

t_stat icp_attach (UNIT *uptr, CONST char *cptr)
{
t_stat reason;

reason = attach_unit (uptr, cptr);
if ((icp_unit.flags & UNIT_ATT) == 0)
    icp_csr = icp_csr | CSR_ERR;
else icp_csr = icp_csr & ~CSR_ERR;
return reason;
}

t_stat icp_detach (UNIT *uptr)
{
icp_csr = icp_csr | CSR_ERR;
return detach_unit (uptr);
}

t_stat icr_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr)
{


// --------------------------------------------------------------------- PiDP-11 BMP180 hack
fprintf (st, "I2C BMP180 Sensor Reader (ICR)\n\n");
fprintf (st, "Crude hack. Do 'att icr junk.txt' to enable the i2c sensor,\n");
fprintf (st, "then 'ex 177716100' gets you the temperature (in C, *10, in octal!)\n");
fprintf (st, "then 'ex 177716102' gets you the pressure (in Pa, divide by 10000 for bar, in octal!)n");
fprintf (st, "The stuff below is from the PTR device, whose code we copied...\n\n");
fprint_set_help (st, dptr);
fprint_show_help (st, dptr);
fprint_reg_help (st, dptr);
fprintf (st, "\nError handling is as follows:\n\n");
fprintf (st, "    error         STOP_IOE   processed as\n");
fprintf (st, "    not attached  1          report error and stop\n");
fprintf (st, "                  0          out of tape\n\n");
fprintf (st, "    end of file   1          report error and stop\n");
fprintf (st, "                  0          out of tape\n");
fprintf (st, "    OS I/O error  x          report error and stop\n");
// ---------------------------------------------------------------------


return SCPE_OK;
}

const char *icr_description (DEVICE *dptr)
{
return "I2C device: reader for BMP180 sensor";
}

t_stat icp_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, const char *cptr)
{


// --------------------------------------------------------------------- PiDP-11 BMP180 hack
fprintf (st, "I2c Paper Tape Punch (ICP)\n\n");
fprintf (st, "Unused so far. The stuff below is from the PTP device, whose code we copied...\n\n");
// ---------------------------------------------------------------------



fprintf (st, "The paper tape punch (PTP) writes data to a disk file.  The POS register\n");
fprintf (st, "specifies the number of the next data item to be written.  Thus, by changing\n");
fprintf (st, "POS, the user can backspace or advance the punch.\n");
fprint_set_help (st, dptr);
fprint_show_help (st, dptr);
fprint_reg_help (st, dptr);
fprintf (st, "\nError handling is as follows:\n\n");
fprintf (st, "    error         STOP_IOE   processed as\n");
fprintf (st, "    not attached  1          report error and stop\n");
fprintf (st, "                  0          out of tape\n\n");
fprintf (st, "    OS I/O error  x          report error and stop\n");
return SCPE_OK;
}

const char *icp_description (DEVICE *dptr)
{
return "I2C paper tape punch";
}
