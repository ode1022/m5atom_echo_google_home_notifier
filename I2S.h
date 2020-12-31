#ifndef _I2S_H
#define _I2S_H
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "esp_system.h"

enum MicType {
  NO_USE,
  ADMP441,
  ICS43434,
  SPH0645LM4H,
  M5GO,
  M5STACKFIRE,
  M5ATOM_ECHO
};

class I2S {
  MicType micType;
  i2s_bits_per_sample_t BITS_PER_SAMPLE;
public:
  I2S(MicType micType);
  void initMic();
  int read(char* data, int numData);
  int getBitPerSample();
  size_t write(unsigned char audio_data[], int numData);
};

#endif // _I2S_H
