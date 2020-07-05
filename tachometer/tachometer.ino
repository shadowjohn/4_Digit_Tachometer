//用來作排序的套件
#include <ArduinoSort.h> 

//數位模組
#include <TM1637.h>

//實來實作超轉燈 D0 沒有 PWM ，所以用 D1 來作呼吸燈
//詳見 : http://web.htjh.tp.edu.tw/B4/106iot/NodeMCU%E4%BD%BF%E7%94%A8%E4%BB%8B%E7%B4%B9.pdf
const int overRPMLedPin = D1;

//用來偵測 D3 腳抓引擎轉速的部分
const int firePin = D3;
unsigned long rpm = 0;
unsigned long checkZeroCounts = 0; //用來檢查引擎是不是熄了，連續一段時間沒偵測到轉速， loop 就重來

//範例：https://github.com/Seeed-Studio/Grove_4Digital_Display/blob/master/examples/DisplayNum/DisplayNum.ino
//範例：https://github.com/Seeed-Studio/Grove_4Digital_Display/blob/master/examples/DisplayStr/DisplayStr.ino
//用來處理 4 Digit-Diplay TM1637 的部分
#define CLK D4 //pins definitions for TM1637 and can be changed to other ports
#define DIO D5 //給 4 Digit-Display TM1637 使用
TM1637 tm1637(CLK,DIO);

unsigned long start = 0;
unsigned long pulseCounts = 0;
unsigned long st = 0; //用來紀錄時間序

//取得的rpm不停的寫入 clean_rpms，0~19 陣列，排序後，取中間，減少判差
//可以用來作最後20次rpm變化量，作延遲觸發事件
unsigned int clean_rpms[20] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
unsigned int rpm_step = 0; //上面陣列儲存的 index，循序寫入，最後 % 陣列長度後回到 index 0

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);    

  // 4 Digit-Display
  tm1637.init();  
  tm1637.set(BRIGHT_TYPICAL); //BRIGHT_TYPICAL = 2,BRIGHT_DARKEST = 0,BRIGHTEST = 7;
  playFirstTime();
  playFirstTime();
  //回歸零
  tm1637.display(0,-1); //-1 for blank
  tm1637.display(1,-1); //-1 for blank
  tm1637.display(2,-1); //-1 for blank
  tm1637.display(3,0);
  
  //超轉燈
  pinMode(overRPMLedPin, OUTPUT);
  digitalWrite(overRPMLedPin,LOW); //預設不亮

  //讀入轉速
  pinMode(firePin, INPUT);  
}
void playFirstTime()
{
  // 0000~9999 跑二次
  for(int i=0;i<= 9;i++)
  {
    for(int j=0;j<4;j++)
    {
      tm1637.display(j,i);    
    }
    delay(100);  
  }
}

void diaplayOnLed(int show_rpm)
{
  //將轉速，變成顯示值
  //只顯示 萬千百十
  //如果要顯示 千百十個，就不用除了
  //太多數位有點眼花
  String rpm_str = String(show_rpm/10);
  if(rpm_str.length()<=3)
  {
    rpm_str = lpad(rpm_str,4,"X"); // 變成如 "XXX0"
  }
  Serial.print("\nAfter lpad:");
  Serial.println(rpm_str);
  for(int i=0;i<4;i++)
  { 
      if(rpm_str[i]=='X')
      {
        tm1637.display(i,-1); //-1 代表 blank 一片黑    
      }
      else
      {
        // Serial.println(rpm_str[i]);
        // 腦包直接轉回 String 再把單字轉 int
        // From : https://www.arduino.cc/en/Tutorial.StringToIntExample
        tm1637.display(i,String(rpm_str[i]).toInt());    
      }
  }
}
String lpad(String temp , byte L , String theword){
  //用來補LED左邊的空白
  byte mylen = temp.length();
  if(mylen > (L - 1))return temp.substring(0,L-1);
  for (byte i=0; i< (L-mylen); i++) 
    temp = theword + temp;
  return temp;
}
void overRPM(){
  //超轉燈，呼吸燈
  if( rpm >= 8000 && rpm < 12000)
  {
    // 詳見 https://www.youtube.com/watch?v=27GkMk8ct0s
    // 0~255，直接就從 半亮至全亮 128~255
    // 12000:255, 8000:128
    //digitalWrite(overRPMLedPin,HIGH);    
    int rpm_val = map(rpm, 8000, 12000, 128, 255);
    analogWrite (overRPMLedPin, rpm_val);
  }
  else if( rpm >= 12000) //超過12000轉就整顆全亮
  {
    analogWrite (overRPMLedPin,255);
  }
  else //低於 8000 以下，暗
  {
    analogWrite (overRPMLedPin,0);
  }
}
boolean getPulseStatus(){
  //取得轉速訊號是否為高電位
  return (digitalRead(firePin)==HIGH);
}


void loop() {
  //用來計算每一個 pulse 經過多長時間
  
  checkZeroCounts = 0;
  while(!getPulseStatus()) {   //等待 Low，隔一次偵測，首次不管
    //當轉速訊號消失一段時間後，轉速表也要歸零
    checkZeroCounts++;
    if(checkZeroCounts>= 600000){ //超時，低於50rpm
      //直接輸出 0 
      checkZeroCounts = 0;
      rpm = 0;
      diaplayOnLed(rpm);
      overRPM();
      return;
    }
  };
  checkZeroCounts = 0;
  while(getPulseStatus()){ start = micros(); //變正了，不停的抓時間
    //當轉速訊號消失一段時間後，轉速表也要歸零
    checkZeroCounts++;
    if(checkZeroCounts>= 600000){ //超時，低於50rpm
      //直接輸出 0 
      checkZeroCounts = 0;
      rpm = 0;
      diaplayOnLed(rpm);
      overRPM();
      return;
    }
  }
  checkZeroCounts = 0;
  while(!getPulseStatus()){
    //當轉速訊號消失一段時間後，轉速表也要歸零
    checkZeroCounts++;
    if(checkZeroCounts>= 600000){ //超時，低於50rpm
      //直接輸出 0 
      checkZeroCounts = 0;
      rpm = 0;
      diaplayOnLed(rpm);
      overRPM();
      return;
    }
  };   //等待 Low 經過時間
  checkZeroCounts = 0;
  while(getPulseStatus()){
    //當轉速訊號消失一段時間後，轉速表也要歸零
    checkZeroCounts++;
    if(checkZeroCounts>= 600000){ //超時，低於50rpm
      //直接輸出 0 
      checkZeroCounts = 0;
      rpm = 0;
      diaplayOnLed(rpm);
      overRPM();
      return;
    }
  }    //又變回正 經過時間
  pulseCounts=micros()-start; //總算抓到一個 pulse 時間，用現在時間減去開始時間
  /*
   * 參考：http://stm32-learning.blogspot.com/2014/05/arduino.html
   * 轉速   100 轉 = 每分鐘   100 轉，每秒  1.67 轉，1轉多少秒呢，一轉 = 0.598802   秒 = 598.802 ms = 598802us
   * 轉速   200 轉 = 每分鐘   200 轉，每秒  3.3  轉，1轉多少秒呢，一轉 = 0.300003   秒 = 300.003 ms = 300003us
   * 轉速   600 轉 = 每分鐘   600 轉，每秒  10   轉，1轉多少秒呢，一轉 = 0.1        秒 = 100.000 ms = 100000us
   * 轉速  1500 轉 = 每分鐘  1500 轉，每秒  25   轉，1轉多少秒呢，一轉 = 0.04       秒 =  40.000 ms =  40000us
   * 轉速  6000 轉 = 每分鐘  6000 轉，每秒  60   轉，1轉多少秒呢，一轉 = 0.01666... 秒 =  16.667 ms =  16667us
   * 轉速 14000 轉 = 每分鐘 14000 轉，每秒 233.3 轉，1轉多少秒呢，一轉 = 0.0042863. 秒 =   4.286 ms =   4286us
   * 轉速 16000 轉 = 每分鐘 16000 轉，每秒 266.6 轉，1轉多少秒呢，一轉 = 0.0037500. 秒 =   3.750 ms =   3750us
   */ 
  if( pulseCounts>3000 && pulseCounts<300003 ){ //T=3000us 同 20000rpm; T=300003us 同 200rpm // 頻率
    rpm = 60000000/pulseCounts;          //rpm=(1sec/T)*60=60000000(us)/T(us)
  }
  //用來過濾雜訊
  clean_rpms[rpm_step] = rpm;
  rpm_step++;
  rpm_step%=20;
  //排序收集到的轉速
  sortArray(clean_rpms, 20);
  /*int rpm_sum = 0;
  for(int i=6;i<15;i++)
  {
    rpm_sum+=clean_rpms[i];
  }
  rpm = rpm_sum / (15-6); //永遠取中間的平均值，避免雜訊
  */
  //直接取中間值，頭尾雜訊不要了
  rpm = clean_rpms[10] - (clean_rpms[10]%100);

  //讓 LED 不要刷的太快，太快眼睛跟不上字會黏在一起
  if (micros()-st > 55000) //550ms 眼睛才受的了
  {       
    diaplayOnLed(rpm); //在 TM1637 顯示   
    Serial.print("Rpm: ");   
    Serial.print(rpm);
    Serial.println();   
    st = micros(); 
  }

  overRPM();


}

