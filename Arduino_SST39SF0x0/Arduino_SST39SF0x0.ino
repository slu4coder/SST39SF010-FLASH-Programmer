// *********************************************************************************
// *****                                                                       *****
// ***** SST39SF0x0 FLASH Programmer by Carsten Herting, last update 29.6.2023 *****
// *****                                                                       *****
// *********************************************************************************

#define SET_OE(state)     bitWrite(PORTB, 0, state)   // must be high for write process
#define SET_WE(state)     bitWrite(PORTB, 1, state)   // must be a 100-1000ns low pulse
#define READ_DATA         (((PIND & 0b01111100) << 1) | (PINC & 0b00000111))
#define LED(state)        bitWrite(PORTB, 5, state)   // Indicator LED

int state=0;                      // state machine of Arduino programmer
long readsize;                    // bytesize is transmitted by host programmer

void setup()
{
  PORTB = 0b00000011;             // B5: LED off, /WE=HIGH, /OE=HIGH, B2-4: 74595 SER, SRCLK, RCLK
  DDRB = 0b00111111;              // set all bits to outputs
  PORTC = 0; DDRC = 0b00111000;   // C3-5: address lines A16, A17, A18
  PORTD = 0; DDRD = 0b00000000;
  Serial.begin(115200, SERIAL_8N1);
}

void loop()
{
  switch(state)
  {
    case -1: // blinking LED = error
    {
      LED(HIGH); delay(100); LED(LOW); delay(100); // blinking
      break;
    }
    case 0: // waiting for handshake 'a'
    {
      if (Serial.available() > 0)
      {
        if (Serial.read() == 'a')  // confirm first handshake
        {
          Serial.write('A');
          readsize = 0;
          LED(HIGH);
          state = 1;
        }
        else state = -1; // unexpected char => error
      }
      break;
    }
    case 1: // waiting for bytesize and handshake 'b'
    {
      if (Serial.available() > 0)
      {
        char c = Serial.read();
        if (c >= '0' && c <= '9') { readsize = readsize*10 + c - '0'; Serial.write(c); } // echo
        else if (c == 'b') { Serial.write('B'); state = 2; break; } // confirm received bytesize
        else state = -1; // unexpected char => error
      }
      break;
    }  
    case 2: // receiving and writing data to FLASH
    {
      LED(LOW);
      if (EraseFLASH() == true) // completely erase the FLASH IC first
      {
        Serial.write('C');
        long adr = 0; // always commence writing at address zero
        do
        {
          byte chunk[32];
          int p=0;
          long lastmillis = millis();
          do
          {
            if (Serial.available() > 0)
            {
              chunk[p++] = Serial.read(); lastmillis = millis();
            }
          } while (p < 32 && millis() - lastmillis < 500);

          if (p > 0)                                 // received some data?
          {
            for(int i=0; i<p; i++) WriteFLASH(adr++, chunk[i]);
            Serial.write('D');
          }
        } while (adr < readsize);
        state = 3;
      } else state = -1;
      break;
    }
    case 3: // readout FLASH and send back the data for verification
    {
      LED(HIGH);
      ToRead();
      SET_OE(LOW);                                    // activate EEPROM outputs
      for(long i=0; i<readsize; i++)
      {
        SetAddress(i);
        Serial.write(READ_DATA);
      }
      SET_OE(HIGH);                                   // deactivate EEPROM outputs
      LED(LOW);
      state = 0;
      break;
    }
    default: // received some undefined character
      state = -1; break;
  }
}

void SetAddress(long adr)
{ 
  for (byte i=0; i<16; i++)
  {
    bitWrite(PORTB, 2, adr & 1); adr = adr>>1;            // push the bits (lowest first) into SER
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
  DDRC |= 0b00000111;       // set C0..2 to outputs
  PORTC = (PORTC & 0b00111000) | (data & 0b00000111); // write the lower 3 bits to C0-2
  DDRD |= 0b01111100;       // set D2..6 to outputs
  PORTD = (PORTD & 0b10000011) | ((data & 0b11111000) >> 1);  // write the upper 5 bits to D2-6
}

bool EraseFLASH()
{
  SET_OE(HIGH);
  SetAddress(0x5555); WriteTo(0xaa); SET_WE(HIGH); SET_WE(LOW); SET_WE(HIGH);   // invoke 'Chip Erase' command 
  SetAddress(0x2aaa); WriteTo(0x55); SET_WE(HIGH); SET_WE(LOW); SET_WE(HIGH);
  SetAddress(0x5555); WriteTo(0x80); SET_WE(HIGH); SET_WE(LOW); SET_WE(HIGH);
  SetAddress(0x5555); WriteTo(0xaa); SET_WE(HIGH); SET_WE(LOW); SET_WE(HIGH);
  SetAddress(0x2aaa); WriteTo(0x55); SET_WE(HIGH); SET_WE(LOW); SET_WE(HIGH);
  SetAddress(0x5555); WriteTo(0x10); SET_WE(HIGH); SET_WE(LOW); SET_WE(HIGH);
  ToRead();
  SET_OE(LOW);
  int c = 0; while ((READ_DATA & 128) != 128 && c < 2000) { c++; delayMicroseconds(100); }
  SET_OE(HIGH);
  return c < 2000; // SUCCESS condition
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
  int c = 0; while (((READ_DATA&128) != (data&128)) && (c < 100)) c++;   // success < 17
  SET_OE(HIGH);             // deactivate the outputs
  return c < 100;           // SUCCESS condition
}
