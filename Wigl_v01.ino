#define LOG_OUT 1 // use the log output function
#define FHT_N 256 // set to 256 point fht

#include <FHT.h> // include the library

#define DEBUG 0

//PINOUT//
//motors, 4 DI/DO pins, 2 PWM pins
#define motor1ENPIN 5  //PWM
#define motor2ENPIN 6  //PWM
#define motor1aPIN 7
#define motor2aPIN 8
#define motor3aPIN 12
#define motor4aPIN 13
//RGB LEDs
#define redPin A3
#define greenPin A4
#define bluePin A5
//audio pins
#define audioPin A0

//DELAYS and SPEEDS
#define delayMsWiggle 500  // ms delay between wiggle rotations
#define delayMsProgTurn 860  //programspeedMed ms delay for turning (try to approximate 90 degrees with this), settled on 850
#define delayMsProgMove 860  //programspeedMed ms delay for forward movement
#define speedSlow 100
#define speedMed 200  //changed from 100

//software hack to deal with bad motor alignment!
//with current wiring (2014-10-14, left is motor2en, right is motor1en
//2015-02-07 - changed back to 200/200
#define speedLeft      200
#define speedRight     200
#define speedLeftSlow  90
#define speedRightSlow 90

//1 = normal movement mode
//0 = programming mode
boolean auto_move = true;

//programming mode variables
#define historyElements 20
char history[historyElements] = {};

//FHT variables//

#define numNotes 9
int binArray[numNotes] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
int binMargin = 1;

int binLow = 0;
int binHigh = 0;

//Sound Processing//
#define numNotes 9
#define SAMPLE_RATE_HZ 4780  //at delayus of 100

//880Hz A                   D5, E5,  F4,  G5,  A5,  B5,  C6,   D6,   E6
int freqArray[numNotes] = {587, 659, 740, 784, 880, 987, 1047, 1175, 1319};

//440Hz A                   D4, E4,  F4,  G4,  A4,  B4,  C5,   D5,   E5
//int freqArray[numNotes] = {294, 329, 349, 392, 440, 494, 524, 587, 660};
//220Hz A                   D3, E3,  F3,  G3,  A3,  B3,  C4,   D4,   E4
//int freqArray[numNotes] = {147, 165, 175, 196, 220, 247, 262, 294, 329};

int minMag = 58;
boolean isENote = false;
boolean isFNote = false;
boolean isGNote = false;
boolean isANote = false;
boolean isBNote = false;
boolean isCNote = false;
boolean isDNote = false;
unsigned long silenceCounter = 0;  //container for timing the tone
unsigned long silenceLimit = 2;  //for 328p, set to 1-2
/////////////////////////////////////////////////
//songs
char lionSleeps[15] = {'G', 'A', 'B', 'A', 'B',
                       'C', 'B', 'A', 'G', 'A',
                       'B', 'A', 'G', 'B', 'A'
                      };
boolean bLionSleeps = false;

char lionSleeps2[15] = {'D', 'B', 'A', 'B', 'D',
                        'C', 'B', 'A', 'G', 'A',
                        'B', 'A', 'G', 'B', 'A'
                       };
boolean bLionSleeps2 = false;
//////////////////////////////////////////////////

void setup() {

  if (DEBUG)
    Serial.begin(115200); // use the serial port

  //RGB LEDs
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  //motors
  pinMode(motor1ENPIN, OUTPUT);
  pinMode(motor2ENPIN, OUTPUT);
  pinMode(motor1aPIN, OUTPUT);
  pinMode(motor2aPIN, OUTPUT);
  pinMode(motor3aPIN, OUTPUT);
  pinMode(motor4aPIN, OUTPUT);

  //FHT PROCESSING
  //convert freqArray to binArray
  for (int i = 0; i < numNotes; i++)
  {
    binArray[i] = frequencyToBin(freqArray[i]);
    if (DEBUG) {
      Serial.print(binArray[i]);
      Serial.print(" ");
    }
  }
  //assign binHigh and binLow
  binLow = binArray[0] - binMargin;
  binHigh = binArray[numNotes - 1] + binMargin;
  if (DEBUG) {
    Serial.print("\n");
    Serial.print("binLow: ");
    Serial.print(binLow);
    Serial.print(", binHigh: ");
    Serial.println(binHigh);
  }
  //END of FHT processing//
  //initialize history array
  for (int i = 0; i < historyElements; i++)
    history[i] = 'Z';

  blinkWhiteLED(2, 200);
}

void loop() {

  processFFT();
  calcMaxMag();

}

int frequencyToBin(float frequency) {
  float binFrequency = float(SAMPLE_RATE_HZ) / float(FHT_N);
  return int(frequency / binFrequency);
}

void processFFT()
{
  for (int i = 0 ; i < FHT_N ; i++) // save 256 samples
  {
    fht_input[i] = analogRead(audioPin) * 2; // put real data into bins
    delayMicroseconds(100);  //possible delay to decrease sampling time, 100, was 200
  }
  fht_window(); // window the data for better frequency response
  fht_reorder(); // reorder the data before doing the fht
  fht_run(); // process the data in the fht
  fht_mag_log(); // take the output of the fht
  //uncomment if using Processing App to view data
  //Serial.write(255); // send a start byte
  //Serial.write(fht_log_out, FHT_N/2); // send out the data
}

void calcMaxMag()
{
  int j = 0;  //the max magnitude
  int k = 0;  //the bin that carries the max magnitude
  int l = 0;  //average magnitude

  for (int i = binLow; i < binHigh; i++)
  {
    if (fht_log_out[i] > j)
    {
      j = fht_log_out[i]; //calculate the maximum magnitude
      k = i;              //find the bin with the maximum magnitude
    }
    l += fht_log_out[i];
  }

  l = l / (binHigh - binLow - 1); //calculate the average magnitude

  if (DEBUG) {
    Serial.print("Avg Mag: ");
    Serial.print(l);
    Serial.print("Max Mag: ");
    Serial.print(j);
    Serial.print(" Bin: ");
    Serial.print(k);
    Serial.print(" binLow: ");
    Serial.print(binLow);
    Serial.print(", binHigh: ");
    Serial.print(binHigh);
    Serial.print("\n");
  }

  //FILTERS!!!//
  if (j > minMag)  //(A) minimum volume necessary
  {
    if (((j - fht_log_out[k - 1]) < l) || ((j - fht_log_out[k + 1]) < l)) //(B) check for staccato
    {
      for (int i = 0; i < numNotes; i++)
        if ((k >= binArray[i] - binMargin) && (k <= binArray[i] + binMargin))
          toneDetected(i);
    }
  }
  else  //treat the sound as silence
  {
    silenceCounter++;
    if (silenceCounter > silenceLimit)
    {
      stopM();
      setRGBLED(0, 0, 0); //turn off
      isENote = false;
      isFNote = false;
      isGNote = false;
      isANote = false;
      isBNote = false;
      isCNote = false;
      isDNote = false;
      silenceCounter = 0;
    }
  }
}

void toneDetected(int note)
{
  //reset silence
  silenceCounter = 0;

  switch (note) {
    /*case 0:
      Serial.println("D5");
      setRGBLED(0, 0, 255); // blue
      break;
    case 1:
      Serial.println(F("E5"));
      setRGBLED(255, 255, 255); // white
      if(!isENote)
      {
        isENote = true;
        history_push('E');
        history_print();
        history_check();
      }
      break;*/
    case 2:
      Serial.println(F("F#5"));
      setRGBLED(0, 255, 255); // aqua
      if (isENote || isGNote || isANote || isBNote || isCNote || isDNote)
        //if transitioning from a different note
      {
        stopM();          //stop motors
        isENote = false;  //clear out all other confirmations of notes
        isGNote = false;
        isANote = false;
        isBNote = false;
        isCNote = false;
        isDNote = false;
      }
      if (!isFNote)       //check to see if note has already been confirmed, if not, continue
      {
        isFNote = true;
        if (auto_move)
          //swingLeft();
          history_push('F');
        history_print();
        history_check();
      }
      break;
    case 3:
      Serial.println(F("G5"));
      setRGBLED(255, 0, 255); // purple
      if (isENote || isFNote || isANote || isBNote || isCNote || isDNote)
      {
        stopM();
        isENote = false;
        isFNote = false;
        isANote = false;
        isBNote = false;
        isCNote = false;
        isDNote = false;
      }
      if (!isGNote)
      {
        isGNote = true;
        if (auto_move)
          moveBackward();
        history_push('G');
        history_print();
        history_check();
      }

      break;
    case 4:
      Serial.println(F("A5"));
      setRGBLED(0, 255, 0); // green
      if (isENote || isFNote || isGNote || isBNote || isCNote || isDNote)
      {
        stopM();
        isENote = false;
        isFNote = false;
        isGNote = false;
        isBNote = false;
        isCNote = false;
        isDNote = false;
      }
      if (!isANote)
      {
        isANote = true;
        if (auto_move)
          moveForward();
        history_push('A');
        history_print();
        history_check();
      }

      break;
    case 5:
      Serial.println(F("B5"));
      setRGBLED(255, 0, 0); // red
      if (isENote || isFNote || isGNote || isANote || isCNote || isDNote)
      {
        stopM();
        isENote = false;
        isFNote = false;
        isGNote = false;
        isANote = false;
        isCNote = false;
        isDNote = false;
      }
      if (!isBNote)
      {
        isBNote = true;
        if (auto_move)
          turnRight();
        history_push('B');
        history_print();
        history_check();
      }
      break;
    case 6:
      Serial.println(F("C6"));
      setRGBLED(255, 255, 0); // yellow
      if (isENote || isFNote || isGNote || isBNote || isANote || isDNote)
      {
        stopM();
        isENote = false;
        isFNote = false;
        isGNote = false;
        isBNote = false;
        isANote = false;
        isDNote = false;
      }
      if (!isCNote)
      {
        isCNote = true;
        if (auto_move)
          turnLeft();
        history_push('C');
        history_print();
        history_check();
      }

      break;
    case 7:
      Serial.println(F("D6"));
      setRGBLED(0, 0, 255);  //blue
      if (isENote || isFNote || isGNote || isBNote || isCNote || isANote)
      {
        stopM();
        isENote = false;
        isFNote = false;
        isGNote = false;
        isBNote = false;
        isCNote = false;
        isANote = false;
      }
      if (!isDNote)
      {
        isDNote = true;
        history_push('D');
        history_print();
        history_check();
      }
      break;
    case 8:
      Serial.println("E6");
      setRGBLED(128, 128, 128); //gray
      break;
  }
}

void setRGBLED(int red, int green, int blue)
{
  analogWrite(redPin, red);
  analogWrite(greenPin, green);
  analogWrite(bluePin, blue);
}

void blinkWhiteLED(int n, int blinkDelay)
{
  for (int i = 0; i < n; i++)
  {
    setRGBLED(255, 255, 255); // white
    delay(blinkDelay);
    setRGBLED(0, 0, 0); // off
    delay(blinkDelay);
  }
}

void stopM()
{
  analogWrite(motor1ENPIN, 0);
  analogWrite(motor2ENPIN, 0);
  digitalWrite(motor1aPIN, LOW);
  digitalWrite(motor2aPIN, LOW);
  digitalWrite(motor3aPIN, LOW);
  digitalWrite(motor4aPIN, LOW);
}

void moveForward()
{
  //set direction forward
  digitalWrite(motor3aPIN, HIGH);
  digitalWrite(motor4aPIN, LOW);
  
  digitalWrite(motor1aPIN, HIGH);
  digitalWrite(motor2aPIN, LOW);

  

  //enable
  analogWrite(motor1ENPIN, speedRight);
  analogWrite(motor2ENPIN, speedLeft);
}

void moveBackward()
{
  //set direction backward
  digitalWrite(motor3aPIN, LOW);
  digitalWrite(motor4aPIN, HIGH);
  
  digitalWrite(motor1aPIN, LOW);
  digitalWrite(motor2aPIN, HIGH);

  

  //enable
  analogWrite(motor1ENPIN, speedRight+30);
  analogWrite(motor2ENPIN, speedLeft);
}

void turnRight()
{
  //set direction right
  digitalWrite(motor1aPIN, LOW);
  digitalWrite(motor2aPIN, HIGH);

  digitalWrite(motor3aPIN, HIGH);
  digitalWrite(motor4aPIN, LOW);

  //enable
  analogWrite(motor1ENPIN, speedRightSlow);
  analogWrite(motor2ENPIN, speedLeftSlow);
}

void turnLeft()
{
  //set direction left
  digitalWrite(motor1aPIN, HIGH);
  digitalWrite(motor2aPIN, LOW);

  digitalWrite(motor3aPIN, LOW);
  digitalWrite(motor4aPIN, HIGH);

  //enable
  analogWrite(motor1ENPIN, speedRightSlow);
  analogWrite(motor2ENPIN, speedLeftSlow);
}

void wiggle(int n)
{
  //do the wiggle n times
  for (int i = 0; i < n; i++)
  {
    turnRight();
    delay(delayMsWiggle);
    stopM();
    turnLeft();
    delay(delayMsWiggle);
    stopM();
  }
  stopM();
}

void history_push(char push)
{
  for (int i = historyElements; i > 0; i--) //shift history to the right
    history[i] = history[i - 1];
  history[0] = push;  //add new note to history
}

void history_print()
{
  if (DEBUG) {
    for (int i = 0; i < historyElements; i++)
    {
      Serial.print(history[i]);
      Serial.print(F(" "));
    }
    Serial.print(F("\n"));
  }
}

void history_check()
{
  //special move #0  -   MODE CHANGE
  if (history[0] == 'C')
    if (history[1] == 'C')
      if (history[2] == 'C')
        if (history[3] == 'C')
          if (history[4] == 'C')
            if (history[5] == 'C')
            {
              stopM();
              blinkWhiteLED(3, 200);
              auto_move = !auto_move;
              if (DEBUG) {
                Serial.print(F("Change Mode, Mode = "));
                Serial.print(auto_move);
                Serial.print(F("\n"));
              }
              history_clear();
            }

  if (auto_move)  //mode = 1, free-run mode
  {
    //special move #1  -  Lion Sleeps Tonight
    bLionSleeps = true;  //assume true, see if any mismatch proves me wrong
    for (int i = 0; i < sizeof(lionSleeps); i++)
    {
      if (history[i] != lionSleeps[sizeof(lionSleeps) - (i + 1)])
      {
        bLionSleeps = false;
      }
      else if (DEBUG)
        Serial.println(F("Lion Note Match"));
    }

    if (DEBUG) {
      Serial.print(F("Lion check: "));
      Serial.print(bLionSleeps);
      Serial.print(F("\n"));
    }

    if (bLionSleeps == true)
    {
      stopM();          //turn off motor, bug-fix
      delay(100);  //let the last LED stay on for a bit
      setRGBLED(0, 0, 0); //clear LED, bug-fix

      if (DEBUG)
        Serial.println(F("Lion Sleeps Tonight!"));

      wiggle(3);
      blinkWhiteLED(6, 100);
      history_clear();
    }
  }

  if (!auto_move)  //mode = 0, programming mode
  {
    if (history[0] == 'D')  //ENTER KEY
    {
      moveProgram();
      history_clear();
    }
  }
}

void history_clear()
{
  for (int i = 0; i < historyElements; i++)
    history[i] = 'Z';
  bLionSleeps = false;
  bLionSleeps2 = false;
}

void moveProgram()
{
  //read history and execute movements
  for (int i = historyElements; i > 0; i--)
  {
    if (history[i] == 'A')
    {
      if (DEBUG)
        Serial.println(F("PROGRAMMED MOVE FORWARD"));

      moveForward();
      delay(delayMsProgMove);
      stopM();
    }
    if (history[i] == 'C')
    {
      if (DEBUG)
        Serial.println(F("PROGRAMMED MOVE LEFT"));

      turnLeft();
      delay(delayMsProgTurn);
      stopM();
    }
    if (history[i] == 'B')
    {
      if (DEBUG)
        Serial.println(F("PROGRAMMED MOVE RIGHT"));

      turnRight();
      delay(delayMsProgTurn);
      stopM();
    }

    if (history[i] == 'G')
    {
      if (DEBUG)
        Serial.println(F("PROGRAMMED MOVE BACK"));

      moveBackward();
      delay(delayMsProgMove);
      stopM();
    }
  }
  history_clear();
}

