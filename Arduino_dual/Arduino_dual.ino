#include "Ethernet.h"
#include <SPI.h>

// Upload to Arduin Duemilanova w/ Atmega328

int debugflow = 1;                    // Are we debugging flow via serial interface
int debughttp = 1;                    // Are we debugging http via serial interfaces

const int numCircuits = 2;            // number of water sensors connected
const int numReadings = 25;           // number of pulses to check for flow calculation

int pin_input[] = {
  5, 7};             // On what pins are the water seonsors connected
double pulses_per_liter[] = {169,492};
                                      // Number of pulses per liter water
int pin_output = 6;                   // On what pin is the circuit breaker connected

long override_time= 120000;  // Give a 2 minute startup
int override_mode = 0;                // Are we monitoring the flow
int Is_Power_On = 0;                  // Keep the state of the power breaker

int lastpulse[numCircuits];           // Was the last pulse high or low
unsigned long last_pulse_time[numCircuits];
                                      // When was the last pulse
unsigned long dymmy;
unsigned long readings[numReadings][numCircuits];
                                      // Array to keep pin readings to calculate flow
int index[numCircuits];               // keep track where we are in the Array
double Pulses_Per_Second[numCircuits];// The calculated pulses per second
double Liter_per_minute[numCircuits]; // The calculated liters per minute
int valuesreset[numCircuits];         // Indicate if values are reset

unsigned long current_time;           // get the current system time
unsigned long timeout_millisecs = 5000;
                                      // Nr of miliseconds of no flow before activating circuit breakers

const double MilliPerSecond = 1000;   // a field to force caculcation as a double
unsigned long report_count = 0;       // the last time that a caulcation for flow was ade
int report_interval = 1000;           // Nr of miliseconds to calculate flow numbers

// Ethernet library configuration
byte mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; //physical mac address
byte ip[] = { 
  192, 168, 0, 8 }; // ip in lan
byte gateway[] = { 
  192, 168, 0, 1 }; // internet access via router
byte smtp_server[] = { 
  192, 168, 0, 5}; // douwedejong.com

// HMTL processing variables
EthernetServer server(80); //server port
String readString = ""; //string to get incoming data
char c;
char buffer[10];
int dataLength =0;
// Buffer containing processed hmtl data
char data[50];
char datamotor[5];

int jsonresponse = 0;

void setup() {

  for (int z = 0; z < numCircuits; z++) {
    pinMode(pin_input[z], INPUT);
    lastpulse[z]=0;
    last_pulse_time[z]=0;
    index[z]=0;
    valuesreset[z]=0;
  }
  pinMode(pin_output, OUTPUT);

  //attachInterrupt(0, blink, FALLING);
  Serial.begin(9600);

  for (int z = 0; z < numCircuits; z++)
    for (int x = 0; x < numReadings; x++)
      readings[x][z] = 0.0;     

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

  // read the input pin:
  for (int z = 0; z < numCircuits; z++) {
    int buttonState = digitalRead(pin_input[z]);
    //Serial.print(z);
    //Serial.print(">");
    //Serial.print(buttonState);
    //Serial.print(" ");

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

  // check if the power should switch off  (for now only check first loop)
  //  for (int z = 0; z < numCircuits; z++) {
    if(current_time > (timeout_millisecs + last_pulse_time[0]) ) {
      if (debugflow) Serial.print("[");
      if (debugflow) Serial.print(0);
      if (debugflow) Serial.print("] ");
      if (debugflow) Serial.print("ALERT. No flow for ");
      if (debugflow) Serial.print(timeout_millisecs);
      if (debugflow) Serial.print(" milliseconds. ");
      if (override_mode) {
        if (debugflow) Serial.println("KILL SIGNAL SKIPPED)!");
      } 
      else {
        digitalWrite(pin_output, LOW);
        if (debugflow) Serial.println("SEND KILL SIGNAL");
        if (Is_Power_On) {
          Is_Power_On = 0;
        }
      }
    }
  //}

  // report some interersting things
  if( (current_time / report_interval) > report_count) {
    if (debugflow) Serial.print("Time: ");
    if (debugflow) Serial.println(current_time);

    for (int z = 0; z < numCircuits; z++) {

      // Show the average for last readings
      double total = 0;
      int looper;

      if ( index[z] != 0)
        total +=  readings[0][z] - readings[numReadings - 1][z];

      for ( looper = 1; looper < index[z]; looper++)
        total += readings[looper][z] - readings[looper - 1][z];    

      for ( looper = index[z] + 1; looper < numReadings; looper++)
        total += readings[looper][z] - readings[looper - 1][z];     

      Pulses_Per_Second[z] = MilliPerSecond / (total / (numReadings-1));

      if (debugflow) Serial.print("[");
      if (debugflow) Serial.print(z);
      if (debugflow) Serial.print("] ");     
      if (debugflow) Serial.print("Average : ");
      if (debugflow) Serial.println(Pulses_Per_Second[z]);

      Liter_per_minute[z] = Pulses_Per_Second[z] * 60 / pulses_per_liter[z];
      if (debugflow) Serial.print("[");
      if (debugflow) Serial.print(z);
      if (debugflow) Serial.print("] ");      
      if (debugflow) Serial.print("Liter /pm: ");
      if (debugflow) Serial.println(Liter_per_minute[z]);
    }

    if (debugflow) Serial.print("    POWER:");
    if (debugflow) Serial.println(Is_Power_On);

    // countdown the time for override mode
    if(override_mode) {
      override_time -= report_interval;
      if (debugflow) Serial.print("    IN OVERRIDE MODE time left: ");
      if (debugflow) Serial.print(override_time); 
      if (debugflow) Serial.println(" milliseconds "); 
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
        if (debugflow) Serial.print("[");
        if (debugflow) Serial.print(z);
        if (debugflow) Serial.print("] ");     
        if (debugflow) Serial.println(" (values reset)");
      }
      else {
        if (valuesreset[z]) {
          if((current_time - last_pulse_time[z]) < (2 * timeout_millisecs) ) {
            valuesreset[z]=0;
            if (debugflow) Serial.print("[");
            if (debugflow) Serial.print(z);
            if (debugflow) Serial.print("] ");     
            if (debugflow) Serial.println(" (flow detected)");          
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
      while (client.available()) { // Receive client data

        if (debughttp) Serial.print(".");
        c = client.read(); //read char by char HTTP request
        readString +=c;
        //Serial.print(c); //output chars to serial port

        // If first request upon connexion, the 3 first characters will be "GET"
        // If "GET" is caught, skip the request info
        if( readString.equals("GET")) {
          if (debughttp) Serial.println("");
          c = client.read();
          c = client.read();
          c = client.read();
          if (c == 'X' || c == '1' ) 
            jsonresponse=1;
          else {
            if ((c == 'Y' || c == '2') && numCircuits > 1) 
              jsonresponse=2;
            else {
              if ((c == 'Z' || c =='3') && numCircuits > 2)                 
                jsonresponse=3;
              else
                jsonresponse=0;
            }
          }
          if (debughttp) Serial.print("JSON=");
          if (debughttp) Serial.print(jsonresponse);  
          if (debughttp) Serial.println(", GET");
          break;
        }

        // Otherwise, if the request contains data,
        // the first characters will be "POST"
        // We then skip the request header and this "if" becomes our main function
        if( readString.equals("POST")) {
          if (debughttp) Serial.println("");
          if (debughttp) Serial.println("POST");
          // 320 is arbitrary. The actual length that has to be skipped depends on
          // several user settings ( browser, language, addons...)
          // the skipped length has not to be too long to skip relevant data
          // and not to short to waste computing time
          for(int i=0; i<320; i++) {
            c = client.read();
            if (debughttp) Serial.print(c); // UNCOMMENT FOR DEBUG
          }

          //Searches for "Length: "
          while(c != 'L') {         
            c = client.read();
            if (debughttp) Serial.print(c); // UNCOMMENT FOR DEBUG
          }

          // Skip "Length: "
          for (int i=0; i<7; i++)
          {
            c = client.read();
            if (debughttp) Serial.print(c); // UNCOMMENT FOR DEBUG
          }

          // Read the data package length
          readString="";
          c = client.read();

          while(c != '\n')
          {
            readString += c;
            if (debughttp) Serial.print(c);
            c = client.read();   
          }
          // convert data read from String to int
          readString.toCharArray(buffer, readString.length());
          dataLength = atoi(buffer);
          if (debughttp) Serial.println("");
          if (debughttp) Serial.print("dataLength: ");
          if (debughttp) Serial.println(dataLength);

          // gets DATA
          client.read(); // skips additional newline
          client.read();
          for (int i=0; i<dataLength; i++) {
            data[i] = client.read();
          }

          if (debughttp) Serial.println("");
          if (debughttp) Serial.print("data: ");
          if (debughttp) Serial.println(data);

          readString ="";
          int i = 0;
          while(data[i] != '=')
            i++;
          i++;

          if (debughttp) Serial.println(data + i);

          override_time = atoi(data + i);
          if (override_time>150)
            override_time = 150;

          override_time *= 60000;
          if (debughttp) Serial.print("Override period: ");
          if (debughttp) Serial.println(override_time);

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
        client.print(",\"Loop\":");
        client.print(jsonresponse-1);
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
        //        client.println("HTTP/1.1 200 OK");
        //        client.println("<meta http-equiv=\"Content-Type\" content=\"text/html;charset=ISO-8859-8\">"); 
        client.print("<meta http-equiv=\"refresh\" content=\"");
        client.print(report_interval/1000*5);
        client.println("\">");
        client.println("<html>");
        //Serial.print(report_interval/1000*5);

        if (Is_Power_On)
          client.print("The power is ON");
        else
          client.print("The power is OFF");

        for (int z = 0; z < numCircuits; z++) {
          client.print("<br>[");
          client.print(z);
          client.print("] ");
          client.print("Flow per second: ");
          client.print(Pulses_Per_Second[z]);

          if (valuesreset[z]>=1)
            client.print(" (values reset)");  
          client.print("<br>[");
          client.print(z);
          client.print("] ");
          client.print("Liters per minute: ");
          client.print(Liter_per_minute[z]);
          if (valuesreset[z]>=1)
            client.print(" (values reset)");  
        }    

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








