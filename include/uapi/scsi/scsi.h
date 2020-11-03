#ifndef _UAPI_SCSI_SCSI_H
#define _UAPI_SCSI_SCSI_H

/*
 *  SENSE KEYS
 */

#define NO_SENSE        0x00
#define RECOVERED_ERROR 0x01
#define NOT_READY       0x02
#define MEDIUM_ERROR    0x03
#define HARDWARE_ERROR  0x04
#define ILLEGAL_REQUEST 0x05
#define UNIT_ATTENTION  0x06
#define DATA_PROTECT    0x07
#define BLANK_CHECK     0x08
#define VENDOR_SPECIFIC 0x09
#define COPY_ABORTED    0x0a
#define ABORTED_COMMAND 0x0b
#define VOLUME_OVERFLOW 0x0d
#define MISCOMPARE      0x0e

#endif
