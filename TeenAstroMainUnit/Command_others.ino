
void Command_dollar()
{
  switch (command[1])
  {
  case '$':
    for (int i = 0; i < XEEPROM.length(); i++)
    {
      XEEPROM.write(i, 0);
    }
  case '!':
    Serial.end();
    Serial1.end();
    Serial2.end();
    delay(1000);
    _reboot_Teensyduino_();
    break;
  case 'X':
    initmotor(true);
    break;
  default:
    commandError = true;
    break;
  }
}

//----------------------------------------------------------------------------------
//   A - Alignment Commands

void Command_A()
{
  switch (command[1])
  {
  case 'W':
    saveAlignModel();
    break;
  case 'C':
    initTransformation(true);
    syncPolarHome();
    break;
  case '0':
    // telescope should be set in the polar home for a starting point
    initTransformation(true);
    syncPolarHome();
    // enable the stepper drivers
    enable_Axis(true);
    delay(10);
    // start tracking
    sideralTracking = true;
    lastSetTrakingEnable = millis();
    break;
  case '2':
  {
    double  newTargetHA = haRange(rtk.LST() * 15.0 - newTargetRA);
    double Azm, Alt;
    EquToHorApp(newTargetHA, newTargetDec, &Azm, &Alt);
    if (alignment.getRefs() == 0)
    {
      syncAzAlt(Azm, Alt, GetPierSide());
    }

    cli();
    double Axis1 = posAxis1 / StepsPerDegreeAxis1;
    double Axis2 = posAxis2 / StepsPerDegreeAxis2;
    sei();

    alignment.addReferenceDeg(Azm, Alt, Axis1, Axis2);
    if (alignment.getRefs() == 2)
    {
      alignment.calculateThirdReference();
      if (alignment.isReady())
      {
        hasStarAlignment = true;
        cli();
        targetAxis1 = posAxis1;
        targetAxis2 = posAxis2;
        sei();
      }
    }
    break;
  }
  case '3':
  {
    double newTargetHA = haRange(rtk.LST() * 15.0 - newTargetRA);
    double Azm, Alt;
    EquToHorApp(newTargetHA, newTargetDec, &Azm, &Alt);
    if (alignment.getRefs() == 0)
    {
      syncAzAlt(Azm, Alt, GetPierSide());
    }
    cli();
    double Axis1 = posAxis1 / StepsPerDegreeAxis1;
    double Axis2 = posAxis2 / StepsPerDegreeAxis2;
    sei();
    alignment.addReferenceDeg(Azm, Alt, Axis1, Axis2);
    if (alignment.isReady())
    {
      hasStarAlignment = true;
      cli();
      targetAxis1 = posAxis1;
      targetAxis2 = posAxis2;
      sei();
    }
    break;
  }
  default:
    commandError = true;
  }

}


//----------------------------------------------------------------------------------
//   B - Reticule/Accessory Control
//  :B+#   Increase reticule Brightness
//         Returns: Nothing
//  :B-#   Decrease Reticule Brightness
//         Returns: Nothing
void Command_B()
{
  if (command[1] != '+' && command[1] != '-')
    return;
#ifdef RETICULE_LED_PINS
  if (reticuleBrightness > 255) reticuleBrightness = 255;
  if (reticuleBrightness < 31) reticuleBrightness = 31;

  if (command[1] == '-') reticuleBrightness /= 1.4;
  if (command[1] == '+') reticuleBrightness *= 1.4;

  if (reticuleBrightness > 255) reticuleBrightness = 255;
  if (reticuleBrightness < 31) reticuleBrightness = 31;

  analogWrite(RETICULE_LED_PINS, reticuleBrightness);
#endif
  quietReply = true;
}

//   C - Sync Control
//  :CA#   Synchonize the telescope with the current Azimuth and Altitude coordinates
//         Returns: Nothing (Sync's fail silently)
//  :CS#   Synchonize the telescope with the current right ascension and declination coordinates
//         Returns: Nothing (Sync's fail silently)
//  :CM#   Synchonize the telescope with the current database object (as above)
//         Returns: "N/A#" on success, "En#" on failure where n is the error code per the :MS# command
void Command_C()
{
  if ((parkStatus == PRK_UNPARKED) &&
    !movingTo &&
    (command[1] == 'A' || command[1] == 'M' || command[1] == 'S' || command[1] == 'U'))
  {
    PierSide targetPierSide = GetPierSide();
    if (newTargetPierSide != PIER_NOTVALID)
    {
      targetPierSide = newTargetPierSide;
      newTargetPierSide = PIER_NOTVALID;
    }
    switch (command[1])
    {
    case 'M':
    case 'S':
    {
      double newTargetHA = haRange(rtk.LST() * 15.0 - newTargetRA);
      i = syncEqu(newTargetHA, newTargetDec, targetPierSide);
      break;
    }
    case 'U':
    {
      // :CU# sync with the User Defined RA DEC
      newTargetRA = (double)XEEPROM.readFloat(EE_RA);
      newTargetDec = (double)XEEPROM.readFloat(EE_DEC);
      double newTargetHA = haRange(rtk.LST() * 15.0 - newTargetRA);
      i = syncEqu(newTargetHA, newTargetDec, targetPierSide);
      break;
    }
    case 'A':
      i = syncAzAlt(newTargetAzm, newTargetAlt, targetPierSide);
      break;
    }
    i = 0;
    if (command[1] == 'M' || command[1] == 'A' || command[1] == 'U')
    {
      if (i == 0) strcpy(reply, "N/A");
      if (i > 0) { reply[0] = 'E'; reply[1] = '0' + i; reply[2] = 0; }
    }
    quietReply = true;
  }
  else
  {
    commandError = true;
  }
}


//----------------------------------------------------------------------------------
//   D - Distance Bars
//  :D#    returns an "\0x7f#" if the mount is moving, otherwise returns "#".
void Command_D()
{
  if (command[1] != 0)
    return;
  if (movingTo)
  {
    reply[0] = (char)127;
    reply[1] = 0;
  }
  else
  {
    reply[0] = 0;
  }
  quietReply = true;
}

//----------------------------------------------------------------------------------
//  h - Home Position Commands
void Command_h()
{
  switch (command[1])
  {
  case 'F':
    //  :hF#   Reset telescope at the home position.  This position is required for a Cold Start.
    //         Point to the celestial pole with the counterweight pointing downwards (CWD position).
    //         Returns: Nothing
    syncPolarHome();
    quietReply = true;
    break;
  case 'C':
    //  :hC#   Moves telescope to the home position
    //          Return: 0 on failure
    //                  1 on success
    if (!goHome()) commandError = true;
    break;
  case 'O':
    // : hO#   Reset telescope at the Park position if Park position is stored.
    //          Return: 0 on failure
    //                  1 on success
    if (!syncAtPark()) commandError = true;
    break;
  case 'P':
    // : hP#   Goto the Park Position
      //         Returns: Nothing
    if (park()) commandError = true;
    break;
  case 'Q':
    //  :hQ#   Set the park position
    //          Return: 0 on failure
    //                  1 on success
    if (!setPark()) commandError = true;
    break;
  case 'R':
    //  :hR#   Restore parked telescope to operation
    //          Return: 0 on failure
    //                  1 on success
    unpark();
    break;
  default:
    commandError = true;
    break;
  }
}

//----------------------------------------------------------------------------------
//   Q - Halt Movement Commands
void Command_Q()
{
  switch (command[1])
  {
  case 0:
    //  :Q#    Halt all slews, stops goto
    //         Returns: Nothing
    doSpiral = false;
    if ((parkStatus == PRK_UNPARKED) || (parkStatus == PRK_PARKING))
    {
      if (movingTo)
      {
        abortSlew = true;
      }
      else if (GuidingState == GuidingRecenter || GuidingState == GuidingST4 || GuidingState == GuidingPulse)
      {
        if (guideDirAxis1)
          StopAxis1();
        if (guideDirAxis2)
          StopAxis2();
      }
      else
      {
        if (guideDirAxis1)
          guideDirAxis1 = 'b';
        if (guideDirAxis2)
          guideDirAxis2 = 'b';
      }
    }
    quietReply = true;
    break;
  case 'e':
  case 'w':
    //  :Qe# & Qw#   Halt east/westward Slews
    //         Returns: Nothing
  {
    if ((parkStatus == PRK_UNPARKED) && !movingTo)
    {
      if (guideDirAxis1)
        StopAxis1();
    }
    quietReply = true;
  }
  break;

  case 'n':
  case 's':
    //  :Qn# & Qs#   Halt north/southward Slews
    //         Returns: Nothing
  {
    if ((parkStatus == PRK_UNPARKED) && !movingTo)
    {
      if (guideDirAxis2)
        StopAxis2();
    }
    quietReply = true;
  }
  break;

  default:
    commandError = true;
    break;
  }
}

//----------------------------------------------------------------------------------
//   R - Slew Rate Commands
void Command_R()
{
  //  :RG#   Set Slew rate to Guiding Rate (slowest) user defined
  //  :RC#   Set Slew rate to Centering rate (2nd slowest) 4X
  //  :RM#   Set Slew rate to Find Rate (2nd Fastest) 32X
  //  :RS#   Set Slew rate to max (fastest) ?X (MaxRate)
  //  :Rn#   Set Slew rate to n, where n=0..9
  //         Returns: Nothing
#define RG 0
#define RC 1
#define RM 2
#define RS 3
  int i = 5;
  switch (command[1])
  {
  case 'G':
    i = RG;
    break;
  case 'C':
    i = RC;
    break;
  case 'M':
    i = RM;
    break;
  case 'S':
    i = RS;
    break;
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
    i = command[1] - '0';
    break;
  default:
    commandError = true;
    return;
  }
  if (!movingTo && GuidingState == GuidingOFF)
  {
    enableGuideRate(i, false);
  }
  else
  {
    commandError = true;
  }

  quietReply = true;
}

//----------------------------------------------------------------------------------
//   T - Tracking Commands
//  :T+#   Master sidereal clock faster by 0.1 Hertz (I use a fifth of the LX200 standard, stored in XEEPROM)
//  :T-#   Master sidereal clock slower by 0.1 Hertz (stored in XEEPROM)
//  :TS#   Track rate solar
//  :TL#   Track rate lunar
//  :TQ#   Track rate sidereal
//  :TR#   Master sidereal clock reset (to calculated sidereal rate, stored in EEPROM)
//  :TK#   Track rate king
//  :Te#   Tracking enable  (replies 0/1)
//  :Td#   Tracking disable (replies 0/1)
//  :Tr#   Track refraction enable  (replies 0/1)
//  :Tn#   Track refraction disable (replies 0/1)
//         Returns: Nothing
void Command_T()
{

  switch (command[1])

  {
  case '+':
    siderealInterval -= HzCf * (0.02);
    quietReply = true;
    break;
  case '-':
    siderealInterval += HzCf * (0.02);
    quietReply = true;
    break;
  case 'S':
    // solar tracking rate 60Hz 
    SetTrackingRate(TrackingSolar);
    sideralMode = SIDM_SUN;
    correct_tracking = false;
    quietReply = true;
    break;
  case 'L':
    // lunar tracking rate 57.9Hz
    SetTrackingRate(TrackingLunar);
    sideralMode = SIDM_MOON;
    correct_tracking = false;
    quietReply = true;
    break;
  case 'Q':
    // sidereal tracking rate
    SetTrackingRate(default_tracking_rate);
    sideralMode = SIDM_STAR;
    correct_tracking = XEEPROM.read(EE_corr_track);
    correct_tracking = false;
    quietReply = true;
    break;
  case 'R':
    // reset master sidereal clock interval
    siderealInterval = masterSiderealInterval;
    quietReply = true;
    break;
  case 'K':
    // king tracking rate 60.136Hz
    SetTrackingRate(0.99953004401);
    correct_tracking = false;
    quietReply = true;
    break;
  case 'e':
    if (parkStatus == PRK_UNPARKED)
    {
      lastSetTrakingEnable = millis();
      atHome = false;
      sideralTracking = true;
    }
    else
      commandError = true;
    break;
  case 'd':
    if (parkStatus == PRK_UNPARKED)
    {
      sideralTracking = false;
    }
    else
      commandError = true;
    break;

  case 'r':
    // turn compensation on, defaults to base sidereal tracking rate
    correct_tracking = true;
    SetTrackingRate(default_tracking_rate);
    break;

  case 'n':
    // turn compensation off, sidereal tracking rate resumes     
    correct_tracking = false;
    SetTrackingRate(default_tracking_rate);
    break;

  default:
    commandError = true;
    break;
  }

  // Only burn the new rate if changing the sidereal interval
  if ((!commandError) && ((command[1] == '+') || (command[1] == '-') ||
    (command[1] == 'R')))
  {
    XEEPROM.writeLong(EE_siderealInterval, siderealInterval);
    updateSideral();
  }
}

//   U - Precision Toggle
//  :U#    Toggle between low/hi precision positions
//         Low -  RA/Dec/etc. displays and accepts HH:MM.M sDD*MM
//         High - RA/Dec/etc. displays and accepts HH:MM:SS sDD*MM:SS
//         Returns Nothing
void Command_U()
{
  if (command[1] == 0)
  {
    highPrecision = !highPrecision;
    quietReply = true;
  }
  else
    commandError = true;
}

//   W - Site Select/Site get
//  :Wn#
//         Sets current site to n, 0..3 or queries site with ?
//         Returns: Nothing or current site ?#
void Command_W()
{
  if ((command[1] >= '0') && (command[1] <= '3'))
  {
    uint8_t currentSite = command[1] - '0';
    XEEPROM.write(EE_currentSite, currentSite);
    localSite.ReadSiteDefinition(currentSite);
    rtk.resetLongitude(*localSite.longitude());
    initCelestialPole();
    initTransformation(true);
    quietReply = true;
  }
  else
    if (command[1] == '?') {
      quietReply = true;
      sprintf(reply, "%d", localSite.siteIndex());
    }
    else
      commandError = true;
}
