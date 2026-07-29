//ADS1256 cpp file
/*
 Name:		ADS1256.cpp
 Created:	2022/07/14
 Author:	Curious Scientist
 Editor:	Notepad++
 Comment: Visit https://curiousscientist.tech/blog/ADS1256-custom-library
 Special thanks to:
 Abraão Queiroz for spending time on the code and suggesting corrections for ESP32 microcontrollers
 Benjamin Pelletier for pointing out and fixing an issue around the handling of the DRDY signal
 RadoMmm for suggesting an improvement on the ADC-to-Volts conversion
*/

// ================== GIẢI THÍCH CHUNG (dành cho người mới) ==================
// File nay chua "phan than" (implementation) cua cac ham da khai bao trong
// ADS1256.h. Noi don gian: day la noi Arduino thuc su "noi chuyen" voi chip
// ADS1256 qua chuan giao tiep SPI (gui/nhan tung byte du lieu).
// Ban khong can sua file nay de su dung thu vien - chi can goi cac ham cong
// khai (public) tu file .ino cua ban.
// =============================================================================

#include "Arduino.h"
#include "ADS1256.h"
#include "SPI.h"

//Macro chuyen 1 so nguyen 24-bit (khong dau) thanh so co dau (am/duong)
//ADS1256 tra ve gia tri 24-bit dang "bu 2", can chuyen doi de doc dung dau
#define convertSigned24BitToLong(value) ((value) & (1l << 23) ? (value) - 0x1000000 : value)

//Constructor - ham nay chay 1 lan khi ban tao doi tuong ADS1256 (vd: ADS1256 A(...);)
//No chi luu lai cac chan (pin) se dung va cau hinh chieu (INPUT/OUTPUT) cho tung chan
ADS1256::ADS1256(const int8_t DRDY_pin, const int8_t RESET_pin, const int8_t SYNC_pin, const int8_t CS_pin,float VREF, SPIClass* spi):  _spi(spi),
  _DRDY_pin(DRDY_pin),  _RESET_pin(RESET_pin),  _SYNC_pin(SYNC_pin),  _CS_pin(CS_pin),  _VREF(VREF),  _PGA(0)
{
	pinMode(_DRDY_pin, INPUT);	//DRDY luon la chan nhan tin hieu (INPUT)

	if(RESET_pin != PIN_UNUSED) //Neu chan RESET co duoc su dung (khong phai PIN_UNUSED)
	{
	pinMode(_RESET_pin, OUTPUT);
	}

	if(SYNC_pin !=  PIN_UNUSED) //Neu chan SYNC co duoc su dung
	{
	pinMode(_SYNC_pin, OUTPUT);
	}

	if(CS_pin != PIN_UNUSED) //Neu chan CS co duoc su dung
	{
	pinMode(_CS_pin, OUTPUT);
	}

	updateConversionParameter(); //Tinh truoc he so doi ra Volt ung voi PGA mac dinh
}

//Initialization - goi ham nay trong setup() de "danh thuc" va cau hinh chip ADS1256
void ADS1256::InitializeADC()
{
  //Chip select LOW
  CS_LOW();

  //We do a manual chip reset on the ADS1256 - Datasheet Page 27/ RESET
  //Neu co noi chan RESET, ta chu dong keo xuong thap roi len cao de reset "cung" con chip
  if(_RESET_pin != PIN_UNUSED)
  {
  digitalWrite(_RESET_pin, LOW);
  delay(200);
  digitalWrite(_RESET_pin, HIGH); //RESET is set to high
  delay(1000);
  }

  //Sync pin is also treated if it is defined
  //Neu co noi chan SYNC, dua no ve muc cao de chip hoat dong binh thuong (khong o che do power-down)
  if(_SYNC_pin != PIN_UNUSED)
  {
  digitalWrite(_SYNC_pin, HIGH); //RESET is set to high
  }

#ifndef ADS1256_SPI_ALREADY_STARTED //Guard macro to allow external initialization of the SPI
  _spi->begin();	  //Khoi dong giao tiep SPI (bo qua neu ban tu khoi dong SPI ben ngoai)
#endif

  //Applying arbitrary default values to speed up the starting procedure if the user just want to get quick readouts
  //We both pass values to the variables and then send those values to the corresponding registers
  //Doan duoi day ghi cac gia tri MAC DINH vao chip de ban co the do ngay,
  //ban co the thay doi sau bang cac ham setPGA(), setMUX(), setDRATE()...
  delay(200);

  _STATUS = 0b00110110; //BUFEN and ACAL enabled, Order is MSB, rest is read only
  writeRegister(STATUS_REG, _STATUS);
  delay(200);

  _MUX = 0b00000001; //MUX AIN0+AIN1 - Mac dinh do vi sai giua chan A0 va A1
  writeRegister(MUX_REG, _MUX);
  delay(200);

  _ADCON = 0b00000000; //ADCON - CLK: OFF, SDCS: OFF, PGA = 0 (+/- 5 V)
  writeRegister(ADCON_REG, _ADCON);
  delay(200);

  updateConversionParameter(); //Cap nhat lai he so doi ra Volt ung voi PGA vua thiet lap

  _DRATE = 0b10000010; //100SPS - Mac dinh toc do lay mau la 100 lan/giay
  writeRegister(DRATE_REG, _DRATE);
  delay(200);

  sendDirectCommand(0b11110000); //Offset and self-gain calibration - Tu hieu chuan sai so va he so khuech dai
  delay(200);

  _isAcquisitionRunning = false; //Bao chua co qua trinh doc lien tuc nao dang chay
}

//Cho den khi chan DRDY xuong muc THAP - nghia la chip da co san ket qua do moi
void ADS1256::waitForLowDRDY()
{
    while (digitalRead(_DRDY_pin) == HIGH) {}
}

//Cho den khi chan DRDY len muc CAO
void ADS1256::waitForHighDRDY()
{
#if F_CPU >= 48000000  //Fast MCUs need this protection to wait until DRDY goes high after a conversion
	while (digitalRead(_DRDY_pin) == LOW) {}
#endif
}

//Dung qua trinh doc lien tuc (gui lenh SDATAC) de dua chip ve trang thai binh thuong
void ADS1256::stopConversion() //Sending SDATAC to stop the continuous conversion
{
	//SUA LOI: neu chua co qua trinh doc lien tuc nao duoc bat dau (readSingleContinuous/
	//cycleSingle/cycleDifferential), thi cung chua co "phien SPI" (transaction) nao dang mo.
	//Truoc day, goi stopConversion() trong truong hop nay se lam _spi->transfer() ma khong co
	//beginTransaction() truoc do, co the gay loi/treo may tren mot so board (ESP32, RP2040).
	//Vi vay, neu khong co gi dang chay thi don gian la khong lam gi ca.
	if(_isAcquisitionRunning == false)
	{
		return;
	}

	waitForLowDRDY(); //SDATAC should be called after DRDY goes LOW (p35. Figure 33)
	_spi->transfer(0b00001111); //Send SDATAC to the ADC
	CS_HIGH(); //We finished the command sequence, so we switch it back to HIGH
	_spi->endTransaction();

	_isAcquisitionRunning = false; //Reset to false, so the MCU will be able to start a new conversion
}

//Dat toc do lay mau (bao nhieu lan do moi giay)
void ADS1256::setDRATE(uint8_t drate) //Setting DRATE (sampling frequency)
{
	writeRegister(DRATE_REG, drate);
	_DRATE = drate;
	delay(200);
}

//Chon kenh dau vao se duoc do (vd: A0+A1, hoac A2+GND...)
void ADS1256::setMUX(uint8_t mux) //Setting MUX (input channel)
{
	writeRegister(MUX_REG, mux);
	_MUX = mux;
	delay(200);
}

//Dat he so khuech dai (PGA) - anh huong toi dai dien ap co the do duoc
void ADS1256::setPGA(uint8_t pga) //Setting PGA (input voltage range)
{
	_PGA = pga;
	_ADCON = readRegister(ADCON_REG); //Read the most recent value of the register

	_ADCON = (_ADCON & 0b11111000) | (_PGA & 0b00000111); // Clearing and then setting bits 2-0 based on pga

	writeRegister(ADCON_REG, _ADCON);
	delay(200);

	updateConversionParameter(); //Update the multiplier according top the new PGA value
}

//Doc lai gia tri PGA hien tai dang duoc cau hinh trong chip
uint8_t ADS1256::getPGA() //Reading PGA from the ADCON register
{
	uint8_t pgaValue = readRegister(ADCON_REG) & 0b00000111;
	//Reading the ADCON_REG and keeping the first three bits.

	return(pgaValue);
}

//Cau hinh chan xuat xung clock (CLKOUT) ra ngoai - thuong khong can dung toi
void ADS1256::setCLKOUT(uint8_t clkout) //Setting CLKOUT
{
	_ADCON = readRegister(ADCON_REG); //Read the most recent value of the register

	//Values: 0, 1, 2, 3

	if(clkout == 0)
	{
		//00
		bitWrite(_ADCON, 6, 0);
		bitWrite(_ADCON, 5, 0);
	}
	else if(clkout == 1)
	{
		//01 (default)
		bitWrite(_ADCON, 6, 0);
		bitWrite(_ADCON, 5, 1);
	}
	else if(clkout == 2)
	{
		//10
		bitWrite(_ADCON, 6, 1);
		bitWrite(_ADCON, 5, 0);
	}
	else if(clkout == 3)
	{
		//11
		bitWrite(_ADCON, 6, 1);
		bitWrite(_ADCON, 5, 1);
	}
	else{}

	writeRegister(ADCON_REG, _ADCON);
	delay(100);
}

//Cau hinh Sensor Detect Current Sources (SDCS) - tinh nang phat hien cam bien bi dut/hong
void ADS1256::setSDCS(uint8_t sdcs) //Setting SDCS
{
	_ADCON = readRegister(ADCON_REG); //Read the most recent value of the register

	//Values: 0, 1, 2, 3

	if(sdcs == 0)
	{
		//00 (default)
		bitWrite(_ADCON, 4, 0);
		bitWrite(_ADCON, 3, 0);
	}
	else if(sdcs == 1)
	{
		//01
		bitWrite(_ADCON, 4, 0);
		bitWrite(_ADCON, 3, 1);
	}
	else if(sdcs == 2)
	{
		//10
		bitWrite(_ADCON, 4, 1);
		bitWrite(_ADCON, 3, 0);
	}
	else if(sdcs == 3)
	{
		//11
		bitWrite(_ADCON, 4, 1);
		bitWrite(_ADCON, 3, 1);
	}
	else{}

	writeRegister(ADCON_REG, _ADCON);
	delay(100);
}

//Dat thu tu bit khi doc/ghi du lieu: MSB (bit lon truoc, mac dinh) hoac LSB (bit nho truoc)
void ADS1256::setByteOrder(uint8_t byteOrder) //Setting byte order (MSB/LSB)
{
	_STATUS = readRegister(STATUS_REG); //Read the most recent value of the register

	if(byteOrder == 0)
	{
		//Byte order is MSB (default)
		bitWrite(_STATUS, 3, 0);
		//Set value of _STATUS at the third bit to 0
	}
	else if(byteOrder == 1)
	{
		//Byte order is LSB
		bitWrite(_STATUS, 3, 1);
		//Set value of _STATUS at the third bit to 1
	}
	else{}

	writeRegister(STATUS_REG, _STATUS);
	delay(100);
}

//Doc lai thu tu bit (MSB/LSB) hien tai
uint8_t ADS1256::getByteOrder() //Getting byte order (MSB/LSB)
{
	uint8_t statusValue = readRegister(STATUS_REG);	//Read the whole STATUS register

	return bitRead(statusValue, 3);
}

//Bat/tat tinh nang tu dong hieu chuan (Auto-Calibration)
void ADS1256::setAutoCal(uint8_t acal) //Setting ACAL (Automatic SYSCAL)
{
	_STATUS = readRegister(STATUS_REG); //Read the most recent value of the register

	if(acal == 0)
	{
		//Auto-calibration is disabled (default)
		bitWrite(_STATUS, 2, 0);
		//_STATUS |= B00000000;
	}
	else if(acal == 1)
	{
		//Auto-calibration is enabled
		bitWrite(_STATUS, 2, 1);
		//_STATUS |= B00000100;
	}
	else{}

	writeRegister(STATUS_REG, _STATUS);
	delay(100);
}

//Doc lai trang thai tu dong hieu chuan hien tai
uint8_t ADS1256::getAutoCal() //Getting ACAL (Automatic SYSCAL)
{
	uint8_t statusValue = readRegister(STATUS_REG);	//Read the whole STATUS register

	return bitRead(statusValue, 2);
}

//Bat/tat bo dem dau vao (Input Buffer) - nen bat (1) trong da so truong hop
void ADS1256::setBuffer(uint8_t bufen) //Setting input buffer (Input impedance)
{
	_STATUS = readRegister(STATUS_REG); //Read the most recent value of the register

	if(bufen == 0)
	{
		//Analog input buffer is disabled (default)
		//_STATUS |= B00000000;
		bitWrite(_STATUS, 1, 0);
	}
	else if(bufen == 1)
	{
		//Analog input buffer is enabled (recommended)
		//_STATUS |= B00000010;
		bitWrite(_STATUS, 1, 1);
	}
	else{}

	writeRegister(STATUS_REG, _STATUS);
	delay(100);
}

//Doc lai trang thai bo dem dau vao hien tai
uint8_t ADS1256::getBuffer() //Getting input buffer (Input impedance)
{
	uint8_t statusValue = readRegister(STATUS_REG);	//Read the whole STATUS register

	return bitRead(statusValue, 1);
}

//Dat chieu (INPUT/OUTPUT) cho 4 chan GPIO phu (D0-D3) tren chip ADS1256
void ADS1256::setGPIO(uint8_t dir0, uint8_t dir1, uint8_t dir2, uint8_t dir3) //Setting GPIO
{
	_GPIO = readRegister(IO_REG); //Read the most recent value of the register

	//Default: 11100000 - DEC: 224 - Ref: p32 I/O section
	//Sets D3-D0 as input or output
	uint8_t GPIO_bit7, GPIO_bit6, GPIO_bit5, GPIO_bit4;

	//Bit7: DIR3
	if(dir3 == 1)
	{
		GPIO_bit7 = 1; //D3 is input (default)
	}
	else
	{
		GPIO_bit7 = 0; //D3 is output
	}
	bitWrite(_GPIO, 7, GPIO_bit7);
	//-----------------------------------------------------
	//Bit6: DIR2
	if(dir2 == 1)
	{
		GPIO_bit6 = 1; //D2 is input (default)
	}
	else
	{
		GPIO_bit6 = 0; //D2 is output
	}
	bitWrite(_GPIO, 6, GPIO_bit6);
	//-----------------------------------------------------
	//Bit5: DIR1
	if(dir1 == 1)
	{
		GPIO_bit5 = 1; //D1 is input (default)
	}
	else
	{
		GPIO_bit5 = 0; //D1 is output
	}
	bitWrite(_GPIO, 5, GPIO_bit5);
	//-----------------------------------------------------
	//Bit4: DIR0
	if(dir0 == 1)
	{
		GPIO_bit4 = 1; //D0 is input
	}
	else
	{
		GPIO_bit4 = 0; //D0 is output (default)
	}
	bitWrite(_GPIO, 4, GPIO_bit4);
	//-----------------------------------------------------

	writeRegister(IO_REG, _GPIO);
	delay(100);
}

//Ghi gia tri (0 hoac 1) ra 4 chan GPIO phu - CHI dung sau khi da goi setGPIO() de dat chieu la OUTPUT
void ADS1256::writeGPIO(uint8_t dir0value, uint8_t dir1value, uint8_t dir2value, uint8_t dir3value) //Writing GPIO
{
	_GPIO = readRegister(IO_REG);

	//Sets D3-D0 output values
	//It is important that first one must use setGPIO, then writeGPIO

	uint8_t GPIO_bit3, GPIO_bit2, GPIO_bit1, GPIO_bit0;

	//Bit3: DIR3
	if(dir3value == 1)
	{
		GPIO_bit3 = 1;
	}
	else
	{
		GPIO_bit3 = 0;
	}
	bitWrite(_GPIO, 3, GPIO_bit3);
	//-----------------------------------------------------
	//Bit2: DIR2
	if(dir2value == 1)
	{
		GPIO_bit2 = 1;
	}
	else
	{
		GPIO_bit2 = 0;
	}
	bitWrite(_GPIO, 2, GPIO_bit2);
	//-----------------------------------------------------
	//Bit1: DIR1
	if(dir1value == 1)
	{
		GPIO_bit1 = 1;
	}
	else
	{
		GPIO_bit1 = 0;
	}
	bitWrite(_GPIO, 1, GPIO_bit1);
	//-----------------------------------------------------
	//Bit0: DIR0
	if(dir0value == 1)
	{
		GPIO_bit0 = 1;
	}
	else
	{
		GPIO_bit0 = 0;
	}
	bitWrite(_GPIO, 0, GPIO_bit0);
	//-----------------------------------------------------

	writeRegister(IO_REG, _GPIO);
	delay(100);
}

//Doc gia tri hien tai cua 1 chan GPIO phu (gpioPin phai la 0, 1, 2 hoac 3)
uint8_t ADS1256::readGPIO(uint8_t gpioPin) //Reading GPIO
{
	//SUA LOI: khoi tao GPIO_return = 0 ngay tu dau. Truoc day neu gpioPin duoc
	//truyen vao mot gia tri khong hop le (khac 0-3), bien nay se KHONG duoc gan
	//trong switch-case ben duoi va ham se tra ve mot gia tri "rac" (chua xac dinh).
	uint8_t GPIO_bit3, GPIO_bit2, GPIO_bit1, GPIO_bit0, GPIO_return = 0;

	_GPIO = readRegister(IO_REG); //Read the GPIO register

	//Save each bit values in a variable
	GPIO_bit3 = bitRead(_GPIO, 3);
	GPIO_bit2 = bitRead(_GPIO, 2);
	GPIO_bit1 = bitRead(_GPIO, 1);
	GPIO_bit0 = bitRead(_GPIO, 0);

	delay(100);

	switch(gpioPin) //Selecting which value should be returned
	{
		case 0:
		GPIO_return = GPIO_bit0;
		break;

		case 1:
		GPIO_return = GPIO_bit1;
		break;

		case 2:
		GPIO_return = GPIO_bit2;
		break;

		case 3:
		GPIO_return = GPIO_bit3;
		break;

		default:
		//SUA LOI: gpioPin khong hop le (chi chap nhan 0-3) - tra ve 0 thay vi gia tri rac
		GPIO_return = 0;
		break;
	}

	return GPIO_return;

}

//Gui truc tiep 1 lenh (command) bat ky toi chip - dung cho cac lenh khong can tham so (vd: SELFCAL, RESET, STANDBY...)
void ADS1256::sendDirectCommand(uint8_t directCommand)
{
  //Direct commands can be found in the datasheet Page 34, Table 24.
  _spi->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));

  CS_LOW(); //REF: P34: "CS must stay low during the entire command sequence"
  delayMicroseconds(5);
  _spi->transfer(directCommand); //Send Command
  delayMicroseconds(5);
  CS_HIGH(); //REF: P34: "CS must stay low during the entire command sequence"

  _spi->endTransaction();
}


//Chuyen doi gia tri so nguyen 24-bit (rawData) thanh dien ap thuc te theo don vi Volt
float ADS1256::convertToVoltage(int32_t rawData) //Converting the 24-bit data into a voltage value
{
  return(conversionParameter * rawData);
}

//Ghi 1 gia tri (registerValueToWrite) vao 1 thanh ghi (registerAddress) cua chip
void ADS1256::writeRegister(uint8_t registerAddress, uint8_t registerValueToWrite)
{
  waitForLowDRDY();

  _spi->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));
  //SPI_MODE1 = output edge: rising, data capture: falling; clock polarity: 0, clock phase: 1.

  CS_LOW(); //CS must stay LOW during the entire sequence [Ref: P34, T24]

  delayMicroseconds(5); //see t6 in the datasheet

  _spi->transfer(0x50 | registerAddress); // 0x50 = 01010000 = WREG

  _spi->transfer(0x00); //2nd (empty) command byte

  _spi->transfer(registerValueToWrite); //pass the value to the register

  CS_HIGH();
  _spi->endTransaction();
  delay(100);

}

//Doc gia tri hien tai cua 1 thanh ghi (registerAddress) tu chip
long ADS1256::readRegister(uint8_t registerAddress) //Reading a register
{
   waitForLowDRDY();

  _spi->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));
  //SPI_MODE1 = output edge: rising, data capture: falling; clock polarity: 0, clock phase: 1.

  CS_LOW(); //CS must stay LOW during the entire sequence [Ref: P34, T24]

  _spi->transfer(0x10 | registerAddress); //0x10 = 0001000 = RREG - OR together the two numbers (command + address)

  _spi->transfer(0x00); //2nd (empty) command byte

  delayMicroseconds(5); //see t6 in the datasheet

  uint8_t regValue = _spi->transfer(0xFF); //read out the register value

  CS_HIGH();
  _spi->endTransaction();
  delay(100);
  return regValue;
}


//Doc 1 gia tri do DUY NHAT (do 1 lan roi dung) tu kenh dang duoc chon (qua setMUX)
long ADS1256::readSingle() //Reading a single value ONCE using the RDATA command
{
	_spi->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));
	CS_LOW(); //REF: P34: "CS must stay low during the entire command sequence"
	waitForLowDRDY();
	_spi->transfer(0b00000001); //Issue RDATA (0000 0001) command
	delayMicroseconds(7); //Wait t6 time (~6.51 us) REF: P34, FIG:30.

	_outputBuffer[0] = _spi->transfer(0); // MSB
	_outputBuffer[1] = _spi->transfer(0); // Mid-byte
	_outputBuffer[2] = _spi->transfer(0); // LSB

	//Shifting and combining the above three items into a single, 24-bit number
	//Ghep 3 byte rieng le thanh 1 so nguyen 24-bit duy nhat
	_outputValue = ((long)_outputBuffer[0]<<16) | ((long)_outputBuffer[1]<<8) | (_outputBuffer[2]);
	_outputValue = convertSigned24BitToLong(_outputValue);

	CS_HIGH(); //We finished the command sequence, so we set CS to HIGH
	_spi->endTransaction();

	return(_outputValue);
}

//Doc lien tuc, nhieu lan lien nhau, tren CUNG 1 kenh dau vao (nhanh nhat, dung cho lay mau toc do cao)
long ADS1256::readSingleContinuous() //Reads the recently selected input channel using RDATAC
{
	if(_isAcquisitionRunning == false)
	{
	  _isAcquisitionRunning = true;
	  _spi->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));
	  CS_LOW(); //REF: P34: "CS must stay low during the entire command sequence"
	  waitForLowDRDY();
	  _spi->transfer(0b00000011);  //Issue RDATAC (0000 0011)
	  delayMicroseconds(7); //Wait t6 time (~6.51 us) REF: P34, FIG:30.
	}
	else
	{
		waitForLowDRDY();
	}

	_outputBuffer[0] = _spi->transfer(0); // MSB
	_outputBuffer[1] = _spi->transfer(0); // Mid-byte
	_outputBuffer[2] = _spi->transfer(0); // LSB

	_outputValue = ((long)_outputBuffer[0]<<16) | ((long)_outputBuffer[1]<<8) | (_outputBuffer[2]);
	_outputValue = convertSigned24BitToLong(_outputValue);

	waitForHighDRDY();

	return _outputValue;
}

//Lan luot doc qua 8 kenh don (single-ended: Ax + GND), moi lan goi ham se tra ve 1 kenh tiep theo
long ADS1256::cycleSingle()
{
	if(_isAcquisitionRunning == false)
	{
	  _isAcquisitionRunning = true;
	  _cycle = 0;
	  _spi->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));
	  CS_LOW(); //CS must stay LOW during the entire sequence [Ref: P34, T24]
	  _spi->transfer(0x50 | 1); // 0x50 = WREG //1 = MUX
      _spi->transfer(0x00);
      _spi->transfer(SING_0); //AIN0+AINCOM
	  CS_HIGH();
	  delay(50);
	  CS_LOW(); //CS must stay LOW during the entire sequence [Ref: P34, T24]
	}
	else
	{}

	if(_cycle < 8)
	{
	  _outputValue = 0;
	  waitForLowDRDY();
      //Step 1. - Updating MUX
      switch (_cycle)
      {
        //Channels are written manually
        case 0: //Channel 2
		updateMUX(SING_1);  //AIN1+AINCOM
          break;

        case 1: //Channel 3
          updateMUX(SING_2);  //AIN2+AINCOM
          break;

        case 2: //Channel 4
          updateMUX(SING_3);  //AIN3+AINCOM
          break;

        case 3: //Channel 5
          updateMUX(SING_4);  //AIN4+AINCOM
          break;

        case 4: //Channel 6
          updateMUX(SING_5);  //AIN5+AINCOM
          break;

        case 5: //Channel 7
          updateMUX(SING_6);  //AIN6+AINCOM
          break;

        case 6: //Channel 8
          updateMUX(SING_7);  //AIN7+AINCOM
          break;

        case 7: //Channel 1
          updateMUX(SING_0); //AIN0+AINCOM
          break;
      }
      //Step 2.
      _spi->transfer(0b11111100); //SYNC
      delayMicroseconds(4); //t11 delay 24*tau = 3.125 us //delay should be larger, so we delay by 4 us
      _spi->transfer(0b11111111); //WAKEUP

      //Step 3.
      //Issue RDATA (0000 0001) command
      _spi->transfer(0b00000001);
      delayMicroseconds(7); //Wait t6 time (~6.51 us) REF: P34, FIG:30.

	  _outputBuffer[0] = _spi->transfer(0x0F); // MSB
	  _outputBuffer[1] = _spi->transfer(0x0F); // Mid-byte
	  _outputBuffer[2] = _spi->transfer(0x0F); // LSB

	  _outputValue = ((long)_outputBuffer[0]<<16) | ((long)_outputBuffer[1]<<8) | (_outputBuffer[2]);
	  _outputValue = convertSigned24BitToLong(_outputValue);

	  _cycle++; //Increase cycle - This will move to the next MUX input channel
	  if(_cycle == 8)
	  {
		  _cycle = 0; //Reset to 0 - Restart conversion from the 1st input channel
	  }
    }

	return _outputValue;
}

//Lan luot doc qua 4 kenh vi sai (differential: Ax + Ay), moi lan goi ham se tra ve 1 kenh tiep theo
long ADS1256::cycleDifferential()
{
	if(_isAcquisitionRunning == false)
	{
	  _cycle = 0;
	  _isAcquisitionRunning = true;
	  _spi->beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1));

	  //Set the AIN0+AIN1 as inputs manually
	  CS_LOW(); //CS must stay LOW during the entire sequence [Ref: P34, T24]
	  _spi->transfer(0x50 | 1); // 0x50 = WREG //1 = MUX
	  _spi->transfer(0x00);
	  _spi->transfer(DIFF_0_1);  //AIN0+AIN1
	  CS_HIGH();
	  delay(50);
	  CS_LOW(); //CS must stay LOW during the entire sequence [Ref: P34, T24]
	}
	else
	{}

	if(_cycle < 4)
    {
	  _outputValue = 0;
      //DRDY has to go low
	  waitForLowDRDY();

      //Step 1. - Updating MUX
      switch (_cycle)
      {
        case 0: //Channel 2
		  updateMUX(DIFF_2_3);  //AIN2+AIN3
          break;

        case 1: //Channel 3
          updateMUX(DIFF_4_5); //AIN4+AIN5
          break;

        case 2: //Channel 4
          updateMUX(DIFF_6_7); //AIN6+AIN7
          break;

        case 3: //Channel 1
         updateMUX(DIFF_0_1); //AIN0+AIN1
          break;
      }

      _spi->transfer(0b11111100); //SYNC
      delayMicroseconds(4); //t11 delay 24*tau = 3.125 us //delay should be larger, so we delay by 4 us
      _spi->transfer(0b11111111); //WAKEUP

      //Step 3.
      _spi->transfer(0b00000001); //Issue RDATA (0000 0001) command
      delayMicroseconds(7); //Wait t6 time (~6.51 us) REF: P34, FIG:30.

	  _outputBuffer[0] = _spi->transfer(0); // MSB
	  _outputBuffer[1] = _spi->transfer(0); // Mid-byte
	  _outputBuffer[2] = _spi->transfer(0); // LSB

	  _outputValue = ((long)_outputBuffer[0]<<16) | ((long)_outputBuffer[1]<<8) | (_outputBuffer[2]);
	  _outputValue = convertSigned24BitToLong(_outputValue);

	  _cycle++;
	  if(_cycle == 4)
	  {
		  _cycle = 0;
		  //After the 4th cycle, we reset to zero so the next iteration reads the 1st MUX again
	  }
    }

	return _outputValue;
}

//Tinh lai he so nhan (conversionParameter) dung de doi gia tri so (raw) sang Volt, dua tren VREF va PGA hien tai
void ADS1256::updateConversionParameter()
{
	conversionParameter = ((2.0 * _VREF) / 8388608.0) / (pow(2, _PGA)); //Calculate the "bit to Volts" multiplier
	//8388608 = 2^{23} - 1, REF: p23, Table 16.
}

//Cap nhat thanh ghi MUX (doi kenh dau vao) trong luc dang o giua qua trinh doc lien tuc (khong dung rieng le)
void ADS1256::updateMUX(uint8_t muxValue)
{
    _spi->transfer(0x50 | MUX_REG); //Write to the MUX register (0x50 is the WREG command)
    _spi->transfer(0x00);
    _spi->transfer(muxValue);       //Write the new MUX value
}

//Ha chan Chip Select (CS) xuong muc THAP - bao hieu bat dau 1 phien giao tiep voi chip
inline void ADS1256::CS_LOW()
{
    if (_CS_pin != PIN_UNUSED) //Sets CS LOW if it is not an unused pin
	{
        digitalWrite(_CS_pin, LOW);
	}
}

//Nang chan Chip Select (CS) len muc CAO - bao hieu ket thuc phien giao tiep voi chip
inline void ADS1256::CS_HIGH()
{
    if (_CS_pin != PIN_UNUSED) //Sets CS HIGH if it is not an unused pin
	{
        digitalWrite(_CS_pin, HIGH);
	}
}
