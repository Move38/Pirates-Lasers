enum blinkModes {SHIP, LASER, MIRROR};
byte blinkMode = SHIP;
byte orientation = 0;//travels around from

enum signals {INERT, HEAL, DAMAGE, RESET, RESOLVE};
byte faceSignal[6] = {INERT, INERT, INERT, INERT, INERT, INERT};

enum healthStates {DAMAGED, HEALTHY, HEALING, TRANSFERRING};
byte health[5] = {HEALTHY, HEALTHY, HEALTHY, HEALTHY, HEALTHY};
byte healthTotal = 5;
Timer healingTimer;
#define HEAL_TIME 1000

Timer laserTimer;//used when a laser blast has been recieved
#define LASER_BLAST_DURATION 1500//how long the big beam stays fully lit
#define LASER_FADE 250
#define EXPLOSION_DURATION 2000
#define WORLD_FADE_IN 1000

#define EXPLOSION_DELAY 255
#define HUE_BEGIN 25
byte phaseOffset[6] = {0, (EXPLOSION_DELAY / 5) * 2, (EXPLOSION_DELAY / 5) * 5, (EXPLOSION_DELAY / 5) * 3, (EXPLOSION_DELAY / 5) * 1, (EXPLOSION_DELAY / 5) * 4};

#define LASER_FULL_DURATION 4750
bool laserFaces[6] = {false, false, false, false, false, false};

// SYNCHRONIZED WAVES
Timer syncTimer;
#define PERIOD_DURATION 6000
#define PERIOD_DURATION_ALT 4000
#define BUFFER_DURATION 400
byte neighborState[6];
byte syncVal = 0;

void setup() {
  // put your setup code here, to run once:

}

void loop() {

  //listen for inputs
  if (buttonMultiClicked()) {//reset all health
    fullBroadcast(RESET);
    fullHeal();
  }

  if (buttonDoubleClicked()) {//change mode
    if (isAlone()) {
      blinkMode = (blinkMode + 1) % 3;
      fullHeal();
    }
  }

  if (buttonSingleClicked()) {
    //if it's a laser, FIRE
    switch (blinkMode) {
      case LASER:
        //FIRE ALL LASER FACES
        faceSignal[orientation] = DAMAGE;
        faceSignal[(orientation + 2) % 6] = DAMAGE;
        faceSignal[(orientation + 4) % 6] = DAMAGE;
        //UPDATE ORIENTATION
        orientation = (orientation + 1) % 6;
        //TRIGGER ANIMATION
        laserTimer.set(LASER_FULL_DURATION);
        break;
      case SHIP:
        //SEND HEALING PULSE!
        faceSignal[5] = HEAL;
        //TAKE DAMAGE
        takeDamage();
        break;
    }
  }

  //deal with signals
  FOREACH_FACE(f) {
    switch (faceSignal[f]) {
      case INERT:
        inertLoop(f);
        break;
      case RESET:
        resetLoop(f);
        break;
      case DAMAGE:
        damageLoop(f);
        break;
      case HEAL:
        healLoop(f);
        break;
      case RESOLVE:
        resolveLoop(f);
        break;
    }
  }

  //resolve healing
  for (byte i = 0; i < 5; i++) {
    if (health[i] == HEALING) {
      if (healingTimer.isExpired()) {
        health[i] = HEALTHY;
      }
    } else if (health[i] == TRANSFERRING) {
      if (healingTimer.isExpired()) {
        health[i] = DAMAGED;
      }
    }
  }

  //do communication
  FOREACH_FACE(f) {
    byte sendData = (blinkMode << 4) + (faceSignal[f] << 1) + (syncVal);
    setValueSentOnFace(sendData, f);
  }

  //do display
  syncLoop();
  waterDisplay();
  switch (blinkMode) {
    case SHIP:
      shipDisplay();
      break;
    case LASER:
      laserDisplay();
      break;
    case MIRROR:
      mirrorDisplay();
      break;
  }
}

void inertLoop(byte face) {

  //listen for signals
  if (!isValueReceivedOnFaceExpired(face)) {//neighbor!
    switch (getFaceSignal(getLastValueReceivedOnFace(face))) {
      case DAMAGE:
        passDamage(face);
        takeDamage();
        //trigger animation
        laserTimer.set(LASER_FULL_DURATION);
        break;
      case HEAL:
        faceSignal[face] = HEAL;
        getHealed();
        break;
      case RESET:
        fullBroadcast(RESET);
        fullHeal();
        break;
    }
  }
}

void damageLoop(byte face) {
  //listen for appropriate signals
  if (!isValueReceivedOnFaceExpired(face)) {//neighbor!
    switch (getFaceSignal(getLastValueReceivedOnFace(face))) {
      case DAMAGE:
        faceSignal[face] = RESOLVE;
        break;
      case RESOLVE:
        faceSignal[face] = RESOLVE;
        break;
      case RESET:
        fullBroadcast(RESET);
        fullHeal();
        break;
    }
  } else {//no neighbor, just go INERT
    faceSignal[face] = INERT;
  }
}

void healLoop(byte face) {
  //listen for appropriate signals
  if (!isValueReceivedOnFaceExpired(face)) {//neighbor!
    switch (getFaceSignal(getLastValueReceivedOnFace(face))) {
      case DAMAGE:
        passDamage(face);
        takeDamage();
        break;
      case HEAL:
        faceSignal[face] = RESOLVE;
        break;
      case RESOLVE:
        faceSignal[face] = RESOLVE;
        break;
      case RESET:
        fullBroadcast(RESET);
        fullHeal();
        break;
    }
  } else {//no neighbor, just go INERT
    faceSignal[face] = INERT;
  }
}

void resetLoop(byte face) {
  //listen for appropriate signals
  if (!isValueReceivedOnFaceExpired(face)) {//neighbor!
    switch (getFaceSignal(getLastValueReceivedOnFace(face))) {
      case RESET:
        faceSignal[face] = RESOLVE;
        break;
      case RESOLVE:
        faceSignal[face] = INERT;
        break;
    }
  } else {//no neighbor, just go INERT
    faceSignal[face] = INERT;
  }
}

void resolveLoop(byte face) {
  //listen for appropriate signals
  if (!isValueReceivedOnFaceExpired(face)) {//neighbor!
    switch (getFaceSignal(getLastValueReceivedOnFace(face))) {
      case INERT:
      case RESOLVE:
        faceSignal[face] = INERT;
        break;
    }
  } else {//no neighbor, just go INERT
    faceSignal[face] = INERT;
  }
}

void passDamage(byte face) {
  //reset laser faces
  FOREACH_FACE(f) {
    laserFaces[f] = false;
  }

  //now actually do the pass
  if (blinkMode == MIRROR) {//I'm a mirror

    byte otherFace = (orientation + 2) % 6;//this is the other face that can redirect
    if (face == orientation || face == otherFace) {//am I being damaged on that face?
      faceSignal[orientation] = DAMAGE;
      laserFaces[orientation] = true;
      faceSignal[otherFace] = DAMAGE;
      laserFaces[otherFace] = true;
    } else {
      faceSignal[face] = DAMAGE;
      faceSignal[(face + 3) % 6] = DAMAGE;

      laserFaces[face] = true;
      laserFaces[(face + 3) % 6] = true;
    }
  } else {//just a regular ship piece or actual laser piece
    faceSignal[face] = DAMAGE;
    faceSignal[(face + 3) % 6] = DAMAGE;

    laserFaces[face] = true;
    laserFaces[(face + 3) % 6] = true;
  }
}

void takeDamage() {

  //update health total
  healingTimer.set(HEAL_TIME);
  updateHealthTotal();

  //if we're even alive, choose a random segment to damage
  if (healthTotal > 0) {
    //start a countdown
    byte countdown = random(healthTotal - 1) + 1;//a number 1 - health total

    //run through healthy segments, damage one when countdown becomes 0
    for (byte j = 0; j < 5; j++) {
      if (health[j] == HEALTHY || health[j] == HEALING) {//this bit is alive
        countdown--;//decrement the countdown
        if (countdown == 0) {//this the one we're gonna damage - let it rip
          health[j] = TRANSFERRING;//this will allow healing graphics to work, and will not change laser graphics
          healthTotal--;
          countdown = 255;//just a giant number, allows the loop to run through
        }
      }
    }
  }

}

void getHealed() {
  
  healingTimer.set(HEAL_TIME);
  
  //update health total
  updateHealthTotal();

  //if we're damaged and not dead, heal a segment
  if (healthTotal > 0 && healthTotal < 5) {
    //start a countdown
    byte countdown = random(4 - healthTotal) + 1;//we know health is from 1-4

    //run through healthy segments, heal one when countdown becomes 0
    for (byte j = 0; j < 5; j++) {
      if (health[j] == DAMAGED) {//this bit is damaged
        countdown--;//decrement the countdown
        if (countdown == 0) {//this the one we're gonna heal - let it rip
          health[j] = HEALING;
          healthTotal++;
          countdown = 255;//just a giant number, allows the loop to run through
        }
      }
    }
  }

}

void fullHeal() {

  healingTimer.set(HEAL_TIME);
  //reset health on this blink
  healthTotal = 5;
  for (byte i = 0; i < 5; i++) {
    health[i] = HEALING;
  }
}

void updateHealthTotal() {
  healthTotal = 0;
  for (byte i = 0; i < 5; i++) {
    if (health[i] == HEALTHY || health[i] == HEALING) {
      healthTotal++;
    }
  }
}

void fullBroadcast(byte sig) {
  FOREACH_FACE(f) {
    faceSignal[f] = sig;
  }
}

byte getBlinkMode(byte data) {
  return (data >> 4);//returns bits 1 2
}

byte getFaceSignal(byte data) {
  return ((data >> 1) & 7);//returns bits 3 4 5
}

bool getSyncVal(byte data) {
  return (data & 1);//returns bit 6
}

void syncLoop() {

  bool didNeighborChange = false;

  // look at our neighbors to determine if one of them passed go (changed value)
  // note: absent neighbors changing to not absent don't count
  FOREACH_FACE(f) {
    if (isValueReceivedOnFaceExpired(f)) {
      neighborState[f] = 2; // this is an absent neighbor
    }
    else {
      byte data = getLastValueReceivedOnFace(f);
      if (neighborState[f] != 2) {  // wasn't absent
        if (getSyncVal(data) != neighborState[f]) { // passed go (changed value)
          didNeighborChange = true;
        }
      }

      neighborState[f] = getSyncVal(data);  // update our record of state now that we've check it
    }
  }

  // if our neighbor passed go and we haven't done so within the buffer period, catch up and pass go as well
  // if we are due to pass go, i.e. timer expired, do so
  if ( (didNeighborChange && syncTimer.getRemaining() < PERIOD_DURATION - BUFFER_DURATION)
       || syncTimer.isExpired()
     ) {

    if (random(20) == 0) {
      syncTimer.set(PERIOD_DURATION_ALT); // aim to pass go in the defined duration
    } else {
      syncTimer.set(PERIOD_DURATION); // aim to pass go in the defined duration

    }
    syncVal = !syncVal; // change our value everytime we pass go
  }
}

#define WATER_HUE 150
#define WATER_MIN_BRIGHTNESS 50
#define WATER_MAX_BRIGHTNESS 150

void waterDisplay() { //just displays the water beneath any piece with missing bits (lasers, mirrors, damaged ships)

  byte syncProgress = map(syncTimer.getRemaining(), 0, PERIOD_DURATION, 0, 255);
  byte syncProgressSin = sin8_C(syncProgress);
  byte syncProgressMapped = map(syncProgressSin, 0, 255, WATER_MIN_BRIGHTNESS, WATER_MAX_BRIGHTNESS);

  //byte syncProgressMapped = map(syncTimer.getRemaining(), 0, PERIOD_DURATION, 0, 255);
  setColor(makeColorHSB(WATER_HUE, 255, syncProgressMapped));
}

void shipDisplay() {
  if (laserTimer.isExpired()) {//normal display
    for (byte i = 0; i < 5; i++) {
      if (health[i] == HEALTHY) {
        setColorOnFace(YELLOW, i);
      } else if (health[i] == HEALING) {
        byte healingBrightness = 255 - map(healingTimer.getRemaining(), 0, HEAL_TIME, 0, 255);
        setColorOnFace(dim(WHITE, healingBrightness), i);
      } else if (health[i] == TRANSFERRING) {
        byte transferBrightness = map(healingTimer.getRemaining(), 0, HEAL_TIME, 0, 255);
        setColorOnFace(dim(WHITE, transferBrightness), i);
      }
    }

    if (healthTotal > 0) {
      setColorOnFace(GREEN, 5);
    }
  } else {//laser display
    if (LASER_FULL_DURATION - laserTimer.getRemaining() < LASER_BLAST_DURATION) {//laser full display


      FOREACH_FACE(f) {
        if (laserFaces[f] == true) {//I'm in the laser path, just gonna be red this whole time
          setColorOnFace(RED, f);
        } else {//I'm a laser edge, I will fade to nothing immediately
          if (LASER_FULL_DURATION - laserTimer.getRemaining() < LASER_FADE) {//are we in the fade period?
            byte fade = 255 - map(LASER_FULL_DURATION - laserTimer.getRemaining(), 0, LASER_FADE, 0, 255);
            setColorOnFace(dim(RED, fade), f);
          } else {//we're already off
            setColorOnFace(OFF, f);
          }
        }
      }
    } else if (LASER_FULL_DURATION - laserTimer.getRemaining() > LASER_BLAST_DURATION && LASER_FULL_DURATION - laserTimer.getRemaining() < LASER_BLAST_DURATION + LASER_FADE) {
      //laser fade down
      FOREACH_FACE(f) {
        if (laserFaces[f] == true) {
          byte laserBrightness = 255 - map(LASER_FULL_DURATION - laserTimer.getRemaining() - LASER_BLAST_DURATION, 0, LASER_FADE, 0, 255);
          setColorOnFace(makeColorHSB(0, 255, laserBrightness), f);
        } else {
          setColorOnFace(OFF, f);
        }
      }
    } else if (laserTimer.getRemaining() < EXPLOSION_DURATION + WORLD_FADE_IN && laserTimer.getRemaining() > WORLD_FADE_IN) {

      //explosion time!
      byte currentHue = map(laserTimer.getRemaining(), WORLD_FADE_IN, EXPLOSION_DURATION + WORLD_FADE_IN, 0, HUE_BEGIN);
      FOREACH_FACE(f) {//do explosions on each face with phase offsets
        if (laserTimer.getRemaining() < EXPLOSION_DURATION + WORLD_FADE_IN - phaseOffset[f]) { //should this face be exploding?
          byte progress = map(laserTimer.getRemaining(), WORLD_FADE_IN, EXPLOSION_DURATION + WORLD_FADE_IN - phaseOffset[f], 0, 255); //255-0 as the explosion carries on
          setColorOnFace(makeColorHSB(currentHue, 255 - progress, progress), f);
        }
      }
    } else { //world fade up
      byte fadeupBrightness = 255 - map(laserTimer.getRemaining(), 0, WORLD_FADE_IN, 0, 255);
      for (byte i = 0; i < 5; i++) {
        if (health[i] == HEALTHY) {
          setColorOnFace(dim(YELLOW, fadeupBrightness), i);
        }
      }

      if (healthTotal > 0) {
        setColorOnFace(dim(GREEN, fadeupBrightness), 5);
      }
    }
  }
}

void laserDisplay() {

  if (!laserTimer.isExpired() && LASER_FULL_DURATION - laserTimer.getRemaining() < LASER_BLAST_DURATION) {//brighten the non-active faces while the laser is going strong
    setColor(RED);
  } else if (!laserTimer.isExpired() && LASER_FULL_DURATION - laserTimer.getRemaining() > LASER_BLAST_DURATION && LASER_FULL_DURATION - laserTimer.getRemaining() < LASER_BLAST_DURATION + LASER_FADE) {
    //we are in that little laser fading time, so we should do the same
    byte laserBrightness = 255 - map(LASER_FULL_DURATION - laserTimer.getRemaining() - LASER_BLAST_DURATION, 0, LASER_FADE, 0, 255);
    setColor(makeColorHSB(0, 255, laserBrightness));
  }

  setColorOnFace(RED, orientation);
  setColorOnFace(RED, (orientation + 2) % 6);
  setColorOnFace(RED, (orientation + 4) % 6);
}

void mirrorDisplay() {
  setColorOnFace(WHITE, orientation);
  setColorOnFace(WHITE, (orientation + 2) % 6);
}
