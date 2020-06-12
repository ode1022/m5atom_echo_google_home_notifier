#include "Audio.h"

Audio::Audio(MicType micType) {
  wavData = new char*[wavDataSize / dividedWavDataSize];
  for (int i = 0; i < wavDataSize / dividedWavDataSize; ++i) wavData[i] = new char[dividedWavDataSize];
  i2s = new I2S(micType);
}

Audio::~Audio() {
  for (int i = 0; i < wavDataSize / dividedWavDataSize; ++i) delete[] wavData[i];
  delete[] wavData;
  delete i2s;
}

void Audio::CreateWavHeader(byte* header, int waveDataSize) {
  header[0] = 'R';
  header[1] = 'I';
  header[2] = 'F';
  header[3] = 'F';
  unsigned int fileSizeMinus8 = waveDataSize + 44 - 8;
  header[4] = (byte)(fileSizeMinus8 & 0xFF);
  header[5] = (byte)((fileSizeMinus8 >> 8) & 0xFF);
  header[6] = (byte)((fileSizeMinus8 >> 16) & 0xFF);
  header[7] = (byte)((fileSizeMinus8 >> 24) & 0xFF);
  header[8] = 'W';
  header[9] = 'A';
  header[10] = 'V';
  header[11] = 'E';
  header[12] = 'f';
  header[13] = 'm';
  header[14] = 't';
  header[15] = ' ';
  header[16] = 0x10;  // linear PCM
  header[17] = 0x00;
  header[18] = 0x00;
  header[19] = 0x00;
  header[20] = 0x01;  // linear PCM
  header[21] = 0x00;
  header[22] = 0x01;  // monoral
  header[23] = 0x00;
  header[24] = 0x80;  // sampling rate 16000
  header[25] = 0x3E;
  header[26] = 0x00;
  header[27] = 0x00;
  header[28] = 0x00;  // Byte/sec = 16000x2x1 = 32000
  header[29] = 0x7D;
  header[30] = 0x00;
  header[31] = 0x00;
  header[32] = 0x02;  // 16bit monoral
  header[33] = 0x00;
  header[34] = 0x10;  // 16bit
  header[35] = 0x00;
  header[36] = 'd';
  header[37] = 'a';
  header[38] = 't';
  header[39] = 'a';
  header[40] = (byte)(waveDataSize & 0xFF);
  header[41] = (byte)((waveDataSize >> 8) & 0xFF);
  header[42] = (byte)((waveDataSize >> 16) & 0xFF);
  header[43] = (byte)((waveDataSize >> 24) & 0xFF);
}

uint8_t gainF2P6;
bool SetGain(float f) {
  if (f > 8.0) f = 8.0;
  if (f < 0.0) f = 0.0;
  gainF2P6 = (uint8_t)(f * (1 << 6));
  return true;
}
inline int16_t Amplify(int16_t s) {
  int32_t v = (s * gainF2P6) >> 6;
  if (v < -65536) return -65536;
  else if (v > 65536) return 65536;
  else return (int16_t)(v & 0xffff);
}


void example_disp_buf(uint8_t* buf, int length)
{
  printf("======\n");
  for (int i = 0; i < length; i++) {
    printf("%02x ", buf[i]);
    if ((i + 1) % 8 == 0) {
      printf("\n");
    }
  }
  printf("======\n");
}

#define AGC_FRAME_BYTES     320
void Audio::AGCTask()
{
  // http://shokai.org/blog/archives/5053

  int bitBitPerSample = i2s->GetBitPerSample();
  
  int16_t max = 0;
  int dataSize = 8;
  if (bitBitPerSample == 16) {
    dataSize = 4;
  }
  for (int j = 0; j < wavDataSize / dividedWavDataSize; ++j) {
    for (int i = 0; i < i2sBufferSize / 8; ++i) {
      int16_t s = wavData[j][2 * i + 1] << dataSize | wavData[j][2 * i];
      if (max < s) {
        max = s;
      }
    }
  }
  Serial.printf("max data=%d\n", max);
  double volume_ratio = 32768l / (double) max;
  Serial.printf("volume_ratio=%lf\n", volume_ratio);
  
  for (int j = 0; j < wavDataSize / dividedWavDataSize; ++j) {
    for (int i = 0; i < i2sBufferSize / 8; ++i) {
      int16_t s = wavData[j][2 * i + 1] << 8 | wavData[j][2 * i];
      s = s * volume_ratio;
      //s = s * 128;
      wavData[j][2 * i] = s & 0xff;
      wavData[j][2 * i + 1] = s >> 8;
    }
  }

}


void Audio::InitMic() {
    i2s->InitMic();
}

void Audio::Record() {
  CreateWavHeader(paddedHeader, wavDataSize);
  int bitBitPerSample = i2s->GetBitPerSample();
  SetGain(4);
  if (bitBitPerSample == 16) {
    for (int j = 0; j < wavDataSize / dividedWavDataSize; ++j) {
      i2s->Read(i2sBuffer, i2sBufferSize / 2);
      for (int i = 0; i < i2sBufferSize / 8; ++i) {
        wavData[j][2 * i] = i2sBuffer[4 * i + 2];
        wavData[j][2 * i + 1] = i2sBuffer[4 * i + 3];
      }
    }
  }
  else if (bitBitPerSample == 32) {
    for (int j = 0; j < wavDataSize / dividedWavDataSize; ++j) {
      i2s->Read(i2sBuffer, i2sBufferSize);
      for (int i = 0; i < i2sBufferSize / 8; ++i) {
        wavData[j][2 * i] = i2sBuffer[8 * i + 2];
        wavData[j][2 * i + 1] = i2sBuffer[8 * i + 3];
      }
    }

    //example_disp_buf((uint8_t*) i2sBuffer, i2sBufferSize);
  }
  AGCTask();
}


void Audio::InitSpeaker() {
  i2s->InitSpeaker();
}

void Audio::Play(unsigned char audio_data[], int numData) {
  i2s->Write(audio_data, numData);
}
