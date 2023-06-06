/*
  Het bereken van het maximale vermogen punt
  met een ISR van precies 1 sec voor het bij houden van de tijd.
  Gelogde waarde naar SD kaart geschreven
  
  PIB-2023 zonnepaneel opbrengst voorspeller
  Diede Hilhorst
*/

#include <SPI.h>
#include <Wire.h>
#include <SD.h>
#include <Adafruit_INA219.h>
#include <Adafruit_MCP4725.h>
#define led1 0
//#define led2 1

File myFile;
Adafruit_MCP4725 dac;
Adafruit_INA219 ina219;

uint16_t counter = 10; 
unsigned long starten;

char fileName[] = "KLANT:00000.txt";
int klantnummer;
float vermogen;
float current;
float spanning;
int temperatuur = A3;
float Vmp;

volatile uint16_t sampletijdsec;
volatile uint8_t seconden;
char recordArray[5][20];
char aRecord[20];
uint8_t recordNum;  
uint8_t charNum;

uint16_t sampletijd;
uint8_t dag, maand, jaar;
uint8_t uur, minuut;
uint8_t PaneelVmp = 12;
unsigned long waardeTijd;

#define ntc_pin A3         // Pin,to which the voltage divider is connected      // 5V for the voltage divider
#define nominal_resistance 90000       //Nominal resistance at 25⁰C
#define nominal_temeprature 25   // temperature for nominal resistance (almost always 25⁰ C)
#define samplingrate 5    // Number of samples
#define beta 3950  // The beta coefficient or the B value of the thermistor (usually 3000-4000) check the datasheet for the accurate value.
#define Rref 33000   //Value of  resistor used for the voltage divider 

void vermogenBerekenen(){
  waardeTijd = micros() - starten;
  vermogen += (current * spanning * (waardeTijd*pow(10,-6)));  
  starten = micros();
  spanning = (ina219.getShuntVoltage_mV()/1000) + ina219.getBusVoltage_V() - 0.008; //shunt moet naar V,  nu in mVwe
  current = ina219.getCurrent_mA()*0.0001; //Shunt = 1 Ohm 
  if(current < 0.003){current = 0.000;}
  if(spanning < 0){spanning = 0;} 
  VMPT();
}

void VMPT(){ //VMPT berekenen en spanning aanpassen 
  uint8_t VoltageControl;
  float voltage = ina219.getBusVoltage_V() + (ina219.getShuntVoltage_mV()/1000);
  if(Vmp > voltage){
    VoltageControl = true;
  }
  else if (Vmp < voltage){
    VoltageControl = false;
  }
  unsigned long begin = millis();
  //while bijven totdat spanning > Vmp of spanning < Vmp of langer in while dan 4 seconden
  while(1){
    switch(VoltageControl){
      case 0:
      counter++;
      dac.setVoltage(counter, false);
      break;

      case 1:
      counter--;
      dac.setVoltage(counter, false);
      break;     
    }

    voltage = ina219.getBusVoltage_V() + (ina219.getShuntVoltage_mV()/1000);
    if((VoltageControl == true && Vmp <= voltage) || (VoltageControl == false && Vmp >= voltage)){
      dac.setVoltage(counter-=8, false);
      break;
    }
    else if(millis() - begin > 4000){
     break;
    } 
  } 
  counter = min(max(counter, 1), 500);
  Vmp = PaneelVmp - ((temperatuurLezen() - 25)*0.0036*PaneelVmp);
}

int temperatuurLezen(){ //temperatuur berekenen van NTC
  float average = analogRead(temperatuur);
  average = 1023 / average - 1;
  average = Rref / average;
  float temperature;
  temperature = average / nominal_resistance;     // (R/Ro)
  temperature = log(temperature);                  // ln(R/Ro)
  temperature /= beta;                   // 1/B * ln(R/Ro)
  temperature += 1.0 / (nominal_temeprature + 273.15); // + (1/To)
  temperature = 1.0 / temperature;                 // Invert
  temperature -= 273.15; 
  return (round(temperature));
}

void GegevensNaarInt(){
    myFile = SD.open("GEGEVENS.txt"); // lezen bestand
    while (myFile.available()) { //Zet lijnen in char arrays
      char inChar = myFile.read();
      if (inChar == '\n'){
        strcpy(recordArray[recordNum], aRecord);
        recordNum++;
        charNum = 0;
      }
      else{
        aRecord[charNum++] = inChar;
        aRecord[charNum] = '\0';
      }
    }
    myFile.close();
    //bepaalde punten in char array naar decimale getallen
    dag = atoi(&recordArray[0][7]);
    maand = atoi(&recordArray[0][10]);
    jaar = atoi(&recordArray[0][13]);
    uur = atoi(&recordArray[1][6]);
    minuut = atoi(&recordArray[1][9]);
    klantnummer = atoi(&recordArray[2][7]);
    PaneelVmp = atof(&recordArray[3][5]);
    sampletijd = atoi(&recordArray[4][12]);
    sprintf(fileName, "KLANT%03d.txt", klantnummer); //maak bestand genaamd KLANT"NUMMER".txt
    delay(500); 
}


ISR(TIMER1_OVF_vect){ //timer interrupt, precies 1 seconden
  seconden++;
  sampletijdsec++;
  TCNT1 = 18661;
}

void BerekenTijd(){
  if(minuut > 59){
    minuut = 0;
    uur++;
  }
  if(uur > 23){
    uur = 0;
    dag++;
  }
  uint8_t dagen_naar_maand[] = {0,31,29,31,30,31,30,31,31,30,31,30,31};
  if((jaar%4) != 1){
    dagen_naar_maand[2] = 28;
  }
  if(dag > dagen_naar_maand[maand]){
    maand++;
    dag = 1;
  }
  if(maand > 12){
    jaar++;
    maand = 1;
  }
}
//schrijven naar SD
/*
klantnummer
datum
tijd
vermogen in sampletijd
-----
*/

void SchrijvenSD2(){ 
    myFile = SD.open(fileName, FILE_WRITE);
    myFile.println(klantnummer);

    myFile.print(dag);
    myFile.print("-");
    myFile.print(maand);
    myFile.print("-");
    myFile.println(jaar);

    if(uur < 10){
    myFile.print("0");
    }
    myFile.print(uur);
    myFile.print(":");
    if(minuut < 10){
    myFile.print("0");
    }
    myFile.println(minuut);
    myFile.println(vermogen, 0);
    myFile.println("----");
    myFile.close();
}

void FailFlash(uint16_t wait){ //LED fout code
  delay(wait);
  digitalWrite(led1, LOW);
  delay(wait);
  digitalWrite(led1, HIGH);
}

void setup() {
  SPI.setClockDivider(SPI_CLOCK_DIV2); //SPI snelheid verlagen
  pinMode(led1, OUTPUT);
  int failDetect = 3;
  if(!ina219.begin()){failDetect = 0;}
  if(!dac.begin(0x62)){failDetect = 1;}
  if(!SD.begin(9)){failDetect = 2;}
    while (!SD.begin(9) || !dac.begin(0x62) || !ina219.begin()){     
      switch(failDetect){
        case 0:
        FailFlash(200);
        break;

        case 1:
        FailFlash(600);
        break;

        case 2:
        FailFlash(1200);
        break;
      }
    }
  digitalWrite(led1, HIGH); //hoog = led uit
  delay(200);
  GegevensNaarInt();
  delay(200);
  seconden = 0;
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 18661;
  TCCR1B |= (1<<CS12); //prescaler
  TIMSK1 |= (1<<TOIE1); //Enable timer interrupt, (TIMSK1 &= ~(1<<TOIE1) = disable timer)
  Vmp = PaneelVmp - ((temperatuurLezen() - 25)*0.0036*PaneelVmp); //berekende maximale vermogen spanning
}

void loop() {
  vermogenBerekenen();
  if(seconden > 59){ //als seconden 1 minuut is tijden aanpassen
    minuut++;
    BerekenTijd();
    seconden = 0;
  }
  if(sampletijdsec >= sampletijd){ //schrijven naar sd als seconden > ingestelde tijd
    SchrijvenSD2();
    sampletijdsec = 0;
    vermogen = 0;
  }  
}
