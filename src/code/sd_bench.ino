#include <SPI.h>
#include <SD.h>

// set up variables using the SD utility library functions:
Sd2Card card;
SdVolume volume;
SdFile root;
File myFile;

const int chipSelect = 4;

// Size of read/write.
const size_t BUF_SIZE = 64;

// File size in MB where MB = 1,000,000 bytes.
const uint32_t FILE_SIZE_MB = 5;

// Write pass count.
const uint8_t WRITE_COUNT = 2;

// Read pass count.
const uint8_t READ_COUNT = 2;
//==============================================================================
// End of configuration constants.
//------------------------------------------------------------------------------
// File size in bytes.
const uint32_t FILE_SIZE = 1000000UL * FILE_SIZE_MB;

uint8_t buf[BUF_SIZE];

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("\nInitializing SD card...");
}

void loop() {
  // put your main code here, to run repeatedly:
  float s;
  uint32_t t;
  uint32_t maxLatency;
  uint32_t minLatency;
  uint32_t totalLatency;

  // Discard any input.
  do {
    delay(10);
  } while (Serial.available() && Serial.read() >= 0);

  // F( stores strings in flash to save RAM
  Serial.print("Type any character to start\n");
  while (!Serial.available()) {
    //    SysCall::yield();
  }
  if (!card.init(SPI_FULL_SPEED, chipSelect)) {
    return;
  }
  Serial.print("\nCard type: ");
  switch (card.type()) {
    case SD_CARD_TYPE_SD1:
      Serial.println("SD1");
      break;
    case SD_CARD_TYPE_SD2:
      Serial.println("SD2");
      break;
    case SD_CARD_TYPE_SDHC:
      Serial.println("SDHC");
      break;
    default:
      Serial.println("Unknown");
  }

  if (!volume.init(card)) {
    Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
    return;
  }


  // print the type and size of the first FAT-type volume
  uint32_t volumesize;
  Serial.print("\nVolume type is FAT");
  Serial.println(volume.fatType(), DEC);
  Serial.println();

  volumesize = volume.blocksPerCluster();    // clusters are collections of blocks
  volumesize *= volume.clusterCount();       // we'll have a lot of clusters
  volumesize *= 512;                            // SD card blocks are always 512 bytes
  Serial.print("Volume size (bytes): ");
  Serial.println(volumesize);
  Serial.print("Volume size (Kbytes): ");
  volumesize /= 1024;
  Serial.println(volumesize);
  Serial.print("Volume size (Mbytes): ");
  volumesize /= 1024;
  Serial.println(volumesize);

  myFile = SD.open("bench.txt", FILE_WRITE);

  // fill buf with known data
  for (uint16_t i = 0; i < (BUF_SIZE - 2); i++) {
    buf[i] = 'A' + (i % 26);
  }
  buf[BUF_SIZE - 2] = '\r';
  buf[BUF_SIZE - 1] = '\n';

  Serial.print("File size "); Serial.print(FILE_SIZE_MB); Serial.println(" MB");
  Serial.print("Buffer size "); Serial.print(BUF_SIZE); Serial.println(" bytes");
  Serial.println("Starting writ test, please wait");

  uint32_t n = FILE_SIZE / sizeof(buf);
  Serial.println("write speed and latency") ;
  Serial.println("speed,max,min,avg");
  Serial.println("KB/Sec,usec,usec,usec");

  for (uint8_t nTest = 0; nTest < WRITE_COUNT; nTest++) {
    //    file.truncate(0);
    maxLatency = 0;
    minLatency = 9999999;
    totalLatency = 0;
    t = millis();
    for (uint32_t i = 0; i < n; i++) {
      uint32_t m = micros();
     myFile.write(buf, sizeof(buf));

      m = micros() - m;
      if (maxLatency < m) {
        maxLatency = m;
      }
      if (minLatency > m) {
        minLatency = m;
      }
      totalLatency += m;
    }
    t = millis() - t;
    s = myFile.size();
    Serial.print(s / t); Serial.print(','); Serial.print(maxLatency); Serial.print(',');
    Serial.print(minLatency); Serial.print(','); Serial.println(totalLatency / n);
  }
  myFile.close();
  myFile = SD.open("bench.dat");
  Serial.println("Starting read test, please wait");
  Serial.println("read speed and latency") ;
  Serial.println("speed,max,min,avg");
  Serial.println("KB/Sec,usec,usec,usec");
  for (uint8_t nTest = 0; nTest < WRITE_COUNT; nTest++) {
    //    file.truncate(0);
    maxLatency = 0;
    minLatency = 9999999;
    totalLatency = 0;
    t = millis();
    for (uint32_t i = 0; i < n; i++) {
      buf[BUF_SIZE-1] = 0;
      uint32_t m = micros();
      int32_t nr = myFile.read(buf, sizeof(buf));
      if (nr != sizeof(buf)) {
        Serial.println("read failed");
        myFile.close();
        return;
      }
      m = micros() - m;
      if (maxLatency < m) {
        maxLatency = m;
      }
      if (minLatency > m) {
        minLatency = m;
      }
      totalLatency += m;
//      if (buf[BUF_SIZE-1] != '\n') {
////        error("data check");
//      }
    }
    t = millis() - t;
    s = myFile.size();
    Serial.print(s / t); Serial.print(','); Serial.print(maxLatency); Serial.print(',');
    Serial.print(minLatency); Serial.print(','); Serial.println(totalLatency / n);
  }
  Serial.println("done");
  myFile.close();
}
