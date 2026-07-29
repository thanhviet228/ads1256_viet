//This code belongs to the ADS1256 library developed by Curious Scientist
//A very detailed documentation can be found at: https://curiousscientist.tech/ads1256-custom-library


// ================== GIẢI THÍCH CHUNG (dành cho người không rành lập trình) ==================
// Day la file vi du (demo) cho thu vien ADS1256. ADS1256 la mot con chip chuyen
// dung de do dien ap (analog) mot cach rat chinh xac, va no noi voi Arduino/vi
// dieu khien qua chuan giao tiep SPI.
//
// Cach hoat dong tong quat cua file nay:
// 1) Trong ham setup() (chay 1 lan luc khoi dong): khoi tao chip ADS1256, dat mot vai
//    cau hinh mac dinh (do khuech dai, kenh do, toc do lay mau).
// 2) Trong ham loop() (chay lien tuc, lap di lap lai): cho nguoi dung go 1 ky tu
//    lenh qua "Serial Monitor" (cua so giao tiep may tinh <-> Arduino), roi thuc
//    hien hanh dong tuong ung (do 1 lan, do lien tuc, doi kenh, doi toc do...).
//
// Ban khong can hieu sau ve SPI hay thanh ghi de dung file nay - chi can mo
// Serial Monitor, dat toc do 115200 baud, va go cac ky tu lenh duoc liet ke
// trong tung "case" ben duoi (vd: go 'T' de kiem tra ket noi, go 'a' de do 1 lan,
// go 'I' de kiem tra + in giai thich trang thai TAT CA cac thanh ghi hien tai).
// ===============================================================================================

#include <ADS1256.h>

//#define ADS1256_SPI_ALREADY_STARTED  //prevent internal _spi->begin() to allow custom SPI initialization

//Platform-specific pin definitions
//Doan duoi day tu dong nhan dien loai vi dieu khien (RP2040, STM32, Teensy, ESP32...)
//va gan san cac chan SPI phu hop cho tung loai. Ban khong can sua doan nay,
//tru khi ban muon doi sang dung 1 bus SPI khac (vd SPI1, SPI2, HSPI...).
#if defined(ARDUINO_ARCH_RP2040)
#pragma message "Using RP2040"
//#define USE_SPI1 //Alternative USE_SPI for RP2040 - Uncomment to use SPI1
#if defined(USE_SPI1)
#pragma message "Using SPI1 (SPI1)"
#define SPI_MOSI 11
#define SPI_MISO 12
#define SPI_SCK 10
#define USE_SPI SPI1
#else
#pragma message "Using SPI (SPI0)"
#define SPI_MOSI 3  //19
#define SPI_MISO 4  //16
#define SPI_SCK 2   //18
#define USE_SPI SPI
#endif
//-----------------------------------------

#elif defined(ARDUINO_ARCH_STM32)
#pragma message "Using STM32"
//#define USE_SPI2  //Uncomment to use SPI2
#if defined(USE_SPI2)
#pragma message "Using SPI2"
#define USE_SPI spi2
SPIClass spi2(PB15, PB14, PB13);  //MOSI, MISO, SCK
#else
#pragma message "Using SPI (SPI1)"
#define USE_SPI SPI  //Default SPI1, pre-instantiated as 'SPI' on PA7, PA6, PA5
#endif
                                  //-----------------------------------------
#elif defined(TEENSYDUINO)
#pragma message "Using Teensy"
//#define USE_SPI1 //Uncomment to use SPI1 on Teensy 4.0 or 4.1
//#define USE_SPI2 //Uncomment to use SPI2 on Teensy 4.0 or 4.1
#if defined(USE_SPI2)
#pragma message "Using SPI2 (SPI3)"
#define USE_SPI SPI2
#elif defined(USE_SPI1)
#pragma message "Using SPI1 (SPI2)"
#define USE_SPI SPI1
#else
#pragma message "Using SPI (SPI1)"
#define USE_SPI SPI
#endif
//-----------------------------------------

#elif defined(ARDUINO_ARCH_ESP32)
#pragma message "Using ESP32"
SPIClass hspi(HSPI);
//#define USE_HSPI  // Uncomment to use HSPI instead of VSPI
#if defined(USE_HSPI)
#pragma message "Using HSPI"
#define USE_SPI hspi
#else
#pragma message "Using VSPI"
#define USE_SPI SPI
#endif
//-----------------------------------------
#else  //Default fallback (Arduino AVR)
#define SPI_MOSI MOSI
#define SPI_MISO MISO
#define SPI_SCK SCK
#define USE_SPI SPI
//-----------------------------------------
#endif


//Below a few examples of pin descriptions for different microcontrollers I used:
//QUAN TRONG: chi duoc BO COMMENT (bo dau // o dau dong) DUY NHAT 1 dong ADS1256 A(...)
//ben duoi, tuong ung voi loai board ban dang dung, va comment lai (them // vao) cac dong con lai.
//Thu tu tham so la: DRDY, RESET, SYNC(PDWN), CS, VREF (dien ap tham chieu, dang so thuc).
//ADS1256 A(2, ADS1256::PIN_UNUSED, 8, 10, 2.500, &USE_SPI); //DRDY, RESET, SYNC(PDWN), CS, VREF(float).    //Arduino Nano/Uno - OK
//ADS1256 A(7, ADS1256::PIN_UNUSED, 10, 9, 2.500, &USE_SPI); //DRDY, RESET, SYNC(PDWN), CS, VREF(float).      //ATmega32U4 -OK
//ADS1256 A(16, 17, ADS1256::PIN_UNUSED, 15, 2.500, &USE_SPI); //DRDY, RESET, SYNC(PDWN), CS, VREF(float).   //ESP32 WROOM 32 - OK (HSPI+VSPI)
//ADS1256 A(7, ADS1256::PIN_UNUSED, 8, 10, 2.500, &USE_SPI); //DRDY, RESET, SYNC(PDWN), CS, VREF(float).    //Teensy 4.0 - OK
//ADS1256 A(7, ADS1256::PIN_UNUSED, 6, 5, 2.500, &USE_SPI); //DRDY, RESET, SYNC(PDWN), CS, VREF(float).    //RP2040 Waveshare Mini - OK - dòng này liên tục gây lỗi, cần xem lại
ADS1256 A(18, 20, 21, 19, 2.499523, &USE_SPI); //DRDY, RESET, SYNC(PDWN), CS, VREF(float), SPI bus.  //RP2040 Zero - OK
//ADS1256 A(15, ADS1256::PIN_UNUSED, 14, 17, 2.500, &USE_SPI);  //DRDY, RESET, SYNC(PDWN), CS, VREF(float), SPI bus.  //RP2040 Pico W - OK
//ADS1256 A(PA2, ADS1256::PIN_UNUSED, ADS1256::PIN_UNUSED, PA4, 2.500, &USE_SPI); //DRDY, RESET, SYNC(PDWN), CS, VREF(float). //STM32 "blue pill" - SPI1 - OK
//ADS1256 A(PB10, PB11, ADS1256::PIN_UNUSED, PB12, 2.500, &USE_SPI);  // DRDY, RESET, SYNC, CS, VREF, SPI //STM32 "blue pill" - SPI2 - OK

long rawConversion = 0;  //24-bit raw value - Ket qua do "tho" (dang so nguyen), chua doi ra Volt
float voltageValue = 0;  //human-readable floating point value - Ket qua do da doi ra Volt, de doc

int singleEndedChannels[8] = { SING_0, SING_1, SING_2, SING_3, SING_4, SING_5, SING_6, SING_7 };  //Array to store the single-ended channels
int differentialChannels[4] = { DIFF_0_1, DIFF_2_3, DIFF_4_5, DIFF_6_7 };                         //Array to store the differential channels
int inputChannel = 0;                                                                             //Number used to pick the channel from the above two arrays
char inputMode = ' ';                                                                             //can be 's' and 'd': single-ended and differential

int pgaValues[7] = { PGA_1, PGA_2, PGA_4, PGA_8, PGA_16, PGA_32, PGA_64 };  //Array to store the PGA settings
int pgaSelection = 0;                                                       //Number used to pick the PGA value from the above array

int drateValues[16] = {
  DRATE_30000SPS,
  DRATE_15000SPS,
  DRATE_7500SPS,
  DRATE_3750SPS,
  DRATE_2000SPS,
  DRATE_1000SPS,
  DRATE_500SPS,
  DRATE_100SPS,
  DRATE_60SPS,
  DRATE_50SPS,
  DRATE_30SPS,
  DRATE_25SPS,
  DRATE_15SPS,
  DRATE_10SPS,
  DRATE_5SPS,
  DRATE_2SPS
};  //Array to store the sampling rates

int drateSelection = 0;  //Number used to pick the sampling rate from the above array

//Ten hien thi (dang chu, de doc) tuong ung 1-1 voi tung gia tri trong mang drateValues[] o tren.
//Dung de "tra nguoc": tu 1 gia tri byte doc duoc trong thanh ghi DRATE, tim ra ten toc do de in cho de hieu.
const char* drateNames[16] = {
  "30000 SPS", "15000 SPS", "7500 SPS", "3750 SPS", "2000 SPS", "1000 SPS",
  "500 SPS", "100 SPS", "60 SPS", "50 SPS", "30 SPS", "25 SPS",
  "15 SPS", "10 SPS", "5 SPS", "2 SPS"
};

String registers[11] = {
  "STATUS",
  "MUX",
  "ADCON",
  "DRATE",
  "IO",
  "OFC0",
  "OFC1",
  "OFC2",
  "FSC0",
  "FSC1",
  "FSC2"
};  //Array to store the registers

int registerToRead = 0;        //Register number to be read
int registerToWrite = 0;       //Register number to be written
int registerValueToWrite = 0;  //Value to be written in the selected register

//SUA LOI (#2): ham tro giup nay thay cho kieu "while (!Serial.available());" cu.
//Truoc day, neu nguoi dung go 1 lenh (vd: 'M') nhung khong go tiep phan con lai,
//chuong trinh se bi "treo" vinh vien tai do vi cho du lieu mai khong den.
//Ham nay chi cho toi da "timeoutMs" mili-giay; neu het thoi gian van khong co
//du lieu moi, no in ra thong bao loi va tra ve false de lenh hien tai bi huy bo,
//chuong trinh quay lai vong loop() binh thuong thay vi bi treo.
bool waitForSerialInput(unsigned long timeoutMs = 3000) {
  unsigned long startTime = millis();
  while (!Serial.available()) {
    if (millis() - startTime > timeoutMs) {
      Serial.println("Loi: Het thoi gian cho du lieu tu Serial. Lenh bi huy.");
      return false;
    }
  }
  return true;
}

//In 1 byte duoi dang nhi phan, luon du 8 chu so (co so 0 dung truoc neu can)
//Vi du: gia tri 3 (0b00000011) se duoc in ra la "00000011" thay vi chi "11"
void printBinary8(byte value) {
  for (int8_t b = 7; b >= 0; b--) {
    Serial.print(bitRead(value, b));
  }
}

//Doi 1 gia tri "nibble" (4 bit, 0-15) trong thanh ghi MUX thanh ten kenh de nguoi dung de doc
//Theo datasheet: 0-7 la AIN0-AIN7, 8 la AINCOM (chan mass noi bo dung cho che do don)
String muxChannelName(uint8_t nibble) {
  if (nibble <= 7) {
    return "AIN" + String(nibble);
  } else if (nibble == 8) {
    return "AINCOM";
  } else {
    return "khong xac dinh";
  }
}

//====================================================================================================
//HAM KIEM TRA TRANG THAI CAC THANH GHI: doc toan bo 11 thanh ghi cua ADS1256, in len Serial Monitor
//va giai thich y nghia tung phan (giong nhu "dashboard" trang thai cua chip).
//Goi ham nay bang cach go ky tu 'I' (Inspect) trong Serial Monitor.
//Muc dich: giup nguoi dung nhanh chong biet chip dang duoc cau hinh nhu the nao (kenh dang do,
//he so khuech dai, toc do lay mau, cac gia tri hieu chuan...) ma khong can tra tay tung thanh ghi.
//====================================================================================================
void printAllRegistersStatus() {
  Serial.println();
  Serial.println(F("================ TRANG THAI CAC THANH GHI ADS1256 ================"));

  //Doc tat ca 11 thanh ghi (dia chi 0 den 10) ve truoc, luu vao mang de xu ly ben duoi
  byte regValues[11];
  for (int i = 0; i <= 10; i++) {
    regValues[i] = (byte)A.readRegister(i);
    delay(20);
  }

  //Bang tong quan: ten thanh ghi - thap phan - hex - nhi phan
  Serial.println(F("Dia chi | Ten     | DEC | HEX  | Nhi phan"));
  for (int i = 0; i <= 10; i++) {
    Serial.print(F("  "));
    Serial.print(i);
    Serial.print(F("     | "));
    Serial.print(registers[i]);
    for (unsigned int p = registers[i].length(); p < 8; p++) {
      Serial.print(' ');  //can le cho cac ten thanh ghi ngan hon
    }
    Serial.print(F("| "));
    Serial.print(regValues[i]);
    Serial.print(F(" | 0x"));
    if (regValues[i] < 0x10) Serial.print('0');
    Serial.print(regValues[i], HEX);
    Serial.print(F(" | "));
    printBinary8(regValues[i]);
    Serial.println();
  }

  //---------------- STATUS (0x00) ----------------
  Serial.println();
  Serial.println(F("--- STATUS (thanh ghi trang thai chung) ---"));
  byte statusValue = regValues[0];
  Serial.print(F("  ID chip (bit 7-4, chi doc): "));
  Serial.println((statusValue >> 4) & 0x0F);
  Serial.print(F("  Thu tu bit ORDER (bit 3): "));
  Serial.println(bitRead(statusValue, 3) == 0 ? F("MSB truoc (mac dinh)") : F("LSB truoc"));
  Serial.print(F("  Tu dong hieu chuan ACAL (bit 2): "));
  Serial.println(bitRead(statusValue, 2) == 1 ? F("DANG BAT") : F("DANG TAT"));
  Serial.print(F("  Bo dem dau vao BUFEN (bit 1): "));
  Serial.println(bitRead(statusValue, 1) == 1 ? F("DANG BAT (khuyen nghi)") : F("DANG TAT"));
  Serial.print(F("  DRDY (bit 0, chi doc): "));
  Serial.println(bitRead(statusValue, 0) == 0 ? F("San sang du lieu moi") : F("Chua san sang / dang chuyen doi"));

  //---------------- MUX (0x01) ----------------
  Serial.println();
  Serial.println(F("--- MUX (kenh dau vao dang chon) ---"));
  byte muxValue = regValues[1];
  uint8_t pSel = (muxValue >> 4) & 0x0F;
  uint8_t nSel = muxValue & 0x0F;
  Serial.print(F("  Kenh duong PSEL (bit 7-4): "));
  Serial.println(muxChannelName(pSel));
  Serial.print(F("  Kenh am  NSEL (bit 3-0): "));
  Serial.println(muxChannelName(nSel));
  if (nSel == 8) {
    Serial.print(F("  => Dang do CHE DO DON (single-ended): "));
    Serial.print(muxChannelName(pSel));
    Serial.println(F(" so voi AINCOM (mass)"));
  } else {
    Serial.print(F("  => Dang do CHE DO VI SAI (differential): "));
    Serial.print(muxChannelName(pSel));
    Serial.print(F(" - "));
    Serial.println(muxChannelName(nSel));
  }

  //---------------- ADCON (0x02) ----------------
  Serial.println();
  Serial.println(F("--- ADCON (khuech dai & clock) ---"));
  byte adconValue = regValues[2];
  uint8_t clkBits = (adconValue >> 5) & 0x03;
  uint8_t sdcsBits = (adconValue >> 3) & 0x03;
  uint8_t pgaBits = adconValue & 0x07;
  Serial.print(F("  CLKOUT (bit 6-5): "));
  Serial.println(clkBits);
  Serial.print(F("  SDCS - Sensor Detect Current (bit 4-3): "));
  Serial.println(sdcsBits == 0 ? F("TAT (mac dinh)") : F("DANG BAT"));
  Serial.print(F("  PGA - He so khuech dai (bit 2-0): x"));
  Serial.println((int)pow(2, pgaBits));
  Serial.print(F("  => Dai dien ap dau vao toi da khoang +/- "));
  Serial.print(5.0 / pow(2, pgaBits), 4);
  Serial.println(F(" V (uoc tinh voi VREF chuan = 2.5V)"));

  //---------------- DRATE (0x03) ----------------
  Serial.println();
  Serial.println(F("--- DRATE (toc do lay mau) ---"));
  byte drateValue = regValues[3];
  bool drateFound = false;
  for (int i = 0; i < 16; i++) {
    if (drateValues[i] == drateValue) {
      Serial.print(F("  => Toc do lay mau hien tai: "));
      Serial.println(drateNames[i]);
      drateFound = true;
      break;
    }
  }
  if (!drateFound) {
    Serial.println(F("  => Gia tri khong khop voi bang toc do chuan (co the la gia tri tuy chinh)"));
  }

  //---------------- IO (0x04) ----------------
  Serial.println();
  Serial.println(F("--- IO (4 chan GPIO phu D0-D3) ---"));
  byte ioValue = regValues[4];
  for (int pinNumber = 3; pinNumber >= 0; pinNumber--) {
    Serial.print(F("  D"));
    Serial.print(pinNumber);
    Serial.print(F(": chieu = "));
    Serial.print(bitRead(ioValue, pinNumber + 4) == 1 ? F("INPUT ") : F("OUTPUT"));
    Serial.print(F(", gia tri hien tai = "));
    Serial.println(bitRead(ioValue, pinNumber));
  }

  //---------------- OFC0/OFC1/OFC2 (0x05-0x07) - Hieu chuan Offset ----------------
  Serial.println();
  Serial.println(F("--- OFC0/OFC1/OFC2 (he so hieu chuan OFFSET, 24-bit, co dau) ---"));
  long ofcRaw = ((long)regValues[7] << 16) | ((long)regValues[6] << 8) | regValues[5];
  if (ofcRaw & 0x800000) {  //Neu bit dau (bit 23) la 1 -> day la so am dang bu 2
    ofcRaw -= 0x1000000;
  }
  Serial.print(F("  Gia tri Offset Calibration (thap phan, co dau): "));
  Serial.println(ofcRaw);

  //---------------- FSC0/FSC1/FSC2 (0x08-0x0A) - Hieu chuan Full-Scale ----------------
  Serial.println();
  Serial.println(F("--- FSC0/FSC1/FSC2 (he so hieu chuan FULL-SCALE, 24-bit, khong dau) ---"));
  unsigned long fscRaw = ((unsigned long)regValues[10] << 16) | ((unsigned long)regValues[9] << 8) | regValues[8];
  Serial.print(F("  Gia tri Full-Scale Calibration (thap phan): "));
  Serial.println(fscRaw);
  Serial.println(F("  (Gia tri nay do chip tu tinh khi chay SELFCAL/SYSGCAL, khong can chinh tay)"));

  Serial.println(F("====================================================================="));
  Serial.println();
}

void setup() {
  Serial.begin(115200);  //The value does not matter if you use an MCU with native USB

  while (!Serial) {
    ;  //Wait until the serial becomes available
  }

  Serial.println("ADS1256 - Custom Library Demo File by Curious Scientist - 2025-05-28");

#if defined(ARDUINO_ARCH_RP2040)  //If RP2040 is used, we need to pass the SPI pins
  SPI.setSCK(SPI_SCK);
  SPI.setTX(SPI_MOSI);
  SPI.setRX(SPI_MISO);
#endif

#if defined(USE_HSPI)      //If ESP32 is used, we need to start SPI with a non-strapping MISO pin
  hspi.begin(14, 25, 13);  //SCK, MISO (safe), MOSI
#endif

  A.InitializeADC();  //See the documentation for every details
  //Setting up CS, RESET, SYNC and SPI
  //Assigning default values to: STATUS, MUX, ADCON, DRATE
  //Performing a SYSCAL

  //Below is a demonstration to change the values through the built-on functions of the library
  //Set a PGA value
  A.setPGA(PGA_1);  //0b00000000 - DEC: 0
  //--------------------------------------------

  //Set input channels
  A.setMUX(DIFF_6_7);  //0b01100111 - DEC: 103
  //--------------------------------------------

  //Set DRATE
  A.setDRATE(DRATE_5SPS);  //0b00010011 - DEC: 19
  //--------------------------------------------

  //Read back the above 3 values to check if the writing was succesful
  Serial.print("PGA: ");
  Serial.println(A.getPGA());
  delay(100);
  //--
  Serial.print("MUX: ");
  Serial.println(A.readRegister(MUX_REG));
  delay(100);
  //--
  Serial.print("DRATE: ");
  Serial.println(A.readRegister(DRATE_REG));
  delay(100);

  //Freeze the display for 3 sec
  delay(3000);
}

void loop() {
  /* Here I implemented some typical functions that can be useful for using the ADS1256
      Changing the registers by the built-in functions or by directly writing the registers
      Reading the registers to check the value of it
      Read a single conversion
      Read a single channel continuously
      Read multiple channels continuously
      Stop the conversion
      ...etc.
      All the above things are done through the serial port, the use only has to send certain commands
  */

  // ------ GIAI THICH: ------
  // Cu moi vong lap, chuong trinh kiem tra xem co ky tu nao duoc go vao Serial
  // Monitor hay khong. Neu co, no doc ky tu do va dua vao khoi switch-case ben
  // duoi de quyet dinh phai lam gi (do 1 lan, do lien tuc, doi kenh...).

  if (Serial.available() > 0) {
    char commandCharacter = Serial.read();  //we use characters (letters) for controlling the switch-case

    switch (commandCharacter)  //based on the command character, we decide what to do
    {
      case 's':  //SDATAC - Stop Reading Data Continously
        A.stopConversion();
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'L':  //Perform a self calibration
        A.sendDirectCommand(SELFCAL);
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'G':                       //Read a single input continuously
        while (Serial.read() != 's')  //The conversion is stopped by a character received from the serial port
        {
          Serial.println(A.convertToVoltage(A.readSingleContinuous()), 8);
          //The conversion is printed in Volts with 6 decimal digits
          //Note: Certain serial terminals cannot keep up with high speed datastream!
        }
        A.stopConversion();
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'C':                       //Cycle single ended inputs (A0+GND, A1+GND ... A7+GND)
        while (Serial.read() != 's')  //The conversion is stopped by a character received from the serial port
        {
          float channels[8];  //Buffer that holds 8 conversions (8 single-ended channels)
          for (int j = 0; j < 8; j++) {
            channels[j] = A.convertToVoltage(A.cycleSingle());  //store the converted single-ended results in the buffer
          }
          for (int i = 0; i < 8; i++) {
            Serial.print(channels[i], 8);  //print the converted single-ended results with 4 digits

            if (i < 7)  //Only printing tab between the first 7 conversions
            {
              Serial.print("\t");  //tab separator to separate the 8 conversions shown in the same line
            }
          }
          Serial.println();  //Printing a linebreak - this will put the next 8 conversions in a new line
        }
        A.stopConversion();
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'D':                       //Cycle differential inputs (A0+A1, A2+A3, A4+A5, A6+A7)
        while (Serial.read() != 's')  //The conversion is stopped by a character received from the serial port
        {
          float channels[4];  //Buffer that holds 4 conversions (4 differential channels)
          for (int j = 0; j < 4; j++) {
            channels[j] = A.convertToVoltage(A.cycleDifferential());  //store the converted differential results in the buffer
          }

          //After the 4 conversions are in the buffer, the contents are printed on the serial terminal
          for (int i = 0; i < 4; i++) {
            Serial.print(channels[i], 8);  //print the converted differential results from the buffer

            if (i < 3)  //Only printing tab between the first 3 conversions
            {
              Serial.print("\t");  //tab separator to separate the 4 conversions shown in the same line
            }
          }
          Serial.println();  //Printing a linebreak - this will put the next 4 conversions in a new line
        }
        A.stopConversion();
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'B':  //Speed test
        {
          //Variables to store and measure elapsed time and define the number of conversions
          long numberOfSamples = 30000;  //Number of conversions
          long finishTime = 0;
          long startTime = micros();

          for (long i = 0; i < numberOfSamples; i++) {
            A.readSingleContinuous();
            //Note: here we just perform the readings and we don't print the results
          }

          finishTime = micros() - startTime;  //Calculate the elapsed time

          A.stopConversion();

          //Printing the results
          Serial.print("Total conversion time for 30k samples: ");
          Serial.print(finishTime);
          Serial.println(" us");

          Serial.print("Sampling rate: ");
          Serial.print(numberOfSamples * (1000000.0 / finishTime), 3);
          Serial.println(" SPS");
        }
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'T':  //Testing the serial connection
        Serial.println("The serial connection is OK!");
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'I':  //Inspect - Kiem tra va in trang thai toan bo cac thanh ghi, kem giai thich y nghia
        printAllRegistersStatus();
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'a':  //Testing a single conversion - Only one single result is returned

        rawConversion = A.readSingle();                    //Reading the raw value from a previously selected input, passing it to a variable
        voltageValue = A.convertToVoltage(rawConversion);  //Converting the above conversion into a floating point number

        //Printing the results
        Serial.print("Single-ended conversion result: ");
        Serial.println(voltageValue, 8);  //Print the floating point number with 8 digits.
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'M':  //set MUX - Chon kenh dau vao. Cu phap: "Ms<so>" (don) hoac "Md<so>" (vi sai)
        {
          //SUA LOI (#2): dung waitForSerialInput() thay vi while(!Serial.available());
          //de tranh treo may neu khong co du lieu tiep theo duoc gui den.
          if (!waitForSerialInput()) break;
          inputMode = Serial.read();  //Read the input mode

          if (inputMode == 's')  //single-ended
          {
            if (!waitForSerialInput()) break;
            inputChannel = Serial.parseInt();
            //SUA LOI (#1): kiem tra inputChannel co nam trong khoang hop le (0-7) hay khong
            //truoc khi dung no de truy cap mang singleEndedChannels[]. Truoc day, mot gia tri
            //ngoai khoang (vd 99) se doc/ghi ngoai vung nho cua mang, gay loi khong luong truoc.
            if (inputChannel < 0 || inputChannel > 7) {
              Serial.println("Loi: So kenh (single-ended) khong hop le. Chi chap nhan 0-7.");
              break;
            }
            A.setMUX(singleEndedChannels[inputChannel]);
            //Example: "Ms1" selects the SING_1 as input channel
          }

          if (inputMode == 'd')  //differential
          {
            if (!waitForSerialInput()) break;
            inputChannel = Serial.parseInt();
            //SUA LOI (#1): kiem tra inputChannel co nam trong khoang hop le (0-3) hay khong
            if (inputChannel < 0 || inputChannel > 3) {
              Serial.println("Loi: So kenh (differential) khong hop le. Chi chap nhan 0-3.");
              break;
            }
            A.setMUX(differentialChannels[inputChannel]);
            //Example: "Md0" selects the DIFF_0_1 as input channel
          }
        }
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'P':  //Set PGA - Dat he so khuech dai. Cu phap: "P<so>" (0-6)
        {
          if (!waitForSerialInput()) break;
          pgaSelection = Serial.parseInt();
          //SUA LOI (#1): kiem tra pgaSelection co nam trong khoang hop le (0-6) hay khong
          //truoc khi truy cap mang pgaValues[] (mang co 7 phan tu, chi so 0-6).
          if (pgaSelection < 0 || pgaSelection > 6) {
            Serial.println("Loi: Gia tri PGA khong hop le. Chi chap nhan 0-6.");
            break;
          }
          A.setPGA(pgaValues[pgaSelection]);
          //Example: P4 will select the PGA = 16
          Serial.print("PGA value: ");
          Serial.println(A.getPGA());  //Read the PGA from the register and print it
        }
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'F':  //Set sampling frequency - Dat toc do lay mau. Cu phap: "F<so>" (0-15)
        {
          if (!waitForSerialInput()) break;
          drateSelection = Serial.parseInt();  //Parse the number (item number in the array) Phân tích số (số mục trong mảng)
          //SUA LOI (#1): kiem tra drateSelection co nam trong khoang hop le (0-15) hay khong
          //truoc khi truy cap mang drateValues[] (mang co 16 phan tu, chi so 0-15).
          if (drateSelection < 0 || drateSelection > 15) {
            Serial.println("Loi: Gia tri toc do lay mau khong hop le. Chi chap nhan 0-15.");
            break;
          }
          delay(100);
          Serial.print("DRATE is selected as: ");
          Serial.println(drateValues[drateSelection]);  //Print the value from the array
          delay(100);
          A.setDRATE(drateValues[drateSelection]);  //Pass the value to the register on the ADS1256
          delay(100);
          Serial.print("DRATE is set to ");
          Serial.println(A.readRegister(DRATE_REG));  //Read the register to see if the value was updated correctly
          //Example: F3 will make the DRATE = 3750 SPS
        }
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'R':  //read register - Doc 1 thanh ghi. Cu phap: "R<so>" (0-10)
        {
          if (!waitForSerialInput()) break;
          registerToRead = Serial.parseInt();  //This part reads the number of the register from the serial port
          //SUA LOI (#1): kiem tra registerToRead co nam trong khoang hop le (0-10) hay khong
          //truoc khi truy cap mang registers[] (mang co 11 phan tu kieu String, chi so 0-10).
          //Neu khong kiem tra, mot gia tri ngoai khoang (vd "R99") co the doc ngoai vung nho.
          if (registerToRead < 0 || registerToRead > 10) {
            Serial.println("Loi: So thanh ghi khong hop le. Chi chap nhan 0-10.");
            break;
          }
          Serial.print("Value of ");
          Serial.print(registers[registerToRead]);
          Serial.print(" register is: ");
          Serial.println(A.readRegister(registerToRead));
          //Example: "R2" will read the register at address 2 which is the ADCON register
          //Note: The value is printed as a decimal number
        }
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'W':  //Write register - Ghi 1 gia tri vao 1 thanh ghi. Cu phap: "W<so_thanh_ghi> <gia_tri>"
        {
          if (!waitForSerialInput()) break;
          registerToWrite = Serial.parseInt();  //This part reads the number of the register from the serial port
          //SUA LOI (#1): kiem tra registerToWrite co nam trong khoang hop le (0-10) hay khong
          if (registerToWrite < 0 || registerToWrite > 10) {
            Serial.println("Loi: So thanh ghi khong hop le. Chi chap nhan 0-10.");
            break;
          }
          if (!waitForSerialInput()) break;
          registerValueToWrite = Serial.parseInt();  //This part reads the value of the register from the serial port
          //SUA LOI (#1): kiem tra registerValueToWrite co nam trong khoang 1 byte (0-255) hay khong,
          //vi thanh ghi cua ADS1256 chi rong 8-bit.
          if (registerValueToWrite < 0 || registerValueToWrite > 255) {
            Serial.println("Loi: Gia tri ghi khong hop le. Chi chap nhan 0-255.");
            break;
          }

          A.writeRegister(registerToWrite, registerValueToWrite);
          //Example: "W1 35" will write 35 ("00100011") on register 1 which is the MUX register.
          //This will make the input as DIFF_2_3 (A2(+) & A1(-))
        }
        break;
        //--------------------------------------------------------------------------------------------------------
    }
  }
}
