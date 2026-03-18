void homeXAxis(){
  checkIO();
  startTimedMotion(30, XStepper, -650);
  while(limitState){ //Wait for limit switch
    steppers.do_tasks();
    checkIO();
  }
  Serial.println("Complete!");
  steppers.stop(XStepper);
  currentX = 0;
  lastCommand = "homeX";
}

void gotoKey(int input){ //0 = previous, 1 = next
  //Check if any keys are set
  bool temp = false;
  int currentXKey = -1;
  for(int i = 0; i < 5; i++){
    if(key_Set[i]){
      temp = true;
    }
  }
  if(!temp){
    sendEvent("LOG:No Keys set");
    return;
  }

  int targetKey = -1;
  //First, check if we are on any keys
  for(int i = 0; i < 5; i++){
    if(key_Set[i]){
      if(key_X[i] == currentX && key_R[i] == currentR && key_T[i] == currentT){
        targetKey = i;
        currentXKey = i;
      }
    }
  }
  //If we aren't exactly on a key, find the closest key (X only currently)
  if(targetKey == -1){
    int resultX = 1000000;

    for(int i = 0; i < 5; i++){
      if(key_Set[i]){
        if(abs(currentX - key_X[i]) < resultX){
          resultX = abs(currentX - key_X[i]);
          targetKey = i;
        }
      }
    }
  }

  //Find next set key in list
  if(input == 0){
    targetKey = findPreviousKey(targetKey);
  } else {
    targetKey = findNextKey(targetKey);
  }

  if(targetKey == -1){
    sendEvent("LOG:No more keys!");
    return;
  }

  if(currentX != key_X[targetKey]){
    startTimedMotion(5, XStepper, key_X[targetKey]);
  }
  if(currentR != key_R[targetKey]){
    startTimedMotion(5, RStepper, key_R[targetKey]);
  }
  if(currentT != key_T[targetKey]){
    startTimedMotion(5, TStepper, key_T[targetKey]);
  }
  while(steppers.get_remaining_steps(XStepper) > 0 || steppers.get_remaining_steps(RStepper) > 0 || steppers.get_remaining_steps(TStepper) > 0){
    steppers.do_tasks();
  }
  sendEvent("LOG:Key " + String(targetKey + 1));
  currentR = key_R[targetKey];
  currentX = key_X[targetKey];
  currentT = key_T[targetKey];

  if(input == 0){
    lastCommand = "previousKey";
  } else {
    lastCommand = "nextKey";
  }

}

int findPreviousKey(int index){
  for(int i = index -1; i >= 0; i--){
    if(key_Set[i]){
      return i;
    }
  }
  return -1;
}

int findNextKey(int index){
  for(int i = index +1; i < 5; i++){
    if(key_Set[i]){
      return i;
    }
  }
  return -1;
}

void returnToKey1(){
  int firstKeySet;
  for(int i = 5; i > 0; i--){
    if(key_Set[i-1] == true){
      firstKeySet = i-1;
    }
  }
  if(currentR == key_R[firstKeySet] && currentX == key_X[firstKeySet] && currentT == key_T[firstKeySet]){
    inStartLocation = true;
    return;
  }
  if(currentX != key_X[firstKeySet]){
    startTimedMotion(10, XStepper, key_X[firstKeySet]);
  }
  if(currentR != key_R[firstKeySet]){
    startTimedMotion(10, RStepper, key_R[firstKeySet]);
  }
  if(currentT != key_T[firstKeySet]){
    startTimedMotion(10, TStepper, key_T[firstKeySet]);
  }
  while(steppers.get_remaining_steps(XStepper) > 0 || steppers.get_remaining_steps(RStepper) > 0 || steppers.get_remaining_steps(TStepper) > 0){
    steppers.do_tasks();
  }
  currentR = key_R[firstKeySet];
  currentX = key_X[firstKeySet];
  currentT = key_T[firstKeySet];
  inStartLocation = true;
  lastCommand = "returnToP1";
}

void programmedMove(){
  bool firstKey = true;

  for(int i = 0; i < 5; i++){
    if(key_Set[i]){ //If this key has been set
      if(!firstKey){
        sendEvent("LOG:Key: " + String(i));
        if(key_Delay[i] > 0){
          sendEvent("LOG:Delay " + String(key_Delay[i]) + "s");
          delay(key_Delay[i] * 1000); //Delay for amount of time specified
        }
        sendEvent("LOG:Begin Move");
        if(key_Time[i] < 60){ //If movement is short and fast, use acceleration. If not, use timedMotion for better precision on long moves.
          startAccelMotion(key_Time[i], (float)key_Time[i] / 5, key_X[i], key_R[i], key_T[i]);
        } else {
          startTimedMotion(key_Time[i], XStepper, key_X[i]);
          startTimedMotion(key_Time[i], RStepper, key_R[i]);
          startTimedMotion(key_Time[i], TStepper, key_T[i]);
        }
      } else {
        firstKey = false;
      }
      
      while(steppers.get_remaining_steps(XStepper) > 0 || steppers.get_remaining_steps(RStepper) > 0 || steppers.get_remaining_steps(TStepper) > 0){
        steppers.do_tasks();
      }
      if(i != 0){
        sendEvent("LOG:Key " + String(i) + " Complete!");
      }
      currentR = key_R[i];
      currentX = key_X[i];
      currentT = key_T[i];

    }
  }

  sendEvent("LOG:Program Complete!");

}

void startTimedMotion(float time, int motor, float target){ //Time in seconds, motor number, distance in Millimeters or Revs
  inStartLocation = false;
  
  int stepMultiplier;
  long delay;
  float steps;
  float currentPos;

  //Establish correct steps per mm/rev
  if(motor == XStepper){ //X Axis
    stepMultiplier = X_AXIS_STEPSMM;
    currentPos = currentX;
  } else if(motor == RStepper){ //R Axis
    stepMultiplier = R_AXIS_STEPSREV;
    currentPos = currentR;
  } else if(motor == TStepper){
    stepMultiplier = T_AXIS_STEPSREV;
    currentPos = currentT;
  }

  //Calculate distance to move
  steps = target - currentPos;

  if(motor == RStepper || motor == TStepper){
    steps = steps / 360;
  }

  //No Move required
  if(steps == 0){
    Serial.println("Steps to target = 0");
    return;
  }

  //Calculate total number of steps and delay
  steps = steps * stepMultiplier;
  delay = ((int64_t)time * 1000000L) / abs(steps);

  if(debug){
    Serial.println("Timed Move");
    Serial.print("Time = ");
    Serial.println(time);
    Serial.print("Motor = ");
    Serial.println(motor);
    Serial.print("Delay between steps = ");
    Serial.println(delay);
    Serial.print("Steps to travel = ");
    Serial.println(steps);
  }

  //Establish direction based on positive or negative steps
  if(steps < 0){
    if(motor == XStepper){
      digitalWrite(X_DIRECTION_PIN, HIGH);
    } else if(motor == RStepper){
      digitalWrite(R_DIRECTION_PIN, HIGH);
    } else if(motor == TStepper){
      digitalWrite(T_DIRECTION_PIN, LOW); // Tilt is inverted
    }
  } else {
    if(motor == XStepper){
      digitalWrite(X_DIRECTION_PIN, LOW);
    } else if(motor == RStepper){
      digitalWrite(R_DIRECTION_PIN, LOW);
    } else if(motor == TStepper){
      digitalWrite(T_DIRECTION_PIN, HIGH); // Tilt is inverted
    }
  }
  //Begin move
  steppers.start_finite(motor, delay, abs(steps));
}

void startAccelMotion(float totalTime, float accelTime, float targetX, float targetR, float targetT){ //Total time for move in seconds, total time for accel/decel in seconds, Total X move steps, Total R move steps, Total T move steps.
  inStartLocation = false;
  
  //Used for keeping track of partial steps and final step count for updating motor position.
  float xStepRemainder = 0.0;
  float rStepRemainder = 0.0;
  float tStepRemainder = 0.0;
  int totalXSteps = 0;
  int totalRSteps = 0;
  int totalTSteps = 0;
  
  //Used to control which motors move
  bool xMove = false;
  bool rMove = false;
  bool tMove = false;

  float xAccelSteps;
  long xCruiseSteps;
  float xCruiseDelay;
  float rAccelSteps;
  long rCruiseSteps;
  float rCruiseDelay;
  float tAccelSteps;
  long tCruiseSteps;
  float tCruiseDelay;

  //Calculate Move Distances
  targetX = targetX - currentX;
  targetR = targetR - currentR;
  targetT = targetT - currentT;

  //Set Directions
  if(targetX < 0){
    digitalWrite(X_DIRECTION_PIN, HIGH);
  } else {
    digitalWrite(X_DIRECTION_PIN, LOW);
  }
  if(targetR < 0){
    digitalWrite(R_DIRECTION_PIN, HIGH);
  } else {
    digitalWrite(R_DIRECTION_PIN, LOW);
  }
  if(targetT < 0){
    digitalWrite(T_DIRECTION_PIN, LOW);
  } else {
    digitalWrite(T_DIRECTION_PIN, HIGH);
  }

  //Enable moves for > 0 distance
  if(targetX != 0){
    xMove = true;
    targetX = targetX * X_AXIS_STEPSMM;
    Serial.print("targetX = ");
    Serial.println(targetX, 4);
  }
  if(targetR != 0){
    rMove = true;
    targetR = targetR / 360;
    targetR = targetR * R_AXIS_STEPSREV;
    Serial.print("targetR = ");
    Serial.println(targetR, 4);
  }
  if(targetT != 0){
    tMove = true;
    targetT = targetT / 360;
    targetT = targetT * T_AXIS_STEPSREV;
    Serial.print("targetT = ");
    Serial.println(targetT, 4);
  }

  //Ensure positive targets
  targetX = abs(targetX);
  targetR = abs(targetR);
  targetT = abs(targetT);

  float cruiseTime = (float)totalTime - (accelTime * 2); //Time spent at cruise speed

  float accelToFullRatio = (float)(accelTime * 2) / totalTime; //Ratio of time spent accel/decel vs full speed.

  if(debug){
    Serial.print("cruiseTime: ");
    Serial.println(cruiseTime);
    Serial.print("accelToFullRatio: ");
    Serial.println(accelToFullRatio);
  }

  //X Motor
  if(xMove){
    xAccelSteps = ((float)targetX * accelToFullRatio) / 2; //Number of steps to take for complete accel X motor
    xCruiseSteps = abs(targetX) - xAccelSteps; //Number of steps to take at cruise speed
    xCruiseDelay = (float)cruiseTime * 1000000L / xCruiseSteps; //Delay time for cruise speed
    xAccelSteps = (xAccelSteps / accelSteps) / 2; //Number of steps to take per acceleration Loop for X motor
    if(debug){
      Serial.print("xCruiseSteps: ");
      Serial.println(xCruiseSteps);
      Serial.print("xCruiseDelay: ");
      Serial.println(xCruiseDelay);
      Serial.print("xAccelSteps: ");
      Serial.println(xAccelSteps);
    }
  }
  //R Motor
  if(rMove){
    rAccelSteps = ((float)targetR * accelToFullRatio) / 2;
    rCruiseSteps = targetR - rAccelSteps;
    rCruiseDelay = (float)cruiseTime * 1000000L / rCruiseSteps;
    rAccelSteps = (rAccelSteps / accelSteps) / 2;
    if(debug){
      Serial.print("rCruiseSteps: ");
      Serial.println(rCruiseSteps);
      Serial.print("rCruiseDelay: ");
      Serial.println(rCruiseDelay);
      Serial.print("rAccelSteps: ");
      Serial.println(rAccelSteps);
    }
  }
  //T Motor
  if(tMove){
    tAccelSteps = ((float)targetT * accelToFullRatio) / 2;
    tCruiseSteps = targetT - tAccelSteps;
    tCruiseDelay = (float)cruiseTime * 1000000L / tCruiseSteps;
    tAccelSteps = (tAccelSteps / accelSteps) / 2;
    if(debug){
      Serial.print("tCruiseSteps: ");
      Serial.println(tCruiseSteps);
      Serial.print("tCruiseDelay: ");
      Serial.println(tCruiseDelay);
      Serial.print("tAccelSteps: ");
      Serial.println(tAccelSteps);
    }
  }

  //Arrays for Precalculated Ramp Values
  float accDelayX[accelSteps + 1];
  float accDelayR[accelSteps + 1];
  float accDelayT[accelSteps + 1];

  int xStepsThisCycle[accelSteps + 1] = {0};
  int rStepsThisCycle[accelSteps + 1] = {0};
  int tStepsThisCycle[accelSteps + 1] = {0};

  //Pre-Calculate Accel/decel Ramp Values
  for(int i = 1; i <= accelSteps; i++){
    float exactSteps;
    int stepsThisCycle;
    if(xMove){
      exactSteps = abs(xAccelSteps) + xStepRemainder;
      stepsThisCycle = (int)exactSteps;
      xStepRemainder = exactSteps - stepsThisCycle;
      totalXSteps += stepsThisCycle * 2; //Steps need to be counted twice since they are used for Accel and Decel
      accDelayX[i] = getDelayForCycle(i, abs(xCruiseDelay), stepsThisCycle, accelSteps, accelTime);
      xStepsThisCycle[i] = stepsThisCycle;
    }
    if(rMove){
      exactSteps = abs(rAccelSteps) + rStepRemainder;
      stepsThisCycle = (int)exactSteps;
      rStepRemainder = exactSteps - stepsThisCycle;
      totalRSteps += stepsThisCycle * 2;
      accDelayR[i] = getDelayForCycle(i, abs(rCruiseDelay), stepsThisCycle, accelSteps, accelTime);
      rStepsThisCycle[i] = stepsThisCycle;
    }
    if(tMove){
      exactSteps = abs(tAccelSteps) + tStepRemainder;
      stepsThisCycle = (int)exactSteps;
      tStepRemainder = exactSteps - stepsThisCycle;
      totalTSteps += stepsThisCycle * 2;
      accDelayT[i] = getDelayForCycle(i, abs(tCruiseDelay), stepsThisCycle, accelSteps, accelTime);
      tStepsThisCycle[i] = stepsThisCycle;
    }
  }
  Serial.println("Accel Compute Complete");

  //Acceleration
  int tempTimer = millis();

  for(int i = 1; i <= accelSteps; i++){
    if(xMove){
      steppers.start_finite(XStepper, accDelayX[i], xStepsThisCycle[i]);
    }
    if(rMove){
      steppers.start_finite(RStepper, accDelayR[i], rStepsThisCycle[i]);
    }
    if(tMove){
      steppers.start_finite(TStepper, accDelayT[i], tStepsThisCycle[i]);
    }

    while(steppers.get_remaining_steps(XStepper) > 0 || steppers.get_remaining_steps(RStepper) > 0 || steppers.get_remaining_steps(TStepper) > 0){
      steppers.do_tasks();
    }
  }

  //Cruise
  if(xMove){
    steppers.start_finite(XStepper, abs(xCruiseDelay), abs(xCruiseSteps));
    totalXSteps += abs(xCruiseSteps);
  }
  if(rMove){
    steppers.start_finite(RStepper, abs(rCruiseDelay), abs(rCruiseSteps));
    totalRSteps += abs(rCruiseSteps);
  }
  if(tMove){
    steppers.start_finite(TStepper, abs(tCruiseDelay), abs(tCruiseSteps));
    totalTSteps += abs(tCruiseSteps);
  }

  while(steppers.get_remaining_steps(XStepper) > 0 || steppers.get_remaining_steps(RStepper) > 0 || steppers.get_remaining_steps(TStepper) > 0){
    steppers.do_tasks();
  }

  //Deceleration
  for(int i = accelSteps; i > 0; i--){
    if(xMove){
      steppers.start_finite(XStepper, accDelayX[i], xStepsThisCycle[i]);
    }
    if(rMove){
      steppers.start_finite(RStepper, accDelayR[i], rStepsThisCycle[i]);
    }
    if(tMove){
      steppers.start_finite(TStepper, accDelayT[i], tStepsThisCycle[i]);
    }
    
    while(steppers.get_remaining_steps(XStepper) > 0 || steppers.get_remaining_steps(RStepper) > 0 || steppers.get_remaining_steps(TStepper) > 0){
      steppers.do_tasks();
    }

  }
  if(debug){
    Serial.println("Movement Complete!");
    Serial.print("Time = ");
    Serial.println(millis() - tempTimer);
    Serial.print("Total X Steps: ");
    Serial.println(totalXSteps);
    Serial.print("Total R Steps: ");
    Serial.println(totalRSteps);
    Serial.print("Total T Steps: ");
    Serial.println(totalTSteps);
  }
}

void startSpeedMotion(int speed, int motor, float target){ //Speed in steps per Second, motor number 1 or 0, distance in Millimeters or Revs
  inStartLocation = false;
  
  int stepMultiplier;
  long delay;
  float steps;
  float currentPos;

  //Establish correct steps per mm/rev
  if(motor == XStepper){ //X Axis
    stepMultiplier = X_AXIS_STEPSMM;
    currentPos = currentX;
  } else if(motor == RStepper){ //R Axis
    stepMultiplier = R_AXIS_STEPSREV;
    currentPos = currentR;
  } else if(motor == TStepper){
    stepMultiplier = T_AXIS_STEPSREV;
    currentPos = currentT;
  }

  //Calculate distance to move
  steps = target - currentPos;

  if(motor == RStepper || motor == TStepper){
    steps = steps / 360;
  }

  //No Move required
  if(steps == 0){
    return;
  }

  //Calculate total number of steps and delay
  steps = steps * stepMultiplier;
  Serial.print("Steps = ");
  Serial.println(steps);
  delay = 1000000L / speed;

  //Establish direction based on positive or negative steps
  if(steps < 0){
    if(motor == XStepper){
      digitalWrite(X_DIRECTION_PIN, HIGH);
    } else if(motor == RStepper){
      digitalWrite(R_DIRECTION_PIN, HIGH);
    } else if(motor == TStepper){
      digitalWrite(T_DIRECTION_PIN, HIGH);
    }
  } else {
    if(motor == XStepper){
      digitalWrite(X_DIRECTION_PIN, LOW);
    } else if(motor == RStepper){
      digitalWrite(R_DIRECTION_PIN, LOW);
    } else if(motor == TStepper){
      digitalWrite(T_DIRECTION_PIN, LOW);
    }
  }

  //Begin move
  steppers.start_finite(motor, delay, abs(steps));
}

float getDelayForCycle(int cycleIndex, float cruiseDelay, float accelPulses, int accelSteps, float accelTime){
  float totalTimeUS = accelTime * 1000000.0;
  float targetSum = totalTimeUS / accelPulses;

  float low = cruiseDelay;  // minimum possible
  float high = cruiseDelay * 50.0;  // arbitrary upper bound
  float d1;

  for(int iter = 0; iter < 25; iter++){
    d1 = (low + high) * 0.5;

    float v1 = 1.0 / d1;
    float v2 = 1.0 / cruiseDelay;

    float sum = 0.0;

    for(int i = 0; i < accelSteps; i++){
      float x = (float)i / (accelSteps - 1);
      float v = v1 + (v2 - v1) * x;
      sum += 1.0 / v;
    }

    if(sum > targetSum)
        high = d1;  //too slow so reduce d1
    else
        low = d1; //too fast so increase d1
  }

  float v1 = 1.0 / d1;
  float v2 = 1.0 / cruiseDelay;

  float x = (float)(cycleIndex - 1) / (accelSteps - 1);
  float v = v1 + (v2 - v1) * x;

  return 1.0 / v;
}

void jogX(int mm) {
  if(lastCommand != "jogX"){
    targetX = currentX;
  }
  
  if(targetX + mm > maxX){
    targetX = maxX;
  } else if(targetX + mm < 0){
    targetX = 0;
  } else {
    targetX = targetX + mm;
  }

  float moveTime = (float)mm / jogXAccelMultiplier;
  
  startAccelMotion(moveTime, (float)moveTime / 5, targetX, currentR, currentT);

  currentX = targetX;
  lastCommand = "jogX";
}

void jogR(int input) {
  if(lastCommand != "jogR"){
    targetR = currentR;
  }

  float moveTime = (float)input / jogRTAccelMultiplier;

  targetR += (float)input;
  startAccelMotion(moveTime, (float)moveTime / 5, currentX, targetR, currentT);

  currentR = targetR;
  lastCommand = "jogR";
}

void jogT(int input) {
  if(lastCommand != "jogT"){
    targetT = currentT;
  }

  float moveTime = (float)input / jogRTAccelMultiplier;

  targetT += (float)input;

  startAccelMotion(moveTime, (float)moveTime / 5, currentX, currentR, targetT);

  currentT = targetT;
  lastCommand = "jogT";
}