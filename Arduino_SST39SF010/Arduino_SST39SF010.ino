// SST39SF010 Flash EEPROM Programmer by Carsten Herting 25.12.2020
// This will program images files up to 128KB into SST39SF010
// This will program images files up to 64KB into SST39SF020/040
// If you want to program images > 64KB into SST39SF020 or SST39SF040 please use the
// updated schematic 1.1 that includes address lines A16-18.
#define READSIZE          0x2000                      // ADJUST YOURSELF: bytesize of the chunk to read from the chip

#define SET_OE(state)     bitWrite(PORTB, 0, state)   // must be high for write process
#define SET_WE(state)     bitWrite(PORTB, 1, state)   // must be a 100-1000ns low pulse
#define READ_DATA         (((PIND & 0b01111100) << 1) | (PINC & 0b00000111))
#define LED(state)        bitWrite(PORTB, 5, state)   // Indicator LED

void setup()
{
  PORTB = 0b00100011;             // LED on, /WE=HIGH, /OE=HIGH
  DDRB = 0b00111111;
  PORTC = 0; DDRC = 0b00111000;   // for upwards compatibility: new breadboard version uses C3-5 for A16-18
  PORTD = 0; DDRD = 0b10000000;   // used by old version for A16
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
        Serial.write('W'); LED(LOW);
        if (EraseFLASH() == true)
        {
          long nowmillis, lastmillis = millis();
          long adr = 0;
          do
          {
            do nowmillis = millis(); while (Serial.available() < 32 && nowmillis - lastmillis < 200);
            if (nowmillis - lastmillis < 200)
            {
              for(int i=0; i<32; i++)
              {
                byte dat = Serial.read();                  // read byte
                while (WriteFLASH(adr, dat) == false);     // try writing the bytes again and again
                adr++;
              }
              Serial.write(1);                             // send handshake
              lastmillis = nowmillis;
            }
          } while (nowmillis - lastmillis < 200);
        }
        break;
      case 'r':        
        Serial.write('R'); LED(LOW);
        ToRead();
        SET_OE(LOW);                                    // activate EEPROM outputs
        for(long i=0; i<READSIZE; i++) { SetAddress(i); Serial.write(READ_DATA); }
        SET_OE(HIGH);                                   // deactivate EEPROM outputs
        break;
      default:
        break;
    }
  }      
}

void SetAddress(long adr)
{ 
  for (byte i=0; i<16; i++)
  {
    bitWrite(PORTB, 2, adr & 1); adr = adr>>1;
    bitWrite(PORTB, 3, LOW); bitWrite(PORTB, 3, HIGH);
  }
  bitWrite(PORTB, 4, HIGH); bitWrite(PORTB, 4, LOW);

  bitWrite(PORTD, 7, adr & 1);
}

void ToRead()
{
  DDRC = 0b00111000;            // set to input and switch off pull-ups
  PORTC = 0b00000000;
  DDRD &= 0b10000011;
  PORTD &= 0b10000011;
}

void WriteTo(byte data)
{
  DDRC = 0b00111111;          // set to outputs
  PORTC = data & 0b00000111;
  DDRD |= 0b01111100;
  PORTD = (PIND & 0b10000011) | ((data & 0b11111000) >> 1);
}

bool EraseFLASH()
{
  SET_OE(HIGH);
  SetAddress(0x5555); WriteTo(0xaa); SET_WE(HIGH); SET_WE(LOW); SET_WE(HIGH);   // Chip Erase command 
  SetAddress(0x2aaa); WriteTo(0x55); SET_WE(HIGH); SET_WE(LOW); SET_WE(HIGH);
  SetAddress(0x5555); WriteTo(0x80); SET_WE(HIGH); SET_WE(LOW); SET_WE(HIGH);
  SetAddress(0x5555); WriteTo(0xaa); SET_WE(HIGH); SET_WE(LOW); SET_WE(HIGH);
  SetAddress(0x2aaa); WriteTo(0x55); SET_WE(HIGH); SET_WE(LOW); SET_WE(HIGH);
  SetAddress(0x5555); WriteTo(0x10); SET_WE(HIGH); SET_WE(LOW); SET_WE(HIGH);
  ToRead();
  SET_OE(LOW);
  int c = 0; while ((READ_DATA&128) != 128 && c < 2000) { c++; delayMicroseconds(100); }  // success < 700
  SET_OE(HIGH);
  if (c < 2000) return true; else return false;
}

bool WriteFLASH(long adr, byte data)
{
  SET_WE(HIGH);
  SET_OE(HIGH);
  SetAddress(0x5555); WriteTo(0xaa); SET_WE(HIGH); SET_WE(LOW); SET_WE(LOW); SET_WE(HIGH);
  SetAddress(0x2aaa); WriteTo(0x55); SET_WE(HIGH); SET_WE(LOW); SET_WE(LOW); SET_WE(HIGH);
  SetAddress(0x5555); WriteTo(0xa0); SET_WE(HIGH); SET_WE(LOW); SET_WE(LOW); SET_WE(HIGH);
  SetAddress(adr); WriteTo(data); SET_WE(HIGH); SET_WE(LOW); SET_WE(LOW); SET_WE(HIGH);
  ToRead();
  SET_OE(LOW);              // activate the output for data polling
  byte c = 0; while ((READ_DATA&128) != (data&128) && c < 100) c++;   // success < 17
  SET_OE(HIGH);             // deactivate the outputs
  if (c < 100) return true; else return false;
  return true;
}
