/**
 * @file    HART.c
 * @brief   HART protocol driver — implementation.
 *
 * Handles the digital portion of HART communication: frame construction,
 * transmission, reception, and validation.  The FSK modem is external.
 */

#define HART
#include "HART.h"

#if defined UsingHART

/*============================================================================
 * Module state
 *==========================================================================*/

/* HART message buffer (layout documented in HART.h). */
volatile unsigned char HART_Msg[MAX_HART_Buffer_Length];

/* Parsed frame-header flags. */
volatile HART_Flags_t HART_Flags;

/*============================================================================
 * Initialisation
 *==========================================================================*/

/**
 * @brief  Initialise the HART frame header with default values.
 *
 * Sets preambles, start character (long frame, master → slave, STX),
 * manufacturer / device-type bytes, and the local device ID.
 */
void HART_ini(void)
{
    HART_Preamble1 = 0xFF;      /* Preamble 1                              */
    HART_Preamble2 = 0xFF;      /* Preamble 2                              */
    HART_StartChar = 0x82;      /* Long frame, master → slave, STX         */
    HART_Adrs_ID_1 = 0x80;      /* Primary master, manufacturer code 0     */
    HART_Adrs_ID_2 = 0x32;      /* Device type code (5850s)                */

    Set_HART_MyDeviceID();
}

/*============================================================================
 * Device ID helpers
 *==========================================================================*/

/**
 * @brief  Store a 24-bit device ID in the message header.
 *
 * @param  DeviceID  Device ID (only the lower 24 bits are used).
 */
void Set_HART_DeviceID(unsigned long DeviceID)
{
    HART_Adrs_ID_3 = (unsigned char)(DeviceID >> 16);   /* MSB */
    HART_Adrs_ID_4 = (unsigned char)(DeviceID >> 8);
    HART_Adrs_ID_5 = (unsigned char)(DeviceID);         /* LSB */
}

/**
 * @brief  Store the local device ID (HART_MyDeviceID) in the header.
 */
void Set_HART_MyDeviceID(void)
{
    HART_Adrs_ID_3 = 0x00;
    HART_Adrs_ID_4 = 0x00;
    HART_Adrs_ID_5 = (unsigned char)HART_MyDeviceID;
}

/**
 * @brief  Check whether the address in the current message targets us.
 *
 * @return 1 on match, 0 otherwise.
 */
unsigned char Compare_HART_DeviceID(void)
{
    if ((HART_Adrs_ID_3 == 0x00) &&
        (HART_Adrs_ID_4 == 0x00) &&
        (HART_Adrs_ID_5 == HART_MyDeviceID))
    {
        return 1;
    }

    return 0;
}

/*============================================================================
 * Frame construction and transmission
 *==========================================================================*/

/**
 * @brief  Build and transmit a HART frame.
 *
 * The caller must populate HART_Msg[10 .. 10+DataBytesLen-1] with the
 * data payload before calling.  Header fields and checksum are filled
 * in automatically.
 *
 * @param  Command        HART command byte.
 * @param  DataBytesLen   Number of data bytes that follow.
 *
 * @return 1 on success, 0 if the UART reported a transmission error.
 */
unsigned char SendCommand(unsigned char Command, unsigned char DataBytesLen)
{
    unsigned char i;
    unsigned char errors = 0;

    HART_Command = Command;
    HART_DataLen = DataBytesLen;

    /* Append the checksum after the last data byte. */
    HART_Msg[10 + DataBytesLen] = CheckSum();

    FlushUART();

    for (i = 0; i <= (10 + DataBytesLen); i++)
    {
        errors += TransmitByte(HART_Msg[i]);
    }

    return (errors > 0) ? 0 : 1;
}

/*============================================================================
 * Checksum
 *==========================================================================*/

/**
 * @brief  Compute the HART checksum over HART_Msg[].
 *
 * XOR of every byte from the start character (HART_Msg[2]) through the
 * last data byte (HART_Msg[9 + HART_DataLen]).  Preambles are excluded.
 *
 * @return The computed checksum byte.
 */
unsigned char CheckSum(void)
{
    unsigned char i;
    unsigned char sum = 0;

    for (i = 2; i < (HART_DataLen + 10); i++)
    {
        sum ^= HART_Msg[i];
    }

    return sum;
}

/*============================================================================
 * Frame reception
 *==========================================================================*/

/**
 * @brief  Receive and validate one HART frame from the UART.
 *
 * Expects a complete frame to already sit in the UART RX buffer.
 * Strips the preamble, parses the start character and address, verifies
 * the device ID and checksum, and returns a status code.
 *
 * @return HART_OK on success, or one of the HART_ERR_* codes on failure.
 */
unsigned char ReceiveMessage(void)
{
    unsigned char i;
    unsigned char MessageCounter;
    unsigned char k;
    unsigned char Preambles;

    /* --- 1. Sanity check on RX buffer size --- */
    if (NumOfByteInRxBuffer < 11)
    {
        return HART_ERR_SHORT;
    }

    SetOverTimeControl(100);

    MessageCounter = NumOfByteInRxBuffer;
    Preambles      = 0;

    /* --- 2. Count leading 0xFF preambles --- */
    for (i = 0; i < MessageCounter; i++)
    {
        k = ReceiveByte();

        if (k == 0xFF)
        {
            Preambles++;
        }
        else
        {
            HART_StartChar = k;
            break;
        }
    }

    if (Preambles < 2)
    {
        return HART_ERR_PREAMBLE;
    }

    /* Store the preambles in the message buffer. */
    for (i = 0; i < Preambles; i++)
    {
        HART_Msg[i] = 0xFF;
    }

    MessageCounter = Preambles + 1;

    /* --- 3. Decode the start character --- */
    switch (HART_StartChar)
    {
        case 0x02:  /* Short frame, master → slave, STX */
            HART_Flags.Master2Slave = 1;
            HART_Flags.ShortFrame   = 1;
            HART_Flags.MessageType  = 1;
            break;

        case 0x06:  /* Short frame, slave → master, ACK */
            HART_Flags.Master2Slave = 0;
            HART_Flags.ShortFrame   = 1;
            HART_Flags.MessageType  = 0;
            break;

        case 0x82:  /* Long frame, master → slave, STX */
            HART_Flags.Master2Slave = 1;
            HART_Flags.ShortFrame   = 0;
            HART_Flags.MessageType  = 1;
            break;

        case 0x86:  /* Long frame, slave → master, ACK */
            HART_Flags.Master2Slave = 0;
            HART_Flags.ShortFrame   = 0;
            HART_Flags.MessageType  = 0;
            break;

        default:
            return HART_ERR_STARTCHAR;
    }

    /* --- 4. Read the address --- */
    HART_Adrs_ID_1 = ReceiveByte();
    MessageCounter++;

    HART_Flags.PrimaryMaster = (HART_Adrs_ID_1 & 0x80) ? 1 : 0;

    if (!HART_Flags.ShortFrame)
    {
        /* Long frame: 4 more address bytes follow. */
        HART_Adrs_ID_2 = ReceiveByte(); MessageCounter++;
        HART_Adrs_ID_3 = ReceiveByte(); MessageCounter++;
        HART_Adrs_ID_4 = ReceiveByte(); MessageCounter++;
        HART_Adrs_ID_5 = ReceiveByte(); MessageCounter++;
    }
    else
    {
        /* Short frame: only the low nibble of ID_1 is the address. */
        HART_Adrs_ID_2 = 0x00;
        HART_Adrs_ID_3 = 0x00;
        HART_Adrs_ID_4 = 0x00;
        HART_Adrs_ID_5 = (HART_Adrs_ID_1 & 0x0F);
    }

    /* --- 5. Verify the device address --- */
    if (!Compare_HART_DeviceID())
    {
        return HART_ERR_ADDRESS;
    }

    /* --- 6. Read command, length, and data --- */
    HART_Command = ReceiveByte();
    MessageCounter++;

    HART_DataLen = ReceiveByte();
    MessageCounter++;

    for (i = 10; i < (HART_DataLen + 10); i++)
    {
        HART_Msg[i] = ReceiveByte();
        MessageCounter++;
    }

    /* --- 7. Verify checksum --- */
    i = ReceiveByte();

    if (i != CheckSum())
    {
        return HART_ERR_CHECKSUM;
    }

    /* --- 8. Verify the receive did not time out --- */
    if (Status & BIT(5))
    {
        return HART_ERR_TIMEOUT;
    }

    return HART_OK;
}

#endif /* UsingHART */