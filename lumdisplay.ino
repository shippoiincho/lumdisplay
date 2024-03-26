#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoOTA.h>
#include "k8x12lfont.h"
#include "jisconv.h"

// WiFi setting

const char* ssid       = "YourWiFiSSIDHere";
const char* password   = "YourWiFiPasswordHere";

// LUM Display pin assign

#define PIN_A0       26
#define PIN_A1       18
#define PIN_A2       25
#define PIN_A3       19

#define PIN_LATCH    33
#define PIN_CLOCK    32
#define PIN_ENABLE   21
#define PIN_LDATA    14
#define PIN_RDATA    22

#define PIN_LLED     23
#define PIN_RLED     27

//

#define YAHOO_INTERVAL 600*1000
#define YAHOO_MAXLEN 60
#define SCROLL_SPEED 80

//

uint8_t num = 0;

volatile uint8_t lum_buffer[77];  // 28 x 11 pixels
volatile uint8_t lum_address = 0;

char *yahoo_headlines[10];
int yahoo_last_millis = -(YAHOO_INTERVAL);

uint8_t bitmap_buffer[(YAHOO_MAXLEN + 16) * 11];
int failurecount;

hw_timer_t * timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR onTimer() {

  uint32_t ldata, rdata;

  portENTER_CRITICAL_ISR(&timerMux);

  digitalWrite(PIN_ENABLE, HIGH);

  // set address

  if ((lum_address & 1) == 0) {
    digitalWrite(PIN_A0, LOW);
  } else {
    digitalWrite(PIN_A0, HIGH);
  }
  if ((lum_address & 2) == 0) {
    digitalWrite(PIN_A1, LOW);
  } else {
    digitalWrite(PIN_A1, HIGH);
  }
  if ((lum_address & 4) == 0) {
    digitalWrite(PIN_A2, LOW);
  } else {
    digitalWrite(PIN_A2, HIGH);
  }
  if ((lum_address & 8) == 0) {
    digitalWrite(PIN_A3, LOW);
  } else {
    digitalWrite(PIN_A3, HIGH);
  }

  // send line data

  ldata = (lum_buffer[lum_address] << 20) + (lum_buffer[lum_address + 11] << 12) + (lum_buffer[lum_address + 22] << 4) + ((lum_buffer[lum_address + 33] & 0xf0) >> 4) ;
  rdata = ((lum_buffer[lum_address + 33] & 0xf) << 24) + (lum_buffer[lum_address + 44] << 16) + (lum_buffer[lum_address + 55] << 8) + lum_buffer[lum_address + 66] ;

  for (int i = 0; i < 28; i++) {

    if ((ldata & 0x8000000) == 0) {
      digitalWrite(PIN_LDATA, LOW);
    } else {
      digitalWrite(PIN_LDATA, HIGH);
    }

    ldata <<= 1;

    if ((rdata & 0x8000000) == 0) {
      digitalWrite(PIN_RDATA, LOW);
    } else {
      digitalWrite(PIN_RDATA, HIGH);
    }
    rdata <<= 1;

    digitalWrite(PIN_CLOCK, HIGH);
    digitalWrite(PIN_CLOCK, LOW);

  }

  digitalWrite(PIN_LATCH, HIGH);
  digitalWrite(PIN_LATCH, LOW);

  digitalWrite(PIN_ENABLE, LOW);



  lum_address++;
  if (lum_address > 10 ) lum_address = 0;


  portEXIT_CRITICAL_ISR(&timerMux);

}

String https_Web_Get(const char* host1, String target_page, char char_tag, String Final_tag, String Begin_tag, String End_tag, String Paragraph) {

  String ret_str;

  WiFiClientSecure https_client;

  https_client.setInsecure();

  if (https_client.connect(host1, 443)) {
    Serial.print(host1); Serial.print(F("-------------"));
    Serial.println(F("connected"));
    //    Serial.println(F("-------WEB HTTPS GET Request Send"));

    String str1 = String("GET https://") + String( host1 ) + target_page + " HTTP/1.1\r\n";
    str1 += "Host: " + String( host1 ) + "\r\n";
    str1 += "User-Agent: BuildFailureDetectorESP32\r\n";
    str1 += "Connection: close\r\n\r\n"; //closeを使うと、サーバーの応答後に切断される。最後に空行必要
    str1 += "\0";

    https_client.print(str1); //client.println にしないこと。最後に改行コードをプラスして送ってしまう為
    https_client.flush(); //client出力が終わるまで待つ
    //    Serial.print(str1);
    //    Serial.flush(); //シリアル出力が終わるまで待つ

  } else {
    Serial.println(F("------connection failed"));
  }

  delay(1);

  if (https_client) {
    String dummy_str;
    uint16_t from, to;
    //   Serial.println(F("-------WEB HTTPS Response Receive"));

    while (https_client.connected()) {
      while (https_client.available()) {
        if (dummy_str.indexOf(Final_tag) == -1) {
          dummy_str = https_client.readStringUntil(char_tag);

          if (dummy_str.indexOf(Begin_tag) >= 0) {
            from = dummy_str.indexOf(Begin_tag) + Begin_tag.length();
            to = dummy_str.indexOf(End_tag);
            ret_str += Paragraph;
            ret_str += dummy_str.substring(from, to);
            ret_str += "  ";
          }
        } else {
          while (https_client.available()) {
            https_client.read(); //サーバーから送られてきた文字を１文字も余さず受信し切ることが大事
            //delay(1);
          }
          delay(10);
          https_client.stop(); //特に重要。コネクションが終わったら必ず stop() しておかないとヒープメモリを食い尽くしてしまう。
          delay(10);
          Serial.println(F("-------Client Stop"));

          break;
        }
        delay(1);
      }
      delay(1);
    }
  }

  ret_str += "\0";
  ret_str.replace("&amp;", "&"); //XMLソースの場合、半角&が正しく表示されないので、全角に置き換える
  ret_str.replace("&#039;", "\'"); //XMLソースの場合、半角アポストロフィーが正しく表示されないので置き換える
  ret_str.replace("&#39;", "\'"); //XMLソースの場合、半角アポストロフィーが正しく表示されないので置き換える
  ret_str.replace("&apos;", "\'"); //XMLソースの場合、半角アポストロフィーが正しく表示されないので置き換える
  ret_str.replace("&quot;", "\""); //XMLソースの場合、ダブルクォーテーションが正しく表示されないので置き換える

  if (ret_str.length() < 20) ret_str = "※ニュース記事を取得できませんでした";

  if (https_client) {
    delay(10);
    https_client.stop(); //特に重要。コネクションが終わったら必ず stop() しておかないとヒープメモリを食い尽くしてしまう。
    delay(10);
    Serial.println(F("-------Client Stop"));
  }
  //  Serial.flush(); //シリアル出力が終わるまで待つ

  return ret_str;
}

void getYahooHeadlines() {

  String topnews;

  // Get Yahoo News

  if ((millis() - yahoo_last_millis) > YAHOO_INTERVAL) {
    topnews = https_Web_Get("news.yahoo.co.jp", "/rss/topics/top-picks.xml", '\n', "</rss>", "<title>", "</title>", "\n");
    topnews += '\n';
    if (topnews.length() > 100) { // Success
      int l = 0, pos = 0;
      for (int i = 0; i < topnews.length(); i++) {
        if (l < 10) {
          if (topnews.charAt(i) != '\n' ) {
            yahoo_headlines[l][pos] = topnews.charAt(i);
            pos++;
          } else {
            yahoo_headlines[l][pos] = '\0' ;
            pos = 0;
            l++;
          }
        }
      }
      failurecount = 0;
    } else {
      failurecount++;
      if (failurecount > 4) ESP.restart();
    }

    yahoo_last_millis = millis();

  }
  delay(100);

}

//

char* UFT8toUTF16( uint16_t *pUTF16, char *pUTF8 ) {

  // 1Byte Code
  if ( pUTF8[0] < 0x80 ) {
    // < 0x80 : ASCII Code
    //    *pUTF16 = pUTF8[0];
    // convert Hankaku ASCII to ZENKAKU
    if (pUTF8[0] > 0x20) {
      *pUTF16 = 0xff00 + (pUTF8[0] - 0x20);
    } else {
      *pUTF16 = 0x3000; // Blank
    }
    return pUTF8 + 1;
  }

  // 2Byte Code
  if ( pUTF8[0] < 0xE0 )  {
    *pUTF16 = ( ( pUTF8[0] & 0x1F ) << 6 ) + ( pUTF8[1] & 0x3F );
    return pUTF8 + 2;
  }

  // 3Byte Code
  if ( pUTF8[0] < 0xF0 ) {
    *pUTF16 = ( ( pUTF8[0] & 0x0F ) << 12 ) + ( ( pUTF8[1] & 0x3F ) << 6 ) + ( pUTF8[2] & 0x3F );
    return pUTF8 + 3;
  } else {
    // 4byte Code
    *pUTF16 = 0;
    return pUTF8 + 4;
  }
}

int UTF16toJIS(int utf16char) {

  uint16_t jiscode;

  //  if (utf16char > 0xff60) { //　Hankaku Kana
  //    jiscode = utf16kanajis[utf16char - 0xff60];
  //  }

  jiscode = utf16jis[utf16char];

  if (jiscode == 0) jiscode = 0x2121; // blank

  return jiscode;
}


int renderFonts(uint8_t *bitmap, char *render_str) {

  int text_length = 0;
  uint16_t utf16char, jischar;
  uint16_t jisku, jisten;
  char *str;
  //  uint8_t font_buf[MAX_FONT_NUM][16] = {0};
  //  uint8_t font_buf2[32] = {0};

  str = render_str;

  while ( *str != 0) {

    str = UFT8toUTF16(&utf16char, str);
    jischar = UTF16toJIS(utf16char);

    jisku = ((jischar & 0xff00) >> 8) - 0x21;
    jisten = (jischar & 0xff) - 0x21;

    for (int i = 0; i < 11; i++) {

      bitmap[text_length * 11 + i] = k8x12lfont[(jisku * 94 + jisten) * 11 + i];

    }

    text_length++;

  }

  return text_length;
}

void displayScrollBitmap(uint8_t *bitmap, int width) {

  uint16_t total_pixels = 0;
  uint8_t bitshift;

  total_pixels = (width + 7) * 8;

  for (int xx = 0; xx < total_pixels; xx++) {

    for (int x = 0; x < 7; x++) {
      for (int j = 0; j < 11; j++) {
        bitshift = xx % 8;
        if (xx % 8 == 0) {
          lum_buffer[x * 11 + j] = bitmap[(x + (xx / 8)) * 11 + j];
        } else {
          lum_buffer[x * 11 + j] = (bitmap[(x + (xx / 8)) * 11 + j] << bitshift) | (bitmap[(x + (xx / 8) + 1) * 11 + j] >> (8 - bitshift));
        }
      }
    }

    delay(SCROLL_SPEED);
    ArduinoOTA.handle();

  }
}



void setup() {


  pinMode(PIN_A0, OUTPUT);
  pinMode(PIN_A1, OUTPUT);
  pinMode(PIN_A2, OUTPUT);
  pinMode(PIN_A3, OUTPUT);
  pinMode(PIN_LATCH, OUTPUT);
  pinMode(PIN_CLOCK, OUTPUT);
  pinMode(PIN_ENABLE, OUTPUT);
  pinMode(PIN_LDATA, OUTPUT);
  pinMode(PIN_RDATA, OUTPUT);
//  pinMode(PIN_LLED, OUTPUT);
//  pinMode(PIN_RLED, OUTPUT);

  // connect wifi

  WiFi.begin(ssid, password);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA.begin();

  configTime(9 * 3600L, 0, "ntp.nict.jp", "time.google.com", "ntp.jst.mfeed.ad.jp");

//  digitalWrite(PIN_LLED, HIGH);
//  digitalWrite(PIN_RLED, HIGH);

  // start display

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000, true);
  timerAlarmEnable(timer);

  for (int i = 0; i < 10; i++) {
    yahoo_headlines[i] = (char*)malloc(YAHOO_MAXLEN);
    yahoo_headlines[i][0] = '\0';
  }

}

void loop() {

  int text_length;
  struct tm timeInfo;
  char timestr[64];

  ArduinoOTA.handle();

  getLocalTime(&timeInfo);
  sprintf(timestr, "%d月%d日%2d時%02d分",
          timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min);
  memset(bitmap_buffer, 0, (YAHOO_MAXLEN + 16) * 11);
  text_length = renderFonts(bitmap_buffer + 77, timestr);
  displayScrollBitmap(bitmap_buffer, text_length);

  getYahooHeadlines();
  for (int i = 2; i < 10 ; i++) {
    memset(bitmap_buffer, 0, (YAHOO_MAXLEN + 16) * 11);
    text_length = renderFonts(bitmap_buffer + 77, yahoo_headlines[i]);
    displayScrollBitmap(bitmap_buffer, text_length);
  }

}
