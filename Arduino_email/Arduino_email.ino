#include "Ethernet.h"
#include <SPI.h>

int pin_input = 3;
int pin_output = 4;

// Ethernet library configuration
byte mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; //physical mac address
byte ip[] = { 
  192, 168, 8, 11 }; // ip in lan
byte gateway[] = { 
  192, 168, 8, 1 }; // internet access via router
byte smtp_server[] = { 
  192, 168, 8, 5}; // douwedejong.com

// HMTL processing variables
EthernetServer server(80); //server port
String readString = ""; //string to get incoming data
char c;
char buffer[10];
int dataLength =0;
// Buffer containing processed hmtl data
char data[50];
char datamotor[5];

// new code
int debugflow = 1;
int jsonresponse = 0;
int lastpulse = 0;
int Is_Power_On = 0;
unsigned long last_pulse_time = 0;
unsigned long current_time;
unsigned long report_count = 0;
int report_interval = 1000;
unsigned long timeout_millisecs = 5000;
const double MilliPerSecond = 1000;
double Pulses_Per_Second;
double Liter_per_minute;
double dummy_since_there_seem_to_be_ram_problems_here;
long override_time= 120000;  // Give a 2 minute startup --- since a human will be in the room
int override_mode = 0;

// Keep the last 10 seconds time stamps
const int numReadings = 20;
unsigned long readings[numReadings];
int index = 0;

void setup() {

  pinMode(pin_input, INPUT);
  pinMode(pin_output, OUTPUT);
  //attachInterrupt(0, blink, FALLING);
  Serial.begin(9600);

  for (index = 0; index < numReadings; index++)
    readings[index] = 0.0;     

  Ethernet.begin(mac, ip, gateway); //start Ethernet
  delay(1000);   

  digitalWrite(pin_output, HIGH);
  Is_Power_On = 1;

  Serial.print("POWER:");
  Serial.println(Is_Power_On);

  if(override_time>0)
    override_mode=1;
  else
    override_mode=0;

}

void loop() {

  // remove jitter
  // delay(1);

  // read the input pin:
  int buttonState = digitalRead(pin_input);
  Serial.print(buttonState);

  // count pulses in the loop
  if ( buttonState != lastpulse ) {
    lastpulse = buttonState;

    if(lastpulse) {
      last_pulse_time=millis();

      readings[index]=last_pulse_time;
      index++;
      if (index >= numReadings) 
        index=0;
    }
  }

  current_time = millis();

  // check if the power should switch off
  if(current_time > (timeout_millisecs + last_pulse_time) ) {
    if (debugflow) Serial.print("ALERT. No flow for ");
    if (debugflow) Serial.print(timeout_millisecs);
    if (debugflow) Serial.print(" milliseconds. ");
    if (override_mode) {
      if (debugflow) {
        Serial.println("KILL SIGNAL SKIPPED)!");
      }
    } 
    else {
      digitalWrite(pin_output, LOW);
      if (debugflow) Serial.println("SEND KILL SIGNAL");
      if (Is_Power_On) {
        Is_Power_On = 0;
        sendmail();  
      }
    }
  }

  // ---- Hmmmmmmm this is not needed !!!!
  // if (current_time==10000) {
  //     Serial.println("sent email");   
  //     sendmail();
  //  }

  // report some interersting things
  if( (current_time / report_interval) > report_count) {
    if (debugflow) Serial.print("Time: ");
    if (debugflow) Serial.println(current_time);

    // Show the average for last readings
    double total = 0;
    int looper;

    if ( index != 0)
      total +=  readings[0] - readings[numReadings - 1];

    for ( looper = 1; looper < index; looper++)
      total += readings[looper] - readings[looper - 1];    

    for ( looper = index + 1; looper < numReadings; looper++)
      total += readings[looper] - readings[looper - 1];     

    Pulses_Per_Second = MilliPerSecond / (total / numReadings);

    if (debugflow) Serial.print("Average : ");
    if (debugflow) Serial.println(Pulses_Per_Second);

    Liter_per_minute = Pulses_Per_Second * 60 / 169;
    if (debugflow) Serial.print("Liter /pm: ");
    if (debugflow) Serial.println(Liter_per_minute);

    if (debugflow) Serial.print("POWER:");
    if (debugflow) Serial.println(Is_Power_On);

    // countdown the time for override mode
    if(override_mode) {
      override_time -= report_interval;
      if (debugflow) Serial.print("IN OVERRIDE MODE time left: ");
      if (debugflow) Serial.print(override_time); 
      if (debugflow) Serial.println(" milliseconds "); 
      if (override_time<0) {
        override_mode=0;
        override_time=0;
      }          
    }

    report_count++;
  }  

  handle_http();

}

void sendmail() {

  EthernetClient SMTPclient;

  if(!SMTPclient.connect(smtp_server, 25))
  {
    Serial.println("connection failed.");
    return;
  }

  Serial.println("Email Connected.");
  delay(10000);

  SMTPclient.println("ehlo douwedejong.com"); 
  delay(10000);

  SMTPclient.println("mail from:failsafe@douwedejong.com");
  SMTPclient.println("rcpt to:douwe.jong@gmail.com");

  SMTPclient.println("data");
  SMTPclient.println("from: failsafe@douwedejong.com");
  SMTPclient.println("to: douwe.jong@gmail.com");
  SMTPclient.println("subject: FLOW ALERT at JPH");
  SMTPclient.println("The power was switched off.");
  SMTPclient.println(".");
  SMTPclient.println("quit");

  delay(5000);

  Serial.println("Disconnecting.");
  SMTPclient.stop();
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
      while (client.available()) { // Receive client data

        Serial.print(".");
        c = client.read(); //read char by char HTTP request
        readString +=c;
        //Serial.print(c); //output chars to serial port

        // If first request upon connexion, the 3 first characters will be "GET"
        // If "GET" is caught, skip the request info
        if( readString.equals("GET")) {
          Serial.println("");
          c = client.read();
          c = client.read();
          c = client.read();
          if (c == 'X') 
            jsonresponse=1;
          else
            jsonresponse=0;
          Serial.print("JSON X=");
          Serial.print(jsonresponse);  
          Serial.println(", GET");
          break;
        }

        // Otherwise, if the request contains data,
        // the first characters will be "POST"
        // We then skip the request header and this "if" becomes our main function
        if( readString.equals("POST")) {
          Serial.println("");
          Serial.println("POST");
          // 320 is arbitrary. The actual length that has to be skipped depends on
          // several user settings ( browser, language, addons...)
          // the skipped length has not to be too long to skip relevant data
          // and not to short to waste computing time
          for(int i=0; i<320; i++) {
            c = client.read();
            Serial.print(c); // UNCOMMENT FOR DEBUG
          }

          //Searches for "Length: "
          while(c != 'L') {         
            c = client.read();
            Serial.print(c); // UNCOMMENT FOR DEBUG
          }

          // Skip "Length: "
          for (int i=0; i<7; i++)
          {
            c = client.read();
            Serial.print(c); // UNCOMMENT FOR DEBUG
          }

          // Read the data package length
          readString="";
          c = client.read();

          while(c != '\n')
          {
            readString += c;
            Serial.print(c);
            c = client.read();   
          }
          // convert data read from String to int
          readString.toCharArray(buffer, readString.length());
          dataLength = atoi(buffer);
          Serial.println("");
          Serial.print("dataLength: ");
          Serial.println(dataLength);

          // gets DATA
          client.read(); // skips additional newline
          client.read();
          for (int i=0; i<dataLength; i++) {
            data[i] = client.read();
          }

          Serial.println("");
          Serial.print("data: ");
          Serial.println(data);

          readString ="";
          int i = 0;
          while(data[i] != '=')
            i++;
          i++;

          Serial.println(data + i);

          override_time = atoi(data + i);
          if (override_time>150)
            override_time = 150;

          override_time *= 60000;
          Serial.print("Override period: ");
          Serial.println(override_time);

          if(override_time>0)
            override_mode=1;
          else
            override_mode=0;
        }
      }

      if (jsonresponse) {
        // JSON CODE

        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: application/json");
        client.println("Connection: close");
        client.println();
        client.print("{\"Power\":");
        client.print(Is_Power_On);
        client.print(",\"FlowPerSecond\":");
        client.print(Pulses_Per_Second);
        client.print(",\"LitersPerSecond\":");
        client.print(Liter_per_minute);
        client.print(",\"OverrideTime\":");
        client.print(override_time);
        client.print(",\"CurrentTime\":");
        client.print(current_time);
        client.print(",\"data\":[");

        int looper;          
        for ( looper = index; looper < numReadings; looper++) {
          client.print(readings[looper]);    
          client.print(",");
        }

        for ( looper = 0; looper < index; looper++) {
          client.print(readings[looper]);    
          if (looper < (index-1))
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
        //Serial.print(report_interval/1000*5);

        if (Is_Power_On)
          client.print("The power is ON");
        else
          client.print("The power is OFF");

        client.print("<br>Flow per second: ");
        client.print(Pulses_Per_Second);

        client.print("<br>Liters per minute: ");
        client.print(Liter_per_minute);

        if (override_time>0) {
          client.print("<br>Override mode, time remaining: ");
          client.print(override_time / 1000 / 60);
          client.print(":");
          int remainder = (override_time / 1000) % 60;
          if (remainder < 10)
            client.print("0");
          client.print(remainder);
          client.print("<br><br>*** WARNING - OVERRIDE MODE WILL NOT DISABLE POWER ***");
        } 
        else 
          client.print("<br>Monitoring mode");
        client.print("<br> ");

        client.println("<br><form name=input method=post>Set override minutes : ");
        client.println("<input type=\"text\" value=\"");
        //     client.print(override_time);
        client.print("\" name=\"override_time\" />");
        client.print(" (0=off,150=max) ");
        client.println("<input type=\"submit\" value=\"Reset\" />");                               
        client.println("</form> <br /> ");

        client.println("</html>");        
      }
      client.stop();
    }
  }

  reset_http_vals();
}







