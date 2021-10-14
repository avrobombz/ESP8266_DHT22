#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <SPI.h>
#include <DHT.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>

#define DHTPIN 5    //pin 5 for MF,BU
#define DHTTYPE DHT22   
DHT dht(DHTPIN, DHTTYPE); 

//*************************************************
const char* host = "esp8266-BU"; //device host name
const char* location = "office"; //device location
//Sensor Calibration
const float t_cal = 0; //temperature offset
const float h_cal = 0; //humidity offset
//*************************************************

//SQL VARIABLES-DECLARATIONS
    char insert[128];
    char qiexist[128];
    char qip[128];
    char qhost[128];
    char qloc[128];
    char ins_ip[128];
    char ins_host[128];
    char ins_loc[128];
    char ins_log[128];
    char mac[23];
    char buffip[17];
    String scheckip;
    String scheckhost;
    String scheckloc;
    char checkip[17];
    char checkhost[128];
    char checkloc[128];
    row_values *row = NULL;
    int id = 0;
    char* data;
    const char* server = "10.10.20.10"; //influxdb server ip
    unsigned long pre = 0;
    const long inter = 420000;
    float hum;  //Stores humidity value
    float temp; //Stores temperature value

//DECLARE FUNCTION-VARIABLES
HTTPClient client;
WiFiClient wificlient;
WiFiClient wific;
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;
MySQL_Connection conn(&wificlient);
MySQL_Cursor* cursor;

//SQL CREDENTIALS
IPAddress server_addr(10,10,20,10); //MySQL/MariaDB ip address
char db[] = ""; //database to use
char user[] = "";  //db username
char sqlpass[] = ""; //db password

//WIFI CREDENTIALS
const char* ssid = ""; //WIFI SSID
const char* password = ""; //WIFI Password

void setup() {

Serial.begin(115200);
Serial.println();

//START DHT SENSOR
dht.begin();
delay(100);

//WIFI
WiFi.hostname(host);
WiFi.mode(WIFI_STA);
WiFi.begin(ssid, password);

while(WiFi.waitForConnectResult() != WL_CONNECTED){
WiFi.begin(ssid, password);
Serial.println("WiFi failed, retrying.");
}

MDNS.begin(host);

httpUpdater.setup(&httpServer);
httpServer.begin();

MDNS.addService("http", "tcp", 80);
Serial.printf("HTTPUpdateServer ready! Open http://%s.home/update in your browser\n", host);
Serial.println("");
Serial.print("Connected to ");
Serial.println(ssid);
Serial.print("IP address: ");
Serial.println(WiFi.localIP());
Serial.print("Connecting to SQL...  ");
while (conn.connect(server_addr, 3306, user, sqlpass, db) != true) { //connect to SQL Server
  delay(10000);
  Serial.print(".");
}

//SQL INSERTS
char INSERT_SQL1[] = "INSERT INTO main.IOT (HOST,IP,MAC_ADD,LOCATION) VALUES ('%s','%s','%s','%s')";
char UPDATE_SQL2[] = "UPDATE main.IOT SET IP='%s' WHERE ID= %d";
char UPDATE_SQL3[] = "UPDATE main.IOT SET HOST='%s' WHERE ID= %d";
char UPDATE_SQL4[] = "UPDATE main.IOT SET LOCATION='%s' WHERE ID= %d";

//LOG INSERTS
char LOG_SQL1[] = "INSERT INTO main.Logs (DEVICE_ID,APP_NAME,LOG_LEVEL,LOG_TYPE,STRING)	VALUES ('%s','ESP_TH_LOG','%s','%s','%s')";

//SQL QUERIES
char QUERY_SQL1[] = "SELECT ID FROM main.IOT WHERE MAC_ADD = '%s'";
char QUERY_SQL2[] = "SELECT IP FROM main.IOT WHERE MAC_ADD = '%s'";
char QUERY_SQL3[] = "SELECT HOST  FROM main.IOT WHERE MAC_ADD = '%s'";
char QUERY_SQL4[] = "SELECT LOCATION FROM main.IOT WHERE MAC_ADD = '%s'";

//IOT VARIABLES - DATA CLEANUP
IPAddress ip(WiFi.localIP());
sprintf(buffip,"%d.%d.%d.%d",ip[0], ip[1], ip[2], ip[3]);
String smac = WiFi.macAddress();
smac.toCharArray(mac,23);

//DEBUG
// Serial.println("");
// Serial.println(buffip);
// Serial.println(host);
// Serial.println(location);
// Serial.println(mac);

//QUERY BUILDER

//inserts
sprintf(insert,INSERT_SQL1,host,buffip,mac,location);

//queries
sprintf(qiexist,QUERY_SQL1,mac);
sprintf(qip,QUERY_SQL2,mac);
sprintf(qhost,QUERY_SQL3,mac);
sprintf(qloc,QUERY_SQL4,mac);
  
//DEBUG
// Serial.println(insert); 
// Serial.println(qiexist);
// Serial.println(qip);
// Serial.println(qhost);
// Serial.println(qloc);

//STARTUP QUERIES
cursor = new MySQL_Cursor(&conn);

if (conn.connected()) { //check if i exist
      cursor->execute(qiexist);
      column_names *columns = cursor->get_columns();

      do {
        row = cursor->get_next_row();
        if (row != NULL) {
          id = atol(row->values[0]);
        }
      } while (row != NULL);
    }

if (id > 0) { //post message if i exist
  Serial.println("I exist in DB");
  Serial.print("ID: ");
  Serial.println(id);
}

if (id == 0) { //if ID doesn't exist create new row
  Serial.println("I dont exist in DB");
  cursor = new MySQL_Cursor(&conn);
  if (conn.connected()) {
      cursor->execute(insert);
      sprintf(ins_log,LOG_SQL1,host,"MED","INFO","New IOT Host created"); //(DEVICE_ID,APP_NAME,LOG_LEVEL,LOG_TYPE,STRING)
      cursor->execute(ins_log);

      cursor->execute(qiexist);
      column_names *columns = cursor->get_columns();

      do {
        row = cursor->get_next_row();
        if (row != NULL) {
          id = atol(row->values[0]);
        }
      } while (row != NULL);
    delete cursor;
      Serial.println("I created myself in DB");
      Serial.print("ID: ");
      Serial.println(id);
  }
}

if (conn.connected()) { //IP check
    cursor = new MySQL_Cursor(&conn);
    cursor->execute(qip);
    column_names *columns = cursor->get_columns();
    do {
      row = cursor->get_next_row();
      if (row != NULL) {
        scheckip = (row->values[0]);
      }
    } while (row != NULL);
}

if (conn.connected()) { //host check
    cursor->execute(qhost);
    column_names *columns = cursor->get_columns();
    do {
      row = cursor->get_next_row();
      if (row != NULL) {
        scheckhost = (row->values[0]);
      }
    } while (row != NULL);
}

if (conn.connected()) { //location check
      cursor->execute(qloc);
      column_names *columns = cursor->get_columns();
      do {
        row = cursor->get_next_row();
        if (row != NULL) {
          scheckloc = (row->values[0]);
        }
      } while (row != NULL);
  }

//convert strings to char
scheckip.toCharArray(checkip,17);
scheckhost.toCharArray(checkhost,128);
scheckloc.toCharArray(checkloc,128);
delete cursor;

//DEBUG sql output
  // Serial.println(id);
  // Serial.println(checkip);
  // Serial.println(checkhost);
  // Serial.println(checkloc);

//UPATE QUERIES BUILDER
sprintf(ins_ip,UPDATE_SQL2,buffip,id);
sprintf(ins_host,UPDATE_SQL3,host,id);
sprintf(ins_loc,UPDATE_SQL4,location,id);

//DEBUG
// Serial.println(ins_ip);
// Serial.println(ins_host);
// Serial.println(ins_loc);

//changes
if(strcmp(buffip, checkip) == 0) {//compare DB IP with Device IP
  Serial.println("IP is correct");
}
else {
  Serial.println("IP is incorrect, sending update");
  if (id > 0) { //update if ID exists
      cursor = new MySQL_Cursor(&conn);
      if (conn.connected()) {
        cursor->execute(ins_ip);
        sprintf(ins_log,LOG_SQL1,host,"MED","WARN","IP has changed"); //(DEVICE_ID,APP_NAME,LOG_LEVEL,LOG_TYPE,STRING)
        cursor->execute(ins_log);
  delete cursor;
      }
    }     
  }
if(strcmp(host, checkhost) == 0) {//compare DB HOST with Device HOST
  Serial.println("Host is correct");
}
else {
  Serial.println("Host is incorrect, sending update");
  if (id > 0) { //update if ID exists
      cursor = new MySQL_Cursor(&conn);
      if (conn.connected()) {
        cursor->execute(ins_host);
        sprintf(ins_log,LOG_SQL1,host,"MED","WARN","Host has changed"); //(DEVICE_ID,APP_NAME,LOG_LEVEL,LOG_TYPE,STRING)
        cursor->execute(ins_log);
  delete cursor;
      }
    }     
  }
if(strcmp(location, checkloc) == 0) {//compare DB Location with Device Location
  Serial.println("Location is correct");
}
else {
  Serial.println("Location is incorrect, sending update");
  if (id > 0) { //update if ID exists
      cursor = new MySQL_Cursor(&conn);
      if (conn.connected()) {
        cursor->execute(ins_loc);
        sprintf(ins_log,LOG_SQL1,host,"MED","WARN","Location has changed"); //(DEVICE_ID,APP_NAME,LOG_LEVEL,LOG_TYPE,STRING)
        cursor->execute(ins_log);
  delete cursor;
      }
    }     
  }

conn.close();

}

void influxtemp(float temp, float hum){

  conn.connect(server_addr, 3306, user, sqlpass, db); //connect to SQL Server
  char LOG_SQL1[] = "INSERT INTO main.Logs (DEVICE_ID,APP_NAME,LOG_LEVEL,LOG_TYPE,STRING)	VALUES ('%s','ESP_TH_LOG','%s','%s','%s')";
  char string_msg[128];
  char cdata[128];

  String data = "sensors,host=";
    data += host;
    data += ",location=";
    data += location;
    data += " temp=";
    data += temp;
    data += ",hum=";
    data += hum;
  
  //client.begin(server, 8086, "/write?db=htemp"); //dbtest = test environment, htemp = prod
  client.begin(wific, server, 8086, "/write?db=htemp");  
  Serial.println(data);
  client.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int httpcode = client.POST(data);
  client.end();
  Serial.println(httpcode);

    if(httpcode != 204){
        
        cursor = new MySQL_Cursor(&conn);
        
            sprintf(string_msg, "Unable to write to InfluxDB, %d", httpcode);
            sprintf(ins_log,LOG_SQL1,host,"HIGH","ERROR",string_msg); //(DEVICE_ID,APP_NAME,LOG_LEVEL,LOG_TYPE,STRING)    
            
            if (conn.connected()) {         
                cursor->execute(ins_log);
            }
        delete cursor;
    }
    else{
        cursor = new MySQL_Cursor(&conn);        
        data.toCharArray(cdata,128);

            sprintf(string_msg, "Wrote to InfluxDB %d - %s", httpcode, cdata); 
            sprintf(ins_log,LOG_SQL1,host,"LOW","INFO",string_msg); //(DEVICE_ID,APP_NAME,LOG_LEVEL,LOG_TYPE,STRING)
            
            if (conn.connected()) {         
                cursor->execute(ins_log);
            }
        delete cursor;
    }
    
  conn.close();
}

void runtemps() {

  unsigned long current = millis();

  if(current - pre >= inter) {

    pre = current;
  
    hum = dht.readHumidity()+h_cal;
    temp = dht.readTemperature()+t_cal;

    while(isnan(hum)){
        hum = dht.readHumidity()+h_cal;
    }
    while(isnan(temp)){
        temp = dht.readTemperature()+t_cal;
    }

  
    influxtemp(temp,hum);
  }
}

void start_up() { //commands to run once at start
    static bool init = true;
  if (init){
      
    init = false;
    Serial.print("runonce: ");
    hum = dht.readHumidity()+h_cal;
    temp = dht.readTemperature()+t_cal;
    influxtemp(temp,hum);
    return;
  }
}

void loop() {
    httpServer.handleClient();
    start_up();
    runtemps();
}
