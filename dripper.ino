//
// dripper -- colored drips slowly going down a serial LED string
//
// Copyright (c) 2016 Stephen Platt
// Released under Creative Commons 4.0 Attribution Noncommercial Sharealike license.
//

#include <Adafruit_NeoPixel.h>
//#include <SPI.h>
//#include <RF24.h>
#include <TaskManager.h>

// length of the string
// note: 0 is the bottom, 49 is the top
#define LEN 50
// Which pin the strip is on
#define PIN 5

// tick length in ms (was: 60)
#define TICKLEN 30

// random delay between added drop checks  (was: 5, 15)
#define MINDROPADDDELAY  2
#define MAXDROPADDDELAY  6

// number of drops we support
#define MAXDROPS  10

// scale factors for colors.
// This is the number of >> that RGB undergoes along the drop length
// 0 is the head of the drop
// We only support length 5 right now
int dropScale[] = { 0, 1, 2, 4, 6, -1 };

// Defining the information associated with a single active drop
struct drop {
  int len;
  int pos;        // -1 then inactive
  int viscosity;  // in ticks; ticks to move down one light
                  // note: higher = slower drop
  int motion;      // in ticks, 0..dropspeed-1.  at 0, move and reset
  byte r,g,b;
  drop(int dlen, int dvisc, byte dr, byte dg, byte db):
    len(dlen), pos(LEN-1), viscosity(dvisc), motion(dvisc-1),
    r(dr), g(dg), b(db) {};
  drop(): pos(-1) {};
  inline bool isActive() { return pos>=0; }
};

// all of the drops
drop theDrops[MAXDROPS];

Adafruit_NeoPixel string = Adafruit_NeoPixel(LEN, PIN, NEO_GBR+NEO_KHZ800);

struct rgb { byte r; byte g; byte b; };
rgb* stringbytes;


void setup() {
  // mark our drops as inactive
  for(int i=0; i<MAXDROPS; i++) theDrops[i].pos = -1;

  string.begin();

  // get ptr to the string's data array
  stringbytes = (rgb*)string.getPixels();

  // add our tasks
  TaskMgr.addAutoWaitDelay(1, stringdripper, TICKLEN);
  TaskMgr.add(2, dropAdder);

  // debugging...
  Serial.begin(9600);
}


void addAndClip(struct rgb* led, int r, int g, int b) {
  //Adds an RGB value to the given led, clipping the led's rgb values
  // to 255 ea.
  led->r = (led->r+r)>255 ? 255 : (led->r+r);
  led->g = (led->g+g)>255 ? 255 : (led->g+g);
  led->b = (led->b+b)>255 ? 255 : (led->b+b);
}

// Task to add a drop to the drop array.  Only add a drop if there is room.
// Restart the task a random time later on
void dropAdder() {
  // if we can find an open space, add a drop there
  static int prevcolkind = -1; // good start, will never be [0 7]
  int curcolkind;
  int i;
  for(i=0; i<MAXDROPS; i++)
    if(!theDrops[i].isActive()) break;
  // if i==MAXDROPS then we never found one
  if(i<MAXDROPS) {
    // add a new drop

    // pick random drop parameters
    theDrops[i].len = 5;    // we will randomize eventually
    theDrops[i].pos = LEN-1;
    theDrops[i].viscosity = random(2,15);
    theDrops[i].motion = theDrops[i].viscosity;
    // reminder:  random(0,2) returns 0 or 1
    // new code: gen rand [1 7] and use the bits to select r(0x04) g(0x02) b(0x01)
    // We only generate when the light usage differs from the previous one
    // This prevents consecutive RGB "kinds"
    do { curcolkind = random(1,8); } while (curcolkind == prevcolkind);
    prevcolkind = curcolkind;
    theDrops[i].r = (curcolkind & 0x04) ? random(32,256) : 0;
    theDrops[i].g = (curcolkind & 0x02) ? random(32,256) : 0;
    theDrops[i].b = (curcolkind & 0x01) ? random(32,256) : 0;

    // debug log
    Serial << "new drop rgb = " << theDrops[i].r
     << " " << theDrops[i].g << " " << theDrops[i].b << endl; 
  }
  // now delay for a while
  TaskMgr.yieldDelay(random(MINDROPADDDELAY, MAXDROPADDDELAY)*1000);
}

// Task to paint the drop array and then move the drops along
void stringdripper() {
  int i, j;
  // process
  // 1. Paint the string
  // Clear out the data in string.
  // Take the drops and add them in to the string.
  // This makes colors meld when drops overlap
  string.clear();
  for(i=0; i<MAXDROPS; i++) {
    if(theDrops[i].isActive()) {
      for(j=0; j<theDrops[i].len && j+theDrops[i].pos<LEN; j++) {
        addAndClip(&stringbytes[theDrops[i].pos+j], 
          theDrops[i].r >> dropScale[j],
          theDrops[i].g >> dropScale[j],
          theDrops[i].b >> dropScale[j]
          );
      }
    }
  }
  
  // 2. Move along any drops that need moving along
  // Decrement motion counters and see what needs to be moved
  // It need moving if the motion counter is zero.  If it moves, reset
  // the motion counter to viscosity.  (More viscous drops move more slowly.)
  for(i=0; i<MAXDROPS; i++) {
    if(theDrops[i].isActive()) {
      theDrops[i].motion--;
      if(theDrops[i].motion<=0) {
        theDrops[i].motion = theDrops[i].viscosity;
        theDrops[i].pos--;
      }
    }
  }

  // Display the string
  string.show();
  // (Reminder -- this routine auto-reschedules itself)
}
