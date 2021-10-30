
#include <SD.h>
#include <SPI.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "config.h"
#include "gui.h"
#include "date_time.h"
#include "custom_parser.h"

static const uint16_t input_buffer_pixels = 20; // may affect performance
static const uint16_t max_palette_pixels = 256; // for depth <= 8
uint8_t input_buffer[3 * input_buffer_pixels]; // up to depth 24
uint8_t mono_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 b/w
uint8_t color_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 c/w

const char* server = "api.todoist.com";  // Server URL
const char* url = "/rest/v1/tasks";
char tasks[MAX_TASKS][MAX_TODO_STR_LENGTH+1];
uint8_t task_count;
TodoJsonListener todo_listener;

const char* openweathermap_link = "http://api.openweathermap.org/data/2.5/forecast?q="CITY","COUNTRY"&appid="OWM_ID;
char weather_string[10];
WeatherJsonListener weather_listener;

WiFiClientSecure client;

int8_t FetchTODO(){
  //TODO: Handle failures
  String response;
  ArudinoStreamParser parser;
  
  parser.setListener(&todo_listener);

  // Example of how to make a HTTP request with more control
  DEBUG.println("Starting connection to server...");
  if (!client.connect(server, 443)){
    DEBUG.println("Connection failed!");
    return -1;
  } else {
    DEBUG.println("Connected to server!");
    // Make a HTTP request:
    client.print("GET ");
    client.print(url);
    client.println(" HTTP/1.0");
    
    client.print("Host: ");
    client.println(server);
    
    client.print("Authorization: Bearer ");
    client.println(TODOIST_TOKEN);
    
    client.println("Connection: close");
    
    client.println();

    /* Get the headers */
    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        break;
      }
    }

     while (client.connected()) {
        while (client.available()) {
          char c = client.read();
          //file.write(c);
          //DEBUG.write(c);
          parser.parse(c);
        }
    }
    
    client.stop();
    return 0;
  }
}

void display_tasks(GxEPD_Class* display){
  // TODO: Optimize strings allocations
  int16_t  x, y;
  uint16_t w, h;
  uint8_t prev_height = 8;
  int8_t ret;
  
  ret = FetchTODO();
  
  // Draw background
  display->fillRect(TASKS_BASE_X, TASKS_BASE_Y, 140, 250, GxEPD_WHITE);
  display->drawRect(TASKS_BASE_X, TASKS_BASE_Y, 140, 250, GxEPD_BLACK);
  
  display->setFont(MED_FONT);
  display->setTextColor(GxEPD_BLACK);
  display->setTextSize(1);

  display->getTextBounds(F("To-Do List"), 0, 0, &x, &y, &w, &h);
  display->setCursor(TASKS_BASE_X+5, TASKS_BASE_Y+prev_height+h);
  prev_height += h;
  display->println(F("To-Do List"));

  display->setFont(SMALL_FONT);
  display->setTextColor(GxEPD_BLACK);
  display->setTextSize(1);
  display->getTextBounds(F("item"), 0, 0, &x, &y, &w, &h);

  // Display tasks
  if(!ret){
    for(int i = 0; i < task_count; i++){
      display->setCursor(TASKS_BASE_X+5, TASKS_BASE_Y+prev_height+(h+7)*i+20);
      DEBUG.printf("Task: %s\n", tasks[i]);
      display->println((char*)tasks[i]);
    }
  }
}

void diplay_date(GxEPD_Class* display){
  int16_t  x, y;
  uint16_t w, h;
  uint8_t prev_height = 15;
  char date_str[7];
  
  // Time background
  display->fillRect(DATE_BASE_X, DATE_BASE_Y, DATE_WIDTH, DATE_HEIGHT, GxEPD_WHITE);
  display->drawRect(DATE_BASE_X, DATE_BASE_Y, DATE_WIDTH, DATE_HEIGHT, GxEPD_BLACK);

  display->setFont(LARGE_FONT);
  display->setTextColor(GxEPD_BLACK);
  display->setTextSize(1);
  
  display->getTextBounds(now.wday, 0, 0, &x, &y, &w, &h);
  display->setCursor(DATE_BASE_X+(DATE_WIDTH/2)-(w/2), DATE_BASE_Y+prev_height+h);
  prev_height += h;
  display->println(now.wday);

  sprintf(date_str, "%d %s", now.mday, now.month);
  display->getTextBounds(date_str, 0, 0, &x, &y, &w, &h);
  display->setCursor(DATE_BASE_X+(DATE_WIDTH/2)-(w/2), DATE_BASE_Y+prev_height+h+25);
  prev_height += h+25;
  display->println(date_str);
}

void display_weather(GxEPD_Class* display){
  int16_t  x, y;
  uint16_t w, h;
  HTTPClient http;
  ArudinoStreamParser parser;
  
  parser.setListener(&weather_listener);
  
  http.begin(openweathermap_link);
  int httpCode = http.GET();
  
  if (httpCode > 0) { //Check for the return code
          // Parser updates weather_string directly
          http.writeToStream(&parser);
  } else {
        DEBUG.println("Error on HTTP request");
        http.end();
        return;
  }
  
  DEBUG.print("It is ");
  DEBUG.print(weather_string);
  DEBUG.println(" outside!");
  
  display->setFont(MED_FONT);
  display->getTextBounds(F("clouds"), 0, 0, &x, &y, &w, &h);
  display->setCursor(WEATHER_BASE_X-(w/2), WEATHER_BASE_Y+h);
  display->println(weather_string);
}

uint16_t read16(File& f)
{
  // BMP data is stored little-endian, same as Arduino.
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File& f)
{
  // BMP data is stored little-endian, same as Arduino.
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

void drawBitmapFrom_SD_ToBuffer(GxEPD_Class* display, fs::FS &fs, const char *filename, int16_t x, int16_t y, bool with_color)
{
  File file;
  bool valid = false; // valid format to be handled
  bool flip = true; // bitmap is stored bottom-to-top
  uint32_t startTime = millis();
  if ((x >= display->width()) || (y >= display->height())) return;
  DEBUG.println();
  DEBUG.print("Loading image '");
  DEBUG.print(filename);
  DEBUG.println('\'');

  file = fs.open(String("/") + filename, FILE_READ);
  if (!file)
  {
    DEBUG.print("File not found");
    return;
  }

  // Parse BMP header
  if (read16(file) == 0x4D42) // BMP signature
  {
    uint32_t fileSize = read32(file);
    uint32_t creatorBytes = read32(file);
    uint32_t imageOffset = read32(file); // Start of image data
    uint32_t headerSize = read32(file);
    uint32_t width  = read32(file);
    uint32_t height = read32(file);
    uint16_t planes = read16(file);
    uint16_t depth = read16(file); // bits per pixel
    uint32_t format = read32(file);
    if ((planes == 1) && ((format == 0) || (format == 3))) // uncompressed is handled, 565 also
    {
      DEBUG.print("File size: "); DEBUG.println(fileSize);
      DEBUG.print("Image Offset: "); DEBUG.println(imageOffset);
      DEBUG.print("Header size: "); DEBUG.println(headerSize);
      DEBUG.print("Bit Depth: "); DEBUG.println(depth);
      DEBUG.print("Image size: ");
      DEBUG.print(width);
      DEBUG.print('x');
      DEBUG.println(height);
      // BMP rows are padded (if needed) to 4-byte boundary
      uint32_t rowSize = (width * depth / 8 + 3) & ~3;
      if (depth < 8) rowSize = ((width * depth + 8 - depth) / 8 + 3) & ~3;
      if (height < 0)
      {
        height = -height;
        flip = false;
      }
      uint16_t w = width;
      uint16_t h = height;
      if ((x + w - 1) >= display->width())  w = display->width()  - x;
      if ((y + h - 1) >= display->height()) h = display->height() - y;
      valid = true;
      uint8_t bitmask = 0xFF;
      uint8_t bitshift = 8 - depth;
      uint16_t red, green, blue;
      bool whitish, colored;
      if (depth == 1) with_color = false;
      if (depth <= 8)
      {
        if (depth < 8) bitmask >>= depth;
        file.seek(54); //palette is always @ 54
        for (uint16_t pn = 0; pn < (1 << depth); pn++)
        {
          blue  = file.read();
          green = file.read();
          red   = file.read();
          file.read();
          whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
          colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
          if (0 == pn % 8) mono_palette_buffer[pn / 8] = 0;
          mono_palette_buffer[pn / 8] |= whitish << pn % 8;
          if (0 == pn % 8) color_palette_buffer[pn / 8] = 0;
          color_palette_buffer[pn / 8] |= colored << pn % 8;
        }
      }
      //display->fillScreen(GxEPD_WHITE);
      uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;
      for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) // for each line
      {
        uint32_t in_remain = rowSize;
        uint32_t in_idx = 0;
        uint32_t in_bytes = 0;
        uint8_t in_byte = 0; // for depth <= 8
        uint8_t in_bits = 0; // for depth <= 8
        uint16_t color = GxEPD_BLACK;
        file.seek(rowPosition);
        for (uint16_t col = 0; col < w; col++) // for each pixel
        {
          // Time to read more pixel data?
          if (in_idx >= in_bytes) // ok, exact match for 24bit also (size IS multiple of 3)
          {
            in_bytes = file.read(input_buffer, in_remain > sizeof(input_buffer) ? sizeof(input_buffer) : in_remain);
            in_remain -= in_bytes;
            in_idx = 0;
          }
          switch (depth)
          {
            case 24:
              blue = input_buffer[in_idx++];
              green = input_buffer[in_idx++];
              red = input_buffer[in_idx++];
              whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
              colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
              break;
            case 16:
              {
                uint8_t lsb = input_buffer[in_idx++];
                uint8_t msb = input_buffer[in_idx++];
                if (format == 0) // 555
                {
                  blue  = (lsb & 0x1F) << 3;
                  green = ((msb & 0x03) << 6) | ((lsb & 0xE0) >> 2);
                  red   = (msb & 0x7C) << 1;
                }
                else // 565
                {
                  blue  = (lsb & 0x1F) << 3;
                  green = ((msb & 0x07) << 5) | ((lsb & 0xE0) >> 3);
                  red   = (msb & 0xF8);
                }
                whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
                colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
              }
              break;
            case 1:
            case 4:
            case 8:
              {
                if (0 == in_bits)
                {
                  in_byte = input_buffer[in_idx++];
                  in_bits = 8;
                }
                uint16_t pn = (in_byte >> bitshift) & bitmask;
                whitish = mono_palette_buffer[pn / 8] & (0x1 << pn % 8);
                colored = color_palette_buffer[pn / 8] & (0x1 << pn % 8);
                in_byte <<= depth;
                in_bits -= depth;
              }
              break;
          }
          if (whitish)
          {
            color = GxEPD_WHITE;
          }
          else if (colored && with_color)
          {
            color = GxEPD_RED;
          }
          else
          {
            color = GxEPD_BLACK;
          }
          uint16_t yrow = y + (flip ? h - row - 1 : row);
          display->drawPixel(x + col, yrow, color);
        } // end pixel
      } // end line
      DEBUG.print("loaded in "); DEBUG.print(millis() - startTime); DEBUG.println(" ms");
    }
  }
  file.close();
  if (!valid)
  {
    DEBUG.println("bitmap format not handled.");
  }
}
