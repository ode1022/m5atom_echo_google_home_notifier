#include "Audio.h"

Audio::Audio(MicType micType) {
  wavData = new char*[wavDataSize / dividedWavDataSize];
  for (int i = 0; i < wavDataSize / dividedWavDataSize; ++i) wavData[i] = new char[dividedWavDataSize];
  i2s = new I2S(micType);
  // AudioOutputI2SクラスをデフォルトのEXTERNAL_I2Sで初期化すると現状の仕様上はGPIO25でI2Sが初期化されしまい後からEcho用のピンでi2s_set_pinしてもGPIO25は使用できなかった。
  // 一度i2s_set_pinするとgpioとして使えなさそうに見えたのでINTERNAL_PDMで初期化することGPIO25がi2s_set_pinされないように回避する(実際に利用時のI2Sの初期化はinitSpeaker内でおこなうためここは適当でよいため）
  out = new AudioOutputI2S(I2S_NUM_0, AudioOutputI2S::INTERNAL_PDM);
  mp3 = new AudioGeneratorMP3();
  initMic(); // AudioOutputI2Sをnew時にスピーカーのI2Sが初期化されるため、それ以降にマイクを初期化する必要がある
}

Audio::~Audio() {
  for (int i = 0; i < wavDataSize / dividedWavDataSize; ++i) delete[] wavData[i];
  delete[] wavData;
  delete i2s;
  delete mp3;
  delete out;
}

void Audio::createWavHeader(byte* header, int waveDataSize) {
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

void Audio::agcTask()
{
  // http://shokai.org/blog/archives/5053

  int bitBitPerSample = i2s->getBitPerSample();
  
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


void Audio::initMic() {
    i2s->initMic();
}

void Audio::record() {
  createWavHeader(paddedHeader, wavDataSize);
  int bitBitPerSample = i2s->getBitPerSample();
  if (bitBitPerSample == 16) {
    for (int j = 0; j < wavDataSize / dividedWavDataSize; ++j) {
      i2s->read(i2sBuffer, i2sBufferSize / 2);
      for (int i = 0; i < i2sBufferSize / 8; ++i) {
        wavData[j][2 * i] = i2sBuffer[4 * i + 2];
        wavData[j][2 * i + 1] = i2sBuffer[4 * i + 3];
      }
    }
  }
  else if (bitBitPerSample == 32) {
    for (int j = 0; j < wavDataSize / dividedWavDataSize; ++j) {
      i2s->read(i2sBuffer, i2sBufferSize);
      for (int i = 0; i < i2sBufferSize / 8; ++i) {
        wavData[j][2 * i] = i2sBuffer[8 * i + 2];
        wavData[j][2 * i + 1] = i2sBuffer[8 * i + 3];
      }
    }

    //example_disp_buf((uint8_t*) i2sBuffer, i2sBufferSize);
  }
  agcTask();
}


void Audio::initSpeaker() {
  i2s->initSpeaker();
}

void Audio::playWaveBuf(unsigned char audio_data[], int numData) {
  i2s->write(audio_data, numData);
}

// MP3を非同期再生
void Audio::playMP3(char *filename) {
  // 再生中の場合は停止させる
  if (thMP3 != NULL) {
    Serial.printf("MP3 doPlayStop\n");
    doPlayStop = true;
  }
  // 停止するまで待機
  waitMP3();
  
  // https://github.com/earlephilhower/ESP8266Audio/blob/master/src/AudioFileSourceFS.cpp
  // を見る限りAudioFileSourceSPIFFSを一度closeしたら再オープンするようなメソッドはないためnewしなおす。
  file = new AudioFileSourceSPIFFS(filename);
  initSpeaker();
  mp3->begin(file, out);
  Serial.printf("MP3 start\n");

  muxMp3 = xSemaphoreCreateBinary();
  // クラスインスタンスをTask実行
  // https://forum.arduino.cc/index.php?topic=658230.0
  xTaskCreatePinnedToCore(this->loopPlayMP3, "loopPlayMP3", 8192, this, 5, &thMP3, 0); //マルチタスク core 0 実行
}

// MP3の非同期再生が終了するのを待機
void Audio::waitMP3() {
//  while (true) {
//    if (thMP3 == NULL) {
//      break;
//    }
//    delay(5); 
//  }
  if (muxMp3 != NULL) {
    xSemaphoreTake(muxMp3, portMAX_DELAY);
  }
}

void Audio::loopPlayMP3(void *pvParameters) {
  Audio *pThis = (Audio *) pvParameters; 
  
  while (true) {
    if (pThis->mp3->isRunning()) {
      if (!pThis->mp3->loop() || pThis->doPlayStop) {
        pThis->mp3->stop();
        delete pThis->file;
      }
    } else {
      Serial.printf("MP3 done\n");
      break;
    }
    delay(5);    
  }

  xSemaphoreGive(pThis->muxMp3);
  pThis->muxMp3 = NULL;
  pThis->thMP3 = NULL;
  pThis->doPlayStop = false;
  vTaskDelete(NULL);
}
