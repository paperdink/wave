
#ifndef GUI_H
#define GUI_H

#define TASKS_BASE_X 20
#define TASKS_BASE_Y 25

#define DATE_BASE_X 180
#define DATE_BASE_Y 50
#define DATE_WIDTH 200
#define DATE_HEIGHT 200

#define WEATHER_BASE_X 280
#define WEATHER_BASE_Y 200

void drawBitmapFrom_SD_ToBuffer(GxEPD_Class* display, fs::FS &fs, const char *filename, int16_t x, int16_t y, bool with_color);
void display_tasks(GxEPD_Class* display);
void diplay_date(GxEPD_Class* display);
void display_weather(GxEPD_Class* display);
#endif /* GUI_H */
