#include <SoftwareSerial.h>
#include <DeviceHive.h>

// device registration data
// intent numbers should be greater than 255!
// please refer to http://www.devicehive.com/binary/#SystemMessages/RegisterJson for complete syntax of registration info
const char *REG_DATA = "{"
    "id:'CB6A8D90-39EC-4FD6-9C2A-E3DAABD14B53',"
    "key:'ODB-II',"
    "name:'ODB-II demo',"
    "deviceClass:{"
        "name:'CAN_demo',"
        "version:'1.0'},"
    "equipment:[],"
    "commands:[],"
    "notifications:["
        "{intent:10002,name:'ODB-II',params:s}"
    "]"
"}";

const char *ALLJOYN_CHANNEL = "com.devicehive.samples.alljoyn.serial.arduino";

InputMessage rx_msg; // received message

void sendAllJoynInfoResponse(void)
{
    OutputMessage tx_msg(30002);
    tx_msg.putString(ALLJOYN_CHANNEL);
    DH.write(tx_msg);
}

void sendAllJoynSystemExec(const char *cmd)
{
    OutputMessage tx_msg(30004);
    tx_msg.putString(cmd);
    DH.write(tx_msg);
}

void sendServiceData(const char *data)
{
    OutputMessage tx_msg(10002);
    tx_msg.putString(data);
    DH.write(tx_msg);
}

// #define DEBUG
// #define TESTS

#ifdef TESTS
bool done = false;
#endif

const int LED_PIN = 13;
const int pollInterval = 100;  // Interval for sending commands in the loop
char* commands[] =
  {
    "010C1\r", // RPM
    "010D1\r", // Speed
    "01051\r" // Coolant temp
  };

SoftwareSerial softSerial(2, 3); // RX, TX

// HardwareSerial * obd2 = &Serial;
SoftwareSerial * obd2 = &softSerial;

HardwareSerial * debug = &Serial;
// SoftwareSerial * debug = &softSerial;

char serialBuffer[255];
unsigned long serialBufferPos = 0;


unsigned long lastPoll = millis();
int commandIndex = 0;
int numberOfCommands = 0;

void sendOBD2Command(char * command)
{
  obd2->write(command);

  #ifdef DEBUG
  debug->print("Sending command to OBD-II: ");
  debug->println(command);
  #endif

  delay(10);
}

int parseResponse(char * response, byte * responseBytes)
{
  char buf[3];
  byte bufPos = 0;

  unsigned responseBytesPos = 0;

  #ifdef DEBUG
  debug->print("Response: ");
  debug->println(response);
  #endif

  int i = 0;

  while(response[i] != 0)
  {
    if (response[i] != ' ')
    {
      buf[bufPos++] = response[i];
      buf[bufPos] = 0;
    }

    if ((response[i] == ' ') || (bufPos == 2) || (response[i+1] == '\0'))
    {
      if (bufPos != 0)
      {
        byte b = strtol(buf, 0, 16);

        if (b == 0)
        {
          for (int j = 0; j < bufPos; j++)
          {
            if (buf[j] != '0')
            {
              #ifdef DEBUG
              debug->println("Invalid string");
              #endif
              return 0;
            }
          }
        }

        responseBytes[responseBytesPos++] = b;
        bufPos = 0;

      }
    }

    i++;
  }

  #ifdef DEBUG
  debug->print("Parsed response: ");

  for (int i = 0; i < responseBytesPos; i++)
  {
    debug->print(String(responseBytes[i], HEX));
    debug->print(" ");
  }

  debug->println();
  #endif

  return responseBytesPos;
}

unsigned long getLongFromArray(byte * b, int offset, int len)
{
  unsigned long l = 0;
  for (int i = offset; i <  len; i++)
  {
    l <<= 8;
    l |= b[i];
  }

  return l;
}

void processResponse(byte * buf, int len)
{
  if (len < 3)
  {
    #ifdef DEBUG
    debug->print("Invalid OBD response length (should be at least 3 bytes): ");
    debug->println(len);
    #endif
    return;
  }

  if (buf[0] != 0x41)
  {
    #ifdef DEBUG
    debug->print("Invalid OBD response header (expected 0x41): 0x");
    debug->println(String(buf[0], HEX));
    #endif
  }

  unsigned long res = getLongFromArray(buf, 2, len);
  switch(buf[1])
  {
    case 0x0C:
      debug->print("RPM: ");
      debug->println(res / 4);
      break;
    case 0x0D:
      debug->print("Speed (km/h): ");
      debug->println(res);
      break;
    case 0x05:
      debug->print("Coolant Temp (C): ");
      debug->println(res - 40);
      break;
    default:
      debug->print("Unknown command response ID: ");
      debug->println(String(buf[1], HEX));
      break;
  }
}

void setup()
{
  numberOfCommands = sizeof(commands) / 2;

  pinMode(LED_PIN, OUTPUT);

  debug->begin(115200);
  obd2->begin(9600);
  DH.begin(*debug);

#ifndef TESTS
  delay(1500);
  sendOBD2Command("ATZ\r");
  delay(2000);
  sendOBD2Command("ATE0\r");
#endif

    //DH.writeRegistrationResponse(REG_DATA);
    sendAllJoynInfoResponse();
}

#ifdef TESTS
void tests()
{
  char * tests[] = {
    "00 01 01",
    "00 01 1",
    "00 01 FF",
    "00 01 FFAA",
    "0001FFAA",
    "0001FFAA   >",
    "00 1 01",
    "00 1 01 0"
  };

  byte responseBytes[255];
  for (int i = 0; i < sizeof(tests) / 2; i++)
  {
    parseResponse(tests[i], responseBytes);
  }

  char * tests2[] = {
    "41 0C 0E 96",
    "41 0D 0E 96",
    "41 05 0E 96",
    "41 10 0E 96",
    "42 0D 0E 96"
  };

  for (int i = 0; i < sizeof(tests2) / 2; i++)
  {
    int l = parseResponse(tests2[i], responseBytes);
    processResponse(responseBytes, l);
  }

}
#endif

void main_loop()
{
  if ((millis() - lastPoll) > pollInterval)
  {
    lastPoll = millis();

    sendOBD2Command(commands[commandIndex++]);

    if (commandIndex == numberOfCommands)
      commandIndex = 0;
  }

  if (obd2->available())
  {
    char c = obd2->read();

    if ( (c != '\r') && (c != '>') )
      serialBuffer[serialBufferPos++] = c;

    if ( (c == '\r') || (serialBufferPos == sizeof(serialBuffer) - 1) )
    {
      byte responseBytes[255];

      serialBuffer[serialBufferPos] = 0;
      int l = parseResponse(serialBuffer, responseBytes);

      if (l != 0)
      {
        processResponse(responseBytes, l);
        sendServiceData(serialBuffer); // to AllJoyn via DH
      }

      serialBufferPos = 0;
    }
  }


    if (DH.read(rx_msg) == DH_PARSE_OK)
    {
        switch (rx_msg.intent)
        {
            case INTENT_REGISTRATION_REQUEST:   // registration data needed
                DH.writeRegistrationResponse(REG_DATA);
                break;

            case 30001: // alljoyn info request
                sendAllJoynInfoResponse();
                break;

            case 30003: // alljoyn session status
            {
                int state = rx_msg.getByte();
                digitalWrite(LED_PIN, state ? HIGH : LOW);
            } break;
        }

        rx_msg.reset(); // reset for the next message parsing
    }
  }

void loop()
{
#ifndef TESTS
  main_loop();  // RELEASE
#else
  if (!done)
  {
    tests();    // FOR TESTING
    done = true;
  }
#endif
}
