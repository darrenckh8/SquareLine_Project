#include <bsec2.h>

#include <lvgl.h>
#include <TFT_eSPI.h>
#include <ui.h>

#define DEVICE "ESP32-S3-W1C-N16R8"

#include <Wire.h>
#define I2C_SDA 1
#define I2C_SCL 2

#include <WiFiMulti.h>
WiFiMulti wifiMulti;

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

// WiFi AP SSID
#define WIFI_SSID "Darren"
// WiFi password
#define WIFI_PASSWORD "12332108"

#define INFLUXDB_URL "https://us-east-1-1.aws.cloud2.influxdata.com"
#define INFLUXDB_TOKEN "AWKX8Z9uTsheP2D3j-Gq9CqVwKO9apgaRzIbDkMJQEofHLh0cWgW2Fl5kFaLdT5RyLRA8TlbqWLAuUMyfP3u5g=="
#define INFLUXDB_ORG "c8c34fdffd2d0640"
#define INFLUXDB_BUCKET "BME680"

// Declare InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
// Declare Data point
Point sensor("Parameters");

/**
 * @brief : This function checks the BSEC status, prints the respective error code. Halts in case of error
 * @param[in] bsec  : Bsec2 class object
 */
void checkBsecStatus(Bsec2 bsec);

/**
 * @brief : This function is called by the BSEC library when a new output is available
 * @param[in] input     : BME68X sensor data before processing
 * @param[in] outputs   : Processed BSEC BSEC output data
 * @param[in] bsec      : Instance of BSEC2 calling the callback
 */
void newDataCallback(const bme68xData data, const bsecOutputs outputs, Bsec2 bsec);

/* Create an object of the class Bsec2 */
Bsec2 envSensor;

/*Don't forget to set Sketchbook location in File/Preferences to the path of your UI project (the parent foder of this INO file)*/

/*Change to your screen resolution*/
static const uint16_t screenWidth = 480;
static const uint16_t screenHeight = 320;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight); /* TFT instance */

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}

/* Entry point for the example */
void setup(void) {
  /* Desired subscription list of BSEC2 outputs */
  bsecSensor sensorList[] = {
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_RAW_TEMPERATURE,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_HUMIDITY,
  };

  /* Initialize the communication interfaces */
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  /* Valid for boards with USB-COM. Wait until the port is open */
  while (!Serial) delay(10);

  /* Initialize the library and interfaces */
  if (!envSensor.begin(BME68X_I2C_ADDR_LOW, Wire)) {
    checkBsecStatus(envSensor);
  }

  /* Subsribe to the desired BSEC2 outputs */
  if (!envSensor.updateSubscription(sensorList, ARRAY_LEN(sensorList), BSEC_SAMPLE_RATE_LP)) {
    checkBsecStatus(envSensor);
  }

  /* Whenever new data is available call the newDataCallback function */
  envSensor.attachCallback(newDataCallback);

  lv_init();

  tft.begin();        /* TFT init */
  tft.setRotation(1); /* Landscape orientation, flipped */

  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  ui_init();

  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  sensor.addTag("device", DEVICE);
  sensor.addTag("SSID", WiFi.SSID());

  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

/* Function that is looped forever */
void loop(void) {
  /* Call the run function often so that the library can 
     * check if it is time to read new data from the sensor  
     * and process it.
     */

  if (!envSensor.run()) {
    checkBsecStatus(envSensor);
  }

  lv_timer_handler();

  // Check WiFi connection and reconnect if needed
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }
}

void newDataCallback(const bme68xData data, const bsecOutputs outputs, Bsec2 bsec) {
  if (!outputs.nOutputs) {
    return;
  }

  for (uint8_t i = 0; i < outputs.nOutputs; i++) {
    const bsecData output = outputs.output[i];
    switch (output.sensor_id) {
      case BSEC_OUTPUT_IAQ:
        //Serial.println("IAQ: " + String(output.signal));
        //Serial.println("IAQ Accuracy: " + String((int)output.accuracy));
        // Handle IAQ data
        sensor.addField("IAQ", output.signal);
        sensor.addField("Accuracy", int(output.accuracy));
        lv_arc_set_value(ui_Arc4, output.signal);
        char AQIString[30];  // Adjust the size as needed
        sprintf(AQIString, "AQI: %.2f", output.signal);
        lv_label_set_text(ui_Label4, AQIString);
        break;

      case BSEC_OUTPUT_STATIC_IAQ:
        //Serial.println("Static IAQ: " + String(output.signal));
        // Handle Static IAQ data
        sensor.addField("IAQ Static", output.signal);
        break;

      case BSEC_OUTPUT_CO2_EQUIVALENT:
        //Serial.println("CO2 Equivalent: " + String(output.signal));
        // Handle CO2 Equivalent data
        sensor.addField("CO2", output.signal);
        lv_arc_set_value(ui_Arc1, output.signal);
        char CO2String[30];  // Adjust the size as needed
        sprintf(CO2String, "CO2: %.2fPPM", output.signal);
        lv_label_set_text(ui_Label1, CO2String);
        break;

      case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
        //Serial.println("Breath VOC Equivalent: " + String(output.signal));
        // Handle Breath VOC Equivalent data
        sensor.addField("VOC", output.signal);
        lv_arc_set_value(ui_Arc5, output.signal);
        char VOCString[30];  // Adjust the size as needed
        sprintf(VOCString, "VOC: %.2fPPM", output.signal);
        lv_label_set_text(ui_Label5, VOCString);
        break;

      case BSEC_OUTPUT_RAW_TEMPERATURE:
        //Serial.println("Raw Temperature: " + String(output.signal));
        // Handle Raw Temperature data
        sensor.addField("Temperature", output.signal);
        lv_arc_set_value(ui_Arc2, output.signal);
        char temperatureString[30];  // Adjust the size as needed
        sprintf(temperatureString, "TEMP: %.2f°C", output.signal);
        lv_label_set_text(ui_Label2, temperatureString);
        break;

      case BSEC_OUTPUT_RAW_PRESSURE:
        //Serial.println("Raw Pressure: " + String(output.signal));
        // Handle Raw Pressure data
        sensor.addField("Pressure", output.signal / 100);
        lv_arc_set_value(ui_Arc6, output.signal / 100);
        char PRESString[30];  // Adjust the size as needed
        sprintf(PRESString, "Pressure: \n%.2fhPA", output.signal / 100);
        lv_label_set_text(ui_Label6, PRESString);
        break;

      case BSEC_OUTPUT_RAW_HUMIDITY:
        //Serial.println("Raw Humidity: " + String(output.signal));
        // Handle Raw Humidity data
        sensor.addField("Humidity", output.signal);
        lv_arc_set_value(ui_Arc3, output.signal);
        char HUMIDString[30];  // Adjust the size as needed
        sprintf(HUMIDString, "Humidity: %.2f%%", output.signal);
        lv_label_set_text(ui_Label3, HUMIDString);
        break;

      default:
        break;
    }
  }

  if (!client.writePoint(sensor)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  // // Print what are we exactly writing
  // Serial.print("Writing: ");
  // Serial.println(sensor.toLineProtocol());
  //Serial.println("Values Updated");
  sensor.clearFields();
}

void checkBsecStatus(Bsec2 bsec) {
  if (bsec.status < BSEC_OK) {
    Serial.println("BSEC error code : " + String(bsec.status));

  } else if (bsec.status > BSEC_OK) {
    Serial.println("BSEC warning code : " + String(bsec.status));
  }

  if (bsec.sensor.status < BME68X_OK) {
    Serial.println("BME68X error code : " + String(bsec.sensor.status));

  } else if (bsec.sensor.status > BME68X_OK) {
    Serial.println("BME68X warning code : " + String(bsec.sensor.status));
  }
}
