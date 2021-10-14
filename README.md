# ESP8266_DHT22
ESP8266 with DHT22 Temperature and Humidity Sensor

Microcontroller stores device info on SQL database for looking up device specific data, as well as logging when errors occur / tasks are completed
Stores temperature and Humidity in an influx time series database
Includes HTTPserver to handle updating via internal network

Required Libraries:
  adafruit/Adafruit Unified Sensor
  adafruit/DHT sensor library
  chuckbell/MySQL Connector Arduino
