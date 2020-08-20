#include <NfcAdapter.h>

NfcAdapter::NfcAdapter(PN532Interface &interface, uint8_t *staticBuf, unsigned int staticBufSize)
{
    shield = new PN532(interface);
	_staticBufSize = staticBufSize;
	_staticBuf = staticBuf;	
}

NfcAdapter::~NfcAdapter(void)
{
    delete shield;
}

void NfcAdapter::begin(boolean verbose)
{
    shield->begin();

    uint32_t versiondata = shield->getFirmwareVersion();

    if (! versiondata)
    {
#ifdef NDEF_USE_SERIAL
        Serial.print(F("Didn't find PN53x board"));
#endif
        while (1); // halt
    }

    if (verbose)
    {
#ifdef NDEF_USE_SERIAL
        Serial.print(F("Found chip PN5")); Serial.println((versiondata>>24) & 0xFF, HEX);
        Serial.print(F("Firmware ver. ")); Serial.print((versiondata>>16) & 0xFF, DEC);
        Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
#endif
    }
    // configure board to read RFID tags
    shield->SAMConfig();
}

boolean NfcAdapter::tagPresent(unsigned long timeout)
{
    uint8_t success;
    uidLength = 0;

    if (timeout == 0)
    {
        success = shield->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, (uint8_t*)&uidLength);
    }
    else
    {
        success = shield->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, (uint8_t*)&uidLength, timeout);
    }
    return success;
}

boolean NfcAdapter::erase()
{
    NdefMessage message = NdefMessage();
    message.addEmptyRecord();
    return write(message);
}

boolean NfcAdapter::format()
{
    boolean success;
#ifdef NDEF_SUPPORT_MIFARE_CLASSIC
    if (uidLength == 4)
    {
        MifareClassic mifareClassic = MifareClassic(*shield, _staticBuf, _staticBufSize);
        success = mifareClassic.formatNDEF(uid, uidLength);
    }
    else
#endif
    {
#ifdef NDEF_USE_SERIAL
        Serial.print(F("Unsupported Tag."));
#endif
        success = false;
    }
    return success;
}

boolean NfcAdapter::clean()
{
    uint8_t type = guessTagType();

#ifdef NDEF_SUPPORT_MIFARE_CLASSIC
    if (type == NfcTag::MIFARE_CLASSIC)
    {
        #ifdef NDEF_DEBUG
        Serial.println(F("Cleaning Mifare Classic"));
        #endif
        MifareClassic mifareClassic = MifareClassic(*shield, _staticBuf, _staticBufSize);
        return mifareClassic.formatMifare(uid, uidLength);
    }
    else
#endif
#ifdef NDEF_SUPPORT_MIFARE_ULTRA
    if (type == NfcTag::TYPE_2)
    {
        #ifdef NDEF_DEBUG
        Serial.println(F("Cleaning Mifare Ultralight"));
        #endif
        MifareUltralight ultralight = MifareUltralight(*shield, _staticBuf, _staticBufSize);
        return ultralight.clean();
    }
    else
#endif
    {
#ifdef NDEF_USE_SERIAL
        Serial.print(F("No driver for card type "));Serial.println(type);
#endif
        return false;
    }

}


NfcTag NfcAdapter::read()
{
    uint8_t type = guessTagType();

#ifdef NDEF_SUPPORT_MIFARE_CLASSIC
    if (type == NfcTag::MIFARE_CLASSIC)
    {
        #ifdef NDEF_DEBUG
        Serial.println(F("Reading Mifare Classic"));
        #endif
        MifareClassic mifareClassic = MifareClassic(*shield, _staticBuf, _staticBufSize);
        return mifareClassic.read(uid, uidLength);
    }
    else
#endif
#ifdef NDEF_SUPPORT_MIFARE_ULTRA
    if (type == NfcTag::TYPE_2)
    {
        #ifdef NDEF_DEBUG
        Serial.println(F("Reading Mifare Ultralight"));
        #endif
        MifareUltralight ultralight = MifareUltralight(*shield, _staticBuf, _staticBufSize);
        return ultralight.read(uid, uidLength);
    }
    else 
#endif	
	if (type == NfcTag::UNKNOWN)
    {
#ifdef NDEF_USE_SERIAL
        Serial.print(F("Can not determine tag type"));
#endif
        return NfcTag(uid, uidLength);
    }
    else
    {
        // Serial.print(F("No driver for card type "));Serial.println(type);
        // TODO should set type here
        return NfcTag(uid, uidLength);
    }

}

boolean NfcAdapter::write(NdefMessage& ndefMessage)
{
    boolean success;
    uint8_t type = guessTagType();

#ifdef NDEF_SUPPORT_MIFARE_CLASSIC
    if (type == NfcTag::MIFARE_CLASSIC)
    {
        #ifdef NDEF_DEBUG
        Serial.println(F("Writing Mifare Classic"));
        #endif
        MifareClassic mifareClassic = MifareClassic(*shield, _staticBuf, _staticBufSize);
        success = mifareClassic.write(ndefMessage, uid, uidLength);
    }
    else
#endif
#ifdef NDEF_SUPPORT_MIFARE_ULTRA
    if (type == NfcTag::TYPE_2)
    {
        #ifdef NDEF_DEBUG
        Serial.println(F("Writing Mifare Ultralight"));
        #endif
        MifareUltralight mifareUltralight = MifareUltralight(*shield, _staticBuf, _staticBufSize);
        success = mifareUltralight.write(ndefMessage, uid, uidLength);
    }
    else
#endif
	if (type == NfcTag::UNKNOWN)
    {
#ifdef NDEF_USE_SERIAL
        Serial.print(F("Can not determine tag type"));
#endif
        success = false;
    }
    else
    {
#ifdef NDEF_USE_SERIAL
        Serial.print(F("No driver for card type "));Serial.println(type);
#endif
        success = false;
    }

    return success;
}

boolean NfcAdapter::enableRFField() {
    return shield->setRFField(0, 1);
}

boolean NfcAdapter::disableRFField() {
    return shield->setRFField(0, 0);
}

// TODO this should return a Driver MifareClassic, MifareUltralight, Type 4, Unknown
// Guess Tag Type by looking at the ATQA and SAK values
// Need to follow spec for Card Identification. Maybe AN1303, AN1305 and ???
unsigned int NfcAdapter::guessTagType()
{

    // 4 byte id - Mifare Classic
    //  - ATQA 0x4 && SAK 0x8
    // 7 byte id
    //  - ATQA 0x44 && SAK 0x8 - Mifare Classic
    //  - ATQA 0x44 && SAK 0x0 - Mifare Ultralight NFC Forum Type 2
    //  - ATQA 0x344 && SAK 0x20 - NFC Forum Type 4

    if (uidLength == 4)
    {
        return NfcTag::MIFARE_CLASSIC;
    }
    else
    {
        return NfcTag::TYPE_2;
    }
}
