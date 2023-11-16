//includes
//////////
#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include "CWSFFileDB.h"



#define HARDWARE_SDCARD_MISO 13
#define HARDWARE_SDCARD_MOSI 23
#define HARDWARE_SDCARD_SCK  2 //bad choice!
#define HARDWARE_SDCARD_CS   22

#define HARDWARE_MAX_FILES   15



//variables
///////////
SPIClass g_spiSD(VSPI);  


void setup() 
{
  // put your setup code here, to run once:
  Serial.begin(115200);

  Serial.println(F("Init SDCARD"));
    
  g_spiSD.begin(HARDWARE_SDCARD_SCK, HARDWARE_SDCARD_MISO, HARDWARE_SDCARD_MOSI, HARDWARE_SDCARD_CS);
  
  if(SD.begin(HARDWARE_SDCARD_CS, g_spiSD, 4000000, "/sd", HARDWARE_MAX_FILES, false) == false)
  {
    Serial.println(F("Failed to mount SD Card"));
  };


  //remove old table
  SD.remove("/data.tbl");


  //create the table definition:
  //3 Fields, 2 are from the size of an uint32_t (4 byte) and one 25 byte field
  //the data can be anything...
  int nFields[] = {sizeof(uint32_t), sizeof(uint32_t), 25};

  
  CWSFFileDB db((fs::SDFS*)&SD, "/data.tbl", (int*)&nFields, 3, true);

  if(db.open() == true)
  {
    Serial.println(F("DB Open!"));

    //insert data
    /////////////
    void *pInsert[4];
    
    char szTest[26];
    uint32_t dwTest1 = 123456;
    uint32_t dwTest2 = 654432;
    
    for(int n = 0; n <= 25; ++n)
    {
      sprintf(szTest, "test %i\0", n);
      
      pInsert[0] = (void*)&dwTest1;
      pInsert[1] = (void*)&dwTest2;
      pInsert[2] = (void*)&szTest;
      
      db.insertData(pInsert);
    };



    Serial.println(F("\n\n\n Read Demo:"));
    
    //create the recordset 
    CWSFFileDBRecordset rs(&db);
  

    //get & set data
    do
    {
      //prepare data (zero everything)
      memset(szTest, 0, sizeof(szTest));
      dwTest1 = 0;
      dwTest2 = 0;

      //read the data
      rs.getData(0, (void*)&dwTest1, sizeof(dwTest1));
      rs.getData(1, (void*)&dwTest2, sizeof(dwTest2));
      rs.getData(2, (void*)&szTest, sizeof(szTest));

      
      Serial.print("dw1: ");
      Serial.print(dwTest1);
      
      Serial.print(" dw2: ");
      Serial.print(dwTest2);
      
      Serial.print(" sz: ");
      Serial.println(szTest);

      //set everything to other values
      memset(szTest, 0, sizeof(szTest));
      strcpy(szTest, "test123update");
      dwTest1 = 654321;
      dwTest2 = 987654;
      
      rs.setData(0, (void*)&dwTest1, sizeof(dwTest1));
      rs.setData(1, (void*)&dwTest2, sizeof(dwTest2));
      rs.setData(2, (void*)&szTest, strlen(szTest) + 1); 
     
    } while(rs.moveNext() == true);

    Serial.println("Move First...");
    rs.moveFirst();



    Serial.println(F("\n\n\n Read Demo (after update):"));
    
    do
    {
      memset(szTest, 0, sizeof(szTest));
      dwTest1 = 0;
      dwTest2 = 0;

      rs.getData(0, (void*)&dwTest1, sizeof(dwTest1));
      rs.getData(1, (void*)&dwTest2, sizeof(dwTest2));
      rs.getData(2, (void*)&szTest, sizeof(szTest));
      
      Serial.print("dw1: ");
      Serial.print(dwTest1);
      
      Serial.print(" dw2: ");
      Serial.print(dwTest2);
      
      Serial.print(" sz: ");
      Serial.println(szTest);

    } while(rs.moveNext() == true);


    Serial.println("Move First...");
    rs.moveFirst();

    Serial.println(F("\n\n\n Remove every 2nd entry:"));
    int nPos = 0;
    
    do
    {
      if((++nPos % 2) == 0)
      {
        rs.remove();
      }

    } while(rs.moveNext() == true);



    Serial.println("Move First...");
    rs.moveFirst();

    Serial.println(F("\n\n\n Data after Remove:"));
    
    do
    {
      memset(szTest, 0, sizeof(szTest));
      dwTest1 = 0;
      dwTest2 = 0;

      rs.getData(0, (void*)&dwTest1, sizeof(dwTest1));
      rs.getData(1, (void*)&dwTest2, sizeof(dwTest2));
      rs.getData(2, (void*)&szTest, sizeof(szTest));
      
      Serial.print("dw1: ");
      Serial.print(dwTest1);
      
      Serial.print(" dw2: ");
      Serial.print(dwTest2);
      
      Serial.print(" sz: ");
      Serial.println(szTest);

    } while(rs.moveNext() == true);


    Serial.println(F("\n\n\n Insert after Remove:"));

    
    sprintf(szTest, "test %i\0", 1);
    dwTest1 = 0;
    dwTest2 = 0;
      
    pInsert[0] = (void*)&dwTest1;
    pInsert[1] = (void*)&dwTest2;
    pInsert[2] = (void*)&szTest;
    
    db.insertData(pInsert);
    db.insertData(pInsert);
    db.insertData(pInsert);
    db.insertData(pInsert);
    db.insertData(pInsert);
    db.insertData(pInsert);


    Serial.println(F("\n\n\n Read new data:"));

    rs.moveFirst();
    
    do
    {
      memset(szTest, 0, sizeof(szTest));
      dwTest1 = 0;
      dwTest2 = 0;

      rs.getData(0, (void*)&dwTest1, sizeof(dwTest1));
      rs.getData(1, (void*)&dwTest2, sizeof(dwTest2));
      rs.getData(2, (void*)&szTest, sizeof(szTest));
      
      Serial.print("dw1: ");
      Serial.print(dwTest1);
      
      Serial.print(" dw2: ");
      Serial.print(dwTest2);
      
      Serial.print(" sz: ");
      Serial.println(szTest);

    } while(rs.moveNext() == true);
  };
}

void loop() {
  // put your main code here, to run repeatedly:

}
