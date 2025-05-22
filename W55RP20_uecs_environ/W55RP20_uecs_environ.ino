/////////////////////////////////////////
// UARDECS Sample Program "TempHumidSensor_SHT3x"
// I2C connection test with SHT3x Ver1.1
// Original by H. Kurosaki (2018/05/30)
//
// Ported and Modified for W55RP20 by Shin Nakamura
// - Adapted to W55RP20-EVB-PICO using lwIP stack
// - Removed PROGMEM and adjusted for Arduino RP2040 core
// - Ethernet communication enabled via built-in MAC (EthernetCompat)
// - Safe Mode and web-based configuration implemented
// - MAC address auto-generated from board unique ID
// - Compatible with I2C sensor SHT3x via GPIO4 (SDA), GPIO5 (SCL)
/////////////////////////////////////////


//Sensirion SHT3xシリーズから温度と湿度を読み出します。
// W55RP20 EVB Picoでのピン設定
// I2C
//[5]-[SCL]
//[4]-[SDA]
//[5V3]-[VCC]
//[GND]-[GND]

// センサのアドレスはsht3x.hで0x45に設定しています(必要があれば確認してください)

//注意！
//湿度センサは有機溶剤や酸・アルカリで損傷することがあります。
//アルコールなどの溶剤を近くで使わないこと。
//梱包時に使用するビニール袋やケースのプラスチック素材から揮発する有機ガスで
//損傷することがあるので注意すること。

#include <EthernetCompat.h>
#include <W55RP20lwIP.h>
#include "Uardecs_RP20.h"

#include <Wire.h>
#include "SHT3x.h"


// W55RP20をEthernetクラスとして扱えるようにする（型エイリアス） commented by S.Nakamura
using ArduinoWiznet55rp20lwIP = ArduinoEthernet<Wiznet55rp20>;  // added by K.Masaki@B&B Lab.　HW 依存の内容を Arduino のライブラリ形式に移植
ArduinoWiznet55rp20lwIP Ethernet(1);                            // added out by K.Masaki@B&B Lab.
                                                                // コンストラクタに１の引数必要（コンパイルを通すため）      
                                                                
// W55RP20開発ボードのユーザーLED commented by S.Nakamura 2025-05-20
// #define LED_PIN 19

// デフォルトネットワーク設定 added by S.Nakamura 2025-05-20
// 書き込み時に自分のネットワークに合わせて設定してください
byte defaultip[]      = {192,168,11,7};     // IPアドレスのデフォルト設定
byte defaultgateway[] = {192, 168, 11, 1};  // ゲートウェイのデフォルト設定
byte defaultdns[]     = {0,0,0,0};          // dnsのデフォルト設定
byte defaultsubnet[]  = {255,255,255,0};    // サブネットマスクのデフォルト設定

// I2Cのピン設定（必要に応じて変更可能）
#define I2C_SDA_PIN  4   // 例: GPIO4
#define I2C_SCL_PIN  5   // 例: GPIO5

int i2c_sda_pin = I2C_SDA_PIN;
int i2c_scl_pin = I2C_SCL_PIN;

#define OPRMODE_ERR_SHT3xSENSERR       0x20000000 //0 01000 00000 0 0000 0000000000000000 //センサ異常

SHT3x sht3x = SHT3x();

/////////////////////////////////////
//IP reset jupmer pin setting
//IPアドレスリセット用ジャンパーピン設定
/////////////////////////////////////
const byte U_InitPin = 14;      // 端のピンでGNDが隣にあり使用が容易なため commented by S.Nakamura
// const byte U_InitPin_Sense=HIGH;
const byte U_InitPin_Sense=LOW; // アクティブロー設定 commented by S.Nakamura


////////////////////////////////////
//Node basic infomation
//ノードの基本情報
///////////////////////////////////
const char U_name[] = "TempHumid Node";//MAX 20 chars
const char U_vender[] = "B&B Lab.";//MAX 20 chars
const char U_uecsid[] = "000000000000";//12 chars fixed
const char U_footnote[] = "SHT3x Sensor Node";
char U_nodename[20] = "node";//MAX 19chars (This value enabled in safemode)
UECSOriginalAttribute U_orgAttribute;//この定義は弄らないで下さい
//////////////////////////////////
// html page1 setting
//Web上の設定画面に関する宣言
//////////////////////////////////

//Total number of HTML table rows.
//web設定画面で表示すべき項目の総数
const int U_HtmlLine = 0;

//表示素材の登録
struct UECSUserHtml U_html[U_HtmlLine]={
};

//////////////////////////////////
// UserCCM setting
// CCM用の素材
//////////////////////////////////

enum {
  CCMID_InAirTemp,
  CCMID_InAirHumid,
  CCMID_InAirHD,
  CCMID_cnd,
  CCMID_dummy, //CCMID_dummyは必ず最後に置くこと
};


//CCM格納変数の宣言
//ここはこのままにして下さい
const int U_MAX_CCM = CCMID_dummy;
UECSCCM U_ccmList[U_MAX_CCM];

//CCM定義用の素材
const char ccmNameTemp[] = "Temperature";
const char ccmTypeTemp[] = "InAirTemp";
const char ccmUnitTemp[] = "C";

const char ccmNameHumid[] = "Humid";
const char ccmTypeHumid[] = "InAirHumid";
const char ccmUnitHumid[] = "%";

const char ccmNameHD[] = "HumidDiff";
const char ccmTypeHD[] = "InAirHD.mIC";
const char ccmUnitHD[] = "g m-3";

const char ccmNameCnd[] = "NodeCondition";
const char ccmTypeCnd[] = "cnd.mIC";
const char ccmUnitCnd[] = "";

//------------------------------------------------------
//UARDECS初期化用関数
//主にCCMの作成とMACアドレスの設定を行う
//------------------------------------------------------
void UserInit(){
  
  UECSsetCCM(true, CCMID_InAirTemp, ccmNameTemp, ccmTypeTemp, ccmUnitTemp, 29, 1, A_10S_0);
  UECSsetCCM(true, CCMID_InAirHumid, ccmNameHumid, ccmTypeHumid, ccmUnitHumid, 29, 1, A_10S_0);
  UECSsetCCM(true, CCMID_InAirHD, ccmNameHD, ccmTypeHD, ccmUnitHD, 29, 2, A_10S_0);
  UECSsetCCM(true, CCMID_cnd      , ccmNameCnd , ccmTypeCnd , ccmUnitCnd , 29,0, A_1S_0);
}

//---------------------------------------------------------
void OnWebFormRecieved(){
}
//---------------------------------------------------------
void UserEverySecond(){

//------------------------------------------------------温度計測(2秒に一度)  
static char count=0;
static double t=-999;
static double rh=-999;


//SHT3xは計測コマンドの後、データが準備されるのに時間がかかる
//1秒間隔で計測コマンドと読み出しコマンドを交互に送信している
if(count ==0)
  {
    sht3x.startMeasure();
    }
else if(count==1)
    {
     if(sht3x.getTempHumid())
        {
          t=sht3x.temp;
          rh=sht3x.humidity;
	  U_ccmList[CCMID_cnd].value=0;
         }
      else 
        {//エラー
          t=-999;
          rh=-999;
	  U_ccmList[CCMID_cnd].value|=OPRMODE_ERR_SHT3xSENSERR;
        }
    }

U_ccmList[CCMID_InAirTemp].value=(long)(t*10);
U_ccmList[CCMID_InAirHumid].value=(long)(rh*10);


count++;
count%=2;


//------------------------------------------------エラー時にCCM送信を停止する
if(U_ccmList[CCMID_InAirTemp].value<=-999 || U_ccmList[CCMID_InAirHumid].value<=-999)
    {
        U_ccmList[CCMID_InAirTemp].flagStimeRfirst=false;
        U_ccmList[CCMID_InAirHumid].flagStimeRfirst=false;
        U_ccmList[CCMID_InAirHD].flagStimeRfirst=false;
        U_ccmList[CCMID_cnd].value|=OPRMODE_ERR_SHT3xSENSERR;
        return;
    }

  
  //飽差計算
  double humidpress=6.1078*pow(10,(7.5*t/(t+237.3)));
  double humidvol=217*humidpress/(t+273.15);
  U_ccmList[CCMID_InAirHD].value=(100-rh)*humidvol;//少数が下位2桁なのでそのまま出力

  
  

}

//---------------------------------------------------------
void UserEveryMinute(){
}
//---------------------------------------------------------
void UserEveryLoop(){
}

//---------------------------------
void loop(){
UECSloop();
}

//---------------------------------
void setup(){

  Serial.begin(115200);
  delay(2000);   // シリアル通信の確立を待つ(時間がかかります)
  // シリアルモニタが開かれるまで待つ（PCと接続されていないと動作しません）
  // while (!Serial) {
  //   delay(10); // ゆっくり待つ
  // }
  Serial.println("W55RP20 module start");

  sht3x.begin();

  UECSsetup();

  // ネットワーク情報の確認
  Serial.print("Web server running at http://");
  Serial.println(Ethernet.localIP());
}
