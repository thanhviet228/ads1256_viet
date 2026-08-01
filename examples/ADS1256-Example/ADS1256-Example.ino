//Đoạn code này thuộc về thư viện ADS1256 được phát triển bởi Curious Scientist
//Tài liệu chi tiết có thể xem tại: https://curiousscientist.tech/ads1256-custom-library

// ================== GIẢI THÍCH CHUNG (dành cho người không rành lập trình) ==================
// Đây là file ví dụ (demo) cho thư viện ADS1256. ADS1256 là một con chip chuyên
// dùng để đo điện áp (analog) một cách rất chính xác, và nó nối với Arduino/vi
// điều khiển qua chuẩn giao tiếp SPI.
//
// Cách hoạt động tổng quát của file này:
// 1) Trong hàm setup() (chạy 1 lần lúc khởi động): khởi tạo chip ADS1256, đặt một vài
//    cấu hình mặc định (độ khuếch đại, kênh đo, tốc độ lấy mẫu).
// 2) Trong hàm loop() (chạy liên tục, lặp đi lặp lại): cho người dùng gõ 1 ký tự
//    lệnh qua "Serial Monitor" (cửa sổ giao tiếp máy tính <-> Arduino), rồi thực
//    hiện hành động tương ứng (đo 1 lần, đo liên tục, đổi kênh, đổi tốc độ...).
//
// Bạn không cần hiểu sâu về SPI hay thanh ghi để dùng file này - chỉ cần mở
// Serial Monitor, đặt tốc độ 115200 baud, và gõ các ký tự lệnh được liệt kê
// trong từng "case" bên dưới (vd: gõ 'T' để kiểm tra kết nối, gõ 'a' để đo 1 lần,
// gõ 'I' để kiểm tra + in giải thích trạng thái TẤT CẢ các thanh ghi hiện tại).
// ===============================================================================================

#include <ADS1256.h>

//#define ADS1256_SPI_ALREADY_STARTED  
//Ngăn chặn việc gọi _spi->begin() bên trong để cho phép khởi tạo SPI tùy chỉnh.

//Các định nghĩa chân cắm dành riêng cho nền tảng
//Đoạn dưới đây tự động nhận diện loại vi điều khiển (RP2040, STM32, Teensy, ESP32...)
//và gán sẵn các chân SPI phù hợp cho từng loại. Bạn không cần sửa đoạn này,
//trừ khi bạn muốn đổi sang dùng 1 bus SPI khác (vd SPI1, SPI2, HSPI...).
#if defined(ARDUINO_ARCH_RP2040)
#pragma message "Using RP2040"
//#define USE_SPI1 //Tùy chọn USE_SPI cho RP2040 - Bỏ chú thích để sử dụng SPI1
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
#define USE_SPI SPI  //SPI1 mặc định, được khởi tạo sẵn dưới tên 'SPI' trên các chân PA7, PA6, PA5.
#endif
                                  //-----------------------------------------
#elif defined(TEENSYDUINO)
#pragma message "Using Teensy"
//#define USE_SPI1 //Bỏ chú thích để sử dụng SPI1 trên Teensy 4.0 hoặc 4.1
//#define USE_SPI2 //Bỏ chú thích để sử dụng SPI2 trên Teensy 4.0 hoặc 4.1
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
//#define USE_HSPI  // Bỏ chú thích để sử dụng HSPI thay vì VSPI.
#if defined(USE_HSPI)
#pragma message "Using HSPI"
#define USE_SPI hspi
#else
#pragma message "Using VSPI"
#define USE_SPI SPI
#endif
//-----------------------------------------
#else  //Mặc định dự phòng (Arduino AVR)
#define SPI_MOSI MOSI
#define SPI_MISO MISO
#define SPI_SCK SCK
#define USE_SPI SPI
//-----------------------------------------
#endif


//Dưới đây là một vài ví dụ về mô tả chân cho các vi điều khiển khác nhau mà tôi đã sử dụng:
//Thứ tự tham số là: DRDY, RESET, SYNC(PDWN), CS, VREF (điện áp tham chiếu, dạng số thực).
//ADS1256 A(2, ADS1256::PIN_UNUSED, 8, 10, 2.500, &USE_SPI);          //Arduino Nano/Uno - OK
//ADS1256 A(7, ADS1256::PIN_UNUSED, 10, 9, 2.500, &USE_SPI);          //ATmega32U4 -OK
//ADS1256 A(16, 17, ADS1256::PIN_UNUSED, 15, 2.500, &USE_SPI);        //ESP32 WROOM 32 - OK (HSPI+VSPI)
//ADS1256 A(7, ADS1256::PIN_UNUSED, 8, 10, 2.500, &USE_SPI);          //Teensy 4.0 - OK
//ADS1256 A(7, ADS1256::PIN_UNUSED, 6, 5, 2.500, &USE_SPI);           //RP2040 Waveshare Mini - OK - dòng này liên tục gây lỗi, cần xem lại
ADS1256 A(18, 20, 21, 19, 2.499523, &USE_SPI);                        // SPI bus.  //RP2040 Zero - OK
//ADS1256 A(15, ADS1256::PIN_UNUSED, 14, 17, 2.500, &USE_SPI);        // SPI bus.  RP2040 Pico W - OK
//ADS1256 A(PA2, ADS1256::PIN_UNUSED, ADS1256::PIN_UNUSED, PA4, 2.500, &USE_SPI); ///STM32 "blue pill" - SPI1 - OK
//ADS1256 A(PB10, PB11, ADS1256::PIN_UNUSED, PB12, 2.500, &USE_SPI);  //STM32 "blue pill" - SPI2 - OK

long rawConversion = 0;  //24-bit raw value - Kết quả đo "thô" (dạng số nguyên)
float voltageValue = 0;  //Kết quả đo đã đổi ra Volt, dễ đọc

int singleEndedChannels[8] = { SING_0, SING_1, SING_2, SING_3, SING_4, SING_5, SING_6, SING_7 };  //Mảng lưu trữ các kênh đơn đầu - single-ended 
int differentialChannels[4] = { DIFF_0_1, DIFF_2_3, DIFF_4_5, DIFF_6_7 };                         //Mảng lưu trữ các kênh vi sai - differential 
int inputChannel = 0;                                                                             //Số được sử dụng để chọn kênh từ hai mảng nêu trên.
char inputMode = ' ';                                                             //có thể là 's' và 'd': đơn đầu (single-ended) và vi sai (differential)

int pgaValues[7] = { PGA_1, PGA_2, PGA_4, PGA_8, PGA_16, PGA_32, PGA_64 };  //Mảng lưu trữ các thiết lập PGA
int pgaSelection = 0;               //Giá trị số được sử dụng để chọn giá trị PGA từ mảng nêu trên.

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
};  //Mảng lưu trữ các tốc độ lấy mẫu

int drateSelection = 0;  //Số được sử dụng để chọn tốc độ lấy mẫu từ mảng nêu trên.

//Tên hiển thị (dạng chữ, dễ đọc) tương ứng 1-1 với từng giá trị trong mảng drateValues[] ở trên.
//Dùng để "tra ngược": từ 1 giá trị byte đọc được trong thanh ghi DRATE, 
//tìm ra tên tốc độ để in cho dễ hiểu.
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
};  //Mảng để lưu trữ các thanh ghi
int registerToRead = 0;        //Số hiệu thanh ghi cần đọc
int registerToWrite = 0;       //Số hiệu thanh ghi cần ghi
int registerValueToWrite = 0;  //Giá trị cần ghi vào thanh ghi đã chọn

//SỬA LỖI (#2): hàm trợ giúp này thay cho kiểu "while (!Serial.available());" cũ.
//Trước đây, nếu người dùng gõ 1 lệnh (vd: 'M') nhưng không gõ tiếp phần còn lại,
//chương trình sẽ bị "treo" vĩnh viễn tại đó vì chờ dữ liệu mãi không đến.
//Hàm này chỉ cho tối đa "timeoutMs" mili-giây; nếu hết thời gian vẫn không có
//dữ liệu mới, nó in ra thông báo lỗi và trả về false để lệnh hiện tại bị hủy bỏ,
//chương trình quay lại vòng loop() bình thường thay vì bị treo.
bool waitForSerialInput(unsigned long timeoutMs = 3000) {
  unsigned long startTime = millis();
  while (!Serial.available()) {
    if (millis() - startTime > timeoutMs) {
      Serial.println("Lỗi: Hết thời gian chờ dữ liệu từ Serial. Lệnh bị hủy.");
      return false;
    }
  }
  return true;
}

//In 1 byte dưới dạng nhị phân, luôn đủ 8 chữ số (có số 0 đứng trước nếu cần)
//Ví dụ: giá trị 3 (0b00000011) sẽ được in ra là "00000011" thay vì chỉ "11"
void printBinary8(byte value) {
  for (int8_t b = 7; b >= 0; b--) {
    Serial.print(bitRead(value, b));
  }
}

//Đổi 1 giá trị "nibble" (4 bit, 0-15) trong thanh ghi MUX thành tên kênh để người dùng dễ đọc
//Theo datasheet: 0-7 là AIN0-AIN7, 8 là AINCOM (chân mass nội bộ dùng cho chế độ đơn)
String muxChannelName(uint8_t nibble) {
  if (nibble <= 7) {
    return "AIN" + String(nibble);
  } else if (nibble == 8) {
    return "AINCOM";
  } else {
    return "không xác định";
  }
}

//====================================================================================================
//HÀM KIỂM TRA TRẠNG THÁI CÁC THANH GHI: đọc toàn bộ 11 thanh ghi của ADS1256, in lên Serial Monitor
//và giải thích ý nghĩa từng phần (giống như "dashboard" trạng thái của chip).
//Gọi hàm này bằng cách gõ ký tự 'I' (Inspect) trong Serial Monitor.
//Mục đích: giúp người dùng nhanh chóng biết chip đang được cấu hình như thế nào (kênh đang đo,
//hệ số khuếch đại, tốc độ lấy mẫu, các giá trị hiệu chuẩn...) mà không cần tra tay từng thanh ghi.
//====================================================================================================
void printAllRegistersStatus() {
  Serial.println();
  Serial.println(F("================ TRẠNG THÁI CÁC THANH GHI ADS1256 ================"));

  //Đọc tất cả 11 thanh ghi (địa chỉ 0 đến 10) về trước, lưu vào mảng để xử lý bên dưới
  byte regValues[11];
  for (int i = 0; i <= 10; i++) {
    regValues[i] = (byte)A.readRegister(i);
    delay(20);
  }

  //Bảng tổng quan: tên thanh ghi - thập phân - hex - nhị phân
  Serial.println(F("Địa chỉ | Tên     | DEC | HEX  | Nhị phân"));
  for (int i = 0; i <= 10; i++) {
    Serial.print(F("  "));
    Serial.print(i);
    Serial.print(F("     | "));
    Serial.print(registers[i]);
    for (unsigned int p = registers[i].length(); p < 8; p++) {
      Serial.print(' ');  //căn lề cho các tên thanh ghi ngắn hơn
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
  Serial.println(F("--- STATUS (thanh ghi trạng thái chung) ---"));
  byte statusValue = regValues[0];
  Serial.print(F("  ID chip (bit 7-4, chỉ đọc): "));
  Serial.println((statusValue >> 4) & 0x0F);
  Serial.print(F("  Thứ tự bit ORDER (bit 3): "));
  Serial.println(bitRead(statusValue, 3) == 0 ? F("MSB trước (mặc định)") : F("LSB trước"));
  Serial.print(F("  Tự động hiệu chuẩn ACAL (bit 2): "));
  Serial.println(bitRead(statusValue, 2) == 1 ? F("ĐANG BẬT") : F("ĐANG TẮT"));
  Serial.print(F("  Bộ đệm đầu vào BUFEN (bit 1): "));
  Serial.println(bitRead(statusValue, 1) == 1 ? F("ĐANG BẬT (khuyến nghị)") : F("ĐANG TẮT"));
  Serial.print(F("  DRDY (bit 0, chỉ đọc): "));
  Serial.println(bitRead(statusValue, 0) == 0 ? F("Sẵn sàng dữ liệu mới") : F("Chưa sẵn sàng / đang chuyển đổi"));

  //---------------- MUX (0x01) ----------------
  Serial.println();
  Serial.println(F("--- MUX (kênh đầu vào đang chọn) ---"));
  byte muxValue = regValues[1];
  uint8_t pSel = (muxValue >> 4) & 0x0F;
  uint8_t nSel = muxValue & 0x0F;
  Serial.print(F("  Kênh dương PSEL (bit 7-4): "));
  Serial.println(muxChannelName(pSel));
  Serial.print(F("  Kênh âm  NSEL (bit 3-0): "));
  Serial.println(muxChannelName(nSel));
  if (nSel == 8) {
    Serial.print(F("  => Đang đo CHẾ ĐỘ ĐƠN (single-ended): "));
    Serial.print(muxChannelName(pSel));
    Serial.println(F(" so với AINCOM (mass)"));
  } else {
    Serial.print(F("  => Đang đo CHẾ ĐỘ VI SAI (differential): "));
    Serial.print(muxChannelName(pSel));
    Serial.print(F(" - "));
    Serial.println(muxChannelName(nSel));
  }

  //---------------- ADCON (0x02) ----------------
  Serial.println();
  Serial.println(F("--- ADCON (khuếch đại & clock) ---"));
  byte adconValue = regValues[2];
  uint8_t clkBits = (adconValue >> 5) & 0x03;
  uint8_t sdcsBits = (adconValue >> 3) & 0x03;
  uint8_t pgaBits = adconValue & 0x07;
  Serial.print(F("  CLKOUT (bit 6-5): "));
  Serial.println(clkBits);
  Serial.print(F("  SDCS - Sensor Detect Current (bit 4-3): "));
  Serial.println(sdcsBits == 0 ? F("TẮT (mặc định)") : F("ĐANG BẬT"));
  Serial.print(F("  PGA - Hệ số khuếch đại (bit 2-0): x"));
  Serial.println((int)pow(2, pgaBits));
  Serial.print(F("  => Dải điện áp đầu vào tối đa khoảng +/- "));
  Serial.print(5.0 / pow(2, pgaBits), 4);
  Serial.println(F(" V (ước tính với VREF chuẩn = 2.5V)"));

  //---------------- DRATE (0x03) ----------------
  Serial.println();
  Serial.println(F("--- DRATE (tốc độ lấy mẫu) ---"));
  byte drateValue = regValues[3];
  bool drateFound = false;
  for (int i = 0; i < 16; i++) {
    if (drateValues[i] == drateValue) {
      Serial.print(F("  => Tốc độ lấy mẫu hiện tại: "));
      Serial.println(drateNames[i]);
      drateFound = true;
      break;
    }
  }
  if (!drateFound) {
    Serial.println(F("  => Giá trị không khớp với bảng tốc độ chuẩn (có thể là giá trị tùy chỉnh)"));
  }

  //---------------- IO (0x04) ----------------
  Serial.println();
  Serial.println(F("--- IO (4 chân GPIO phụ D0-D3) ---"));
  byte ioValue = regValues[4];
  for (int pinNumber = 3; pinNumber >= 0; pinNumber--) {
    Serial.print(F("  D"));
    Serial.print(pinNumber);
    Serial.print(F(": chiều = "));
    Serial.print(bitRead(ioValue, pinNumber + 4) == 1 ? F("INPUT ") : F("OUTPUT"));
    Serial.print(F(", giá trị hiện tại = "));
    Serial.println(bitRead(ioValue, pinNumber));
  }

  //---------------- OFC0/OFC1/OFC2 (0x05-0x07) - Hiệu chuẩn Offset ----------------
  Serial.println();
  Serial.println(F("--- OFC0/OFC1/OFC2 (hệ số hiệu chuẩn OFFSET, 24-bit, có dấu) ---"));
  long ofcRaw = ((long)regValues[7] << 16) | ((long)regValues[6] << 8) | regValues[5];
  if (ofcRaw & 0x800000) {  //Nếu bit dấu (bit 23) là 1 -> đây là số âm dạng bù 2
    ofcRaw -= 0x1000000;
  }
  Serial.print(F("  Giá trị Offset Calibration (thập phân, có dấu): "));
  Serial.println(ofcRaw);

  //---------------- FSC0/FSC1/FSC2 (0x08-0x0A) - Hiệu chuẩn Full-Scale ----------------
  Serial.println();
  Serial.println(F("--- FSC0/FSC1/FSC2 (hệ số hiệu chuẩn FULL-SCALE, 24-bit, không dấu) ---"));
  unsigned long fscRaw = ((unsigned long)regValues[10] << 16) | ((unsigned long)regValues[9] << 8) | regValues[8];
  Serial.print(F("  Giá trị Full-Scale Calibration (thập phân): "));
  Serial.println(fscRaw);
  Serial.println(F("  (Giá trị này do chip tự tính khi chạy SELFCAL/SYSGCAL, không cần chỉnh tay)"));

  Serial.println(F("====================================================================="));
  Serial.println();
}

void setup() {
  Serial.begin(115200);  //Giá trị này không quan trọng nếu bạn dùng vi điều khiển có USB gốc (native USB)

  while (!Serial) {
    ;  //Chờ cho đến khi cổng serial sẵn sàng
  }

  Serial.println("ADS1256 - Custom Library Demo File by Curious Scientist - 2025-05-28");

#if defined(ARDUINO_ARCH_RP2040)  //Nếu dùng RP2040, cần khai báo các chân SPI
  SPI.setSCK(SPI_SCK);
  SPI.setTX(SPI_MOSI);
  SPI.setRX(SPI_MISO);
#endif

#if defined(USE_HSPI)      //Nếu dùng ESP32, cần khởi động SPI với chân MISO không phải chân "strapping"
  hspi.begin(14, 25, 13);  //SCK, MISO (an toàn), MOSI
#endif

  A.InitializeADC();  //Xem tài liệu để biết chi tiết đầy đủ
  //Thiết lập CS, RESET, SYNC và SPI
  //Gán các giá trị mặc định cho: STATUS, MUX, ADCON, DRATE
  //Thực hiện lệnh SYSCAL

  //Dưới đây là ví dụ minh họa cách thay đổi các giá trị thông qua các hàm có sẵn của thư viện
  //Đặt giá trị PGA
  A.setPGA(PGA_1);  //0b00000000 - DEC: 0
  //--------------------------------------------

  //Đặt kênh đầu vào
  A.setMUX(DIFF_6_7);  //0b01100111 - DEC: 103
  //--------------------------------------------

  //Đặt DRATE
  A.setDRATE(DRATE_5SPS);  //0b00010011 - DEC: 19
  //--------------------------------------------

  //Đọc lại 3 giá trị trên để kiểm tra việc ghi có thành công hay không
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

  //Tạm dừng hiển thị trong 3 giây
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

  // ------ GIẢI THÍCH: ------
  // Cứ mỗi vòng lặp, chương trình kiểm tra xem có ký tự nào được gõ vào Serial
  // Monitor hay không. Nếu có, nó đọc ký tự đó và đưa vào khối switch-case bên
  // dưới để quyết định phải làm gì (đo 1 lần, đo liên tục, đổi kênh...).

  if (Serial.available() > 0) {
    char commandCharacter = Serial.read();  //ta dùng các ký tự (chữ cái) để điều khiển khối switch-case

    switch (commandCharacter)  //dựa vào ký tự lệnh, ta quyết định phải làm gì
    {
      case 's':  //SDATAC - Dừng đọc dữ liệu liên tục
        A.stopConversion();
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'L':  //Thực hiện tự hiệu chuẩn (self calibration)
        A.sendDirectCommand(SELFCAL);
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'G':                       //Đọc liên tục 1 đầu vào
        while (Serial.read() != 's')  //Phép chuyển đổi bị dừng bởi 1 ký tự nhận được từ cổng serial
        {
          Serial.println(A.convertToVoltage(A.readSingleContinuous()), 8);
          //Kết quả chuyển đổi được in ra dưới dạng Volt với 6 chữ số thập phân
          //Lưu ý: Một số phần mềm terminal serial không theo kịp luồng dữ liệu tốc độ cao!
        }
        A.stopConversion();
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'C':                       //Quét lần lượt các đầu vào đơn (A0+GND, A1+GND ... A7+GND)
        while (Serial.read() != 's')  //Phép chuyển đổi bị dừng bởi 1 ký tự nhận được từ cổng serial
        {
          float channels[8];  //Bộ đệm lưu 8 kết quả chuyển đổi (8 kênh đơn - single-ended)
          for (int j = 0; j < 8; j++) {
            channels[j] = A.convertToVoltage(A.cycleSingle());  //lưu các kết quả đơn đã chuyển đổi vào bộ đệm
          }
          for (int i = 0; i < 8; i++) {
            Serial.print(channels[i], 8);  //in các kết quả đơn đã chuyển đổi với 4 chữ số

            if (i < 7)  //Chỉ in tab giữa 7 kết quả đầu tiên
            {
              Serial.print("\t");  //ký tự tab phân cách 8 kết quả hiển thị trên cùng 1 dòng
            }
          }
          Serial.println();  //In xuống dòng - để 8 kết quả tiếp theo nằm ở dòng mới
        }
        A.stopConversion();
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'D':                       //Quét lần lượt các đầu vào vi sai (A0+A1, A2+A3, A4+A5, A6+A7)
        while (Serial.read() != 's')  //Phép chuyển đổi bị dừng bởi 1 ký tự nhận được từ cổng serial
        {
          float channels[4];  //Bộ đệm lưu 4 kết quả chuyển đổi (4 kênh vi sai - differential)
          for (int j = 0; j < 4; j++) {
            channels[j] = A.convertToVoltage(A.cycleDifferential());  //lưu các kết quả vi sai đã chuyển đổi vào bộ đệm
          }

          //Sau khi 4 kết quả đã nằm trong bộ đệm, nội dung được in ra terminal serial
          for (int i = 0; i < 4; i++) {
            Serial.print(channels[i], 8);  //in các kết quả vi sai đã chuyển đổi từ bộ đệm

            if (i < 3)  //Chỉ in tab giữa 3 kết quả đầu tiên
            {
              Serial.print("\t");  //ký tự tab phân cách 4 kết quả hiển thị trên cùng 1 dòng
            }
          }
          Serial.println();  //In xuống dòng - để 4 kết quả tiếp theo nằm ở dòng mới
        }
        A.stopConversion();
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'B':  //Kiểm tra tốc độ
        {
          //Các biến để lưu và đo thời gian trôi qua, và xác định số lượng phép chuyển đổi
          long numberOfSamples = 30000;  //Số lượng phép chuyển đổi
          long finishTime = 0;
          long startTime = micros();

          for (long i = 0; i < numberOfSamples; i++) {
            A.readSingleContinuous();
            //Lưu ý: ở đây ta chỉ thực hiện phép đọc, không in kết quả ra
          }

          finishTime = micros() - startTime;  //Tính thời gian đã trôi qua

          A.stopConversion();

          //In kết quả
          Serial.print("Total conversion time for 30k samples: ");
          Serial.print(finishTime);
          Serial.println(" us");

          Serial.print("Sampling rate: ");
          Serial.print(numberOfSamples * (1000000.0 / finishTime), 3);
          Serial.println(" SPS");
        }
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'T':  //Kiểm tra kết nối serial
        Serial.println("The serial connection is OK!");
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'I':  //Inspect - Kiem tra va in trang thai toan bo cac thanh ghi, kem giai thich y nghia
        printAllRegistersStatus();
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'a':  //Kiểm tra 1 phép chuyển đổi đơn lẻ - Chỉ trả về 1 kết quả duy nhất

        rawConversion = A.readSingle();                    //Đọc giá trị thô từ đầu vào đã chọn trước đó, gán vào 1 biến
        voltageValue = A.convertToVoltage(rawConversion);  //Chuyển đổi giá trị trên thành số thực (floating point)

        //In kết quả
        Serial.print("Single-ended conversion result: ");
        Serial.println(voltageValue, 8);  //In số thực với 8 chữ số.
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'M':  //set MUX - Chọn kênh đầu vào. Cú pháp: "Ms<số>" (đơn) hoặc "Md<số>" (vi sai)
        {
          //SỬA LỖI (#2): dùng waitForSerialInput() thay vì while(!Serial.available());
          //để tránh treo máy nếu không có dữ liệu tiếp theo được gửi đến.
          if (!waitForSerialInput()) break;
          inputMode = Serial.read();  //Đọc chế độ đầu vào

          if (inputMode == 's')  //single-ended
          {
            if (!waitForSerialInput()) break;
            inputChannel = Serial.parseInt();
            //SỬA LỖI (#1): kiểm tra inputChannel có nằm trong khoảng hợp lệ (0-7) hay không
            //trước khi dùng nó để truy cập mảng singleEndedChannels[]. Trước đây, một giá trị
            //ngoài khoảng (vd 99) sẽ đọc/ghi ngoài vùng nhớ của mảng, gây lỗi không lường trước.
            if (inputChannel < 0 || inputChannel > 7) {
              Serial.println("Lỗi: Số kênh (single-ended) không hợp lệ. Chỉ chấp nhận 0-7.");
              break;
            }
            A.setMUX(singleEndedChannels[inputChannel]);
            //Example: "Ms1" selects the SING_1 as input channel
          }

          if (inputMode == 'd')  //differential
          {
            if (!waitForSerialInput()) break;
            inputChannel = Serial.parseInt();
            //SỬA LỖI (#1): kiểm tra inputChannel có nằm trong khoảng hợp lệ (0-3) hay không
            if (inputChannel < 0 || inputChannel > 3) {
              Serial.println("Lỗi: Số kênh (differential) không hợp lệ. Chỉ chấp nhận 0-3.");
              break;
            }
            A.setMUX(differentialChannels[inputChannel]);
            //Example: "Md0" selects the DIFF_0_1 as input channel
          }
        }
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'P':  //Set PGA - Đặt hệ số khuếch đại. Cú pháp: "P<số>" (0-6)
        {
          if (!waitForSerialInput()) break;
          pgaSelection = Serial.parseInt();
          //SỬA LỖI (#1): kiểm tra pgaSelection có nằm trong khoảng hợp lệ (0-6) hay không
          //trước khi truy cập mảng pgaValues[] (mảng có 7 phần tử, chỉ số 0-6).
          if (pgaSelection < 0 || pgaSelection > 6) {
            Serial.println("Lỗi: Giá trị PGA không hợp lệ. Chỉ chấp nhận 0-6.");
            break;
          }
          A.setPGA(pgaValues[pgaSelection]);
          //Example: P4 will select the PGA = 16
          Serial.print("PGA value: ");
          Serial.println(A.getPGA());  //Đọc giá trị PGA từ thanh ghi rồi in ra
        }
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'F':  //Set sampling frequency - Đặt tốc độ lấy mẫu. Cú pháp: "F<số>" (0-15)
        {
          if (!waitForSerialInput()) break;
          drateSelection = Serial.parseInt();  //Phân tích số (số mục trong mảng)
          //SỬA LỖI (#1): kiểm tra drateSelection có nằm trong khoảng hợp lệ (0-15) hay không
          //trước khi truy cập mảng drateValues[] (mảng có 16 phần tử, chỉ số 0-15).
          if (drateSelection < 0 || drateSelection > 15) {
            Serial.println("Lỗi: Giá trị tốc độ lấy mẫu không hợp lệ. Chỉ chấp nhận 0-15.");
            break;
          }
          delay(100);
          Serial.print("DRATE is selected as: ");
          Serial.println(drateValues[drateSelection]);  //In giá trị lấy từ mảng
          delay(100);
          A.setDRATE(drateValues[drateSelection]);  //Truyền giá trị vào thanh ghi trên ADS1256
          delay(100);
          Serial.print("DRATE is set to ");
          Serial.println(A.readRegister(DRATE_REG));  //Đọc lại thanh ghi để kiểm tra giá trị đã được cập nhật đúng chưa
          //Example: F3 will make the DRATE = 3750 SPS
        }
        break;
      //--------------------------------------------------------------------------------------------------------
      case 'R':  //read register - Đọc 1 thanh ghi. Cú pháp: "R<số>" (0-10)
        {
          if (!waitForSerialInput()) break;
          registerToRead = Serial.parseInt();  //Phần này đọc số hiệu thanh ghi từ cổng serial
          //SỬA LỖI (#1): kiểm tra registerToRead có nằm trong khoảng hợp lệ (0-10) hay không
          //trước khi truy cập mảng registers[] (mảng có 11 phần tử kiểu String, chỉ số 0-10).
          //Nếu không kiểm tra, một giá trị ngoài khoảng (vd "R99") có thể đọc ngoài vùng nhớ.
          if (registerToRead < 0 || registerToRead > 10) {
            Serial.println("Lỗi: Số thanh ghi không hợp lệ. Chỉ chấp nhận 0-10.");
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
      case 'W':  //Write register - Ghi 1 giá trị vào 1 thanh ghi. Cú pháp: "W<số_thanh_ghi> <giá_trị>"
        {
          if (!waitForSerialInput()) break;
          registerToWrite = Serial.parseInt();  //Phần này đọc số hiệu thanh ghi từ cổng serial
          //SỬA LỖI (#1): kiểm tra registerToWrite có nằm trong khoảng hợp lệ (0-10) hay không
          if (registerToWrite < 0 || registerToWrite > 10) {
            Serial.println("Lỗi: Số thanh ghi không hợp lệ. Chỉ chấp nhận 0-10.");
            break;
          }
          if (!waitForSerialInput()) break;
          registerValueToWrite = Serial.parseInt();  //Phần này đọc giá trị cần ghi vào thanh ghi từ cổng serial
          //SỬA LỖI (#1): kiểm tra registerValueToWrite có nằm trong khoảng 1 byte (0-255) hay không,
          //vì thanh ghi của ADS1256 chỉ rộng 8-bit.
          if (registerValueToWrite < 0 || registerValueToWrite > 255) {
            Serial.println("Lỗi: Giá trị ghi không hợp lệ. Chỉ chấp nhận 0-255.");
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
