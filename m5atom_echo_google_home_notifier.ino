// conifg.h.exampleをconfig.hにリネームし、各種設定をしてください
#include "config.h"

#include <WiFi.h>
#include <WebServer.h>
#include <esp8266-google-home-notifier.h> // https://github.com/horihiro/esp8266-google-home-notifier
#include <M5Atom.h> // https://github.com/m5stack/M5Atom https://github.com/FastLED/FastLED

#include "SPIFFS.h"
#include "Audio.h" // https://github.com/earlephilhower/ESP8266Audio

Audio* audio;

WebServer server(80);
GoogleHomeNotifier ghn;

void setup() {
  M5.begin(true, false, true);

  printf("esp_get_free_heap_size: %d\n", esp_get_free_heap_size());

  SPIFFS.begin(true);

  Serial.print("connecting to Wi-Fi");

  //WiFi.disconnect(true, true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  //Print the local IP

  if (ip != NULL) {
    ghn.ip(*ip, googleHomeLang, 8009);  
  } else {
    Serial.println("find to Google Home...");
    if (ghn.device(googleHomeDisplayName , googleHomeLang) != true) {
      Serial.println(ghn.getLastError());
      return;
    }
  }
  Serial.print("found Google Home(");
  Serial.print(ghn.getIPAddress());
  Serial.print(":");
  Serial.print(ghn.getPort());
  Serial.println(")");

  server.on("/speech", handleSpeechPath);
  server.on("/", handleRootPath);
  server.on("/record_notify", recordNotify);
  server.on("/notify_sound", notifySound);
  server.on("/sound_data", soundData);
  server.begin();

  audio = new Audio(M5ATOM_ECHO);
  
  printf("esp_get_free_heap_size: %d\n", esp_get_free_heap_size());
  Serial.println("setup end");

  if (recordingWhenBoote) {
    delay(200); // I2S Micの初期化が間に合っていないためなのかノイズが多くはいるのでdelayする
    recordNotify();
  }
}

// 録音 & Google Homeへ再生依頼のリクエストをする
void recordNotify() {
  Serial.println("\r\nRecord start!\r\n");
  audio->record();
  Serial.println("\r\nRecord end!\r\n");

  audio->playMP3("/gra_cre.mp3");
  // mp3再生は非同期のため再生が終了するのを待機する。(ATOM Echoのマイクとスピーカーは排他のため)
  audio->waitMP3(); 
  // マイクが使えるように初期化して戻す（ATOM Echoのスピーカーとマイクは排他のため）
  audio->initMic();

  notifySound();
}

// HTTPリクエストに対して録音済みデータを出力
void soundData()
{
  Serial.println("soundData start.");

  int httpBody2Length = (audio->wavDataSize + sizeof(audio->paddedHeader));
  server.setContentLength(httpBody2Length);
  server.send(200, "audio/wav", "");
  server.client().write(audio->paddedHeader, sizeof(audio->paddedHeader));

  char** wavData = audio->wavData;
  for (int j = 0; j < audio->wavDataSize / audio->dividedWavDataSize; ++j) {
    server.client().write((byte*)wavData[j], audio->dividedWavDataSize);
  }

  Serial.println("soundData stop.");
}

// Google Homeへ再生依頼のリクエストをする
void notifySound()
{
  Serial.println("notifySound start.");
  String mp3url = "http://" + WiFi.localIP().toString() + "/sound_data";
  Serial.println("GoogleHomeNotifier.play() start");
  if (ghn.play(mp3url.c_str()) != true)
  {
    Serial.print("GoogleHomeNotifier.play() error:");
    Serial.println(ghn.getLastError());
  }

  Serial.println("notifySound done.");
}

// Text To Speech用のテキストpost先URL
void handleSpeechPath() {
  String phrase = server.arg("phrase");
  Serial.println("phrase: " + phrase);
  if (phrase == "") {
    server.send(401, "text / plain", "query 'phrase' is not found");
    return;
  }

  Serial.println("send before");

  if (ghn.notify(phrase.c_str()) != true) {
    Serial.println(ghn.getLastError());
    server.send(500, "text / plain", ghn.getLastError());
    return;
  }

  Serial.println("send after");
  server.send(200, "text / plain", "OK");
}

// Text To Speech用のテキスト入力画面URL
void handleRootPath() {
  server.send(200, "text/html", "<html><head></head><body><input type=\"text\"><button>speech</button><script>var d = document;d.querySelector('button').addEventListener('click',function(){xhr = new XMLHttpRequest();xhr.open('GET','/speech?phrase='+encodeURIComponent(d.querySelector('input').value));xhr.send();});</script></body></html>");
}

int appMode = 1;
int wasButtonLongPressed = false;
void loop() {
  server.handleClient();

  // ボタン長押しでモード切り替え
  if (M5.Btn.pressedFor(1000) && !wasButtonLongPressed) {
    Serial.println("button long pressed");
    if (appMode == 1) {
      appMode = 2;
      audio->playMP3("/gra_cre.mp3");
    } else {
      appMode = 1;
      audio->initMic();
    }
    wasButtonLongPressed = true;
  }

  // ボタン押してから離した（長押しと区別するため離した場合で検知）
  if (M5.Btn.wasReleased())
  {
    if (wasButtonLongPressed) {
      // 長押ししてから離した場合は処理しない
      wasButtonLongPressed = false;
    } else {
      // ボタン押した際の処理
      Serial.println("button released");
      switch (appMode) {
        case 1:
          // ブロードキャストモード
          recordNotify();
          break;
        case 2:
          // mp3再生モード
          audio->playMP3("/gra_cre.mp3");
          break;
      }
    }
  }
  M5.update();
}
