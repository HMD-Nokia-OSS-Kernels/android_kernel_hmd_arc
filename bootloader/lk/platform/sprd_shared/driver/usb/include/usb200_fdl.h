/******************************************************************************
 ** File Name:    USB200.h                                                    *
 ** Author:       Daniel.Ding                                                 *
 ** DATE:         3/9/2005                                                    *
 ** Copyright:    2005 Spreatrum, Incoporated. All Rights Reserved.           *
 ** Description:                                                              *
 ******************************************************************************/
/******************************************************************************
 **                   Edit    History                                         *
 **---------------------------------------------------------------------------*
 ** DATE          NAME            DESCRIPTION                                 *
 ** 9/1/2003      Daniel.Ding     Create.                                     *
 ******************************************************************************/
#ifndef _USB200_FDL_H_
#define _USB200_FDL_H_
/*----------------------------------------------------------------------------*
 **                         Dependencies                                      *
 **-------------------------------------------------------------------------- */

#include "sprd_common.h"
/**---------------------------------------------------------------------------*
 **                             Compiler Flag                                 *
 **---------------------------------------------------------------------------*/
#ifdef   __cplusplus
extern   "C"
{
#endif
/**----------------------------------------------------------------------------*
**                               Micro Define                                 **
**----------------------------------------------------------------------------*/
#define MAXIMUM_USB_STRING_LENGTH 255

#define USB_GETSTATUS_SELF_POWERED                0x01
#define USB_GETSTATUS_REMOTE_WAKEUP_ENABLED       0x02

/* Descriptor Types */
#define USB_DEVICE_DESCRIPTOR_TYPES               0x01
#define USB_CONFIGURATION_DESCRIPTOR_TYPES        0x02
#define USB_STRING_DESCRIPTOR_TYPES               0x03
#define USB_INTERFACE_DESCRIPTOR_TYPES            0x04
#define USB_ENDPOINT_DESCRIPTOR_TYPES             0x05

/* 
 * Usb Standard Requests
 */
#define USB_REQUESTS_GET_STATUS                    0x00
#define USB_REQUESTS_CLEAR_FEATURE                 0x01
#define USB_REQUESTS_SET_FEATURE                   0x03
#define USB_REQUESTS_SET_ADDRESS                   0x05
#define USB_REQUESTS_GET_DESCRIPTOR                0x06
#define USB_REQUESTS_SET_DESCRIPTOR                0x07
#define USB_REQUESTS_GET_CONFIGURATION             0x08
#define USB_REQUESTS_SET_CONFIGURATION             0x09
#define USB_REQUESTS_GET_INTERFACE                 0x0A
#define USB_REQUESTS_SET_INTERFACE                 0x0B
#define USB_REQUESTS_SYNC_FRAME                    0x0C

//Define direction ;
#define HOST_IN     0x01        //From device to host ;
#define HOST_OUT    0x00        //From host to device ;

//Define request type;
#define USB_REQ_STANDARD    0x00
#define USB_REQ_CLASS       0x01
#define USB_REQ_VENDOR      0x02
#define USB_REQ_RESERVED    0x03

//Define accepter ;
#define USB_REC_DEVICE      0x00
#define USB_REC_INTERFACE   0x01
#define USB_REC_ENDPOINT    0x02
#define USB_REC_OTHER       0x03

#define USB_MAX_REQ_TYPE    0x04
#define USB_MAX_RECIPIENT   0x05
#define USB_MAX_REQUEST     0x30

#define BUS_POWERED                           0x80
#define SELF_POWERED                          0x40
#define REMOTE_WAKEUP                         0x20

//
// USB power descriptor added to core specification
//

#define USB_SUPPORT_D0_COMMAND      0x01
#define USB_SUPPORT_D1_COMMAND      0x02
#define USB_SUPPORT_D2_COMMAND      0x04
#define USB_SUPPORT_D3_COMMAND      0x08

#define USB_SUPPORT_D1_WAKEUP       0x10
#define USB_SUPPORT_D2_WAKEUP       0x20

/**----------------------------------------------------------------------------*
**                             Data Prototype                                 **
**----------------------------------------------------------------------------*/
//Stand USB device request command type define ;
typedef union _USB_STD_DEV_REQ_T
{
    struct
    {
        union
        {
            struct bit_map
            {
                char direction :1;
                char command   :2;
                char receiver  :5;
            } bits;
            char octet;
        } uReqType;
        char  bReqCode;
        short wValue;
        short wIndex;
        short wLength;
    } sData;
    char bBuf[8];
} USB_STD_DEV_REQ_U;

typedef union
{
    struct
    {
        short   type    :8;
        short   index   :8;
    } sDescriptor;
    short wValue;
} VALUE_U;

//Four bytes buildup a 32bit value ;
typedef union _USB_DWORD_U
{
    union
    {
        struct
        {
            char  c0;
            char  c1;
            char  c2;
            char  c3;
        } sOcts;
        char bBuf[4];
    } uData;
    int dwValue;
} USB_DWORD_U;


//Two bytes buildup a 16bit value ;
typedef union _USB_WORD_U
{
    union
    {
        struct
        {
            char  c0;
            char  c1;
        } sOcts;
        char bBuf[2];
    } uData;
    int wValue;
} USB_WORD_U;

//
// values for bmAttributes Field in
// USB_CONFIGURATION_DESCRIPTOR
//


//
// Standard USB HUB definitions
//
// See Chapter 11
//
typedef struct _USB_DEVICE_DESCRIPTOR
{
    char bLength;
    char bDescriptorType;
    short bcdUSB;
    char bDeviceClass;
    char bDeviceSubClass;
    char bDeviceProtocol;
    char bMaxPacketSize0;
    short idVendor;
    short idProduct;
    short bcdDevice;
    char iManufacturer;
    char iProduct;
    char iSerialNumber;
    char bNumConfigurations;
} USB_DEVICE_DESCRIPTOR, *PUSB_DEVICE_DESCRIPTOR;

typedef struct _USB_CONFIGURATION_DESCRIPTOR
{
    char bLength;
    char bDescriptorType;
    short wTotalLength;
    char bNumInterfaces;
    char bConfigurationValue;
    char iConfiguration;
    char bmAttributes;
    char MaxPower;
} USB_CONFIGURATION_DESCRIPTOR, *PUSB_CONFIGURATION_DESCRIPTOR;

typedef struct _USB_INTERFACE_DESCRIPTOR
{
    char bLength;
    char bDescriptorType;
    char bInterfaceNumber;
    char bAlternateSetting;
    char bNumEndpoints;
    char bInterfaceClass;
    char bInterfaceSubClass;
    char bInterfaceProtocol;
    char iInterface;
} USB_INTERFACE_DESCRIPTOR, *PUSB_INTERFACE_DESCRIPTOR;

typedef struct _USB_STRING_DESCRIPTOR
{
    char bLength;
    char bDescriptorType;
    char bString[1];
} USB_STRING_DESCRIPTOR, *PUSB_STRING_DESCRIPTOR;

typedef struct _USB_ENDPOINT_DESCRIPTOR
{
    char bLength;
    char bDescriptorType;
    union _bEndpointAddress
    {
        struct
        {
            char direction      :1;
            char reserved       :3;
            char number         :4;
        } bits;
        char value;
    } bEndpointAddress;
    union _bmAttributes
    {
        struct
        {
            char reserved       :2;
            char usage_type     :2;
            char sync_type      :2;
            char trans_type     :2;
        } bits;
        char value;
    } bmAttributes;
    short wMaxPacketSize;
    char bInterval;
} USB_ENDPOINT_DESCRIPTOR, *PUSB_ENDPOINT_DESCRIPTOR;


typedef struct _USB_HUB_DESCRIPTOR
{
    char        bDescriptorLength;      // Length of this descriptor
    char        bDescriptorType;        // Hub configuration type
    char        bNumberOfPorts;         // number of ports on this hub
    short       wHubCharacteristics;    // Hub Charateristics
    char        bPowerOnToPowerGood;    // port power on till power good in 2ms
    char        bHubControlCurrent;     // max current in mA
    //
    // room for 255 ports power control and removable bitmask
    char        bRemoveAndPowerMask[64];
} USB_HUB_DESCRIPTOR, *PUSB_HUB_DESCRIPTOR;


typedef struct _USB_STRING_LANGUAGE_DESCRIPTOR
{
    char  bLength;
    char  bDescriptorType;
    short ulanguageID;
} USB_STRING_LANGUAGE_DESCRIPTOR,*PUSB_STRING_LANGUAGE_DESCRIPTOR;

typedef struct _USB_STRING_INTERFACE_DESCRIPTOR
{
    char  bLength;
    char  bDescriptorType;
    char  Interface[22];
} USB_STRING_INTERFACE_DESCRIPTOR,*PUSB_STRING_INTERFACE_DESCRIPTOR;

typedef struct _USB_STRING_CONFIGURATION_DESCRIPTOR
{
    char  bLength;
    char  bDescriptorType;
    char  Configuration[16];
} USB_STRING_CONFIGURATION_DESCRIPTOR,*PUSB_STRING_CONFIGURATION_DESCRIPTOR;

typedef struct _USB_STRING_SERIALNUMBER_DESCRIPTOR
{
    char  bLength;
    char  bDescriptorType;
    char  SerialNum[24];
} USB_STRING_SERIALNUMBER_DESCRIPTOR,*PUSB_STRING_SERIALNUMBER_DESCRIPTOR;

typedef struct _USB_STRING_PRODUCT_DESCRIPTOR
{
    char  bLength;
    char  bDescriptorType;
    char  Product[30];
} USB_STRING_PRODUCT_DESCRIPTOR,*PUSB_STRING_PRODUCT_DESCRIPTOR;

typedef struct _USB_STRING_MANUFACTURER_DESCRIPTOR
{
    char  bLength;
    char  bDescriptorType;
    char  Manufacturer[24];
} USB_STRING_MANUFACTURER_DESCRIPTOR,*PUSB_STRING_MANUFACTURER_DESCRIPTOR;

typedef struct _USB_POWER_DESCRIPTOR
{
    char bLength;
    char bDescriptorType;
    char bCapabilitiesFlags;
    short EventNotification;
    short D1LatencyTime;
    short D2LatencyTime;
    short D3LatencyTime;
    char PowerUnit;
    short D0PowerConsumption;
    short D1PowerConsumption;
    short D2PowerConsumption;
} USB_POWER_DESCRIPTOR, *PUSB_POWER_DESCRIPTOR;


typedef struct _USB_COMMON_DESCRIPTOR
{
    char bLength;
    char bDescriptorType;
} USB_COMMON_DESCRIPTOR, *PUSB_COMMON_DESCRIPTOR;

/**----------------------------------------------------------------------------*
**                         Local Function Prototype                           **
**----------------------------------------------------------------------------*/

/**----------------------------------------------------------------------------*
**                           Function Prototype                               **
**----------------------------------------------------------------------------*/


/**----------------------------------------------------------------------------*
**                         Compiler Flag                                      **
**----------------------------------------------------------------------------*/
#ifdef   __cplusplus
}
#endif
/**---------------------------------------------------------------------------*/
#endif
// End
