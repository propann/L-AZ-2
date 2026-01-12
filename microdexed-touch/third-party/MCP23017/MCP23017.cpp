//
//    FILE: MCP23017.cpp
//  AUTHOR: Rob Tillaart
// VERSION: 0.9.1
// PURPOSE: Arduino library for I2C MCP23017 16 channel port expander
//    DATE: 2019-10-12
//     URL: https://github.com/RobTillaart/MCP23017_RT
//
// WARNING: please read REV D note in readme.md.


#include "MCP23017.h"


MCP23017::MCP23017(uint8_t address, TwoWire *wire)
{
  _address = address;
  _wire    = wire;
  _error   = MCP23017_OK;
}


bool MCP23017::begin(bool pullup)
{
  //  check connected
  if (! isConnected()) return false;

  //  disable address increment - datasheet P20
  //    SEQOP: Sequential Operation mode bit
  //    1 = Sequential operation disabled, address pointer does not increment.
  //    0 = Sequential operation enabled, address pointer increments.
  //  if (! writeReg(MCP23017_IOCR, MCP23017_IOCR_SEQOP)) return false;

  return true;
}


bool MCP23017::isConnected()
{
  _wire->beginTransmission(_address);
  if (_wire->endTransmission() != 0)
  {
    _error = MCP23017_I2C_ERROR;
    return false;
  }
  _error = MCP23017_OK;
  return true;
}


uint8_t MCP23017::getAddress()
{
  return _address;
}


void MCP23017::reverse16ByteOrder(bool reverse)
{
  _reverse16ByteOrder = reverse;
}


///////////////////////////////////////////////////////////////////
//
//  single pin interface
//
//  pin  = 0..15
//  mode = INPUT, OUTPUT, INPUT_PULLUP (= same as INPUT)
//         do NOT use 0 or 1 for mode.
bool MCP23017::pinMode1(uint8_t pin, uint8_t mode)
{
  if (pin > 15)
  {
    _error = MCP23017_PIN_ERROR;
    return false;
  }
  if ((mode != INPUT) && (mode != INPUT_PULLUP) && (mode != OUTPUT))
  {
    _error = MCP23017_VALUE_ERROR;
    return false;
  }

  uint8_t dataDirectionRegister = MCP23x17_DDR_A;
  if (pin > 7)
  {
    dataDirectionRegister = MCP23x17_DDR_B;
    pin -= 8;
  }
  uint8_t val = readReg(dataDirectionRegister);
  if (_error != MCP23017_OK)
  {
    return false;
  }
  uint8_t mask = 1 << pin;
  //  only work with valid
  if (mode == OUTPUT)
  {
    val &= ~mask;
  }
  //  other values won't change val ....
  writeReg(dataDirectionRegister, val);
  if (_error != MCP23017_OK)
  {
    return false;
  }
  return true;
}


//  pin   = 0..15
//  value = LOW, HIGH
bool MCP23017::write1(uint8_t pin, uint8_t value)
{
  if (pin > 15)
  {
    _error = MCP23017_PIN_ERROR;
    return false;
  }
  uint8_t IOR = MCP23x17_GPIO_A;
  if (pin > 7)
  {
    IOR = MCP23x17_GPIO_B;
    pin -= 8;
  }

  uint8_t val = readReg(IOR);
  uint8_t pre = val;
  if (_error != MCP23017_OK)
  {
    return false;
  }

  uint8_t mask = 1 << pin;
  if (value)
  {
    val |= mask;
  }
  else
  {
    val &= ~mask;
  }
  //  only write when changed.
  if (pre != val)
  {
    writeReg(IOR, val);
    if (_error != MCP23017_OK)
    {
      return false;
    }
  }
  return true;
}



///////////////////////////////////////////////////////////////////
//
//  8 pins interface
//
//  whole register at once
//  port = 0..1
//  mask = 0x00..0xFF  bit pattern
//         bit 0 = output mode, bit 1 = input mode
bool MCP23017::pinMode8(uint8_t port, uint8_t mask)
{
  if (port > 1)
  {
    _error = MCP23017_PORT_ERROR;
    return false;
  }
  if (port == 0) writeReg(MCP23x17_DDR_A, mask);
  if (port == 1) writeReg(MCP23x17_DDR_B, mask);
  _error = MCP23017_OK;
  return true;
}


//  port = 0..1
bool MCP23017::write8(uint8_t port, uint8_t value)
{
  if (port > 1)
  {
    _error = MCP23017_PORT_ERROR;
    return false;
  }
  if (port == 0) writeReg(MCP23x17_GPIO_A, value);
  if (port == 1) writeReg(MCP23x17_GPIO_B, value);
  _error = MCP23017_OK;
  return true;
}


int MCP23017::read8(uint8_t port)
{
  if (port > 1)
  {
    _error = MCP23017_PORT_ERROR;
    return MCP23017_INVALID_READ;
  }
  _error = MCP23017_OK;
  if (port == 0) return readReg(MCP23x17_GPIO_A);
  return readReg(MCP23x17_GPIO_B);  //  port == 1
}


///////////////////////////////////////////////////////////////////
//
//  16 pins interface
//
//  two registers at once
//  mask = 0x0000..0xFFFF bit pattern
//         bit 0 = output mode, bit 1 = input mode
bool MCP23017::pinMode16(uint16_t mask)
{
  writeReg16(MCP23x17_DDR_A, mask);
  _error = MCP23017_OK;
  return true;
}


//  value = 0x0000..0xFFFF   bit pattern
bool MCP23017::write16(uint16_t value)
{
  writeReg16(MCP23x17_GPIO_A, value);
  _error = MCP23017_OK;
  return true;
}


//  return = 0x0000..0xFFFF  bit pattern
uint16_t MCP23017::read16()
{
  _error = MCP23017_OK;
  uint16_t value = readReg16(MCP23x17_GPIO_A);
  return value;
}


/////////////////////////////////////////////
//
//  MISC
//
int MCP23017::lastError()
{
  int e = _error;
  _error = MCP23017_OK;  //  reset error after read.
  return e;
}


bool MCP23017::enableControlRegister(uint8_t mask)
{
  uint8_t reg = readReg(MCP23x17_IOCR);
  reg |= mask;
  return writeReg(MCP23x17_IOCR, reg);
}


bool MCP23017::disableControlRegister(uint8_t mask)
{
  uint8_t reg = readReg(MCP23x17_IOCR);
  reg &= ~mask;
  return writeReg(MCP23x17_IOCR, reg);
}


////////////////////////////////////////////////////
//
//  PROTECTED
//

bool MCP23017::writeReg(uint8_t reg, uint8_t value)
{
  _error = MCP23017_OK;

  if (reg > MCP23x17_OLAT_B)
  {
    _error = MCP23017_REGISTER_ERROR;
    return false;
  }

//  start write
  _wire->beginTransmission(_address);
  _wire->write(reg);
  _wire->write(value);
  if (_wire->endTransmission() != 0)
  {
    _error = MCP23017_I2C_ERROR;
    return false;
  }
  return true;
}


uint8_t MCP23017::readReg(uint8_t reg)
{
  uint8_t rv = 0;

  _error = MCP23017_OK;

  if (reg > MCP23x17_OLAT_B)
  {
    _error = MCP23017_REGISTER_ERROR;
    return 0;
  }

//  start read
  _wire->beginTransmission(_address);
  _wire->write(reg);
  if (_wire->endTransmission() != 0)
  {
    _error = MCP23017_I2C_ERROR;
    return 0;
  }

  uint8_t n = _wire->requestFrom(_address, (uint8_t)1);
  if (n != 1)
  {
    _error = MCP23017_I2C_ERROR;
    return 0;
  }
  rv = _wire->read();
  return rv;
}


//  writes HIGH byte first, LOW byte last
bool MCP23017::writeReg16(uint8_t reg, uint16_t value)
{
  _error = MCP23017_OK;

  if (reg > MCP23x17_OLAT_B)
  {
    _error = MCP23017_REGISTER_ERROR;
    return false;
  }

  if (_reverse16ByteOrder)  //  swap regA and regB
  {
    value = (value >> 8) | (value << 8);
  }

//  start write
  _wire->beginTransmission(_address);
  _wire->write(reg);
  _wire->write(value >> 8);
  _wire->write(value & 0xFF);
  if (_wire->endTransmission() != 0)
  {
    _error = MCP23017_I2C_ERROR;
    return false;
  }
  return true;
}


uint16_t MCP23017::readReg16(uint8_t reg)
{
  _error = MCP23017_OK;

  if (reg > MCP23x17_OLAT_B)
  {
    _error = MCP23017_REGISTER_ERROR;
    return 0;
  }

//  start read
  _wire->beginTransmission(_address);
  _wire->write(reg);
  if (_wire->endTransmission() != 0)
  {
    _error = MCP23017_I2C_ERROR;
    return 0;
  }

  uint8_t n = _wire->requestFrom(_address, (uint8_t)2);
  if (n != 2)
  {
    _error = MCP23017_I2C_ERROR;
    return 0;
  }
  uint16_t regA = _wire->read();
  uint16_t regB = _wire->read();

  if (_reverse16ByteOrder)
  {
    return (regB << 8) | regA;
  }
  return (regA << 8) | regB;
}


//  -- END OF FILE --

