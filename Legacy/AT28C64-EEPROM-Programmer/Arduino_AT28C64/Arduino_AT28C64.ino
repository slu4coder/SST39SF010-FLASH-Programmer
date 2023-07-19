// AT28C64 EEPROM Programmer by Carsten Herting 18.12.2020

#define EEPROM_BYTESIZE   0x2000
#define SET_OE(state)     bitWrite(PORTB, 0, state)   // must be high for write process
#define SET_WE(state)     bitWrite(PORTB, 1, state)   // must be a 100-1000ns low pulse
#define READ_DATA         (((PIND & 0b01111100) << 1) | (PINC & 0b00000111))
#define POWER(state)      bitWrite(PORTD, 7, state)   // 5V for EEPROM
#define LED(state)        bitWrite(PORTB, 5, state)   // Indicator LED

void setup()
{
  PORTB = 0b00100011;           // LED on, /WE=HIGH, /OE=HIGH
  DDRB = 0b00111111;
  PORTC = 0; DDRC = 0;
  PORTD = 0; DDRD = 0b10000000;
  Serial.begin(115200);
}

void loop()
{
  PORTB = 0b00100011;           // LED on, /WE=HIGH, /OE=HIGH
  if (Serial.available() != 0)
  {
    switch(Serial.read())
    {
      case 'w':
        Serial.write('W'); LED(LOW); POWER(HIGH); delay(10);
        for(int adr=0; adr<EEPROM_BYTESIZE; adr+=32)
        {
          while (Serial.available() < 32);
          for(int i=0; i<32; i++)
          {
            byte dat = Serial.read();                   // read byte
            while (WriteEEPROM(adr|i, dat) == false);   // try writing it
          }
          Serial.write(1);                              // send handshake
        }          
        delay(10); POWER(LOW);
        break;
      case 'r':        
        Serial.write('R'); LED(LOW); POWER(HIGH); delay(10);
        ToRead();
        SET_OE(LOW);                                    // activate EEPROM outputs
        for(int i=0; i<EEPROM_BYTESIZE; i++) { SetAddress(i); Serial.write(READ_DATA); }
        SET_OE(HIGH);                                   // deactivate EEPROM outputs
        delay(10); POWER(LOW);
        break;
      default:
        break;
    }
  }      
}
    
void SetAddress(int adr)
{ 
  for (byte i=0; i<16; i++)
  {
    bitWrite(PORTB, 2, adr & 1); adr = adr>>1;
    bitWrite(PORTB, 3, LOW); bitWrite(PORTB, 3, HIGH);
  }
  bitWrite(PORTB, 4, HIGH); bitWrite(PORTB, 4, LOW);
}

void ToRead()
{
  DDRC = 0b00000000;            // set to input and switch off pull-ups
  PORTC = 0b00000000;
  DDRD &= 0b10000011;
  PORTD &= 0b10000011;
}

void WriteTo(byte data)
{
  DDRC = 0b00000111;          // set to outputs
  PORTC = data & 0b00000111;
  DDRD |= 0b01111100;
  PORTD = (PIND & 0b10000011) | ((data & 0b11111000) >> 1);
}

bool WriteEEPROM(int adr, byte data)
{
  noInterrupts();
  SetAddress(adr);
  WriteTo(data);
  SET_OE(HIGH);             // deactivate EEPROM outputs
  SET_WE(HIGH);             // was HIGH before
  SET_WE(LOW);              // 625ns LOW pulse (spec: 100ns - 1000ns)
  SET_WE(LOW);
  SET_WE(LOW);
  SET_WE(LOW);
  SET_WE(LOW);
  SET_WE(HIGH);             // rising edge: data latch, write process begins  
  interrupts();
  ToRead();  
  SET_OE(LOW);              // activate the output for data polling
  int c = 0; while (READ_DATA != data && c < 30000) c++;    // warten (meist Erfolg bei < 5000)
  SET_OE(HIGH);             // deactivate the outputs
  if (c < 30000) return true; else return false;
}
