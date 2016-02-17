#ifndef MyTypes_h
#define MyTypes_h

typedef struct{
  byte storedCard[4];    // Stores an ID read from EEPROM
  int pwLength;
  char pw[27];
}record;

#endif

