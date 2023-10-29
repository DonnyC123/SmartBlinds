#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

#include <Arduino.h>
#include <SPIFFS.h>
#include <FS.h>

extern File fs_upload_file;

bool handleFileRead(String path);
void handleFileDelete();
void handleFileCreate();
void handleFileList();
void handleNotFound();
void handleFileUpload();
uint8_t startSPIFFS();

#endif