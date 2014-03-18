#include "Ethernet.h"
#include <SPI.h>
#include "Smtp_Service.h"  // http://www.jayconsystems.com/tutorial/Ethernet_Send_Mail/

int pin_input = 2;
int pin_output = 4;

unsigned long current_time;

// Ethernet library configuration
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; //physical mac address
byte ip[] = { 192, 168, 8, 11 }; // ip in lan
byte gateway[] = { 192, 168, 8, 1 }; // internet access via router

// EMAIL processing variables
byte smtp_server[] = { 192, 168, 8, 8}; // website.nl
const unsigned int SMTP_PORT = 587;
SmtpService smtp_service(smtp_server, SMTP_PORT);
char incString[250];
String domain = "website.com";
String login = "username=";  // http://www.motobit.com/util/base64-decoder-encoder.asp
String password = "password=";  // 
Email email;

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
int lastpulse = 0;
unsigned long last_pulse_time = 0;
unsigned long report_count = 0;
int report_interval = 1000;
unsigned long timeout_millisecs = 5000;
const double MilliPerSecond = 1000;
double Pulses_Per_Second;
double Liter_per_minute;
int Is_Power_On = 0;
    
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
  email.setDomain(domain);
  email.setLogin(login);
  email.setPassword(password);
   
  email.setFrom("failsafe@douwedejong.com");
  email.setTo("douwe.jong@gmail.com");
  email.setCc("");
  email.setSubject("FLOW ALERT at JPH!");
  email.setBody("The power was switched off.");
  
  digitalWrite(pin_output, HIGH);
  Is_Power_On = 1;
}

void loop() {

  // remove jitter
  // delay(1);
   
  // read the input pin:
  int buttonState = digitalRead(pin_input);

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

  // check if the light should switch off
  if(current_time > (timeout_millisecs + last_pulse_time) ) {
//  if (current_time == 7000) {
    digitalWrite(pin_output, LOW);
    Serial.print("FLOW ALERT. No flow for ");
    Serial.print(timeout_millisecs);
    Serial.println(" milliseconds. Sending kill signal!");
    if (Is_Power_On) {
        Is_Power_On = 0;
        smtp_service.send_email(email);  
    }
  }

  // report some interersting things
  if( (current_time / report_interval) > report_count) {
    Serial.print("Time: ");
    Serial.println(current_time);

    // Show the average for last readings
    double total = 0;
    int looper;

    if ( index != 0)
      total = total + readings[0] - readings[numReadings - 1];

    for ( looper = 1; looper < index; looper++)
      total = total + readings[looper] - readings[looper - 1];    

    for ( looper = index + 1; looper < numReadings; looper++)
      total = total + readings[looper] - readings[looper -1];     

    Pulses_Per_Second = MilliPerSecond / (total / numReadings);
    Serial.print("Average last readings: ");
    Serial.println(Pulses_Per_Second);

    Liter_per_minute = Pulses_Per_Second * 60 / 169;
    Serial.print("Liter per Minute: ");
    Serial.println(Liter_per_minute);

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

        Serial.print(".");
        c = client.read(); //read char by char HTTP request
        readString +=c;
        //Serial.print(c); //output chars to serial port

        // If first request upon connexion, the 3 first characters will be "GET"
        // If "GET" is caught, skip the request info
        if( readString.equals("GET")) {
          Serial.println("");
          Serial.println("GET caught, skipping request and printing HTML");
          break;
        }

        // Otherwise, if the request contains data,
        // the first characters will be "POST"
        // We then skip the request header and this "if" becomes our main function
        if( readString.equals("POST")) {
          Serial.println("");
          Serial.println("POST caught, skipping header and acquiring DATA");
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

          timeout_millisecs = atoi(data + i);
          Serial.print("New milliseconds: ");
          Serial.println(timeout_millisecs);
        }
      }

      // HTML CODE
      client.println("<!DOCTYPE html>");
      client.println("<html>");
/*
      client.println("<form name=input method=post>Timeout in milliseconds: ");
      client.println("<input type=\"text\" value=\"");
      client.print(timeout_millisecs);
      client.println("\" name=\"timeoutmilli\" />");
      client.println("<input type=\"submit\" value=\"Update timeout\" />");                               
      client.println("</form> <br /> ");
*/
      
      if (Is_Power_On)
        client.print("The power is ON.");
      else
        client.print("the power is OFF.");

      client.print("<br>Flow per second ");
      client.print(Pulses_Per_Second);
      
      client.print("<br>Liters per minute: ");
      client.print(Liter_per_minute);


      client.println("</html>");        
      client.stop();
    }
  }

  reset_http_vals();
}





