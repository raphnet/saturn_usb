#ifndef __usbconfig_h_included__
#define __usbconfig_h_included__

#define USB_CFG_IOPORTNAME      D
#define USB_CFG_DMINUS_BIT      0
#define USB_CFG_DPLUS_BIT       2
#define USB_CFG_CLOCK_KHZ       (F_CPU/1000)
#define USB_CFG_HAVE_INTRIN_ENDPOINT    1
#define USB_CFG_HAVE_INTRIN_ENDPOINT3   0
#define USB_CFG_IMPLEMENT_HALT          0
#define USB_CFG_INTR_POLL_INTERVAL      10
#define USB_CFG_IS_SELF_POWERED         0
#define USB_CFG_MAX_BUS_POWER           100
#define USB_CFG_IMPLEMENT_FN_WRITE      0
#define USB_CFG_IMPLEMENT_FN_READ       0
#define USB_CFG_IMPLEMENT_FN_WRITEOUT   0
#define USB_CFG_HAVE_FLOWCONTROL        0
#define USB_COUNT_SOF                   0
/* -------------------------- Device Description --------------------------- */

/* This is the Dracal technologies inc. (Raphnet technologies) VID */
#define USB_CFG_VENDOR_ID       0x9B, 0x28
#define USB_CFG_DEVICE_ID       0x05, 0x00 // Saturn joystick
#define USB_CFG_DEVICE_VERSION  0x00, 0x02

#define USB_CFG_VENDOR_NAME     'r', 'a', 'p', 'h', 'n', 'e', 't', '.', 'n', 'e', 't'
#define USB_CFG_VENDOR_NAME_LEN 11

#define USB_CFG_DEVICE_NAME     'S','a','t','u','r','n','_','A','d','a','p','t','e','r','_','2','.','0'
#define USB_CFG_DEVICE_NAME_LEN 	18

#define USB_CFG_SERIAL_NUMBER_LEN   4

#define USB_CFG_DEVICE_CLASS        0
#define USB_CFG_DEVICE_SUBCLASS     0
#define USB_CFG_INTERFACE_CLASS     3   /* HID */
#define USB_CFG_INTERFACE_SUBCLASS  0
#define USB_CFG_INTERFACE_PROTOCOL  0

#define USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH    0  /* total length of report descriptor */

#define USB_CFG_DESCR_PROPS_DEVICE                  USB_PROP_IS_DYNAMIC
#define USB_CFG_DESCR_PROPS_CONFIGURATION           (USB_PROP_IS_DYNAMIC | USB_PROP_IS_RAM)
#define USB_CFG_DESCR_PROPS_STRINGS                 0
#define USB_CFG_DESCR_PROPS_STRING_0                0
#define USB_CFG_DESCR_PROPS_STRING_VENDOR           0
#define USB_CFG_DESCR_PROPS_STRING_PRODUCT          0
#define USB_CFG_DESCR_PROPS_STRING_SERIAL_NUMBER    USB_PROP_LENGTH((6*2))
#define USB_CFG_DESCR_PROPS_HID                     0
#define USB_CFG_DESCR_PROPS_HID_REPORT              USB_PROP_IS_DYNAMIC
#define USB_CFG_DESCR_PROPS_UNKNOWN                 0

#endif /* __usbconfig_h_included__ */
