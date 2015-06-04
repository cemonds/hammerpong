
//#define PLAYER_RED
//#define PLAYER_BLUE
//#define EXTENSION
#define SCOREBOARD

#include <OctoWS2811.h>


const int MAX_POSITION = 299;
const int PUCK_INITIAL_SPEED = 16;
const int PUCK_MAX_SPEED = 4;
const int PLAYER_ZONE_SIZE = 10;

const unsigned long DISABLE_IDLE_TIMEOUT = 4294967295;



#if defined(PLAYER_RED)
  #define PLAYER
  const int LEDS_PER_STRIP = 75;
  
  const byte PLAYER_NUMBER = 1;
  const int PLAYER_END_POSITION = 0;
  const int PLAYER_DIRECTION = 1;

  const int STRIP_MIN_POSITION = 0;
  const int STRIP_MAX_POSITION = 74;
  
  const int MASTER_ZONE_BEGIN = 0;
  const int MASTER_ZONE_END = 70;
  
  #define mapLEDToPosition(x) x
  

#elif defined(PLAYER_BLUE)
  #define PLAYER
  const int LEDS_PER_STRIP = 75;
  
  const byte PLAYER_NUMBER = 2;
  
  const int PLAYER_END_POSITION = MAX_POSITION;
  const int PLAYER_DIRECTION = -1;

  const int STRIP_MIN_POSITION = MAX_POSITION-74;
  const int STRIP_MAX_POSITION = MAX_POSITION;
  
  
  const int MASTER_ZONE_BEGIN = MAX_POSITION-70;
  const int MASTER_ZONE_END = MAX_POSITION;

  #define mapLEDToPosition(x) MAX_POSITION-x

#elif defined(EXTENSION)
  const int LEDS_PER_STRIP = 75;
  
  const byte PLAYER_NUMBER = 0;
  
  const int MASTER_ZONE_BEGIN = 80;
  const int MASTER_ZONE_END = 220;
  
  const int STRIP1_MIN_POSITION = 75;
  const int STRIP1_MAX_POSITION = 149;
  const int STRIP2_MIN_POSITION = 150;
  const int STRIP2_MAX_POSITION = 224;

  
  #define mapLEDToPosition(x) 149-x
  #define mapLEDToPosition2(x) x+150

#elif defined(SCOREBOARD)
  const int LEDS_PER_STRIP = 78;
  
  const byte PLAYER_NUMBER = 0;
  
  const int MASTER_ZONE_BEGIN = 0;
  const int MASTER_ZONE_END = -1;
  
  const int SCOREBOARD_LEDS_SIDE = 11;
  const int SCOREBOARD_LEDS_DIGIT = 28;
  const int SCOREBOARD_LEDS_COUNTER = SCOREBOARD_LEDS_DIGIT * 2;
  const int SCOREBOARD_LEDS_PER_SEGMENT = 4;
  const int SCOREBOARD_NUMBER_OF_SEGMENTS = 7;
  const int SCOREBOARD_SEGMENTS[10][SCOREBOARD_NUMBER_OF_SEGMENTS] = {
    {1, 1, 1, 0, 1, 1, 1},
    {1, 0, 0, 0, 1, 0, 0},
    {0, 1, 1, 1, 1, 1, 0},
    {1, 1, 0, 1, 1, 1, 0},
    {1, 0, 0, 1, 1, 0, 1},
    {1, 1, 0, 1, 0, 1, 1},
    {1, 1, 1, 1, 1, 1, 0},
    {1, 0, 0, 0, 1, 1, 0},
    {1, 1, 1, 1, 1, 1, 1},
    {1, 1, 0, 1, 1, 1, 1}
  }; 

#else
  #error "No mode defined"
#endif

#define signum(x) ((x > 0) ? 1 : ((x < 0) ? -1 : 0))


DMAMEM int displayMemory[LEDS_PER_STRIP*6];
int drawingMemory[LEDS_PER_STRIP*6];

const int config = WS2811_GRB | WS2811_800kHz;

OctoWS2811 leds(LEDS_PER_STRIP, displayMemory, drawingMemory, config);

unsigned long lastUpdate = 0;
unsigned int durationBetweenUpdates = 20;
const int TRACK_BRIGHTNESS = 16;
const int TRAIL_LENGTH = 75;
const unsigned long TRAIL_DECAY_TIME = 4;
const float TRAIL_DECAY_RATE = 0.96;
unsigned long nextTrailUpdate = 0;

#define HWSERIAL Serial2

const int SERIAL_MODE_PIN = 11;
const int BUTTON_PIN = 17;

const byte COMMAND_IDLE = 0;
const byte COMMAND_START_GAME = 1;
const byte COMMAND_GAME_ENDED = 2;
const byte COMMAND_PUCK_STATUS = 3;
const byte COMMAND_PLAYER_HIT = 4;
const byte COMMAND_PLAYER_MISS = 5;
const byte COMMAND_PLAYER_BLOCK = 6;

const unsigned long IDLE_TIMEOUT = 5000;

unsigned long lastPuckUpdate = 0;
unsigned long idleStartTime = 0;
int puckPosition = 0;
int8_t puckSpeed = 0;
boolean idle = false;
boolean inGame = false;
byte hitCounter = 11;

boolean needsRedraw = true;
int rainbowColors[180];

void setup() {
  leds.begin(); 

  Serial.begin(9600);
  HWSERIAL.begin(115200);
  HWSERIAL.transmitterEnable(SERIAL_MODE_PIN);
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  delay(2000);

  for (int i=0; i<180; i++) {
    int hue = i * 2;
    int saturation = 100;
    int lightness = 50;
    // pre-compute the 180 rainbow colors
    rainbowColors[i] = makeColor(hue, saturation, lightness);
  }
}

byte DONT_CARE;

void readMessageData(byte* firstByte, byte* secondByte, byte* thirdByte) {
  *firstByte = HWSERIAL.read();
  *secondByte = HWSERIAL.read();
  *thirdByte = HWSERIAL.read();
}

void readStatusMessage() {
  byte positionLow;
  byte positionHigh;
  readMessageData(&positionLow, &positionHigh, (byte*)&puckSpeed); 
  puckPosition = positionLow | (positionHigh << 8);
}

void readStartGameMessage() {
  byte low;
  byte high;
  readMessageData(&low, &high, (byte*)&puckSpeed); 
  puckPosition = low | (high << 8);
  hitCounter = 0;
  inGame = true;
  idle = false;
  needsRedraw = true;
}

void readGameEndedMessage() {
  readMessageData(&DONT_CARE, &DONT_CARE, &DONT_CARE); 
  inGame = false;
  idleStartTime = millis() + IDLE_TIMEOUT;
  needsRedraw = true;
}

void readPlayerHitMessage() {
  byte player;
  readMessageData(&player, &hitCounter, &DONT_CARE);
  needsRedraw = true;
}

void readPlayerMissMessage() {
  byte player;
  readMessageData(&player, &DONT_CARE, &DONT_CARE); 
}

void readPlayerBlockMessage() {
  byte player;
  readMessageData(&player, &DONT_CARE, &DONT_CARE); 
}

void sendIdleMessage() {
  HWSERIAL.write(COMMAND_IDLE);
  HWSERIAL.write(0);
  HWSERIAL.write(0);
  HWSERIAL.write(0);
}

void sendStatusMessage() {
  HWSERIAL.write(COMMAND_PUCK_STATUS);
  HWSERIAL.write(lowByte(puckPosition));
  HWSERIAL.write(highByte(puckPosition));
  HWSERIAL.write(puckSpeed);
}

void sendStartGameMessage() {
  HWSERIAL.write(COMMAND_START_GAME);
  HWSERIAL.write(lowByte(puckPosition));
  HWSERIAL.write(highByte(puckPosition));
  HWSERIAL.write(puckSpeed);
}

void sendGameEndedMessage() {
  HWSERIAL.write(COMMAND_GAME_ENDED);
  HWSERIAL.write(PLAYER_NUMBER);
  HWSERIAL.write(hitCounter);
  HWSERIAL.write(0);
}

void sendPlayerHitMessage() {
  HWSERIAL.write(COMMAND_PLAYER_HIT);
  HWSERIAL.write(PLAYER_NUMBER);
  HWSERIAL.write(hitCounter);
  HWSERIAL.write(0);
}

void sendPlayerMissMessage() {
  HWSERIAL.write(COMMAND_PLAYER_MISS);
  HWSERIAL.write(PLAYER_NUMBER);
  HWSERIAL.write(0);
  HWSERIAL.write(0);
}

void sendPlayerBlockMessage() {
  HWSERIAL.write(COMMAND_PLAYER_BLOCK);
  HWSERIAL.write(PLAYER_NUMBER);
  HWSERIAL.write(0);
  HWSERIAL.write(0);
}


void updateState() {
  unsigned long now = millis();
#if defined(PLAYER)
  if (digitalRead(BUTTON_PIN) == LOW) {
    if(inGame) {
      if(signum(puckSpeed) != PLAYER_DIRECTION) {
        int playerZoneLowerBound = min(PLAYER_END_POSITION, PLAYER_END_POSITION+PLAYER_DIRECTION*PLAYER_ZONE_SIZE);
        int playerZoneUpperBound = max(PLAYER_END_POSITION, PLAYER_END_POSITION+PLAYER_DIRECTION*PLAYER_ZONE_SIZE);
        if(puckPosition <= playerZoneUpperBound && puckPosition >= playerZoneLowerBound) {
          int newPuckSpeed = max(abs(puckSpeed)-1, PUCK_MAX_SPEED);
          puckSpeed = PLAYER_DIRECTION * newPuckSpeed;
          needsRedraw = true;
          ++hitCounter;
          sendPlayerHitMessage();
          sendStatusMessage();
        }
      }
    } else if (idle) {
      idle = false;
      inGame = true;
      puckSpeed = PLAYER_DIRECTION * PUCK_INITIAL_SPEED;
      puckPosition = PLAYER_END_POSITION;
      lastPuckUpdate = millis();
      hitCounter = 0;
      sendStartGameMessage();
      sendStatusMessage();
      needsRedraw = true;
    }
  }
#endif
  if(inGame && lastPuckUpdate + abs(puckSpeed) <= now) {
    puckPosition += signum(puckSpeed);
    if(puckPosition <= 0) {
      Serial.println("puck <= 0"); 
    }
    lastPuckUpdate = now;
    needsRedraw = true;
    if(inGame && puckPosition < 0) {
      Serial.println("end");
  #if defined(PLAYER_RED)  
      sendPlayerMissMessage();
      sendGameEndedMessage();
  #endif
      inGame = false;
      idleStartTime = now + IDLE_TIMEOUT;
      needsRedraw = true;
    }
    if(inGame && puckPosition > MAX_POSITION) {
  #if defined(PLAYER_BLUE)
      sendPlayerMissMessage();
      sendGameEndedMessage();
  #endif
      inGame = false;
      idleStartTime = now + IDLE_TIMEOUT;
      needsRedraw = true;
    }
    if(puckPosition >= MASTER_ZONE_BEGIN && puckPosition <= MASTER_ZONE_END) {
      sendStatusMessage();
    }
  }
  if(!idle && now > idleStartTime) {
    Serial.println("Start idle");
    idle = true;
    needsRedraw = true;
    idleStartTime = DISABLE_IDLE_TIMEOUT;
//    sendIdleMessage();
  }
  if(nextTrailUpdate < now) {
    nextTrailUpdate += TRAIL_DECAY_TIME;
    needsRedraw = true;
  }
}

void drawVisual() {
//  drawTrack();
  if(inGame) {
    drawPuck();
    drawTrail();
  } else if(idle) {
    drawIdleAnimation();
  }
  decay();
  drawScoreboard();
  leds.show();
  needsRedraw = false;
}

int mapPositionToLED(int position) {
  int led = -1;
#if defined(PLAYER_RED)
  if(position >= STRIP_MIN_POSITION && position <= STRIP_MAX_POSITION) {
    led = position; 
  }
#endif
#if defined(PLAYER_BLUE)
  if(position >= STRIP_MIN_POSITION && position <= STRIP_MAX_POSITION) {
    led = STRIP_MAX_POSITION - position; 
  }
#endif
#if defined(EXTENSION)
  if(position >= STRIP1_MIN_POSITION && position <= STRIP1_MAX_POSITION) {
    led = STRIP1_MAX_POSITION-position; 
  }
#endif
  return led;
}

int mapPositionToLED2(int position) {
  int led = -1;
#if defined(EXTENSION)
  if(position >= STRIP2_MIN_POSITION && position <= STRIP2_MAX_POSITION) {
    led = position - STRIP2_MIN_POSITION; 
  }
#endif
  return led;
}

void drawTrack() {
    
#if defined(PLAYER) || defined(EXTENSION)
  for(int led = 0; led < LEDS_PER_STRIP; ++led) {
    for(int strip = 0; strip < 3; ++strip) {
      leds.setPixel(strip*LEDS_PER_STRIP+led, 0, 0, 0);

      int ledPosition = mapLEDToPosition(led);
      float valueAsFloat = abs(ledPosition-(MAX_POSITION/2))*TRACK_BRIGHTNESS/(MAX_POSITION/2);
      int value = TRACK_BRIGHTNESS-min(TRACK_BRIGHTNESS,max(0,valueAsFloat));
      if(ledPosition < MAX_POSITION / 2) {
        leds.setPixel(strip*LEDS_PER_STRIP+led, TRACK_BRIGHTNESS, value, value);
      } else {
        leds.setPixel(strip*LEDS_PER_STRIP+led, value, value, TRACK_BRIGHTNESS);
      }
    }
  }
#endif

#if defined(EXTENSION)  
  for(int led = 0; led < LEDS_PER_STRIP; ++led) {
    for(int strip = 4; strip < 7; ++strip) {
      int ledPosition = mapLEDToPosition2(led);
      float valueAsFloat = abs(ledPosition-(MAX_POSITION/2))*TRACK_BRIGHTNESS/(MAX_POSITION/2);
      int value = TRACK_BRIGHTNESS-min(TRACK_BRIGHTNESS,max(0,valueAsFloat));
      if(ledPosition < MAX_POSITION / 2) {
        leds.setPixel(strip*LEDS_PER_STRIP+led, TRACK_BRIGHTNESS, value, value);
      } else {
        leds.setPixel(strip*LEDS_PER_STRIP+led, value, value, TRACK_BRIGHTNESS);
      }
    }
  }
#endif
}

void drawPuck() {
    
#if defined(PLAYER) || defined(EXTENSION)
  int led = mapPositionToLED(puckPosition);
  if(led > -1) {
    for(int strip = 0; strip < 3; ++strip) {
      leds.setPixel(strip*LEDS_PER_STRIP+led, 255, 255, 255);
    }
  }
  led = mapPositionToLED(puckPosition+1);
  if(led > -1) {
    leds.setPixel(1*LEDS_PER_STRIP+led, 255, 255, 255);
  }
#endif

#if defined(EXTENSION)  
  led = mapPositionToLED2(puckPosition);
  if(led > -1) {
    for(int strip = 4; strip < 7; ++strip) {
      leds.setPixel(strip*LEDS_PER_STRIP+led, 255, 255, 255);
    }
  }
  led = mapPositionToLED2(puckPosition+1);
  if(led > -1) {
    leds.setPixel(5*LEDS_PER_STRIP+led, 255, 255, 255);
  }
#endif
}

void drawTrail() {
  #if defined(PLAYER) || defined(EXTENSION)
  int direction = -1 * signum(puckSpeed);
  int trailPosition = puckPosition+direction;
  int led = mapPositionToLED(trailPosition);
  if(led > -1) {
    for(int strip = 0; strip < 3; ++strip) {
      int trailBrightness = random(256);
      float valueAsFloat = abs(trailPosition-(MAX_POSITION/2))*trailBrightness/(MAX_POSITION/2);
      int value = trailBrightness-min(trailBrightness,max(0,valueAsFloat));
      if(trailPosition < MAX_POSITION / 2) {
        leds.setPixel(strip*LEDS_PER_STRIP+led, trailBrightness, value, value);
      } else {
        leds.setPixel(strip*LEDS_PER_STRIP+led, value, value, trailBrightness);
      }
    }
  }
  #endif
  #if defined(EXTENSION)  
  led = mapPositionToLED2(trailPosition);
  if(led > -1) {
    for(int strip = 4; strip < 7; ++strip) {
      int trailBrightness = random(256);
      float valueAsFloat = abs(trailPosition-(MAX_POSITION/2))*trailBrightness/(MAX_POSITION/2);
      int value = trailBrightness-min(trailBrightness,max(0,valueAsFloat));
      if(trailPosition < MAX_POSITION / 2) {
        leds.setPixel(strip*LEDS_PER_STRIP+led, trailBrightness, value, value);
      } else {
        leds.setPixel(strip*LEDS_PER_STRIP+led, value, value, trailBrightness);
      }
    }
  }
  #endif
}
 
#define DIM(x) ((((x&0xFF0000)>>18)<<16) | (((x&0x00FF00)>>10)<<8) | ((x&0x0000FF)>>2))

void drawIdleAnimation() {
  unsigned long now = millis();
 #if defined(PLAYER) 
  for(int led=0; led<LEDS_PER_STRIP; ++led) {
    for(int strip = 0; strip < 3; ++strip) {
      int index = ((int)now/10 +led + strip) % 180;
      int color = rainbowColors[index];
//      leds.setPixel(led + strip*LEDS_PER_STRIP, DIM(color));
    }
  }
  #endif
  
#if defined(PLAYER)
  int led = abs(sin(now/1000.f)*40)+10;
  for(int strip = 0; strip < 3; ++strip) {
    leds.setPixel(strip*LEDS_PER_STRIP+led, 255, 255, 255);
  }
  leds.setPixel(1*LEDS_PER_STRIP+led+1, 255, 255, 255);

  int direction = -signum(sin(now/1000.f)*cos(now/1000.f)/abs(sin(now/1000.f)));
  led += direction;
  for(int strip = 0; strip < 3; ++strip) {
    int trailBrightness = random(256);
  #if defined(PLAYER_RED)
    leds.setPixel(strip*LEDS_PER_STRIP+led, trailBrightness, 0, 0);
  #elif defined(PLAYER_BLUE)
    leds.setPixel(strip*LEDS_PER_STRIP+led, 0, 0, trailBrightness);
  #endif
  }
#endif

  #if defined(EXTENSION)  
  for(int led=0; led<LEDS_PER_STRIP; ++led) {
    for(int strip = 4; strip < 7; ++strip) {
      int color = leds.getPixel(strip*LEDS_PER_STRIP+led);
      int red = (color & 0xFF0000) >> 16;
      int green = (color & 0xFF00) >> 8;
      int blue = (color & 0xFF);
      red = (int) (red * TRAIL_DECAY_RATE);
      green = (int) (green * TRAIL_DECAY_RATE);
      blue = (int) (blue * TRAIL_DECAY_RATE);
      leds.setPixel(strip*LEDS_PER_STRIP+led, red, green, blue);
    }
  }
  for(int led=0; led<LEDS_PER_STRIP; ++led) {
    for(int strip = 4; strip < 7; ++strip) {
      int color = leds.getPixel(strip*LEDS_PER_STRIP+led);
      int red = (color & 0xFF0000) >> 16;
      int green = (color & 0xFF00) >> 8;
      int blue = (color & 0xFF);
      red = (int) (red * TRAIL_DECAY_RATE);
      green = (int) (green * TRAIL_DECAY_RATE);
      blue = (int) (blue * TRAIL_DECAY_RATE);
      leds.setPixel(strip*LEDS_PER_STRIP+led, red, green, blue);
    }
  }
  #endif
}

void decay() {
  #if defined(PLAYER) || defined(EXTENSION)
  for(int led=0; led<LEDS_PER_STRIP; ++led) {
    for(int strip = 0; strip < 3; ++strip) {
      int color = leds.getPixel(strip*LEDS_PER_STRIP+led);
      int red = (color & 0xFF0000) >> 16;
      int green = (color & 0xFF00) >> 8;
      int blue = (color & 0xFF);
      red = (int) (red * TRAIL_DECAY_RATE);
      green = (int) (green * TRAIL_DECAY_RATE);
      blue = (int) (blue * TRAIL_DECAY_RATE);
      leds.setPixel(strip*LEDS_PER_STRIP+led, red, green, blue);
    }
  }
  #endif
  
  #if defined(EXTENSION)  
  for(int led=0; led<LEDS_PER_STRIP; ++led) {
    for(int strip = 4; strip < 7; ++strip) {
      int color = leds.getPixel(strip*LEDS_PER_STRIP+led);
      int red = (color & 0xFF0000) >> 16;
      int green = (color & 0xFF00) >> 8;
      int blue = (color & 0xFF);
      red = (int) (red * TRAIL_DECAY_RATE);
      green = (int) (green * TRAIL_DECAY_RATE);
      blue = (int) (blue * TRAIL_DECAY_RATE);
      leds.setPixel(strip*LEDS_PER_STRIP+led, red, green, blue);
    }
  }
  #endif
}

void drawScoreboard() {
#if defined(SCOREBOARD)
  int digit1 = hitCounter % 10;
  int digit2 = (hitCounter / 10) % 10;
  for(int segment = 0; segment < SCOREBOARD_NUMBER_OF_SEGMENTS; ++segment) {
    int firstLEDinSegment = SCOREBOARD_LEDS_SIDE+segment*SCOREBOARD_LEDS_PER_SEGMENT;
    for(int led = firstLEDinSegment; led < firstLEDinSegment+SCOREBOARD_LEDS_PER_SEGMENT; ++led) {
      int pixelValue = SCOREBOARD_SEGMENTS[digit1][segment]*255;
      leds.setPixel(firstLEDinSegment, pixelValue, pixelValue, pixelValue);
      leds.setPixel(firstLEDinSegment+1, pixelValue, pixelValue, pixelValue);
      leds.setPixel(firstLEDinSegment+2, pixelValue, pixelValue, pixelValue);
      leds.setPixel(firstLEDinSegment+3, pixelValue, pixelValue, pixelValue);
    }
  }
  for(int segment = 0; segment < SCOREBOARD_NUMBER_OF_SEGMENTS; ++segment) {
    int firstLEDinSegment = SCOREBOARD_LEDS_SIDE+segment*SCOREBOARD_LEDS_PER_SEGMENT+SCOREBOARD_LEDS_DIGIT;
    for(int led = firstLEDinSegment; led < firstLEDinSegment+SCOREBOARD_LEDS_PER_SEGMENT; ++led) {
      int pixelValue = SCOREBOARD_SEGMENTS[digit2][segment]*255;
      leds.setPixel(firstLEDinSegment, pixelValue, pixelValue, pixelValue);
      leds.setPixel(firstLEDinSegment+1, pixelValue, pixelValue, pixelValue);
      leds.setPixel(firstLEDinSegment+2, pixelValue, pixelValue, pixelValue);
      leds.setPixel(firstLEDinSegment+3, pixelValue, pixelValue, pixelValue);
    }
  }
  for(int led = 0; led < SCOREBOARD_LEDS_SIDE; ++led) {
    leds.setPixel(led, 0, 0, 255);
  }
  for(int led = SCOREBOARD_LEDS_SIDE+SCOREBOARD_LEDS_COUNTER; led < 2*SCOREBOARD_LEDS_SIDE+SCOREBOARD_LEDS_COUNTER+10; ++led) {
    leds.setPixel(led, 255, 0, 0);
  }
#endif
}

void handleMessage() {
  if (HWSERIAL.available() >= 4) {
    int command = HWSERIAL.read();
    switch(command) {
      case COMMAND_IDLE:
        idle = true;
        break;
      case COMMAND_START_GAME:
        readStartGameMessage();
        break;
      case COMMAND_GAME_ENDED:
        readGameEndedMessage();
        break;
      case COMMAND_PUCK_STATUS:
        readStatusMessage();
        break;
      case COMMAND_PLAYER_HIT:
        readPlayerHitMessage();
        break;
      case COMMAND_PLAYER_MISS:
        readPlayerMissMessage();
        break;
      case COMMAND_PLAYER_BLOCK:
        readPlayerBlockMessage();
        break;
      default:
        break; 
    }
/*    Serial.print("UART received: ");
    Serial.println(command, DEC);*/
  }
}

void loop() {
//  Serial.println("Starte Loop...");
  handleMessage();
  updateState();
  if(needsRedraw) {
    drawVisual();
  }
}


// Convert HSL (Hue, Saturation, Lightness) to RGB (Red, Green, Blue)
//
//   hue:        0 to 359 - position on the color wheel, 0=red, 60=orange,
//                            120=yellow, 180=green, 240=blue, 300=violet
//
//   saturation: 0 to 100 - how bright or dull the color, 100=full, 0=gray
//
//   lightness:  0 to 100 - how light the color is, 100=white, 50=color, 0=black
//
int makeColor(unsigned int hue, unsigned int saturation, unsigned int lightness)
{
	unsigned int red, green, blue;
	unsigned int var1, var2;

	if (hue > 359) hue = hue % 360;
	if (saturation > 100) saturation = 100;
	if (lightness > 100) lightness = 100;

	// algorithm from: http://www.easyrgb.com/index.php?X=MATH&H=19#text19
	if (saturation == 0) {
		red = green = blue = lightness * 255 / 100;
	} else {
		if (lightness < 50) {
			var2 = lightness * (100 + saturation);
		} else {
			var2 = ((lightness + saturation) * 100) - (saturation * lightness);
		}
		var1 = lightness * 200 - var2;
		red = h2rgb(var1, var2, (hue < 240) ? hue + 120 : hue - 240) * 255 / 600000;
		green = h2rgb(var1, var2, hue) * 255 / 600000;
		blue = h2rgb(var1, var2, (hue >= 120) ? hue - 120 : hue + 240) * 255 / 600000;
	}
	return (red << 16) | (green << 8) | blue;
}

unsigned int h2rgb(unsigned int v1, unsigned int v2, unsigned int hue)
{
	if (hue < 60) return v1 * 60 + (v2 - v1) * hue;
	if (hue < 180) return v2 * 60;
	if (hue < 240) return v1 * 60 + (v2 - v1) * (240 - hue);
	return v1 * 60;
}

