#ifndef _AUDIO_H
#define _AUDIO_H

#include <Arduino.h>
#include "I2S.h"

// 16bit, monoral, 16000Hz,  linear PCM
class Audio {
  I2S* i2s;
  static const int headerSize = 44;
  static const int i2sBufferSize = 6000;
  char i2sBuffer[i2sBufferSize];
  void CreateWavHeader(byte* header, int waveDataSize);
  void AGCTask();
  
public:
  //static const int wavDataSize = 60000;                   // It must be multiple of dividedWavDataSize. Recording time is about 1.9 second.
  //static const int wavDataSize = 120000;
  static const int wavDataSize = 90000;
  static const int dividedWavDataSize = i2sBufferSize/4;
  char** wavData;                                         // It's divided. Because large continuous memory area can't be allocated in esp32.
  byte paddedHeader[headerSize + 4] = {0};                // The size must be multiple of 3 for Base64 encoding. Additional byte size must be even because wave data is 16bit.

  Audio(MicType micType);
  ~Audio();
  void InitMic();
  void Record();
  void InitSpeaker();
  void Play(unsigned char audio_data[], int numData);
};

#endif // _AUDIO_H
