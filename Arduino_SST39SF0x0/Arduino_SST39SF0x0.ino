// SST39SF0x0 Flash EEPROM Programmer by Carsten Herting 26.3.2021
// Will read and write image filesup to 512KB. Adjust the READSIZE below to your needs.

#define READSIZE          0x2000                      // CHANGE YOURSELF: Bytesize of the MEMORY CHUNK to READ from the chip

#define SET_OE(state)     bitWrite(PORTB, 0, state)   // must be high for write process
#define SET_WE(state)     bitWrite(PORTB, 1, state)   // must be a 100-1000ns low pulse
#define READ_DATA         (((PIND & 0b01111100) << 1) | (PINC & 0b00000111))
#define LED(state)        bitWrite(PORTB, 5, state)   // Indicator LED

void setup()
{
  PORTB = 0b00100011;             // B5: LED on, /WE=HIGH, /OE=HIGH, B2-4: 74595 SER, SRCLK, RCLK
  DDRB = 0b00111111;              // set all to outputs
  PORTC = 0; DDRC = 0b00111000;   // C3-5: address lines A16, A17, A18
  PORTD = 0; DDRD = 0b00000000;
  Serial.begin(115200);
}

void loop()
{
  PORTB = 0b00100011;           // LED on, /WE=HIGH, /OE=HIGH, 74595 all LOW
  
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
    bitWrite(PORTB, 2, adr & 1); adr = adr>>1;            // push in the bits (lowest first)
    bitWrite(PORTB, 3, LOW); bitWrite(PORTB, 3, HIGH);    // toggle SRCLK
  }
  bitWrite(PORTB, 4, HIGH); bitWrite(PORTB, 4, LOW);      // toggle RCLK
  bitWrite(PORTC, 3, adr & 1); adr = adr>>1;              // write the topmost bits
  bitWrite(PORTC, 4, adr & 1); adr = adr>>1;
  bitWrite(PORTC, 5, adr & 1); adr = adr>>1;
}

void ToRead()
{
  DDRC &= 0b00111000;       // set C0-2 to input
  PORTC &= 0b00111000;      // switch off C0-2 pull-ups
  DDRD &= 0b10000011;       // set D2-6 to inputs
  PORTD &= 0b10000011;      // switch off D2-6 pull-ups
}

void WriteTo(byte data)
{
  DDRC |= 0b00000111;       // set C0-2 to outputs
  PORTC = (PORTC & 0b00111000) | (data & 0b00000111); // write the lower 3 bits to C0-2
  DDRD |= 0b01111100;       // set D2-d6 to outputs
  PORTD = (PORTD & 0b10000011) | ((data & 0b11111000) >> 1);  // write the upper 5 bits to D2-6
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
  int c = 0; while ((READ_DATA & 128) != 128 && c < 2000) { c++; delayMicroseconds(100); }  // success < 700
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
