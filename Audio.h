#ifndef _AUDIO_H
#define _AUDIO_H

#include <Arduino.h>
#include "I2S.h"
#include "SPIFFS.h"
#include "AudioFileSourceSPIFFS.h" // https://github.com/earlephilhower/ESP8266Audio
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

// 16bit, monoral, 16000Hz,  linear PCM
class Audio {
  I2S* i2s;
  AudioGeneratorMP3 *mp3;
  AudioFileSourceSPIFFS *file;
  AudioOutputI2S *out;
  TaskHandle_t thMP3 = NULL;
  SemaphoreHandle_t muxMp3 = NULL;
  bool doPlayStop = false;
  static const int headerSize = 44;
  static const int i2sBufferSize = 6000;
  char i2sBuffer[i2sBufferSize];
  void createWavHeader(byte* header, int waveDataSize);
  void agcTask();
  static void loopPlayMP3(void *pvParameters);
  
public:
  //static const int wavDataSize = 60000;                   // It must be multiple of dividedWavDataSize. Recording time is about 1.9 second.
  //static const int wavDataSize = 120000;
  static const int wavDataSize = 90000;
  static const int dividedWavDataSize = i2sBufferSize/4;
  char** wavData;                                         // It's divided. Because large continuous memory area can't be allocated in esp32.
  byte paddedHeader[headerSize + 4] = {0};                // The size must be multiple of 3 for Base64 encoding. Additional byte size must be even because wave data is 16bit.

  Audio(MicType micType = NO_USE);
  ~Audio();
  void initMic();
  void record();
  void initSpeaker();
  void playWaveBuf(unsigned char audio_data[], int numData);
  void playMP3(char *filename);
  void waitMP3();
};

#endif // _AUDIO_H
