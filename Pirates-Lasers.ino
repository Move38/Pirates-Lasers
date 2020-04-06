enum blinkModes {SHIP, LASER, MIRROR};
byte blinkMode = SHIP;
byte orientation = 0;//travels around from

enum signals {INERT, HEAL, DAMAGE, RESET, RESOLVE};
byte faceSignal[6] = {INERT, INERT, INERT, INERT, INERT, INERT};

enum healthStates {DAMAGED, HEALTHY, HEALING};
byte health[5] = {HEALTHY, HEALTHY, HEALTHY, HEALTHY, HEALTHY};
byte healthTotal = 5;
Timer healingTimer;
#define HEAL_TIME 1000

Timer laserTimer;//used when a laser blast has been recieved
bool laserFaces[6] = {false, false, false, false, false, false};

// SYNCHRONIZED WAVES
Timer syncTimer;
#define PERIOD_DURATION 4000
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
  if (blinkMode == MIRROR) {

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
  } else {
    faceSignal[face] = DAMAGE;
    faceSignal[(face + 3) % 6] = DAMAGE;

    laserFaces[face] = true;
    laserFaces[(face + 3) % 6] = true;
  }
}

void takeDamage() {

  //update health total
  updateHealthTotal();

  //if we're even alive, choose a random segment to damage
  if (healthTotal > 0) {
    //start a countdown
    byte countdown = random(healthTotal - 1) + 1;//a number 1 - health total

    //run through healthy segments, damage one when countdown becomes 0
    for (byte j = 0; j < 5; j++) {
      if (health[j] != DAMAGED) {//this bit is alive
        countdown--;//decrement the countdown
        if (countdown == 0) {//this the one we're gonna damage - let it rip
          health[j] = DAMAGED;
          healthTotal--;
          countdown = 255;//just a giant number, allows the loop to run through
        }
      }
    }
  }

}

void getHealed() {

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
  //reset health on this blink
  healthTotal = 5;
  for (byte i = 0; i < 5; i++) {
    health[i] = HEALTHY;
  }
}

void updateHealthTotal() {
  healthTotal = 0;
  for (byte i = 0; i < 5; i++) {
    if (health[i] == HEALTHY) {
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

    syncTimer.set(PERIOD_DURATION); // aim to pass go in the defined duration
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
  //just light up the ship in the healthy spots, plus the healing port

  for (byte i = 0; i < 5; i++) {
    if (health[i] == HEALTHY) {
      setColorOnFace(YELLOW, i);
    }
  }

  if (healthTotal > 0) {
    setColorOnFace(GREEN, 5);
  }
}

void laserDisplay() {
  setColorOnFace(RED, orientation);
  setColorOnFace(RED, (orientation + 2) % 6);
  setColorOnFace(RED, (orientation + 4) % 6);
}

void mirrorDisplay() {
  setColorOnFace(WHITE, orientation);
  setColorOnFace(WHITE, (orientation + 2) % 6);
}