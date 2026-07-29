//ADS1256 header file
/*
 Name:		ADS1256.h
 Created:	2022/07/14
 Author:	Curious Scientist
 Editor:	Notepad++
 Comment: Visit https://curiousscientist.tech/blog/ADS1256-custom-library
 Special thanks to
 Abraão Queiroz for spending time on the code and suggesting corrections for ESP32 microcontrollers
 Benjamin Pelletier for pointing out and fixing an issue around the handling of the DRDY signal
*/

// ================== GIẢI THÍCH CHUNG (dành cho người mới) ==================
// File này định nghĩa "class" ADS1256, tức là một "khuôn mẫu" mô tả cách
// Arduino/vi điều khiển giao tiếp với chip ADS1256 (một con chip chuyên đo
// điện áp analog rất chính xác, giao tiếp qua chuẩn SPI).
// Bạn không cần hiểu hết chi tiết bên dưới để SỬ DỤNG thư viện - chỉ cần xem
// file ví dụ (ADS1256-Example.ino) để biết cách gọi các hàm.
// =============================================================================

#ifndef _ADS1256_h
#define _ADS1256_h

#include <SPI.h>

//Differential inputs
//Che do "vi sai" (differential): do hieu dien the giua 2 chan, vd A0 va A1
#define DIFF_0_1 0b00000001 //A0 + A1 as differential input
#define DIFF_2_3 0b00100011 //A2 + A3 as differential input
#define DIFF_4_5 0b01000101 //A4 + A5 as differential input
#define DIFF_6_7 0b01100111 //A6 + A7 as differential input

//Single-ended inputs
//Che do "don" (single-ended): do dien the giua 1 chan va GND (mass chung)
#define SING_0 0b00001111 //A0 + GND (common) as single-ended input
#define SING_1 0b00011111 //A1 + GND (common) as single-ended input
#define SING_2 0b00101111 //A2 + GND (common) as single-ended input
#define SING_3 0b00111111 //A3 + GND (common) as single-ended input
#define SING_4 0b01001111 //A4 + GND (common) as single-ended input
#define SING_5 0b01011111 //A5 + GND (common) as single-ended input
#define SING_6 0b01101111 //A6 + GND (common) as single-ended input
#define SING_7 0b01111111 //A7 + GND (common) as single-ended input

//PGA settings			  //Input voltage range
//PGA = bo khuech dai lap trinh duoc. So cang lon thi do nhay cang cao,
//nhung dai dien the do duoc se cang nho lai (xem cot ben phai).
#define PGA_1 0b00000000  //± 5 V
#define PGA_2 0b00000001  //± 2.5 V
#define PGA_4 0b00000010  //± 1.25 V
#define PGA_8 0b00000011  //± 625 mV
#define PGA_16 0b00000100 //± 312.5 mV
#define PGA_32 0b00000101 //+ 156.25 mV
#define PGA_64 0b00000110 //± 78.125 mV

//Datarate						  //DEC
//Toc do lay mau (bao nhieu lan do trong 1 giay - SPS = Samples Per Second).
//Toc do cang cao thi do chinh xac/on dinh cang thap va nguoc lai.
#define DRATE_30000SPS 0b11110000 //240
#define DRATE_15000SPS 0b11100000 //224
#define DRATE_7500SPS 0b11010000  //208
#define DRATE_3750SPS 0b11000000  //192
#define DRATE_2000SPS 0b10110000  //176
#define DRATE_1000SPS 0b10100001  //161
#define DRATE_500SPS 0b10010010   //146
#define DRATE_100SPS 0b10000010   //130
#define DRATE_60SPS 0b01110010    //114
#define DRATE_50SPS 0b01100011    //99
#define DRATE_30SPS 0b01010011    //83
#define DRATE_25SPS 0b01000011    //67
#define DRATE_15SPS 0b00110011    //51
#define DRATE_10SPS 0b00100011    //35
#define DRATE_5SPS 0b00010011     //19
#define DRATE_2SPS 0b00000011     //3

//Status register
//Cac tuy chon cau hinh chung cua ADS1256
#define BITORDER_MSB 0 //Thu tu bit: bit lon nhat gui truoc (mac dinh)
#define BITORDER_LSB 1 //Thu tu bit: bit nho nhat gui truoc
#define ACAL_DISABLED 0 //Tat tu dong hieu chuan (auto-calibration)
#define ACAL_ENABLED 1  //Bat tu dong hieu chuan
#define BUFFER_DISABLED 0 //Tat bo dem dau vao
#define BUFFER_ENABLED 1  //Bat bo dem dau vao (khuyen nghi nen bat)

//Register addresses
//Dia chi cac "thanh ghi" (o nho cau hinh) ben trong chip ADS1256
#define STATUS_REG 0x00
#define MUX_REG 0x01
#define ADCON_REG 0x02
#define DRATE_REG 0x03
#define IO_REG 0x04
#define OFC0_REG 0x05
#define OFC1_REG 0x06
#define OFC2_REG 0x07
#define FSC0_REG 0x08
#define FSC1_REG 0x09
#define FSC2_REG 0x0A

//Command definitions
//Cac lenh dieu khien gui truc tiep cho chip ADS1256 qua SPI
#define WAKEUP 0b00000000
#define RDATA 0b00000001
#define RDATAC 0b00000011
#define SDATAC 0b00001111
#define RREG 0b00010000
#define WREG 0b01010000
#define SELFCAL 0b11110000
#define SELFOCAL 0b11110001
#define SELFGCAL 0b11110010
#define SYSOCAL 0b11110011
#define SYSGCAL 0b11110100
#define SYNC 0b11111100
#define STANDBY 0b11111101
#define RESET 0b11111110
//----------------------------------------------------------------


class ADS1256
{
public:
static constexpr int8_t PIN_UNUSED = -1; //Dung gia tri nay khi 1 chan (RESET/SYNC/CS) khong duoc noi day

	//Constructor
	//Ham khoi tao: goi 1 lan duy nhat khi ban tao doi tuong ADS1256, vd:
	//ADS1256 A(DRDY_pin, RESET_pin, SYNC_pin, CS_pin, VREF, &SPI);
	ADS1256(const int8_t DRDY_pin, const int8_t RESET_pin, const int8_t SYNC_pin, const int8_t CS_pin, float VREF, SPIClass* spi = &SPI);

	//Initializing function
	//Goi trong setup() de khoi dong chip: reset, cau hinh cac gia tri mac dinh
	void InitializeADC();
	//ADS1256(int drate, int pga, int byteOrder, bool bufen);

	//Read a register
	//Doc gia tri hien tai cua 1 thanh ghi trong chip
	long readRegister(uint8_t registerAddress);

	//Write a register
	//Ghi 1 gia tri vao 1 thanh ghi trong chip
	void writeRegister(uint8_t registerAddress, uint8_t registerValueToWrite);

	//Individual methods
	//Cac ham tien ich de doc/ghi tung phan cau hinh rieng le
	void setDRATE(uint8_t drate); //Dat toc do lay mau
	void setPGA(uint8_t pga); //Dat he so khuech dai
	uint8_t getPGA(); //Doc lai he so khuech dai hien tai
	void setMUX(uint8_t mux); //Chon kenh dau vao (single-ended hoac differential)
	void setByteOrder(uint8_t byteOrder); //Dat thu tu bit MSB/LSB
	uint8_t getByteOrder(); //Doc lai thu tu bit hien tai
	void setBuffer(uint8_t bufen); //Bat/tat bo dem dau vao
	uint8_t getBuffer(); //Doc lai trang thai bo dem
	void setAutoCal(uint8_t acal); //Bat/tat tu dong hieu chuan
	uint8_t getAutoCal(); //Doc lai trang thai tu dong hieu chuan
	void setGPIO(uint8_t dir0, uint8_t dir1, uint8_t dir2, uint8_t dir3); //Dat chieu (vao/ra) cho 4 chan GPIO phu
	void writeGPIO(uint8_t dir0value, uint8_t dir1value, uint8_t dir2value, uint8_t dir3value); //Ghi gia tri ra cho 4 chan GPIO phu
	uint8_t readGPIO(uint8_t gpioPin);	//Doc gia tri 1 chan GPIO phu (0-3)
	void setCLKOUT(uint8_t clkout); //Cau hinh chan xuat xung clock ra ngoai
	void setSDCS(uint8_t sdcs);	//Cau hinh Sensor Detect Current Sources
	void sendDirectCommand(uint8_t directCommand);	//Gui 1 lenh truc tiep bat ky den chip

	//Get a single conversion
	//Doc 1 gia tri do duy nhat (do 1 lan roi thoi)
	long readSingle();

	//Single input continuous reading
	//Doc lien tuc, nhieu lan, tren CUNG 1 kenh dau vao (toc do nhanh nhat)
	long readSingleContinuous();

	//Cycling through the single-ended inputs
	//Lan luot doc qua tung kenh trong 8 kenh don (Ax + GND)
	long cycleSingle(); //Ax + COM

	//Cycling through the differential inputs
	//Lan luot doc qua tung kenh trong 4 kenh vi sai (Ax + Ay)
	long cycleDifferential(); //Ax + Ay

	//Converts the reading into a voltage value
	//Chuyen doi gia tri so (raw, dang so nguyen 24-bit) sang dien ap that (Volt)
	float convertToVoltage(int32_t rawData);

	//Stop AD
	//Dung qua trinh doc lien tuc (SDATAC) de dua chip ve trang thai nghi
	void stopConversion();

private:

SPIClass* _spi; //Con tro toi doi tuong SPI dang su dung (SPI, SPI1, hspi...)

void waitForLowDRDY(); // Cho den khi chan DRDY xuong muc thap (bao hieu du lieu san sang)
void waitForHighDRDY(); // Cho den khi chan DRDY len muc cao
void updateMUX(uint8_t muxValue); //Cap nhat thanh ghi MUX (kenh dau vao) trong luc dang doc lien tuc
inline void CS_LOW(); //Ha chan Chip Select xuong thap (bat dau giao tiep voi chip)
inline void CS_HIGH(); //Nang chan Chip Select len cao (ket thuc giao tiep voi chip)

void updateConversionParameter(); //Tinh lai he so nhan de chuyen gia tri so sang Volt, dua theo PGA hien tai

float _VREF = 0; //Gia tri dien ap tham chieu (Volt) - phai khai bao dung theo phan cung thuc te
float conversionParameter = 0; //He so nhan (phu thuoc PGA) dung de doi ra Volt
//Pins
int8_t _DRDY_pin; //Chan bao du lieu san sang (Data Ready)
int8_t _RESET_pin; //Chan Reset chip
int8_t _SYNC_pin; //Chan Sync/Power-down
int8_t _CS_pin; //Chan Chip Select

//Register values
//Bien luu lai gia tri da ghi vao tung thanh ghi, de dung lai trong code ma khong can doc lai tu chip
byte _DRATE; //Gia tri thanh ghi DRATE
byte _ADCON; //Gia tri thanh ghi ADCON
byte _MUX; //Gia tri thanh ghi MUX
byte _PGA; //Gia tri PGA hien tai (nam trong ADCON)
byte _GPIO; //Gia tri thanh ghi GPIO
byte _STATUS; //Gia tri thanh ghi STATUS
byte _GPIOvalue; //Gia tri GPIO
byte _ByteOrder; //Thu tu byte

byte _outputBuffer[3]; //Bo dem 3 byte (24-bit) chua ket qua do gan nhat
long _outputValue; //Gia tri 24-bit da duoc ghep tu _outputBuffer[3]
bool _isAcquisitionRunning; //true = dang trong qua trinh doc lien tuc, false = dang nghi
uint8_t _cycle; //Dem xem dang o kenh thu may khi doc xoay vong qua nhieu kenh
};
#endif
