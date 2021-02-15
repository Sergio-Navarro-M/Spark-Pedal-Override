//#include "heltec.h"
#include "SSD1306Wire.h" // https://github.com/ThingPulse/esp8266-oled-ssd1306

#include "SparkClass.h"
#include "SparkPresets.h"

#include "BluetoothSerial.h" // https://github.com/espressif/arduino-esp32

// Device Info Definitions
const String DEVICE_NAME = "Spark Pedal";
const String VERSION = "0.1.1";

// Bluetooth vars
#define SPARK_NAME "Spark 40 Audio"
#define MY_NAME    "Spark Pedal"

// OLED Screen Definitions (SH1106 driven screen can be used in place of a SSD1306 screen, if desired)
SSD1306Wire oled(0x3c, SDA, SCL); // ADDRESS, SDA, SCL 

#define BACKGROUND TFT_BLACK
#define TEXT_COLOUR TFT_WHITE

#define TITLE  00
#define STATUS 15
#define IN     30
#define OUT    50

#define STD_HEIGHT 20
#define PANEL_HEIGHT 68

BluetoothSerial SerialBT;

// Spark vars
SparkClass sc1, sc2, sc3, sc4, scr, scrp;
SparkClass sc_setpreset7f;
SparkClass sc_getserial;

SparkPreset preset;

// ------------------------------------------------------------------------------------------
// Display routintes

// Display vars
#define DISP_LEN 50
char outstr[DISP_LEN+1];
char instr[DISP_LEN+1];
char statstr[DISP_LEN+1];
char titlestr[DISP_LEN+1];

//int bar_pos;
//unsigned long bar_count;

//void do_backgrounds()
//{
//   bar_pos=0;
//   bar_count = millis();
//}

//  void display_bar()
//  {
//     if (millis() - bar_count > 400) {
//        bar_count = millis();
//        bar_pos ++;
//        bar_pos %= 10;
//     }
//  }

void display_val(float val)
{
  int dist;

  dist = uint8_t(val * 100);

  Serial.println("Progress bar: ");
  Serial.println(dist);

  // oled
//  oled.drawRect(13, 16, 102, 4);
//  oled.fillRect(14, 17, dist, 2);
  oled.drawProgressBar(15, 16, 100, 4, dist);
  oled.display();
}

void display_str()
{
  Serial.println("DISPLAY STRING ROUTINE");
  Serial.println(titlestr);
  Serial.println(instr);
  Serial.println(outstr);
  Serial.println(statstr); 

  // oled
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_CENTER);
  oled.drawString(64, 0, titlestr);
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_CENTER);
  oled.drawString(64, 22, instr);
  oled.setTextAlignment(TEXT_ALIGN_CENTER);
  oled.drawString(64, 36,  outstr);
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_CENTER);
  oled.drawString(64, 50, statstr);
  oled.display();
}



// ------------------------------------------------------------------------------------------
// Bluetooth routines

bool connected;
int bt_event;

void btEventCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param){
  // On BT connection close
  if (event == ESP_SPP_CLOSE_EVT ){
    // TODO: Until the cause of connection instability (compared to Pi version) over long durations 
    // is resolved, this should keep your pedal and amp connected fairly well by forcing reconnection
    // in the main loop
    connected = false;
  }
  if (event != ESP_SPP_CLOSE_EVT ) {
     bt_event = event;
  }
}

void start_bt() {
   SerialBT.register_callback(btEventCallback);
   
   if (!SerialBT.begin (MY_NAME, true)) {
      strncpy( statstr, "Bluetooth init fail", 25);
      display_str();
      while (true);
   }    
   connected = false;
   bt_event = 0;
}


void connect_to_spark() {
   int rec;

   while (!connected) {
      strncpy (statstr, "Connecting to Spark", 25);
      display_str();
      connected = SerialBT.connect(SPARK_NAME);
      if (connected && SerialBT.hasClient()) {
         strncpy(statstr, "Connected to Spark", 25);
         display_str();
      }
      else {
         connected = false;
         delay(3000);
      }
   }
   // flush anything read from Spark - just in case

   while (SerialBT.available())
      rec = SerialBT.read(); 
}

// ----------------------------------------------------------------------------------------
// Send messages to the Spark (and receive an acknowledgement where appropriate

void send_bt(SparkClass& spark_class)
{
   int i;

   // if multiple blocks then send all but the last - the for loop should only run if last_block > 0
   for (i = 0; i < spark_class.last_block; i++) {
      SerialBT.write(spark_class.buf[i], BLK_SIZE);
 
   }
      
   // and send the last one      
   SerialBT.write(spark_class.buf[spark_class.last_block], spark_class.last_pos+1);

}


// Helper functions to send an acknoweledgement and to send a requesjames luit for a preset

void send_ack(int seq, int cmd)
{
   byte ack[]{  0x01, 0xfe, 0x00, 0x00, 0x41, 0xff, 0x17, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                0xf0, 0x01, 0xff, 0x00, 0x04, 0xff, 0xf7};
   ack[18] = seq;
   ack[21] = cmd;            

   SerialBT.write(ack, sizeof(ack));
}

void send_preset_request(int preset)
{
   byte req[]{0x01, 0xfe, 0x00, 0x00, 0x53, 0xfe, 0x3c, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0xf0, 0x01, 0x04, 0x00, 0x02, 0x01,
              0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0x00, 0xf7};
 
   req[24] = preset;
   SerialBT.write(req, sizeof(req));      
}


void send_receive_bt(SparkClass& spark_class)
{
   int i;
   int rec;

   // if multiple blocks then send all but the last - the for loop should only run if last_block > 0
   for (i = 0; i < spark_class.last_block; i++) {
      SerialBT.write(spark_class.buf[i], BLK_SIZE);
      // read the ACK
      rec = scr.get_data();
      if (rec <0) Serial.println("WAITING FOR ACK, GOT AN ERROR"); 
   }
      
   // and send the last one      
   SerialBT.write(spark_class.buf[spark_class.last_block], spark_class.last_pos+1); 
   
   // read the ACK
      rec = scr.get_data();
      if (rec <0) Serial.println("WAITING FOR ACK, GOT AN ERROR"); 
}


// ------------------------------------------------------------------------------------------

int i, j, p;
int pres;

int cmd, sub_cmd;
char a_str[STR_LEN+1];
char b_str[STR_LEN+1];
int param;
float val;

unsigned long keep_alive;


void setup() {
  Serial.begin(115200);

  // Initialize device OLED display, and flip screen, as OLED library starts "upside-down" (for some reason?)
  oled.init();
  oled.flipScreenVertically();
  
  // Show welcome screen
  oled.clear();
  oled.setFont(ArialMT_Plain_24);
  oled.setTextAlignment(TEXT_ALIGN_CENTER);
  oled.drawString(64, 12, DEVICE_NAME);
  oled.setFont(ArialMT_Plain_16);
  oled.setTextAlignment(TEXT_ALIGN_CENTER);
  oled.drawString(64, 36, "ESP32 v" + VERSION);
  oled.display();
  delay(4000);
  
   strncpy(statstr,  "Started", 25);
   strncpy(outstr,   "Nothing out", 25);
   strncpy(instr,    "Nothing in", 25);
   strncpy(titlestr, "Spark Override", 25);
   display_str();

   start_bt();
   connect_to_spark();

   keep_alive = millis();

   // set up the change to 7f message for when we send a full preset
   sc_setpreset7f.change_hardware_preset(0x7f);
   sc_getserial.get_serial();
}

void loop() {
   int av;
   int ret;
   int ct;
   
//
//   display_bar();
   
   // this will connect if not already connected
   if (!connected) connect_to_spark();

   // see if this keeps the connection alive
   if (millis() - keep_alive  > 10000) {
      keep_alive = millis();
      send_receive_bt(sc_getserial);
   }
   
   delay(10);
   
   if (SerialBT.available()) {
      // reset timeout
      keep_alive = millis();
      
      if (scr.get_data() >= 0 && scr.parse_data() >=0) {
         for (i=0; i<scr.num_messages; i++) {
            cmd = scr.messages[i].cmd;
            sub_cmd = scr.messages[i].sub_cmd;

            if (cmd == 0x03 && sub_cmd == 0x01) {
               ret = scr.get_preset(i, &preset);
               Serial.println("Get preset");
               Serial.println(ret);
               Serial.println(preset.Name);
               if (ret >= 0){
                  snprintf(instr, DISP_LEN-1, "Preset: %s", preset.Name);
                  display_str();
               }
            }
            else if (cmd == 0x03 && sub_cmd == 0x37) {
               ret = scr.get_effect_parameter(i, a_str, &param, &val);
               Serial.println("Get effect params");
               Serial.println(ret);
               if (ret >=0) {
                  snprintf(instr, DISP_LEN-1, "%s %d %0.2f", a_str, param, val);
                  display_str();  
                  display_val(val);
               }
            }
            else if (cmd == 0x03 && sub_cmd == 0x06) {
               // it can only be an amp change if received from the Spark
               ret = scr.get_effect_change(i, a_str, b_str);
               Serial.println("Get effect change");
               Serial.println(ret);
               if (ret >= 0) {
            
                  snprintf(instr, DISP_LEN-1, "-> %s", b_str);
                  display_str();
            
                  if      (!strncmp(b_str, "FatAcousticV2", STR_LEN-1)) pres = 16;
                  else if (!strncmp(b_str, "GK800", STR_LEN-1)) pres = 17;
                  else if (!strncmp(b_str, "Twin", STR_LEN-1)) pres = 3;
                  else if (!strncmp(b_str, "TwoStoneSP50", STR_LEN-1)) pres = 12;
                  else if (!strncmp(b_str, "OverDrivenJM45", STR_LEN-1)) pres = 5; 
                  else if (!strncmp(b_str, "AmericanHighGain", STR_LEN-1)) pres = 22;
                  else if (!strncmp(b_str, "EVH", STR_LEN-1)) pres = 7;
                                               
                  sc2.create_preset(*presets[pres]);
                  send_receive_bt(sc2);
                  send_receive_bt(sc_setpreset7f);

                  snprintf(outstr, DISP_LEN-1, "Preset: %s", presets[pres]->Name);
                  display_str();

                  send_preset_request(0x7f);
               }
            }
            else {
               snprintf(instr, DISP_LEN-1, "Command %2X %2X", scr.messages[i].cmd, scr.messages[i].sub_cmd);

            }
         }
      }
   }
}
