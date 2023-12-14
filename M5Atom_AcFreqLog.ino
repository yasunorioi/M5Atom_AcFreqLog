/*
 * ラジオペンチ　さんのAmbientに書き込むものを自分仕様に改造
 * ・AmbientからInfluxDBに変更
 * ・UDPで投げる
 * ・WiFiManagerを入れる
*/

// ESP32で商用電源周波数をAmbientに書き込み File:20210303AcFreqLogToAmbient.ino
// 2021/03/03 ラジオペンチ http://radiopench.blog96.fc2.com/
// （セマフォやミューテックスを使った排他制御を行っていないので問題が出るかも？）

#include "M5Atom.h"
#include <WiFi.h>
#include <WiFiUdp.h>

#define AC_pin       32          // AC信号入力ピン
#define AC_FREQ      50          // 電源公称周波数（50 or 60)
#define LOG_INTERVAL 30          // ログ周期（秒）
#define vAnalogPin   34          // アナログ入力ピン
#define K_Freq       1.000035    // 周波数補正係数 (デフォルトは1.0)
#define K_Volt       0.03333     // 電圧換算係数(V/LSB) (100V/3000LSB)
//#define LED_PIN      2           // ESP32 DEVKIT V1 内蔵LEDのピン番号


uint32_t chipId = 0;

WiFiClient client;

/*const char* ssid = "Toiso";         // WiFi SSID
const char* password = "chaylan22";     // WiFi パスワード
*/

volatile uint32_t t1;      // 50(60)サイクル分の周期（us単位、標準1000000us）
volatile boolean flag;

float f1;                  // 周波数の1秒平均値
float fMax = 0.0;          // 最大周波数
float fMin = 1000.0;       // 最小周波数
float fAve;                // 平均周波数
float fSum = 0.0;
float v1;                  // 電圧の1秒平均値
float vAve;
float vSum = 0.0;
boolean ledFlag = true;    // 動作表示LED点滅フラグ
byte host[] = {192, 168, 1, 23};
int port = 8089;

WiFiUDP udp;

void IRAM_ATTR acIrq() {   // ピン割込み(電源1サイクル毎に割込み）
  static uint16_t n;
  static uint32_t lastMicros = 0;
  uint32_t x;
  n++;                     // サイクルカウンタをインクリメント
  if (n >= AC_FREQ) {      // 指定回数(50 or 60)なら
    x = micros();          // 現在の時刻を記録
    t1 = x - lastMicros;   // 経過時間を計算(この値はほぼ100000になる）
    lastMicros = x;
    n = 0;
    flag = true;           // loopに通知するためにフラグをセット
  }
}

void setup() {
  M5.begin(true, false, true); 
  pinMode(0,OUTPUT);
  digitalWrite(0,LOW);
  pinMode(AC_pin, INPUT);                  // 電源交流信号入力ピン
  analogSetAttenuation(ADC_2_5db);         // フルスケール1.4V = 4096
// analogSetAttenuation(ADC_0db);         // 1.04v
pinMode(vAnalogPin, ANALOG);

  WiFi.begin(ssid, password);              //  Wi-Fi APに接続
  while (WiFi.status() != WL_CONNECTED) {  //  Wi-Fi AP接続待ち
    delay(100);
  }

  Serial.print("WiFi connected\r\nIP address: ");
  Serial.println(WiFi.localIP());              // IPアドレス表示
  attachInterrupt(AC_pin, acIrq, FALLING);
    for(int i=0; i<17; i=i+8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  waitData();                                  // 異常値排除のため最初の1回(1秒)読み飛ばす
}

void loop() {
  static uint16_t logCount = 0;
  waitData();                              // データーが準備できるまで待つ
  f1 = K_Freq * 1.0E6 * AC_FREQ / t1;      // 1秒間の平均周波数を計算
  logCount++;
  if (f1 < fMin) fMin = f1;
  if (f1 > fMax) fMax = f1;
  fSum = fSum + f1;
  int voltage = analogRead(vAnalogPin);
//  v1 = K_Volt * analogRead(vAnalogPin);
  v1 = K_Volt * voltage;

  vSum += v1;
  Serial.print(f1, 3); Serial.print(", "); Serial.println(v1); // 1秒毎の周波数、電圧を出力

  if (logCount >= LOG_INTERVAL) {           // 指定サイクル数割込みが入ったら、
    fAve = fSum / LOG_INTERVAL;             // 平均周波数計算
    vAve = vSum / LOG_INTERVAL;             // 平均電圧計算
    String line;
    line="freq device="+String(chipId)+",Freq="+fAve+",V="+vAve;
    Serial.println(line);
    udp.beginPacket(host, port);
    udp.print(line);
    udp.endPacket();
    delay(100);
    fMin = 1000.0;                          // 次回の測定用に変数を初期化
    fMax = 0.0;
    fSum = 0.0;
    logCount = 0;
    vSum = 0;
  }
}

void waitData() {          // データーが揃うまで待つ
  while (flag == false) {  // フラグをポーリング
  }
  flag = false;
}
