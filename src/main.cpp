// An autopick arduino app for lotteries (includes 3 Oz lotteries).g fonts as they take a lot of space

// Author: Carlos Florez
// Date: 6th November, 2021

// Note. if memory becomes too tight, consider omitting/changing fonts as they take a lot of space

#include <Arduino.h>

// (see https://gist.github.com/jlesech/3089916 for how to use asserts with arduino environment

// caf: deal with the OLED functionality 
#include <U8x8lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);   

#define LED_PIN 6 // connect led to D6
#define BUZ_PIN 5 // buzzer pin
#define BTN_PIN 4
#define POT_PIN 14 // could say A0 instead of 14 - but could cause issues?

// make sure these match the indexes of the "menuOptions" array
#define POWERBALL 0
#define OZLOTTO 1
#define LOTTO 2
#define CUSTOM 3

int btnNew;
int btnOld;
/*
setOfNums declared as pointer since it will be dinamically allocated
in setup() and will also be used in loop()
*/
// used in both setup() and loop()
int *setOfNums; // to initially hold numbers in sequence (1,2,3...)
int newLength; // to keep the size of setOfNums[], which will dinamically expand/shrink

int menuSelection;
int maxNumber;
int gameSize;

int randomCell;
int pick;
int pickedSoFar = 0;

void fillInSequence(int startVal, int arr[], int upToNth);
void printArr(char* delimiter, int arr[], int upToNth);
int getRandomNum(int min,  int max);
int splice(int fromIdx, int arr[], int arrLength);

void drawMainMenu(char *choices[], int nChoices, int selection);
int myMap (int value, int fromLow, int fromHigh, int toLow, int toHigh);
void timedTone(int pin, int freq, int ms);
void pre(char *title);

void setup() {
  Serial.begin(9600); //Begin serial communication
  u8x8.begin(); //caf: setup OLED functionality
    
  pinMode(LED_PIN,OUTPUT);
  pinMode(BUZ_PIN,OUTPUT);
  pinMode(POT_PIN, INPUT);
  pinMode(BTN_PIN, INPUT);
    
  char *menuOptions[] = {"PowerBall", "OzLotto", "Lotto", "custom"};
  int nOptions = 4; // set to the number of items in menuOptions[]
  
  // get display ready for rendering of menu
  u8x8.setFlipMode(true); // Sets the rotation of the screen
  u8x8.setFont(u8x8_font_amstrad_cpc_extended_f);

  int potValue;
  // block to draw the main menu
  do {
    // dinamically map/decode a selection from the potentiometer
    potValue = analogRead(POT_PIN);
    menuSelection = myMap(potValue, 0, 1023, nOptions-1, 0);
    
    // draw the menu with the selected option highlighted(inverted)
    drawMainMenu(menuOptions, nOptions, menuSelection);
    
    delay(50);
    btnNew = digitalRead(BTN_PIN);
  } while (btnNew == LOW); // user has not entered a menu selection
  timedTone(BUZ_PIN, 5, 100); // bells and wissles for the button that was just pressed

  // at this point selection has been set (entered) by the user
  switch(menuSelection)
   {
     case POWERBALL :
       maxNumber = 35;
       gameSize = 8; // the 8th pick of the game -"the powerball" is different (range: 1-20)
       break;
     case OZLOTTO :
       maxNumber = 45;
       gameSize = 7; 
       break;
     case LOTTO :
       maxNumber = 45;
       gameSize = 6;
       break;
     case CUSTOM :
       // do nothing, if() statement below will take care
       break;  
    } 
  if (menuSelection == CUSTOM) {
    // Allow user to manually input values...
    // get from the user (using oled, pot & btn), how large is the pool of numbers to pick from
    pre("Max.Number?");
    // consider changing to number-only font (ending with"_n" and not "_r") if memory becomes too tight: 
    u8x8.setFont(u8x8_font_inb33_3x6_r);
    do {
      potValue= analogRead(POT_PIN);
      maxNumber = map(potValue, 0, 1023, 45, 1);
      u8x8.drawString(0, 2, u8x8_u16toa(maxNumber, 5)); // U8g2 Build-In functions
      btnNew = digitalRead(BTN_PIN);
    } while (btnNew == LOW); // user has not entered a value
    timedTone(BUZ_PIN, 5, 100); // bells and wissles for the button that was just pressed (also serves as delayed)
      
    
    // get from the user (using oled, pot & btn), the quantity of random numbers to generate
    pre("howMany in game?");
    u8x8.setFont(u8x8_font_inb33_3x6_r); 
    delay(300); // have shown new slctn. menu, this to allow some time in case user still long pressing btn.
      
    do {
      potValue= analogRead(POT_PIN);
      gameSize = map(potValue, 0, 1023, maxNumber, 1);
      u8x8.drawString(0, 2, u8x8_u16toa(gameSize, 5)); // U8g2 Build-In functions
      btnNew = digitalRead(BTN_PIN);
    } while (btnNew == LOW); // user has not entered a value
    timedTone(BUZ_PIN, 5, 100); // bells and wissles for the button that was just pressed (also serves as delayed)
  }

  randomSeed(analogRead(A2+A3));
  // allocate the memory for the set of numbers array  to be maxNumber long
  setOfNums = (int*)malloc(maxNumber * sizeof(int)); // sizeof(int) should return 1
  
  fillInSequence(1, setOfNums, maxNumber);
    
  // will be modifying the bounds of the array and need newLength initialized
  // before going into loop()
  newLength = maxNumber;

  Serial.print("max Number: ");
  Serial.println(maxNumber);

  Serial.print("game Size: ");
  Serial.println(gameSize);
  
  // get display ready - write header and set font - before going into loop().
  pre("Picks");
  u8x8.setFont(u8x8_font_inb33_3x6_r);
  delay(300); // have shown new slctn. menu, this to allow some time in case user still long pressing btn
}

void loop() {
  btnNew = digitalRead(BTN_PIN);
  if (btnOld==1 && btnNew==0) {
    // generate pick on toggle from high to low
    if (pickedSoFar >= gameSize) {
      // new game begins, reset pool of numbers to initial state
      fillInSequence(1, setOfNums, maxNumber);
      newLength = maxNumber;
      pickedSoFar = 0;
    }
    
    randomCell = getRandomNum(0, newLength-1);
    pick = setOfNums[randomCell];
    
    if (menuSelection == POWERBALL && pickedSoFar == gameSize-1) {
      // Overwrite the current pick as the powerball's last pick (called the powerball) is 
      // an exeption to the rule - its range is "1-20" and not "1-45" as other picks in game.
      pick =  getRandomNum(1, 20);
    }
        
    // bells and wisles
    digitalWrite(LED_PIN, HIGH);
    tone(BUZ_PIN, 5);

    // get oled ready and print to it
    // clear display row, so that no debris at the end of the incoming print if is shorter than previous
    u8x8.setCursor(0, 2);
    u8x8.print("     "); // use as many spaces as chars that can fit horizontally for current font size
    u8x8.setCursor(0, 2);
    
    u8x8.setCursor(0, 2);
    if(pickedSoFar == 0) {
       u8x8.print("."); // leading '.' to identify the first picked number for the game
       Serial.print(".");
    }
    
    u8x8.print(pick);  // **display the picked number**
    
    Serial.println(pick);
    if(pickedSoFar == gameSize-1) {
      u8x8.print("."); // trailing '.' to identify the last picked number for the game
      Serial.print(".");
    }

    if (menuSelection == POWERBALL && pickedSoFar == gameSize-1) {
      // indicate is the powerball pick - also see other previous comments if not clear
      u8x8.print("PB"); // make sure font being used includes alphabetic characters
      pick =  getRandomNum(1, 20);
    }
    
    // remove the last pick (from arr), so no chance it can be picked again  
    newLength = splice(randomCell, setOfNums, newLength);
    pickedSoFar++;
    delay(100);
  }else {
    digitalWrite(LED_PIN, LOW);
    noTone(BUZ_PIN);//Sets the voltage to low and makes no noise
  }
  btnOld = btnNew;
}

int splice(int fromIdx, int arr[], int arrLength) {
   // should assert(length > fromIdx),  and assert both are +ve
   // but don't know how-to use asserts in arduino environment 

   if (fromIdx >= arrLength || arrLength == 0 || fromIdx < 0) {
    return -1;
   }
      
   int i = fromIdx;
   while (i < arrLength-1) {
     arr[i] = arr[i+1];
     i++;
   }
   return arrLength-1;
}

int getRandomNum(int min,  int max) {
   return random(min, max+1);
}

void fillInSequence(int startVal, int arr[], int upToNth) {
  // should verify that startVal is < upToNth and inform
   int i = 0;
   while (i < upToNth) {
      arr[i] = i + startVal;
      i++;
   }
}

void printArr(char* delimiter, int arr[], int upToNth) {
  int i = 0;
   while (i < upToNth) {
    Serial.print(arr[i]);
    Serial.print(delimiter);
    i++;
   }
   Serial.println();
}

// expects an integer *followed by a newLine character*
int serialInputInt(String prompt) {
  int readInt;
  Serial.println(prompt);
  while (Serial.available() == 0) {
      // keep waiting for input
  }
  
  readInt = Serial.parseInt();
  Serial.read();  // flush out the new line character to not interfere with future read().
  return readInt; 
}

void drawMainMenu(char *choices[], int nChoices, int selection) {
  int i = 0;
  int row = 0;
  while (i < nChoices) {
    if (i == selection) {
      u8x8.inverse();
      u8x8.draw1x2String(0, row, choices[i]);
      u8x8.noInverse();
    } else {
      u8x8.draw1x2String(0, row, choices[i]);
    }
    row = row + 2; // increasing by 2 since using '1x2' version of drawString
    i++;
  }
}
/*
 the built-in  map() function has an issue where one extreme (the one that is mapped from 0)
 value bounces, so using one extra value in the mapped range above and also the if() below.
 TODO - toLow can be either greater or lower than toHigh, need to deal with this
      - as of now only dealing when it is greater as is needed in this program  
*/
int myMap (int value, int fromLow, int fromHigh, int toLow, int toHigh) {
  int mappedVal;
  
  toLow++; // this won't work in all cases,  see TODO in function info.
    
  mappedVal =  map(value, fromLow, fromHigh, toLow, toHigh);
    
    if (mappedVal !=  0) {
      mappedVal--;
    }
    return mappedVal;
}


void pre(char *title)
{
  u8x8.setFont(u8x8_font_amstrad_cpc_extended_f); 
  u8x8.clear(); 
  u8x8.inverse();
  u8x8.print(title);
  u8x8.noInverse();
  u8x8.setCursor(0,1);
}

void timedTone(int pin, int freq, int ms) {
   tone(pin, freq);
   delay(ms);
   noTone(pin);
}
