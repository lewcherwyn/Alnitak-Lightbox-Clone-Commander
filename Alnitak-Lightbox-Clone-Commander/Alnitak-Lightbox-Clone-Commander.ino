/*
What: LEDLightBoxAlnitak - PC controlled lightbox implmented using the 
  Alnitak (Flip-Flat/Flat-Man) command set found here:
  http://www.optecinc.com/astronomy/pdf/Alnitak%20Astrosystems%20GenericCommandsR3.pdf

Who: 
  Created By: Jared Wellman - jared@mainsequencesoftware.com
  Updated By: red-man
  Modified By: lewcherwyn

Changelog:

2020-06-25
  Updated to align with spec in link above.  Previously it was appending '000\n' instead
  of 'OOO\r' to the responses and did not work with NINA.
2021-12-22
  开发对应上位机，可调整转动角度，可定时
  Can be controlled by a software which has a timing function, rotates at a specific angle as well.


Typical usage on the command prompt:
Send     : >SOOO\n      //request state
Recieve  : *S19OOO\n    //returned state

Send     : >B128\n      //set brightness 128
Recieve  : *B19128\n    //confirming brightness set to 128

Send     : >JOOO\n      //get brightness
Recieve  : *B19128\n    //brightness value of 128 (assuming as set from above)

Send     : >LOOO\n      //turn light on (uses set brightness value)
Recieve  : *L19OOO\n    //confirms light turned on

Send     : >DOOO\n      //turn light off (brightness value should not be changed)
Recieve  : *D19OOO\n    //confirms light turned off.
*/

volatile int ledPin = 5;      // the pin that the LED is attached to, needs to be a PWM pin.
int brightness = 0;

int angle = 90;    //默认开盖一半的角度，具体情况需要考虑舵机类型
static const int motorPin = 10;  //舵机默认引脚
int pos;    //移动步长
int dis;    //移动距离
int pulsewidth;  //信号半宽

enum devices
{
  FLAT_MAN_L = 10,
  FLAT_MAN_XL = 15,
  FLAT_MAN = 19,
  FLIP_FLAT = 99
};

enum motorStatuses
{
  STOPPED = 0,
  RUNNING
};

enum lightStatuses
{
  OFF = 0,
  ON
};

enum shutterStatuses
{
  UNKNOWN = 0, // ie not open or closed...could be moving
  CLOSED,
  OPEN
};


int deviceId = FLAT_MAN;
int motorStatus = STOPPED;
int lightStatus = OFF;
int coverStatus = UNKNOWN;

void setup()
{
  // initialize the serial communication:
  Serial.begin(9600);
  //Serial.begin(115200);
  // initialize the ledPin as an output:
  pinMode(ledPin, OUTPUT);
  pinMode(motorPin, OUTPUT);
  analogWrite(ledPin, 0);
}

void loop() 
{
  handleSerial();
}


void handleSerial()
{
  if( Serial.available() >= 6 )  // all incoming communications are fixed length at 6 bytes including the \n
  {
    char* cmd;
  char* data;
    char temp[10];
    
    int len = 0;

    char str[20];
    memset(str, 0, 20);
    
  // I don't personally like using the \r as a command character for reading.  
  // but that's how the command set is.
    Serial.readBytesUntil('\r', str, 20);

  cmd = str + 1;
  data = str + 2;
  
  // useful for debugging to make sure your commands came through and are parsed correctly.
    if( false )
    {
      sprintf( temp, "cmd = >%c%s;\n", cmd, data);
      Serial.print(temp);
    } 
    


    switch( *cmd )
    {
    /*
    Ping device
      Request: >POOO\n
      Return : *PiiOOO\n
        id = deviceId
    */
      case 'P':
      sprintf(temp, "*P%dOOO\n", deviceId);
      Serial.print(temp);
      break;

      /*
    Open shutter          //开盖指令
      Request: >OOOO\n
      Return : *OiiOOO\n
        id = deviceId

      This command is only supported on the Flip-Flat!
    */
      case 'O':
      sprintf(temp, "*O%dOOO\n", deviceId);
      SetShutter(angle);
      Serial.print(temp);
      break;

      /*
     Set angle
      Request: >AOOO\n                //电机实际最大角度对应为180，调整范围>AOOO~>A180
      Return : *AiiOOO\n
        id = deviceId

      This command is only supported on the Flip-Flat!
    */
      case 'A':
      angle = atoi(data); 
      sprintf( temp, "*A%d%03d\n", deviceId, angle );
      Serial.print(temp);
      break;

    /*
    Get angle                 //获取角度
      Request: >KOOO\n
      Return : *Kiiyyy\n
        id = deviceId
        yyy = current angle value from 000-180
    */
      case 'K':
      sprintf( temp, "*K%d%03d\n", deviceId, angle);
      Serial.print(temp);
      break;

      /*
    Close shutter             //关盖指令
      Request: >COOO\n
      Return : *CiiOOO\n
        id = deviceId

      This command is only supported on the Flip-Flat!
    */
      case 'C':
      sprintf(temp, "*C%dOOO\n", deviceId);
      SetShutter(0);
      Serial.print(temp);
      break;

    /*
    Turn light on             //打开平场灯
      Request: >LOOO\n
      Return : *LiiOOO\n
        id = deviceId
    */
      case 'L':
      sprintf(temp, "*L%dOOO\n", deviceId);
      Serial.print(temp);
      lightStatus = ON;
      analogWrite(ledPin, brightness);
      break;

    /*
    Turn light off          //关闭平场灯
      Request: >DOOO\n
      Return : *DiiOOO\n
        id = deviceId
    */
      case 'D':
      sprintf(temp, "*D%dOOO\n", deviceId);
      Serial.print(temp);
      lightStatus = OFF;
      analogWrite(ledPin, 0);
      break;

    /*
    Set brightness            //设置平场灯的亮度
      Request: >Bxxx\n
        xxx = brightness value from 000-255
      Return : *Biiyyy\n
        id = deviceId
        yyy = value that brightness was set from 000-255
    */
      case 'B':
      brightness = atoi(data);   
      if( lightStatus == ON ) 
        analogWrite(ledPin, brightness);   
      sprintf( temp, "*B%d%03d\n", deviceId, brightness );
      Serial.print(temp);
        break;

    /*
    Get brightness          //获取平场灯的亮度
      Request: >JOOO\n
      Return : *Jiiyyy\n
        id = deviceId
        yyy = current brightness value from 000-255
    */
      case 'J':
        sprintf( temp, "*J%d%03d\n", deviceId, brightness);
        Serial.print(temp);
        break;
      
    /*
    Get device status:              //获取设备信息
      Request: >SOOO\n
      Return : *SidMLC\n
        id = deviceId
        M  = motor status( 0 stopped, 1 running)
        L  = light status( 0 off, 1 on)
        C  = Cover Status( 0 moving, 1 closed, 2 open)
    */
      case 'S': 
        sprintf( temp, "*S%d%d%d%d\n",deviceId, motorStatus, lightStatus, coverStatus);
        Serial.print(temp);
        break;

    /*
    Get firmware version
      Request: >VOOO\n
      Return : *Vii001\n
        id = deviceId
    */
      case 'V': // get firmware version
      sprintf(temp, "*V%d001\n", deviceId);
      Serial.print(temp);
      break;
    }    

  while( Serial.available() > 0 )
    Serial.read();

  }
}

void SetShutter(int val)
{
  if( val > 0 )
    {
      coverStatus = OPEN;
      GotoAngle(val);
    }
    // TODO: Implement code to OPEN the shutter.
  
  else if( val == 0 && coverStatus != CLOSED )
  {

      coverStatus = CLOSED;
      GotoAngle(val);
    // TODO: Implement code to CLOSE the shutter
  }
  else
  {
    // TODO: Actually handle this case
     coverStatus = UNKNOWN;
  }
  
}


void GotoAngle(int target)
{
  if (target > pos && target <= 180)
  {    
   dis = target - pos;
    for( int i=0; i<dis; i++ )
    {
      servopulse( motorPin, pos+1 );
      delay(20);
      pos = pos + 1;
    }
  } 
  
  else if (target<pos && target>=0)
  {  
   dis = pos - target;
    for( int i=0; i<dis; i++ )
    {
      servopulse( motorPin, pos-1 );
      delay(20);
      pos = pos - 1;
    }
  }
}

void servopulse(int servopin, int myangle)
{
    pulsewidth=(myangle*11)+500;
    digitalWrite(motorPin,HIGH);
    delayMicroseconds(pulsewidth);
    digitalWrite(motorPin,LOW);
    delay(20-pulsewidth/1000);
}
