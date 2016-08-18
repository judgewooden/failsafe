#include "Ethernet.h"
#include <SPI.h>
#include <EEPROM.h>

// Upload to Arduino Duemilanova w/Atmega328

//#define DEBUG
#ifdef DEBUG
 #define DEBUG_PRINT(x)        Serial.print(x)
 #define DEBUG_PRINTDEC(x)     Serial.print(x, DEC)
 #define DEBUG_PRINTLN(x)      Serial.println(x)
#else
 #define DEBUG_PRINT(x)
 #define DEBUG_PRINTDEC(x)
 #define DEBUG_PRINTLN(x)
#endif

//#define DEBUGFLOW
#ifdef DEBUGFLOW
 #define DFLOW_PRINT(x)        Serial.print(x)
 #define DFLOW_PRINTDEC(x)     Serial.print(x, DEC)
 #define DFLOW_PRINTLN(x)      Serial.println(x)
#else
 #define DFLOW_PRINT(x)
 #define DFLOW_PRINTDEC(x)
 #define DFLOW_PRINTLN(x)
#endif

//#define DEBUGHTTP
#ifdef DEBUGHTTP
 #define DHTTP_PRINT(x)        Serial.print(x)
 #define DHTTP_PRINTDEC(x)     Serial.print(x, DEC)
 #define DHTTP_PRINTLN(x)      Serial.println(x)
#else
 #define DHTTP_PRINT(x)
 #define DHTTP_PRINTDEC(x)
 #define DHTTP_PRINTLN(x)
#endif

const int numCircuits = 2;             // number of water sensors connected
const int numReadings = 25;            // number of pulses to check for flow calculation

int pin_input[] = {5, 7, 7};           // On what pins are the water sensors connected
double pulses_per_liter[] = {169,169,169};
                                       // Number of pulses per liter water
int pin_output = 6;                    // On what pin is the circuit breaker connected

long override_time= 120000;             // Give a 2 minute startup
int override_mode = 0;                  // Are we monitoring the flow
int Is_Power_On = 0;                    // Keep the state of the power breaker
int jsonresponse = 0;                   // Should I reply with JSON data on the HTML
int lasteepromcode = 1;                 // 1 == User never set anything, 2 == last code was wrong, 3 == last code was right

int lastpulse[numCircuits];             // Was the last pulse high or low
unsigned long last_pulse_time[numCircuits];
                                        // When was the last pulse
unsigned long readings[numReadings][numCircuits];
                                        // Array to keep pin readings to calculate flow
int index[numCircuits];                 // Keep track where we are in the Array
double Pulses_Per_Second[numCircuits];  // The calculated pulses per second
double Liter_per_minute[numCircuits];   // The calculated liters per minute
int valuesreset[numCircuits];           // Indicate if values are reset
int monitorMode[numCircuits];           // Type op monitoring 1=not monitor, 2=monitor, 3=load balance, else=ERROR

unsigned long current_time;             // Get the current system time
unsigned long timeout_millisecs = 5000; // Nr of miliseconds of no flow before activating circuit breakers

const double MilliPerSecond = 1000;     // a field to force calculation as a double
unsigned long report_count = 0;         // the last time that a calculation for flow was ade
int report_interval = 1000;             // Nr of miliseconds to calculate flow numbers

// Ethernet library configuration
byte mac[] = {  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
                                        // physical mac address
byte ip[] = {192, 168, 0, 8 };          // ip in lan
byte gateway[] = {192, 168, 0, 1 };     // internet access via router

// HMTL processing variables
EthernetServer server(80);              // server port
String readString = "";                 // string to process incoming data
char c;
char buffer[10];
int dataLength=0;

// Buffer containing processed hmtl data
char data[50];
char datamotor[5];

void setup() {

  for (int z = 0; z < numCircuits; z++) {
    pinMode(pin_input[z], INPUT);
    lastpulse[z]=0;
    last_pulse_time[z]=0;
    index[z]=0;
    valuesreset[z]=0;
    monitorMode[z]=2;                    // By default we monitor - except if override from EEPROM
  }
  pinMode(pin_output, OUTPUT);

  Serial.begin(9600);

  for (int z = 0; z < numCircuits; z++)
    for (int x = 0; x < numReadings; x++)
      readings[x][z] = 0.0;

  Ethernet.begin(mac, ip, gateway); //start Ethernet
  delay(1000);

  digitalWrite(pin_output, HIGH);
  Is_Power_On = 1;

  DEBUG_PRINT("POWER:");
  DEBUG_PRINT(Is_Power_On);

  if(override_time>0)
    override_mode=1;
  else
    override_mode=0;

  // See if a value has been saved in NVRAM (check codes 125&221 in first two bytes)
  if ( EEPROM.read(0) == 125 ) {
    if ( EEPROM.read(1) == 221 ) {
      for (int z = 0; z < numCircuits; z++) {
        monitorMode[z]=EEPROM.read(z+2);
        DEBUG_PRINT("[");
        DEBUG_PRINT(z);
        DEBUG_PRINT("] EEPROM Config=");
        DEBUG_PRINTLN(monitorMode[z]);
      }
    }
  }

  // set the default button state - else we start with garabage data
  for (int z = 0; z < numCircuits; z++) {
    int buttonState = digitalRead(pin_input[z]);
    if ( buttonState != lastpulse[z] ) {
      lastpulse[z] = buttonState;
      if(lastpulse[z]) {
        last_pulse_time[z]=millis();
      }
    }
  }
}

void loop() {

  // read the input pin:
  for (int z = 0; z < numCircuits; z++) {
    int buttonState = digitalRead(pin_input[z]);
    DEBUG_PRINT(z);
    DEBUG_PRINT(">");
    DEBUG_PRINT(buttonState);
    DEBUG_PRINT(" ");

    // count pulses in the loop
    if ( buttonState != lastpulse[z] ) {
      lastpulse[z] = buttonState;

      if(lastpulse[z]) {
        last_pulse_time[z]=millis();

        readings[index[z]][z]=last_pulse_time[z];
        index[z]++;
        if (index[z] >= numReadings)
          index[z]=0;
      }
    }
  }

  current_time = millis();

  // check if the power should switch off
  for (int z = 0; z < numCircuits; z++) {
    if (monitorMode[z]==2) {
      DFLOW_PRINT("[");
      DFLOW_PRINT(z);
      DFLOW_PRINT("] ");
      if(current_time > (timeout_millisecs + last_pulse_time[z]) ) {
        DFLOW_PRINT("ALERT. No flow for ");
        DFLOW_PRINT(timeout_millisecs);
        DFLOW_PRINT(" milliseconds. ");
        if (override_mode) {
          DFLOW_PRINTLN("KILL SIGNAL SKIPPED)!");
        }
        else {
          digitalWrite(pin_output, LOW);
          DFLOW_PRINTLN("SEND KILL SIGNAL");
          if (Is_Power_On) {
            Is_Power_On = 0;
          }
        }
      }
    }
  }

  // report values
  if( (current_time / report_interval) > report_count) {
    DFLOW_PRINT("Time: ");
    DFLOW_PRINTLN(current_time);

    for (int z = 0; z < numCircuits; z++) {

      // Show the average for last readings
      double total = 0;
      int looper;

      if ( index[z] != 0)
        total +=  readings[looper][z] - readings[numReadings - 1][z];

      for ( looper = 1; looper < index[z]; looper++)
        total += readings[looper][z] - readings[looper - 1][z];

      for ( looper = index[z] + 1; looper < numReadings; looper++)
        total += readings[looper][z] - readings[looper - 1][z];

      Pulses_Per_Second[z] = MilliPerSecond / (total / (numReadings-1));

      DFLOW_PRINT("[");
      DFLOW_PRINT(z);
      DFLOW_PRINT("] Average : ");
      DFLOW_PRINTLN(Pulses_Per_Second[z]);

      Liter_per_minute[z] = Pulses_Per_Second[z] * 60 / pulses_per_liter[z];
      DFLOW_PRINT("[");
      DFLOW_PRINT(z);
      DFLOW_PRINT("] Liter /pm: ");
      DFLOW_PRINTLN(Liter_per_minute[z]);
    }

    DFLOW_PRINT("    POWER:");
    DFLOW_PRINTLN(Is_Power_On);

    // countdown the time for override mode
    if(override_mode) {
      override_time -= report_interval;
      DFLOW_PRINT("    OVERRIDE MODE time left: ");
      DFLOW_PRINT(override_time);
      DFLOW_PRINTLN(" milliseconds ");
      if (override_time<0) {
        override_mode=0;
        override_time=0;
      }
    }

    // if no flow for double the timeout period blank all flow values
    for (int z = 0; z < numCircuits; z++) {
      if(current_time > ((2 * timeout_millisecs) + last_pulse_time[z]) ) {
        for (int i = 0; i < numReadings; i++) {
          readings[i][z] = 0.0;
        }
        valuesreset[z]=1;
        DFLOW_PRINT("[");
        DFLOW_PRINT(z);
        DFLOW_PRINTLN("] (reset)");
      }
      else {
        if (valuesreset[z]) {
          if((current_time - last_pulse_time[z]) < (2 * timeout_millisecs) ) {
            valuesreset[z]=0;
            DFLOW_PRINT("[");
            DFLOW_PRINT(z);
            DFLOW_PRINTLN("] (flow)");
          }
        }
      }
    }

    report_count++;
  }

  handle_http();
}

void reset_http_vals() {
  // Reinitializing variables
  readString =""; // Reinitialize String
  for (int i=0; i<10; i++)
  {
    buffer[i] = '\0';
  }
  for (int i=0; i<50; i++)
  {
    data[i] = '\0';
  }
  for (int i=0; i<5; i++)
  {
    datamotor[i] = '\0';
  }
  dataLength =0;
}

void handle_http() {
  EthernetClient client = server.available();

  if (client) {
    while (client.connected()) {
      while (client.available()) {

        DHTTP_PRINT(".");
        c = client.read();
        readString +=c;
        DHTTP_PRINT(c);

        if( readString.equals("GET")) {
          DHTTP_PRINTLN(" ");
          c = client.read();
          c = client.read();
          c = client.read();
          if (c == 'X' || c == '0' )
            jsonresponse=1;
          else {
            if ((c == 'Y' || c == '1') && numCircuits > 1)
              jsonresponse=2;
            else {
              if ((c == 'Z' || c =='2') && numCircuits > 2)
                jsonresponse=3;
              else
                jsonresponse=0;
            }
          }
          DHTTP_PRINT("JSON=");
          DHTTP_PRINT(jsonresponse);
          DHTTP_PRINTLN(", GET");
          break;
        }

        if( readString.equals("POST")) {
          DHTTP_PRINTLN("POST");
          // 320 is arbitrary.
          for(int i=0; i<320; i++) {
            c = client.read();
            DHTTP_PRINT(c);
          }

          //Searches for "Length: "
          while(c != 'L') {
            c = client.read();
            DHTTP_PRINT(c);
          }

          // Skip "Length: "
          for (int i=0; i<7; i++)
          {
            c = client.read();
            DHTTP_PRINT(c);
          }

          // Read the data package length
          readString="";
          c = client.read();

          while(c != '\n')
          {
            readString += c;
            DHTTP_PRINT(c);
            c = client.read();
          }
          // convert data read from String to int
          readString.toCharArray(buffer, readString.length());
          dataLength = atoi(buffer);
          DHTTP_PRINTLN("");
          DHTTP_PRINT("dataLength: ");
          DHTTP_PRINTLN(dataLength);

          // gets DATA
          client.read(); // skips additional newline
          client.read();
          for (int i=0; i<dataLength; i++) {
            data[i] = client.read();
          }

          DHTTP_PRINTLN("");
          DHTTP_PRINT("data: ");
          DHTTP_PRINTLN(data);

          DHTTP_PRINT("var: ");
          DHTTP_PRINTLN(data[0]);
          readString ="";
          lasteepromcode=1;

          // User clicked on RESET
          if (data[0]=='O') {

            int i = 0;
            while(data[i] != '=')
              i++;
            i++;

            DHTTP_PRINTLN(data + i);

            override_time = atoi(data + i);
            if (override_time>150)
              override_time = 150;

            override_time *= 60000;
            DHTTP_PRINT("Override period: ");
            DHTTP_PRINTLN(override_time);

            if(override_time>0)
              override_mode=1;
            else
              override_mode=0;
          }
          else { // User clicked on APPLY

            int i = 0;

            for (int z = 0; z < numCircuits; z++) {

              while(data[i] != '=')
                i++;
              i++;

              DHTTP_PRINT("[");
              DHTTP_PRINT(z);
              DHTTP_PRINT("] ");
              if( data[i] == '1') {
                DHTTP_PRINTLN("OFF");
                monitorMode[z]=1;
              }
              if( data[i] == '2') {
                DHTTP_PRINTLN("ON");
                monitorMode[z]=2;
              }
            }

            while(data[i] != '=')
              i++;
            i++;

            long temp;
            temp=atol(data + i);
            DHTTP_PRINT("E=");
            DHTTP_PRINTLN(temp);
            if (temp==125221) {
              DHTTP_PRINTLN("SAVE");
              lasteepromcode=3;

              if ( EEPROM.read(0) != 125 )
                EEPROM.write(0, 125);

              if ( EEPROM.read(1) != 221 )
                EEPROM.write(1, 221);

              for (int z = 0; z < numCircuits; z++) {
                if ( EEPROM.read(z+2) != monitorMode[z])
                  EEPROM.write(z+2, monitorMode[z]);
              }
            } else {
              if(temp>0)
                lasteepromcode=2;
            }
          }
        }
        jsonresponse=0;
      }

      if (jsonresponse) {
        // JSON CODE
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println();
        client.print("{\"Power\":");
        client.print(Is_Power_On);
        client.print(",\"Loop\":");
        client.print(jsonresponse-1);
        client.print(",\"Mode\":");
        client.print(monitorMode[jsonresponse-1]);
        client.print(",\"ValuesReset\":");
        client.print(valuesreset[jsonresponse-1]);
        client.print(",\"FlowPerSecond\":");
        client.print(Pulses_Per_Second[jsonresponse-1]);
        client.print(",\"LitersPerSecond\":");
        client.print(Liter_per_minute[jsonresponse-1]);
        client.print(",\"OverrideTime\":");
        client.print(override_time);
        client.print(",\"CurrentTime\":");
        client.print(current_time);
        client.print(",\"data\":[");

        int looper;
        for ( looper = index[jsonresponse-1]; looper < numReadings; looper++) {
          client.print(readings[looper][jsonresponse-1]);
          client.print(",");
        }

        for ( looper = 0; looper < index[jsonresponse-1]; looper++) {
          client.print(readings[looper][jsonresponse-1]);
          if (looper < (index[jsonresponse-1]-1))
            client.print(",");
        }

        client.println("]}");
        client.println("");

      }
      else {
        // HTML CODE
        client.println("<!DOCTYPE html>");
        client.print("<meta http-equiv=\"refresh\" content=\"");
        client.print(report_interval/1000*5);
        client.println("\">");
        client.println("<html>");

        client.print("Power=");
        if (Is_Power_On)
          client.print("ON");
        else
          client.print("OFF");

        for (int z = 0; z < numCircuits; z++) {
          client.print("<br>[");
          client.print(z);
          client.print("] ");
          client.print("Flow / second: ");
          client.print(Pulses_Per_Second[z]);
          if (valuesreset[z]>=1)
            client.print(" (reset)");

          client.print("<br>[");
          client.print(z);
          client.print("] ");
          client.print("Liters / minute: ");
          client.print(Liter_per_minute[z]);
          if (valuesreset[z]>=1)
            client.print(" (reset)");

          client.print("<br>[");
          client.print(z);
          client.print("] Monitor=");
          if (monitorMode[z]==2)
            client.print("ON");
          else
            client.print("OFF");
        }

        if (override_time>0) {
          client.print("<br><br>Override mode, time remaining: ");
          client.print(override_time / 1000 / 60);
          client.print(":");
          int remainder = (override_time / 1000) % 60;
          if (remainder < 10)
            client.print("0");
          client.print(remainder);
          client.print("<br><br>*** WARNING - OVERRIDE MODE ***");
        }
        else
          client.print("<br>Monitoring mode");
        client.print("<br> ");

        client.println("<br><form name=input method=post>Set minutes: ");
        client.println("<input type=\"text\" value=\"");
        //     client.print(override_time);
        client.print("\" name=\"O\" />");
        client.print(" (0=off,150=max) ");
        client.println("<input type=\"submit\" value=\"Reset\" />");
        client.println("</form> <br /> ");

        client.println("<form name=input method=post>Monitoring:<br>");
        for (int z = 0; z < numCircuits; z++) {
          client.print("[");
          client.print(z);
          client.print("] <select name=\"");
          client.print(z);
          client.print("\"><option value=\"1\"");
          if (monitorMode[z]==1)
            client.print(" selected");
          client.print(">OFF</option><option value=\"2\"");
          if (monitorMode[z]==2)
            client.print(" selected");
          client.print(">ON</option>");
          client.print("</select><br>");
        }
        client.println("Eeprom code: <input type=\"text\" value=\"");
        client.print("\" name=\"E\" />");
        DHTTP_PRINTLN(lasteepromcode);
        if ( lasteepromcode == 2 )
            client.print(" Wrong");
        if ( lasteepromcode == 3 )
            client.print(" Saved");
        client.print("<br><input type=\"submit\" value=\"Apply\"/></form><br>");
        client.println("</html>");
      }
      client.stop();
    }
  }

  reset_http_vals();
}
