/**
 * @file    HART.h
 * @brief   HART protocol driver — public interface.
 *
 * Target   : ATmega8 @ 8 MHz
 * Protocol : HART (Highway Addressable Remote Transducer), digital framing.
 *            The FSK modem (e.g. AD5700) is external; this driver only
 *            handles the digital frame format.
 *
 * Frame format:
 *
 *   | Preamble | Start | Address  | Command | Len | Data | Checksum |
 *   | 2 bytes  | 1     | 1 or 5 B | 1       | 1   | N    | 1        |
 *
 * The driver supports a single device ID and the master/slave roles
 * defined by the HART specification.  Burst mode, multi-drop address
 * scanning, and long-frame auto-detection are not implemented.
 */

#ifndef HART_H
#define HART_H

#include <math.h>
#include <iom8v.h>

/*============================================================================
 * Target selection (define exactly one)
 *==========================================================================*/
#define ATMEGA8
/* #define ATMEGA16 */
/* #define ATMEGA32 */

/*============================================================================
 * Fuse settings (for reference only — program with avrdude)
 *
 *   ATmega8  : lock = 0x00, high = 0xD9, low = 0xE4  (internal 8 MHz)
 *   ATmega16 : lock = 0x00, high = 0xD9, low = 0xEF
 *   ATmega32 : lock = 0x00, high = 0xD9, low = 0xEF
 *==========================================================================*/

/*============================================================================
 * Clock selection (define exactly one)
 *==========================================================================*/
#define XT8000
#define F_CPU 8000000UL

/*============================================================================
 * Generic bit / register helpers
 *==========================================================================*/
#define BIT(x)              (1 << (x))
#define BV(bit)             (1 << (bit))
#define cbi(reg, bit)       ((reg) &= ~BV(bit))
#define sbi(reg, bit)       ((reg) |=  BV(bit))
#define outb(addr, data)    ((addr) = (data))
#define inb(addr)           ((addr))

/* Inline assembly helpers */
#define WDR()   asm("wdr")
#define SEI()   asm("sei")
#define CLI()   asm("cli")
#define NOP()   asm("nop")

/* Port address helpers */
#define DDR(x)  ((x) - 1)
#define PIN(x)  ((x) - 2)

/*============================================================================
 * Boolean / utility macros
 *==========================================================================*/
#define TRUE    (-1)
#define FALSE   (0)

#define MIN(a, b)   ((a) < (b) ? (a) : (b))
#define MAX(a, b)   ((a) > (b) ? (a) : (b))
#define ABS(x)      ((x) > 0 ? (x) : (-x))

#define PI          3.14159265359

/*============================================================================
 * Fixed-width type aliases
 *==========================================================================*/
typedef unsigned long  u32;
typedef unsigned int   u16;
typedef unsigned char  u08;

/*============================================================================
 * UART configuration
 *==========================================================================*/
#define UsingUART
#define UART_mode       1       /* 0 = polling, 1 = interrupt */
#define UART_Interrupt
#define UART_Interrupt_TxMode
#define BaudRate38400

/*============================================================================
 * I2C port mapping (kept for compatibility with sibling modules)
 *==========================================================================*/
#if defined ATMEGA16 || defined ATMEGA32
    #define PORTi2c     PORTC
    #define DDRi2c      DDRC
#elif defined ATMEGA64 || defined ATMEGA128
    #define PORTi2c     PORTD
    #define DDRi2c      DDRD
#endif

/*============================================================================
 * HART configuration
 *==========================================================================*/
#define UsingHART
#define MAX_HART_Buffer_Length  120
#define HART_MyDeviceID         2

/*============================================================================
 * HART message buffer — layout
 *
 *   [0]   Preamble 1
 *   [1]   Preamble 2
 *   [2]   Start character
 *   [3]   Address byte 1 (MSB)
 *   [4]   Address byte 2
 *   [5]   Address byte 3
 *   [6]   Address byte 4
 *   [7]   Address byte 5 (LSB)
 *   [8]   Command
 *   [9]   Data length
 *   [10 .. 10+DataLen-1]  Data / status bytes
 *   [10+DataLen]          Checksum
 *==========================================================================*/
#if defined UsingHART

extern volatile unsigned char HART_Msg[MAX_HART_Buffer_Length];

#define HART_Preamble1          HART_Msg[0]
#define HART_Preamble2          HART_Msg[1]
#define HART_StartChar          HART_Msg[2]
#define HART_Adrs_ID_1          HART_Msg[3]
#define HART_Adrs_ID_2          HART_Msg[4]
#define HART_Adrs_ID_3          HART_Msg[5]
#define HART_Adrs_ID_4          HART_Msg[6]
#define HART_Adrs_ID_5          HART_Msg[7]
#define HART_Command            HART_Msg[8]
#define HART_DataLen            HART_Msg[9]
#define HART_StatusH            HART_Msg[10]
#define HART_StatusL            HART_Msg[11]

/**
 * @brief  Parsed frame-header flags.
 *
 * Populated by ReceiveMessage() from the start character and the first
 * address byte.
 */
typedef struct
{
    unsigned Master2Slave  : 1;  /* 1 = master → slave, 0 = slave → master */
    unsigned ShortFrame    : 1;  /* 1 = 1-byte address, 0 = 5-byte address  */
    unsigned MessageType   : 1;  /* 1 = STX, 0 = ACK                        */
    unsigned PrimaryMaster : 1;  /* 1 = primary master, 0 = secondary       */
} HART_Flags_t;

extern volatile HART_Flags_t HART_Flags;

/* Legacy alias so existing code using `bits1` keeps compiling. */
#define bits1 HART_Flags

/*============================================================================
 * HART return codes
 *==========================================================================*/
enum
{
    HART_OK             = 100,  /* Message received and validated */
    HART_ERR_SHORT      = 0,    /* RX buffer contains too few bytes */
    HART_ERR_PREAMBLE   = 1,    /* Fewer than 2 preambles */
    HART_ERR_STARTCHAR  = 2,    /* Unrecognised start character */
    HART_ERR_MSGTYPE    = 3,    /* Start-char bits 0..2 not 2 or 6 */
    HART_ERR_ADDRESS    = 4,    /* Device address does not match */
    HART_ERR_CHECKSUM   = 5,    /* Checksum mismatch */
    HART_ERR_TIMEOUT    = 6     /* Reception timed out */
};

#endif /* UsingHART */

/*============================================================================
 * Serial (UART) driver — provided by uart.c
 *==========================================================================*/
#if defined UsingUART
void          uart0_init(unsigned char mode);
void          FlushUART(void);
void          FlushTxBuffer(void);
unsigned char TransmitByte(unsigned char data);
unsigned char ReceiveByte(void);
void          EchoSerialPort(void);
extern volatile unsigned char NumOfByteInRxBuffer;
extern volatile unsigned char Status;
void          SetOverTimeControl(unsigned int value);
#endif

/*============================================================================
 * HART driver — provided by HART.c
 *==========================================================================*/
#if defined UsingHART
void          HART_ini(void);
void          Set_HART_DeviceID(unsigned long DeviceID);
void          Set_HART_MyDeviceID(void);
unsigned char Compare_HART_DeviceID(void);
unsigned char SendCommand(unsigned char Command, unsigned char DataBytesLen);
unsigned char ReceiveMessage(void);
unsigned char CheckSum(void);
#endif

#endif /* HART_H */