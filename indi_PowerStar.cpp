/***************************************************************************
File:           indi_PowerStar.cpp
Version:        202110303
Author:         Sifan
Desc:           PowerStar INDI driver
Requires:       PScontrol.cpp hidapi.c
***************************************************************************/

#include "indi_PowerStar.h"
#include "config.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wformat-truncation="

/***************************************************************/
/* BoilerPlate for INDI */
/***************************************************************/
// We declare an auto pointer to PSpower
static std::unique_ptr<PSpower> mydriver(new PSpower());
    
void ISGetProperties(const char *dev)
{
    mydriver->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    mydriver->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    mydriver->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    mydriver->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
               char *formats[], char *names[], int n)
{
    mydriver->ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

void ISSnoopDevice(XMLEle *root)
{
    mydriver->ISSnoopDevice(root);
}

/***************************************************************/
/* Startup config */
/***************************************************************/
PSpower::PSpower() : FI(this), WI(this)
{
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_CAN_SYNC);
    //setSupportedConnections(CONNECTION_NONE);
    setVersion(PS_VERSION_MAJOR, PS_VERSION_MINOR);
}

const char *PSpower::getDefaultName()
{
    return "Power*Star";
}

/***************************************************************/
bool PSpower::Connect()
{   
    PSCTL psctl;
    
    if ( ! psctl.Connect() )  //this does the unlock as well
    {
        LOG_ERROR("No Power*Star found.");
        return false;
    }
    
    AmpHrs = WattHrs = 0;
    //faultMask = 0xffffff;
    
    psctl.getMaxPosition(&maximumPosition);
    psctl.getAbsPosition(&relitivePosition);

    FocusMaxPosN[0].value = maximumPosition;
    FocusAbsPosN[0].max = FocusSyncN[0].max = FocusMaxPosN[0].value;
    FocusAbsPosN[0].step = FocusSyncN[0].step = 100.0;
    FocusAbsPosN[0].min = FocusSyncN[0].min = 0;
    FocusRelPosN[0].max  = FocusMaxPosN[0].value / 2;
    FocusRelPosN[0].step = 100.0;
    FocusRelPosN[0].min  = 0;
    FocusAbsPosN[0].value = relitivePosition; 
    
	LOG_INFO("Power*Star connected successfully.");

    POLLMS = 500;
    
    SetTimer(POLLMS);
    
	return true;
}

/***************************************************************/
bool PSpower::Disconnect()
{
    psctl.Disconnect();
	LOG_INFO("Power*Star disconnected successfully.");
	return true;
}

/***************************************************************/
/* initProperties */
/***************************************************************/
bool PSpower::initProperties()
{
    FI::initProperties(FOCUS_TAB);
    WI::initProperties(MAIN_CONTROL_TAB, ENVIRONMENT_TAB);
    
    setDriverInterface(AUX_INTERFACE | FOCUSER_INTERFACE);
    INDI::DefaultDevice::initProperties();
    
    FI::SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_CAN_SYNC);
    
    addAuxControls();
    
    psctl.getStatus();
    
    /***************/
    /* Main Tab    */
    /***************/
    // Power Statistics 
    IUFillNumber(&PowerSensorsN[SENSOR_VOLTAGE], "SENSOR_VOLTAGE", "Voltage (V)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerSensorsN[SENSOR_CURRENT], "SENSOR_CURRENT", "Current (A)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerSensorsN[SENSOR_POWER], "SENSOR_POWER", "Power (W)", "%4.2f", 0, 999, 100, 0);
    IUFillNumber(&PowerSensorsN[SENSOR_AMP_HOURS], "SENSOR_AMP_HOURS", "Amp Hours", "%4.2f", 0, 999, 100, 99.9);
    IUFillNumber(&PowerSensorsN[SENSOR_WATT_HOURS], "SENSOR_WATT_HOURS", "Watt Hours", "%4.2f", 0, 999, 100, 99.9);
    IUFillNumberVector(&PowerSensorsNP, PowerSensorsN, SENSOR_N, getDeviceName(), "POWER_SENSORS", 
                       "Power Status", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);
    
    // Weather
    /**
    IUFillNumber(&WEATHERN[TEMP], "OTA_TEMP", "Temperature", "%5.2f", 0, 0, 0, 55.5); 
    IUFillNumber(&WEATHERN[HUM], "OTA_HUM", "Humidity", "%5.2f", 0, 0, 0, 55.5);
    IUFillNumber(&WEATHERN[DP], "OTA_DP", "Dew Point", "%5.2f", 0, 0, 0, 55.5);
    IUFillNumber(&WEATHERN[DEP], "OTA_DEP", "DP Depression", "%5.2f", 0, 0, 0, 55.5);
    IUFillNumberVector(&WEATHERNP, WEATHERN, WEATHER_N, getDeviceName(), "WEATHER", "Weather", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);
    **/
    
    // Clear amp/watt fields
    IUFillSwitch(&PowerClearS[0], "SENSOR_CLEAR", "Clear", ISS_OFF);
    IUFillSwitchVector(&PowerClearSP, PowerClearS, 1, getDeviceName(), "Reset AmpHrs/Watts Field", "Power", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);
    
    /***************/
    /* INFO Tab    */
    /***************/
    // PowerStar Firmware
    uint16_t psversion = psctl.getVersion();
    char fversion[5];
    snprintf(fversion, 4, "%i.%i", (psversion & 0xFF00) >> 8, psversion & 0xFF);
    IUFillText(&FirmwareT[FIRMWARE_VERSION], "FIRMWARE", "Firmware", fversion);
    IUFillTextVector(&FirmwareTP, FirmwareT, 1, getDeviceName(), "VERSION_INFO", "Power*Star", INFO_TAB, IP_RO, 60, IPS_IDLE);
    
    /***************/
    /* Options Tab */
    /***************/
    //Autoboot
    IUFillSwitch(&AutoBootS[ABOUT1], "AB_PORT1", "Port1", ISS_OFF);
    IUFillSwitch(&AutoBootS[ABOUT2], "AB_PORT2", "Port2", ISS_OFF);
    IUFillSwitch(&AutoBootS[ABOUT3], "AB_PORT3", "Port3", ISS_OFF);
    IUFillSwitch(&AutoBootS[ABOUT4], "AB_PORT4", "Port4", ISS_OFF);
    IUFillSwitch(&AutoBootS[ABVAR], "AB_VAR", "Variable", ISS_OFF);
    IUFillSwitch(&AutoBootS[ABMP], "AB_MP", "MultiPurpose", ISS_OFF);
    IUFillSwitch(&AutoBootS[ABDEWA], "AB_DEWA", "DewA", ISS_OFF);
    IUFillSwitch(&AutoBootS[ABDEWB], "AB_DEWB", "DewB", ISS_OFF);
    IUFillSwitch(&AutoBootS[ABUSB2], "AB_USB2", "Usb2", ISS_OFF);
    IUFillSwitch(&AutoBootS[ABUSB3], "AB_USB3", "Usb3", ISS_OFF);
    IUFillSwitch(&AutoBootS[ABUSB6], "AB_USB6", "Usb6", ISS_OFF);
    IUFillSwitchVector(&AutoBootSP, AutoBootS, AutoBoot_N, getDeviceName(), "AUTOBOOT_ENABLES", "Autoboot", OPTIONS_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);
    
    // Variable port voltage setting
    IUFillNumber(&VarSettingN[0], "VAR_SETTING", "Volts", "%4.1f", 3, 10, 0.1, 0);
    IUFillNumberVector(&VarSettingNP, VarSettingN, 1, getDeviceName(), "Volts", "Variable Port", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);
    
    // MP mode
    //TODO set switch according to what is current status of P*S
    // int Index = psctl.statusMap["MP"].setting;
    IUFillSwitch(&MPtypeS[DC], "MP_DC", "DC", ISS_ON);
    IUFillSwitch(&MPtypeS[DEW], "MP_DEW", "DEW", ISS_OFF);
    // If DEW, then need to ask % power
    IUFillSwitch(&MPtypeS[PWM], "MP_PWM", "PWM", ISS_OFF);
    // IF PWM, then need to ask % rate
    IUFillSwitchVector(&MPtypeSP, MPtypeS, MP_N, getDeviceName(), "MP_SETTING", "Multiport", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    
    // MP PWM rate
    IUFillNumber(&MPpwmN[0], "MP_PWM", "Rate", "%5.0f", 0, 1023, 100, 0);
    IUFillNumberVector(&MPpwmNP, MPpwmN, 1, getDeviceName(), "MPPWM", "PWM", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);
    
    // MP DEW %
    IUFillNumber(&MPdewN[0], "MP_DEW", "Percent", "%4.0f", 0, 100, 1, 0);
    IUFillNumberVector(&MPdewNP, MPdewN, 1, getDeviceName(), "MPDEW", "Dew", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);
    
    // LED brightness
    IUFillNumber(&PowerLEDN[0], "LED_BRIGHTNESS", "Brightness", "%1.0f", 0, 3, 1, 3);
    IUFillNumberVector(&PowerLEDNP, PowerLEDN, 1, getDeviceName(), "LED", "LED", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);
    
    /***************/
    /* Power Tab   */
    /***************/
    //Port Enables
    IUFillSwitch(&PortCtlS[OUT1], "CPORT1", "Port1", psctl.statusMap["Out1"].state ? ISS_ON : ISS_OFF);
    IUFillSwitch(&PortCtlS[OUT2], "CPORT2", "Port2", psctl.statusMap["Out2"].state ? ISS_ON : ISS_OFF);
    IUFillSwitch(&PortCtlS[OUT3], "CPORT3", "Port3", psctl.statusMap["Out3"].state ? ISS_ON : ISS_OFF);
    IUFillSwitch(&PortCtlS[OUT4], "CPORT4", "Port4", psctl.statusMap["Out4"].state ? ISS_ON : ISS_OFF);
    IUFillSwitch(&PortCtlS[VAR], "CVAR", "Variable", psctl.statusMap["Var"].state ? ISS_ON : ISS_OFF);
    IUFillSwitch(&PortCtlS[MP], "CMP", "MultiPurpose", psctl.statusMap["MP"].state ? ISS_ON : ISS_OFF);
    IUFillSwitchVector(&PortCtlSP, PortCtlS, POWER_N, getDeviceName(), "PORT_ENABLES", "Enable", POWER_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);
    
    // Port enable lights
    IUFillLight(&PORTlightsL[OUT1], "LPORT1", "Port1", IPS_OK);
    IUFillLight(&PORTlightsL[OUT2], "LPORT2", "Port2", IPS_OK);
    IUFillLight(&PORTlightsL[OUT3], "LPORT3", "Port3", IPS_OK);
    IUFillLight(&PORTlightsL[OUT4], "LPORT4", "Port4", IPS_OK);
    IUFillLight(&PORTlightsL[VAR], "LVAR", "Variable", IPS_OK);
    IUFillLight(&PORTlightsL[MP], "LMP", "MultiPurpose", IPS_OK);
    IUFillLightVector(&PORTlightsLP, PORTlightsL, POWER_N, getDeviceName(), "PORT_LIGHTS", "Status", POWER_TAB, IPS_IDLE);
    
    // All On
    IUFillSwitch(&AllS[ALLON], "ALL_ON", "All On", ISS_OFF);
    IUFillSwitch(&AllS[ALLOFF], "ALL_OFF", "All Off", ISS_OFF);
    IUFillSwitch(&AllS[AUTON], "AUTO_ON", "AUTO", ISS_OFF);
    IUFillSwitch(&AllS[REBOOT], "REBOOT", "Reboot", ISS_OFF);
    IUFillSwitchVector(&AllSP, AllS, All_N, getDeviceName(), "ALL", "All Ports", POWER_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    
    
    // Reboot
    IUFillSwitch(&RebootS[0], "REBOOT", "Reboot", ISS_OFF);
    IUFillSwitchVector(&RebootSP, RebootS, 1, getDeviceName(), "REBOOT", "Power", OPTIONS_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    
    //Port Currents
    IUFillNumber(&PortCurrentN[OUT1], "CURRENT_OUT1", "Port 1 Current", "%0.2f", 0, 0, 0, 0);
    IUFillNumber(&PortCurrentN[OUT2], "CURRENT_OUT2", "Port 2 Current", "%0.2f", 0, 0, 0, 0);
    IUFillNumber(&PortCurrentN[OUT3], "CURRENT_OUT3", "Port 3 Current", "%0.2f", 0, 0, 0, 0);
    IUFillNumber(&PortCurrentN[OUT4], "CURRENT_OUT4", "Port 4 Current", "%0.2f", 0, 0, 0, 0);
    IUFillNumber(&PortCurrentN[VAR], "CURRENT_VAR", "Variable Current", "%0.2f", 0, 0, 0, 0);
    IUFillNumber(&PortCurrentN[MP], "CURRENT_MP", "Multipurpose Current", "%0.2f", 0, 0, 0, 0);
    IUFillNumberVector(&PortCurrentNP, PortCurrentN, POWER_N, getDeviceName(), "PORT_CURRENT", "Current", POWER_TAB, IP_RO, 0, IPS_IDLE);
    
    //Port Labels
    IUFillText(&PortLabelsT[OUT1], "PORTLABEL1", "Port1 Name", "Port1");
    IUFillText(&PortLabelsT[OUT2], "PORTLABEL2", "Port2 Name", "Port2");
    IUFillText(&PortLabelsT[OUT3], "PORTLABEL3", "Port3 Name", "Port3");
    IUFillText(&PortLabelsT[OUT4], "PORTLABEL4", "Port4 Name", "Port4");
    IUFillText(&PortLabelsT[VAR], "PORTLABELVAR", "Variable Name", "VAR");
    IUFillText(&PortLabelsT[MP], "PORTLABELMP", "Multipurpose Name", "MP");
    IUFillTextVector(&PortLabelsTP, PortLabelsT, POWER_N, getDeviceName(), "PORTLABELS", "Ports", POWER_TAB, IP_RW, 60, IPS_IDLE);
    
    /***************/
    /* USB Tab     */
    /***************/
    // USB enable 
    IUFillSwitch(&USBpwS[PUSB2], "PUSB2", "USB 2", psctl.statusMap["USB2"].state ? ISS_ON : ISS_OFF);
    IUFillSwitch(&USBpwS[PUSB3], "PUSB3", "USB 3", psctl.statusMap["USB3"].state ? ISS_ON : ISS_OFF);
    IUFillSwitch(&USBpwS[PUSB6], "PUSB6", "USB 6", psctl.statusMap["USB6"].state ? ISS_ON : ISS_OFF);
    IUFillSwitchVector(&USBpwSP, USBpwS, USBPW_N, getDeviceName(), "USB_ENABLES", "Power", USB_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);
    
    // TODO add allon/alloff button
    
    // USB enable lights
    IUFillLight(&USBlightsL[USB1], "LUSB1", "USB3 Port1", IPS_OK);
    IUFillLight(&USBlightsL[USB2], "LUSB2", "USB3 Port2", IPS_OK);
    IUFillLight(&USBlightsL[USB3], "LUSB3", "USB3 Port3", IPS_OK);
    IUFillLight(&USBlightsL[USB4], "LUBB4", "USB3 Port4", IPS_OK);
    IUFillLight(&USBlightsL[USB5], "LUSB5", "USB2 Port5", IPS_OK);
    IUFillLight(&USBlightsL[USB6], "LUSB6", "USB2 Port6", IPS_OK);
    IUFillLightVector(&USBlightsLP, USBlightsL, USB_N, getDeviceName(), "USB_PORT_LIGHTS", "Status", USB_TAB, IPS_IDLE);
    
    // USB Names
    IUFillText(&USBLabelsT[USB1], "USB1_NAME", "USB 1 Name", "USB 1");
    IUFillText(&USBLabelsT[USB2], "USB2_NAME", "USB 2 Name", "USB 2");
    IUFillText(&USBLabelsT[USB3], "USB3_NAME", "USB 3 Name", "USB 3");
    IUFillText(&USBLabelsT[USB4], "USB4_NAME", "USB 4 Name", "USB 4");
    IUFillText(&USBLabelsT[USB5], "USB5_NAME", "USB 5 Name", "USB 5");
    IUFillText(&USBLabelsT[USB6], "USB6_NAME", "USB 6 Name", "USB 6");
    IUFillTextVector(&USBLabelsTP, USBLabelsT, USB_N, getDeviceName(), "USB_LABELS", "USB", USB_TAB, IP_RW, 60, IPS_IDLE);
    
    /***************/
    /* DEW Tab     */
    /***************/
    // Dew % Power (zero is off)
    IUFillNumber(&DewPowerN[DEW1], "DEW1", "Dew1 % Power", "%.0f", 0, 100, 1, 0);
    IUFillNumber(&DewPowerN[DEW2], "DEW2", "Dew2 % Power", "%.0f", 0, 100, 1, 0);
    IUFillNumberVector(&DewPowerNP, DewPowerN, DEW_N, getDeviceName(), "DEWPWR", "% Power", DEW_TAB, IP_RW, 60, IPS_IDLE);
    
    IUFillLight(&DEWlightsL[DEW1], "LDEW1", "Dew 1", IPS_OK);
    IUFillLight(&DEWlightsL[DEW2], "LDEW2", "Dew 2", IPS_OK);
    IUFillLightVector(&DEWlightsLP, DEWlightsL, DEW_N, getDeviceName(), "DEW_LIGHTS", "Status", DEW_TAB, IPS_OK);
   
    // DEW Current draw
    IUFillNumber(&DewCurrentN[DEW1], "CDEW1", "Dew1", "%.2f", 0, 100, 1, 0);
    IUFillNumber(&DewCurrentN[DEW2], "CDEW2", "Dew2", "%.2f", 0, 100, 1, 0);
    IUFillNumberVector(&DewCurrentNP, DewCurrentN, DEW_N, getDeviceName(), "DEWCUR", "% Current", DEW_TAB, IP_RO, 60, IPS_IDLE);
    
    // Dew labels
    IUFillText(&DewLabelsT[DEW1], "DEW1_NAME", "Dew 1 Name", "Dew1");
    IUFillText(&DewLabelsT[DEW2], "DEW2_NAME", "Dew 1 Name", "Dew1");
    IUFillTextVector(&DewLabelsTP, DewLabelsT, DEW_N, getDeviceName(), "DEWLABELS", "Dew", DEW_TAB, IP_RW, 60, IPS_IDLE);
    
    /***************/
    /* FOCUS MOTOR */
    /***************/
    // Template (optional)
    IUFillSwitch(&TemplateS[HSM], "THSM", "HSM", ISS_ON);
    IUFillSwitch(&TemplateS[PDMS], "TPDMS", "PDMS", ISS_OFF);
    IUFillSwitch(&TemplateS[UNI12], "TUNI12", "UNI12", ISS_OFF);
    IUFillSwitchVector(&TemplateSP, TemplateS, Template_N, getDeviceName(), "TEMPLATE", "Template (Optional)", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    
    //Motor Type
    IUFillSwitch(&MtrTypeS[Unipolar], "UNIPOLAR", "Unipolar", ISS_OFF);
    IUFillSwitch(&MtrTypeS[Bipolar], "BIPOLAR", "Bipolar", ISS_ON);
    IUFillSwitchVector(&MtrTypeSP, MtrTypeS, MtrType_N, getDeviceName(), "MOTORTYPE", "Type", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    
    //Prefered Direction
    IUFillSwitch(&PrefDirS[In], "DIN", "In", ISS_ON);
    IUFillSwitch(&PrefDirS[Out], "DOUT", "Out", ISS_OFF);
    IUFillSwitchVector(&PrefDirSP, PrefDirS, PrefDir_N, getDeviceName(), "PREFDIR", "Pref Dir", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    
    //Reverse Motor
    IUFillSwitch(&RevMtrS[Yes], "RYES", "Yes", ISS_OFF);
    IUFillSwitch(&RevMtrS[No], "RNO", "No", ISS_ON);
    IUFillSwitchVector(&RevMtrSP, RevMtrS, RevMtr_N, getDeviceName(), "REVMTR", "Rev Mtr", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    
    //Temperature Compensation 
    IUFillSwitch(&TempCompS[None], "TNONE", "None", ISS_ON);
    IUFillSwitch(&TempCompS[Motor], "TMOTOR", "Motor", ISS_OFF);
    IUFillSwitch(&TempCompS[Env], "TENV", "ENV", ISS_OFF);
    IUFillSwitchVector(&TempCompSP, TempCompS, TempComp_N, getDeviceName(), "TEMPCOMP", "Temp Comp", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    
    // Motor Params
    IUFillNumber(&MtrProfN[Backlash], "TBACK", "Backlash", "%3.0f", 0, 255, 5, 0);
    IUFillNumber(&MtrProfN[IdleCur], "TIDLE", "Idle Current", "%5.0f", 0, 254, 5, 128);
    IUFillNumber(&MtrProfN[StepPer], "TSTEP", "Step Period", "%3.1f", 0, 10, 1, 5);
    IUFillNumber(&MtrProfN[TempCoef], "TCOEF", "Temp Coef", "%5.0f", 0, 255, 5, 0);
    IUFillNumber(&MtrProfN[DrvCur], "TDRV", "Drive Current", "%5.1f", 0, 255, 5, 128);
    IUFillNumber(&MtrProfN[TempHys], "THSY", "Hysteresis", "%5.0f", 0, 25.5, 1, 0);
    IUFillNumberVector(&MtrProfNP, MtrProfN, MtrProf_N, getDeviceName(), "MTRPROF", "Profile", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);
    
    //Disable Perm Focus 
    IUFillSwitch(&PermFocS[Yes], "PYES", "Yes", ISS_OFF);
    IUFillSwitch(&PermFocS[No], "PNO", "No", ISS_ON);
    IUFillSwitchVector(&PermFocSP, PermFocS, RevMtr_N, getDeviceName(), "PERMFOC", "Perm Focus", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    
    
    /***************/
    /* Faults tab  */
    /***************/
    // Faults
    //IUFillNumber(&FaultsN[NFATAL], "NFAULTS", "Non-Fatal", "%04X", 0, 0xffff, 0, 10);
    //IUFillNumber(&FaultsN[FFATAL], "FFAULTS", "Fatal", "%04X", 0, 0xffff, 0, 20);
    //IUFillNumberVector(&FaultsNP, FaultsN, Fatal_N, getDeviceName(), "FAULTS", "Faults", FAULTS_TAB, IP_RW, 0, IPS_OK);

    // Fault Mask number
    //IUFillNumber(&FaultMaskN[0], "FAULT_MASK", "Fault Mask", "%.0f", 0, 0xffffff, 1, 0xffffff);
    //IUFillNumberVector(&FaultMaskNP, FaultMaskN, 1, getDeviceName(), "Fault_Mask", "Fault Mask", FAULTS_TAB, IP_RO, 0, IPS_IDLE);
    
    // Clear faults
    IUFillSwitch(&FaultsClearS[0], "Faults_CLEAR", "Clear Faults", ISS_OFF);
    IUFillSwitchVector(&FaultsClearSP, FaultsClearS, 1, getDeviceName(), "Clear_Faults", "Faults", FAULTS_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);
    
    // Fault Mask field
    IUFillSwitch(&FaultMask1S[FS1OverUnder], "FM1OverUnder", "In Ovr/Undr", ISS_ON);
    IUFillSwitch(&FaultMask1S[FS1TotalCurrent], "FM1TotalCurrent", "Total Curr", ISS_ON);
    IUFillSwitch(&FaultMask1S[FS1Out1], "FM1Out1", "Out1", ISS_ON);
    IUFillSwitch(&FaultMask1S[FS1Out2], "FM1Out2", "Out2", ISS_ON);
    IUFillSwitch(&FaultMask1S[FS1Out3], "FM1Out3", "Out3", ISS_ON);
    IUFillSwitch(&FaultMask1S[FS1Out4], "FM1Out4", "Out4", ISS_ON);
    IUFillSwitch(&FaultMask1S[FS1DewA], "FM1DewA", "DewA", ISS_ON);
    IUFillSwitch(&FaultMask1S[FS1DewB], "FM1DewB", "DewB", ISS_ON);
    IUFillSwitchVector(&FaultMask1SP, FaultMask1S, FaultStatus1_N, getDeviceName(), "FAULTMASK1", "Ignore Fault", FAULTS_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);
    
    IUFillSwitch(&FaultMask2S[FS2VarVoltage], "FM2VarVoltage", "VAR", ISS_ON);
    IUFillSwitch(&FaultMask2S[FS2FMP], "FM2FMP", "MP", ISS_ON);
    IUFillSwitch(&FaultMask2S[FS2PosChange], "FM2PosChange", "PosChange", ISS_ON);
    IUFillSwitch(&FaultMask2S[FS2MotorTemp], "FM2MotorTemp", "Mtr Temp", ISS_ON);
    IUFillSwitch(&FaultMask2S[FS2MotorDriver], "FM2MotorDriver", "Mtr Drv", ISS_ON);
    IUFillSwitch(&FaultMask2S[FS2Internal], "FM2Internal", "Internal", ISS_ON);
    IUFillSwitch(&FaultMask2S[FS2Environment], "FM2Environment", "Env Sensor", ISS_ON);
    IUFillSwitchVector(&FaultMask2SP, FaultMask2S, FaultStatus2_N, getDeviceName(), "FAULTMASK2", "Ignore Fault", FAULTS_TAB, IP_RW, ISR_NOFMANY, 60, IPS_IDLE);
    
    // Fault Status
    IUFillLight(&FaultStatusL[FSOverUnder], "FSOverUnder", "12V Input Over/Under Voltage (11V-14V)", IPS_OK);
    IUFillLight(&FaultStatusL[FSTotalCurrent], "FSTotalCurrent", "Total Current exceeded 20A", IPS_OK);
    IUFillLight(&FaultStatusL[FSVarVoltage], "FSVarVoltage", "Variable Voltage over/under (+/- 6%)", IPS_OK);
    IUFillLight(&FaultStatusL[FSOut1], "FSOut1", "Out1 over current (15A)", IPS_OK);
    IUFillLight(&FaultStatusL[FSOut2], "FSOut2", "Out1 over current (10A)", IPS_OK);
    IUFillLight(&FaultStatusL[FSOut3], "FSOut3", "Out1 over current (6A)", IPS_OK);
    IUFillLight(&FaultStatusL[FSOut4], "FSOut4", "Out1 over current (6A)", IPS_OK);
    IUFillLight(&FaultStatusL[FSDewA], "FSDewA", "Dew A over current (6A)", IPS_OK);
    IUFillLight(&FaultStatusL[FSDewB], "FSDewB", "Dew B over current (6A)", IPS_OK);
    IUFillLight(&FaultStatusL[FSFMP], "FSMP", "MultiPort over current (6A)", IPS_OK);
    IUFillLight(&FaultStatusL[FSPosChange], "FSPOSCHNG", "Position change", IPS_OK);
    IUFillLight(&FaultStatusL[FSMotorTemp], "FSMotorTemp", "Motor temperature sensor (see manual)", IPS_OK);
    IUFillLight(&FaultStatusL[FSMotorDriver], "FSMotorDriver", "Focus motor driver (see manual)", IPS_OK);
    IUFillLight(&FaultStatusL[FSInternal], "FSInternal", "Internal voltage error (see manual)", IPS_OK);
    IUFillLight(&FaultStatusL[FSEnvironment], "FSEnvironment", "Environment sensor error (see manual)", IPS_OK);
    IUFillLightVector(&FaultStatusLP, FaultStatusL, FaultStatus_N, getDeviceName(), "FAULTSTATUS", "Status", FAULTS_TAB, IPS_OK);
    
    /*****************/
    /* User Limits tab  */
    /*****************/
    // User Limits
    IUFillNumber(&UserLimitsN[InLowerLim], "InLowerLim", "Input Lower Limit", "%.0f", 10, 15, .5, 10);
    IUFillNumber(&UserLimitsN[InUpperLim], "InUpperLim", "Input Upper Limit", "%.0f", 12, 14.5, .5, 14.5);
    IUFillNumber(&UserLimitsN[VarLowerLim], "VarLowerLim", "VAR Lower Limit", "%.0f", 2.7, 9, .5, 2.7);
    IUFillNumber(&UserLimitsN[VarUpperLim], "VarUpperLim", "Input Lower Limit", "%.0f", 3.3, 10.9, .5, 10.9);
    IUFillNumber(&UserLimitsN[TotalCurrLim], "TotalCurrLim", "VAR Upper Limit", "%.0f", 0, 22.7, .5, 22.7);
    IUFillNumber(&UserLimitsN[Out1CurrLim], "Out1CurrLim", "Out1 Current Limit", "%.0f", 0, 15, .5, 15);
    IUFillNumber(&UserLimitsN[Out2CurrLim], "Out2CurrLim", "Out2 Current Limit", "%.0f", 0, 10, .5, 10);
    IUFillNumber(&UserLimitsN[Out3CurrLim], "Out3CurrLim", "Out3 Current Limit", "%.0f", 0, 6, .5, 6);
    IUFillNumber(&UserLimitsN[Out4CurrLim], "Out4CurrLim", "Out4 Current Limit", "%.0f", 0, 6, .5, 6);
    IUFillNumber(&UserLimitsN[DewACurrLim], "DewACurrLim", "DewA Current Limit", "%.0f", 0, 5, .5, 5);
    IUFillNumber(&UserLimitsN[DewBCurrLim], "DewBCurrLim", "DewB Current Limit", "%.0f", 0, 5, .5, 5);
    IUFillNumber(&UserLimitsN[MPCurrLim], "MPCurrLim", "MP Current Limit", "%.0f", 0, 6, .5, 6);
    IUFillNumberVector(&UserLimitsNP, UserLimitsN, UserLimits_N, getDeviceName(), "USRLIMIT", "User Limits", USRLIMIT_TAB, IP_RW, 60, IPS_IDLE);
    
    //TODO add settings for weather alerts - value and % of
    /*****************/
    /* Weather tab   */
    /*****************/
    // Weather
    addParameter("WEATHER_TEMPERATURE", "Temperature (F)", 20, 80, 1);
    addParameter("WEATHER_HUMIDITY", "Humidity (%)", 0, 95, 1);
    addParameter("WEATHER_DEW_POINT", "Dew Point (F)", 0, 100, 0);
    addParameter("WEATHER_DP_DEP", "DP Depresion", 3, 0, 1);
    
    setCriticalParameter("WEATHER_TEMPERATURE");
    setCriticalParameter("WEATHER_HUMIDITY");
    setCriticalParameter("WEATHER_DP_DEP");
    
    return true;
}

/***************************************************************/
bool PSpower::updateProperties()
{
	// Call parent update properties first
	INDI::DefaultDevice::updateProperties();

	if (isConnected())
    {
        psctl.getStatus();

        // Main tab
        defineNumber(&PowerSensorsNP);
        defineNumber(&WEATHERNP);
        defineSwitch(&PowerClearSP);
        
        // Options tab
        defineSwitch(&TemplateSP);
        defineSwitch(&MtrTypeSP);
        defineSwitch(&PrefDirSP);
        defineSwitch(&RevMtrSP);
        defineSwitch(&RevMtrSP);
        defineSwitch(&TempCompSP);
        defineNumber(&MtrProfNP);
        defineSwitch(&PermFocSP);
        defineSwitch(&AutoBootSP);
        defineNumber(&PowerLEDNP);
        defineNumber(&VarSettingNP);
        defineSwitch(&MPtypeSP);
        int Index = psctl.statusMap["MP"].setting;
        switch(Index) {
            case DC : {
                deleteProperty(MPpwmNP.name);
                deleteProperty(MPdewNP.name);
                break;
            }
            case PWM : {
                defineNumber(&MPpwmNP);
                deleteProperty(MPdewNP.name);
                break;
            }
            case DEW : {
                defineNumber(&MPdewNP);
                deleteProperty(MPpwmNP.name);
                break;
            }
        }
        
        // Focus Tab
        FI::updateProperties();
        
        // Power tab
        defineSwitch(&PortCtlSP);
        defineLight(&PORTlightsLP);
        defineSwitch(&AllSP);
        defineNumber(&PortCurrentNP);
        defineText(&PortLabelsTP);
        
        // USB tab
        defineSwitch(&USBpwSP);
        defineLight(&USBlightsLP);
        defineText(&USBLabelsTP);
        
        // DEW tab
        defineNumber(&DewPowerNP);
        defineLight(&DEWlightsLP);
        defineNumber(&DewCurrentNP);
        defineText(&DewLabelsTP);
        
        // Weather tab
        WI::updateProperties();
        
        // Faults tab
        //defineNumber(&FaultsNP);
        //defineNumber(&FaultMaskNP);
        defineSwitch(&FaultsClearSP);
        defineSwitch(&FaultMask1SP);
        defineSwitch(&FaultMask2SP);
        defineLight(&FaultStatusLP);
        
        // User Limits
        defineNumber(&UserLimitsNP);
        
        // INFO tab
        defineText(&FirmwareTP);
        
    }
    else
    {
        // Main tab
        deleteProperty(PowerSensorsNP.name);
        deleteProperty(WEATHERNP.name);
        deleteProperty(PowerClearSP.name);
        
        // Settings tab
        deleteProperty(PowerLEDNP.name);
        deleteProperty(MPtypeSP.name);
        deleteProperty(MPpwmNP.name);
        deleteProperty(MPdewNP.name);
        
        // Status tab
        deleteProperty(FirmwareTP.name);
        deleteProperty(FaultsNP.name);
        
        // Power tab
        deleteProperty(PortCtlSP.name);
        deleteProperty(PORTlightsLP.name);
        deleteProperty(PortLabelsTP.name);
        deleteProperty(AllSP.name);
        deleteProperty(PortCurrentNP.name);
        //deleteProperty(AutoBootSP.name);
        
        // USB tab
        deleteProperty(USBLabelsTP.name);
        deleteProperty(USBlightsLP.name);
        deleteProperty(USBpwSP.name);
        
        // DEW tab
        deleteProperty(DewPowerNP.name);
        deleteProperty(DewCurrentNP.name);
        deleteProperty(DEWlightsLP.name);
        deleteProperty(DewLabelsTP.name);
        
        // Focus Motor tab
        deleteProperty(MtrTypeSP.name);
        deleteProperty(TemplateSP.name);
        deleteProperty(PrefDirSP.name);
        deleteProperty(RevMtrSP.name);
        deleteProperty(RevMtrSP.name);
        deleteProperty(TempCompSP.name);
        deleteProperty(MtrProfNP.name);
        deleteProperty(PermFocSP.name);
        
        // Faults tab
        //deleteProperty(FaultsNP.name);
        //deleteProperty(FaultMaskNP.name);
        deleteProperty(FaultsClearSP.name);
        deleteProperty(FaultMask1SP.name);
        deleteProperty(FaultMask2SP.name);
        deleteProperty(FaultStatusLP.name);
        
        FI::updateProperties();
        WI::updateProperties();
        
        // User Limits
        deleteProperty(UserLimitsNP.name);
    }
    return true;
}

/***************************************************************/
/* New Switch */
/***************************************************************/
bool PSpower::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[],
                                 int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Clear Power fields
        if (strcmp(name, PowerClearSP.name) == 0)
        {
            IUUpdateSwitch(&PowerClearSP, states, names, n);
            
            AmpHrs = WattHrs = 0.0;
            
            PowerClearS[0].s = ISS_OFF;
            PowerClearSP.s = IPS_OK;
            IDSetSwitch(&PowerClearSP, nullptr);
            return true;
        }
        
        // Clear Faults
        if (strcmp(name, FaultsClearSP.name) == 0)
        {
            IUUpdateSwitch(&FaultsClearSP, states, names, n);
            psctl.clearFaults();
            FaultsClearS[0].s = ISS_OFF;
            FaultsClearSP.s = IPS_OK;
            IDSetSwitch(&FaultsClearSP, nullptr);
            FatalOccured = false;
            NonFatalOccured = false;
            // clear fault lights
            for (int i=0; i < FaultStatus_N; i++) {
                FaultStatusL[i].s = IPS_OK;
            }
            IDSetLight(&FaultStatusLP, nullptr);
            return true;
        }
        
        // Port power switches
        if (strcmp(name, PortCtlSP.name) == 0)
        {
            IUUpdateSwitch(&PortCtlSP, states, names, n);
            //TODO add test for success and update status flag
            if(!strcmp(names[OUT1], PortCtlS[OUT1].name))
                PortCtlS[OUT1].s ?  psctl.setPowerState("out1", "yes") : psctl.setPowerState("out1", "no");
            if(!strcmp(names[OUT2], PortCtlS[OUT2].name))
                PortCtlS[OUT2].s ?  psctl.setPowerState("out2", "yes") : psctl.setPowerState("out2", "no");
            if(!strcmp(names[OUT3], PortCtlS[OUT3].name))
                PortCtlS[OUT3].s ?  psctl.setPowerState("out3", "yes") : psctl.setPowerState("out3", "no");
            if(!strcmp(names[OUT4], PortCtlS[OUT4].name))
                PortCtlS[OUT4].s ?  psctl.setPowerState("out4", "yes") : psctl.setPowerState("out4", "no");
            if(!strcmp(names[VAR], PortCtlS[VAR].name))
                PortCtlS[VAR].s ?  psctl.setPowerState("var", "yes") : psctl.setPowerState("var", "no");
            // MP is complicated: if DC, then on/off, if dew or pwm, then set to zero to turn off
            if(!strcmp(names[MP], PortCtlS[MP].name)) {
                
                switch (psctl.statusMap["MP"].setting) {
                    case DC : {
                        PortCtlS[MP].s ?  psctl.setPowerState("mp", "yes") : psctl.setPowerState("mp", "no");
                        break;
                    }
                    case PWM : {
                        psctl.setPWM(0);
                        MPpwmN[0].value = 0;
                        IDSetNumber(&MPpwmNP, nullptr);
                        break;
                    }
                    case DEW : {
                        psctl.setDew(2, 0);
                        MPdewN[0].value = 0;
                        IDSetNumber(&MPdewNP, nullptr);
                        break;
                    }
                }
            }
            
            // turn off the 'all' on/off switches since we selected an individual switch
            AllS[ALLON].s = ISS_OFF;
            AllS[ALLOFF].s = ISS_OFF;
            AllS[AUTON].s = ISS_OFF;
            AllS[REBOOT].s = ISS_OFF;
            AllSP.s = IPS_OK;
            IDSetSwitch(&AllSP, nullptr);
            
            // and update the port power switches
            PortCtlSP.s = IPS_OK;
            IDSetSwitch(&PortCtlSP, nullptr);
            return true;
        }
        
        // USB power switches
        if (strcmp(name, USBpwSP.name) == 0)
        {
            IUUpdateSwitch(&USBpwSP, states, names, n);
            //TODO add test for success and update status flag
            if(!strcmp(names[PUSB2], USBpwS[PUSB2].name))
                USBpwS[PUSB2].s ?  psctl.setPowerState("usb2", "yes") : psctl.setPowerState("usb2", "no");
            if(!strcmp(names[PUSB3], USBpwS[PUSB3].name))
                USBpwS[PUSB3].s ?  psctl.setPowerState("usb3", "yes") : psctl.setPowerState("usb3", "no");
            if(!strcmp(names[PUSB6], USBpwS[PUSB6].name))
                USBpwS[PUSB6].s ?  psctl.setPowerState("usb6", "yes") : psctl.setPowerState("usb6", "no");
            
            USBpwSP.s = IPS_OK;
            IDSetSwitch(&USBpwSP, nullptr);
            return true;
        }
        
        /** TODO does not work and has stability issues
        // Auto boot
        if (strcmp(name, AutoBootSP.name) == 0)
        {
            IUUpdateSwitch(&AutoBootSP, states, names, n);
            
            if(!strcmp(names[ABOUT1], AutoBootS[ABOUT1].name))
                AutoBootS[ABOUT1].s ?  psctl.setAutoBoot("out1", "on") : psctl.setAutoBoot("out1", "off");
            if(!strcmp(names[ABOUT2], AutoBootS[ABOUT2].name))
                AutoBootS[ABOUT2].s ?  psctl.setAutoBoot("out2", "on") : psctl.setAutoBoot("out2", "off");
            if(!strcmp(names[ABOUT3], AutoBootS[ABOUT3].name))
                AutoBootS[ABOUT3].s ?  psctl.setAutoBoot("out3", "on") : psctl.setAutoBoot("out3", "off");
            if(!strcmp(names[ABOUT4], AutoBootS[ABOUT4].name))
                AutoBootS[ABOUT4].s ?  psctl.setAutoBoot("out4", "on") : psctl.setAutoBoot("out4", "off");
            if(!strcmp(names[ABVAR], AutoBootS[ABVAR].name))
                AutoBootS[ABVAR].s ?  psctl.setAutoBoot("var", "on") : psctl.setAutoBoot("var", "off");
            if(!strcmp(names[ABMP], AutoBootS[ABMP].name))
                AutoBootS[ABMP].s ?  psctl.setAutoBoot("mp", "on") : psctl.setAutoBoot("mp", "off");
            if(!strcmp(names[ABDEWA], AutoBootS[ABDEWA].name))
                AutoBootS[ABDEWA].s ?  psctl.setAutoBoot("dew1", "on") : psctl.setAutoBoot("dew1", "off");
            if(!strcmp(names[ABDEWB], AutoBootS[ABDEWB].name))
                AutoBootS[ABDEWB].s ?  psctl.setAutoBoot("dew1", "on") : psctl.setAutoBoot("dew2", "off");
            if(!strcmp(names[ABUSB2], AutoBootS[ABUSB2].name))
                AutoBootS[ABUSB2].s ?  psctl.setAutoBoot("usb2", "on") : psctl.setAutoBoot("usb2", "off");
            if(!strcmp(names[ABUSB3], AutoBootS[ABUSB3].name))
                AutoBootS[ABUSB3].s ?  psctl.setAutoBoot("usb3", "on") : psctl.setAutoBoot("usb3", "off");
            if(!strcmp(names[ABUSB6], AutoBootS[ABUSB6].name))
                AutoBootS[ABUSB6].s ?  psctl.setAutoBoot("usb6", "on") : psctl.setAutoBoot("usb6", "off");
            
            // and update the port power switches
            AutoBootSP.s = IPS_OK;
            IDSetSwitch(&AutoBootSP, nullptr);
            return true;
        }
        **/
        
        // MP type
        if (strcmp(name, MPtypeSP.name) == 0)
        {
            IUUpdateSwitch(&MPtypeSP, states, names, n);
            
            int Index = IUFindOnSwitchIndex(&MPtypeSP);
            psctl.setMultiPort(Index);
            switch(Index) {
                case DC : {
                    deleteProperty(MPpwmNP.name);
                    deleteProperty(MPdewNP.name);
                    break;
                }
                case PWM : {
                    defineNumber(&MPpwmNP);
                    deleteProperty(MPdewNP.name);
                    break;
                }
                case DEW : {
                    defineNumber(&MPdewNP);
                    deleteProperty(MPpwmNP.name);
                    break;
                }
            }
            
            IDSetSwitch(&MPtypeSP, nullptr);
            MPtypeSP.s = IPS_OK;
            return true;
        }  
        
        // All on/off
        if (strcmp(name, AllSP.name) == 0)
        {
            IUUpdateSwitch(&AllSP, states, names, n);
            
            // ALLON
            if(IUFindOnSwitchIndex(&AllSP) == ALLON) 
            {
                //for loop to set all to on
                for (int i=0; i < POWER_N; i++)
                    PortCtlS[i].s = ISS_ON;
            
                IDSetSwitch(&PortCtlSP, nullptr);
            
                psctl.setPowerState("out1", "yes");
                psctl.setPowerState("out2", "yes");
                psctl.setPowerState("out3", "yes");
                psctl.setPowerState("out4", "yes");
                psctl.setPowerState("var", "yes");
            
                switch (psctl.statusMap["MP"].setting) {
                    case DC : {
                        psctl.setPowerState("mp", "yes");
                        break;
                    }
                    case PWM : {
                        //TODO look up previous value
                        psctl.setPWM(50);
                        MPpwmN[0].value = 50;
                        IDSetNumber(&MPpwmNP, nullptr);
                        break;
                    }
                    case DEW : {
                        //TODO look up previous value
                        psctl.setDew(2, 50);
                        MPdewN[0].value = 50;
                        IDSetNumber(&MPdewNP, nullptr);
                        break;
                    }
                }
            
                AllS[ALLON].s = ISS_ON;
                AllS[ALLOFF].s = ISS_OFF;
                AllS[AUTON].s = ISS_OFF;
                AllS[REBOOT].s = ISS_OFF;
                AllSP.s = IPS_OK;
                IDSetSwitch(&AllSP, nullptr);
                return true;
            }
            
            // ALLOFF
            else if(IUFindOnSwitchIndex(&AllSP) == ALLOFF)
            {
                IUResetSwitch(&PortCtlSP);
                IDSetSwitch(&PortCtlSP, nullptr);
            
                psctl.setPowerState("out1", "no");
                psctl.setPowerState("out2", "no");
                psctl.setPowerState("out3", "no");
                psctl.setPowerState("out4", "no");
                psctl.setPowerState("var", "no");
            
                switch (psctl.statusMap["MP"].setting) {
                    case DC : {
                        psctl.setPowerState("mp", "no");
                        break;
                    }
                    case PWM : {
                        psctl.setPWM(0);
                        MPpwmN[0].value = 0;
                        IDSetNumber(&MPpwmNP, nullptr);
                        break;
                    }
                    case DEW : {
                        psctl.setDew(2, 0);
                        MPdewN[0].value = 0;
                        IDSetNumber(&MPdewNP, nullptr);
                        break;
                    }
                }
            
                AllS[ALLOFF].s = ISS_ON;
                AllS[ALLON].s = ISS_OFF;
                AllS[AUTON].s = ISS_OFF;
                AllS[REBOOT].s = ISS_OFF;
                AllSP.s = IPS_OK;
                IDSetSwitch(&AllSP, nullptr);
                return true;
            }
            
            
            // AUTON
            else if(IUFindOnSwitchIndex(&AllSP) == AUTON)
            {
                /** TODO does not work and has stability issues
                // Set state according to autoboot selections
                IUUpdateSwitch(&AutoBootSP, states, names, n);
                if(!strcmp(names[ABOUT1], AutoBootS[ABOUT1].name))
                    AutoBootS[ABOUT1].s ?  psctl.setPowerState("out1", "on") : psctl.setPowerState("out1", "off");
                if(!strcmp(names[ABOUT2], AutoBootS[ABOUT2].name))
                    AutoBootS[ABOUT2].s ?  psctl.setPowerState("out2", "on") : psctl.setPowerState("out2", "off");
                if(!strcmp(names[ABOUT3], AutoBootS[ABOUT3].name))
                    AutoBootS[ABOUT3].s ?  psctl.setPowerState("out3", "on") : psctl.setPowerState("out3", "off");
                if(!strcmp(names[ABOUT4], AutoBootS[ABOUT4].name))
                    AutoBootS[ABOUT4].s ?  psctl.setPowerState("out4", "on") : psctl.setPowerState("out4", "off");
                if(!strcmp(names[ABVAR], AutoBootS[ABVAR].name))
                    AutoBootS[ABVAR].s ?  psctl.setPowerState("var", "on") : psctl.setPowerState("var", "off");
                if(!strcmp(names[ABMP], AutoBootS[ABMP].name))
                    AutoBootS[ABMP].s ?  psctl.setPowerState("mp", "on") : psctl.setPowerState("mp", "off");
                if(!strcmp(names[ABDEWA], AutoBootS[ABDEWA].name))
                    AutoBootS[ABDEWA].s ?  psctl.setPowerState("dew1", "on") : psctl.setPowerState("dew1", "off");
                if(!strcmp(names[ABDEWB], AutoBootS[ABDEWB].name))
                    AutoBootS[ABDEWB].s ?  psctl.setPowerState("dew1", "on") : psctl.setPowerState("dew2", "off");
                if(!strcmp(names[ABUSB2], AutoBootS[ABUSB2].name))
                    AutoBootS[ABUSB2].s ?  psctl.setPowerState("usb2", "on") : psctl.setPowerState("usb2", "off");
                if(!strcmp(names[ABUSB3], AutoBootS[ABUSB3].name))
                    AutoBootS[ABUSB3].s ?  psctl.setPowerState("usb3", "on") : psctl.setPowerState("usb3", "off");
                if(!strcmp(names[ABUSB6], AutoBootS[ABUSB6].name))
                    AutoBootS[ABUSB6].s ?  psctl.setPowerState("usb6", "on") : psctl.setPowerState("usb6", "off");
                **/
                LOG_INFO("DIAG: not implemented yet");
                
                AllS[ALLOFF].s = ISS_OFF;
                AllS[ALLON].s = ISS_OFF;
                AllS[AUTON].s = ISS_ON;
                AllS[REBOOT].s = ISS_OFF;
                AllSP.s = IPS_OK;
                IDSetSwitch(&AllSP, nullptr);
                return true;
            }
            
            // Reboot Power*Star Hub
            else if (IUFindOnSwitchIndex(&AllSP) == REBOOT)
            {
                IUUpdateSwitch(&RebootSP, states, names, n);
            
                psctl.restart();
                psctl.Disconnect();
                LOG_ERROR("Power*Star Hub is rebooting");
                PSpower::Disconnect();
                
                AllS[ALLOFF].s = ISS_OFF;
                AllS[ALLON].s = ISS_OFF;
                AllS[AUTON].s = ISS_OFF;
                AllS[REBOOT].s = ISS_ON;
                AllSP.s = IPS_OK;
                IDSetSwitch(&AllSP, nullptr);
                return true;
                
            }
        }  
        
        if (strstr(name, "FOCUS"))
            return FI::processSwitch(dev, name, states, names, n);
    }

    // Nobody has claimed this, so let the parent handle it
    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

/***************************************************************/
/* New Text */
/***************************************************************/
bool PSpower::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Port names
        if (strcmp(name, PortLabelsTP.name) == 0)
        {
            PortLabelsTP.s = IPS_OK;
            IUUpdateText(&PortLabelsTP, texts, names, n);
            IDSetText(&PortLabelsTP, nullptr);
            return true;
        }
        
        // USB names
        if (strcmp(name, USBLabelsTP.name) == 0)
        {
            USBLabelsTP.s = IPS_OK;
            IUUpdateText(&USBLabelsTP, texts, names, n);
            IDSetText(&USBLabelsTP, nullptr);
            return true;
        }
        
        // Dew names
        if (strcmp(name, DewLabelsTP.name) == 0)
        {
            DewLabelsTP.s = IPS_OK;
            IUUpdateText(&DewLabelsTP, texts, names, n);
            IDSetText(&DewLabelsTP, nullptr);
            return true;
        }
        
        return true;
    }
    
    // Nobody has claimed this, so let the parent handle it
    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

/***************************************************************/
/* New Number */
/***************************************************************/
bool PSpower::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // LED brightness
        if (strcmp(name, PowerLEDNP.name) == 0)
        {
            IUUpdateNumber(&PowerLEDNP, values, names, n);
            if (!psctl.setLED(int(PowerLEDN[0].value)))
                PowerLEDNP.s = IPS_ALERT;
            else
                PowerLEDNP.s = IPS_OK;
            
            IDSetNumber(&PowerLEDNP, nullptr);
            return true;
        }
        // Fault mask
        if (strcmp(name, FaultMaskNP.name) == 0)
        {
            IUUpdateNumber(&FaultMaskNP, values, names, n);
            faultMask = int(FaultMaskN[0].value);
            LOGF_INFO("Fault mask is set to: %li", faultMask);
            
            //TODO testing
            FaultMaskN[0].value = 24;
            
            FaultMaskNP.s = IPS_OK;
            IDSetNumber(&FaultMaskNP, nullptr);
            return true;
        }
        
        // Variable voltage setting
        if (strcmp(name, VarSettingNP.name) == 0)
        {
            IUUpdateNumber(&VarSettingNP, values, names, n);
            if (!psctl.setVar(uint8_t(VarSettingN[0].value)*10))
                VarSettingNP.s = IPS_ALERT;
            else
                VarSettingNP.s = IPS_OK;
            
            IDSetNumber(&VarSettingNP, nullptr);
            return true;
        }
        
        // Set Dew %
        if (strcmp(name, DewPowerNP.name) == 0)
        {
            IUUpdateNumber(&DewPowerNP, values, names, n);
            if(!strcmp(names[DEW1], DewPowerN[DEW1].name))
                if(!psctl.setDew(DEW1, uint8_t(DewPowerN[DEW1].value))) {
                    DewPowerNP.s = IPS_ALERT;
                    return false;
                }
                
            if(!strcmp(names[DEW2], DewPowerN[DEW2].name))
                if(!psctl.setDew(DEW2, uint8_t(DewPowerN[DEW2].value))) {
                    DewPowerNP.s = IPS_ALERT;
                    return false;
                }
            
            DewPowerNP.s = IPS_OK;
            IDSetNumber(&DewPowerNP, nullptr);
            return true;
        }
        
        // Set MP PWM Rate
        if (strcmp(name, MPpwmNP.name) == 0)
        {
            IUUpdateNumber(&MPpwmNP, values, names, n);
            if(!psctl.setPWM(uint16_t(MPpwmN[0].value)))
                MPpwmNP.s = IPS_ALERT;
            
            MPpwmNP.s = IPS_OK;
            IDSetNumber(&MPpwmNP, nullptr);
            
            PortCtlS[MP].s = ISS_ON;
            IDSetSwitch(&PortCtlSP, nullptr);
            return true;
        }
        
        // Set MP Dew %
        if (strcmp(name, MPdewNP.name) == 0)
        {
            IUUpdateNumber(&MPdewNP, values, names, n);
            if(!psctl.setDew(2, uint16_t(MPdewN[0].value)))
                MPdewNP.s = IPS_ALERT;
            
            MPdewNP.s = IPS_OK;
            IDSetNumber(&MPdewNP, nullptr);
            
            PortCtlS[MP].s = ISS_ON;
            IDSetSwitch(&PortCtlSP, nullptr);
            return true;
        }

        
        if (strstr(name, "FOCUS_"))
            return FI::processNumber(dev, name, values, names, n);

        if (strstr(name, "WEATHER_"))
            return WI::processNumber(dev, name, values, names, n);
            
    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

/***************************************************************/
/* Supporting functions                                        */
/***************************************************************/
bool PSpower::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    FI::saveConfigItems(fp);
    // TODO not saving port or usb labels
    IUSaveConfigText(fp, &PortLabelsTP);
    IUSaveConfigText(fp, &USBLabelsTP);
    IUSaveConfigText(fp, &DewLabelsTP);
    IUSaveConfigNumber(fp, &FaultMaskNP);
    IUSaveConfigSwitch(fp, &AutoBootSP);
    return true;
}

/***************************************************************/
void PSpower::ISGetProperties(const char *dev)
{
    DefaultDevice::ISGetProperties(dev);
    loadConfig(true, PortLabelsTP.name);
    loadConfig(true, USBLabelsTP.name);
    loadConfig(true, DewLabelsTP.name);
    loadConfig(true, FaultMaskNP.name);
    loadConfig(true, AutoBootSP.name);
}

/***************************************************************/
/*    Timer Hit                                                */
/***************************************************************/
void PSpower::TimerHit()
{
    if (!isConnected())
        return;

    psctl.getStatus();
    
    PSpower::checkFaults();
    
    // handle focus update
    if (psctl.getAbsPosition(&currentTicks))
        FocusAbsPosN[0].value = currentTicks;
    
    m_Motor = static_cast<PS_MOTOR>(psctl.getFocusStatus());
    if (FocusAbsPosNP.s == IPS_BUSY || FocusRelPosNP.s == IPS_BUSY) {
        if (m_Motor == PS_NOT_MOVING && targetPosition == FocusAbsPosN[0].value) {
            if (FocusRelPosNP.s == IPS_BUSY) {
                FocusRelPosNP.s = IPS_OK;
                IDSetNumber(&FocusRelPosNP, nullptr);
            }

            FocusAbsPosNP.s = IPS_OK;
            LOGF_INFO("Focuser now at %d", targetPosition);
            LOG_DEBUG("Focuser reached target position.");
        }
        
        IDSetNumber(&FocusAbsPosNP, nullptr);
    }
    
    //Update sensor data (volts/amps/watts)
    VoltsIn = psctl.statusMap["IN"].levels;
    AmpsIn = psctl.statusMap["IN"].current;
    
    AmpHrs += (AmpsIn * POLLMS)/(60*60*1000.0);
    WattHrs += (VoltsIn * AmpsIn * POLLMS)/(60*60*1000.0);
    
    PowerSensorsN[SENSOR_VOLTAGE].value = VoltsIn;
    PowerSensorsN[SENSOR_CURRENT].value = AmpsIn;
    PowerSensorsN[SENSOR_POWER].value = (VoltsIn * AmpsIn);
    PowerSensorsN[SENSOR_AMP_HOURS].value = AmpHrs;
    PowerSensorsN[SENSOR_WATT_HOURS].value = WattHrs;
    IDSetNumber(&PowerSensorsNP, nullptr);
    
    //Set status according to faults
    if (!PSpower::checkFaults())
        PowerSensorsNP.s = IPS_OK;
    else
        PowerSensorsNP.s = IPS_ALERT;
    
    //TODO add weather alerts
    
    //Update Weather fields
    /**
    Temp = psctl.getTemperature();
    Hum = psctl.getHumidity();
    Dp = Temp - ((100 - Hum)/5.0);
    Dep = Temp - Dp;
    WEATHERN[TEMP].value = Temp;
    WEATHERN[HUM].value = Hum;
    WEATHERN[DP].value = Dp;
    WEATHERN[DEP].value = Dep;
    IDSetNumber(&WEATHERNP, nullptr);
    if (Dep < 1.0)
        WEATHERNP.s = IPS_ALERT;
    else if (Dep < 3.0)
        WEATHERNP.s = IPS_BUSY;
    else
        WEATHERNP.s = IPS_OK;
    **/
    
    // Update weather params
    Temp = psctl.getTemperature();
    Hum = psctl.getHumidity();
    setParameterValue("WEATHER_TEMPERATURE", Temp);
    setParameterValue("WEATHER_HUMIDITY", Hum);
    setParameterValue("WEATHER_DEW_POINT", Temp - ((100 - Hum)/5.0));
    setParameterValue("WEATHER_DP_DEP", Temp - Dp);
    
    WI::syncCriticalParameters();
    
    // Update USB enable lights
    USBlightsL[USB2].s = psctl.statusMap["USB2"].state ? IPS_OK : IPS_ALERT;
    USBlightsL[USB3].s = psctl.statusMap["USB3"].state ? IPS_OK : IPS_ALERT;
    USBlightsL[USB6].s = psctl.statusMap["USB6"].state ? IPS_OK : IPS_ALERT;
    IDSetLight(&USBlightsLP, nullptr);
    
    // Update Power enable lights
    PORTlightsL[OUT1].s = psctl.statusMap["Out1"].state ? IPS_OK : IPS_ALERT;
    PORTlightsL[OUT2].s = psctl.statusMap["Out2"].state ? IPS_OK : IPS_ALERT;
    PORTlightsL[OUT3].s = psctl.statusMap["Out3"].state ? IPS_OK : IPS_ALERT;
    PORTlightsL[OUT4].s = psctl.statusMap["Out4"].state ? IPS_OK : IPS_ALERT;
    PORTlightsL[VAR].s = psctl.statusMap["Var"].state ? IPS_OK : IPS_ALERT;
    //TODO MP is complicated, if dc then on/off, if dew or pwm then 0 is off, everything else is on
    PORTlightsL[MP].s = psctl.statusMap["MP"].state ? IPS_OK : IPS_ALERT;
    IDSetLight(&PORTlightsLP, nullptr);
    
    // Dew enabled lights
    DEWlightsL[DEW1].s = (psctl.statusMap["Dew1"].setting > 0) ? IPS_OK : IPS_ALERT;
    DEWlightsL[DEW2].s = (psctl.statusMap["Dew2"].setting > 0) ? IPS_OK : IPS_ALERT;
    IDSetLight(&DEWlightsLP, nullptr);
    
    // Port Currents
    PortCurrentN[OUT1].value = psctl.statusMap["Out1"].current;
    PortCurrentN[OUT2].value = psctl.statusMap["Out2"].current;
    PortCurrentN[OUT3].value = psctl.statusMap["Out3"].current;
    PortCurrentN[OUT4].value = psctl.statusMap["Out4"].current;
    PortCurrentN[VAR].value = psctl.statusMap["Var"].current;
    PortCurrentN[MP].value = psctl.statusMap["MP"].current;
    IDSetNumber(&PortCurrentNP, nullptr);
    
    // Update dew current fields
    DewCurrentN[DEW1].value = psctl.statusMap["Dew1"].current;
    DewCurrentN[DEW2].value = psctl.statusMap["Dew2"].current;
    DewCurrentN[MP].value = psctl.statusMap["MP"].levels;
    IDSetNumber(&DewCurrentNP, nullptr);
    
    // Update dew % power fields
    DewPowerN[DEW1].value = psctl.statusMap["Dew1"].setting;
    DewPowerN[DEW2].value = psctl.statusMap["Dew2"].setting;
    IDSetNumber(&DewPowerNP, nullptr);
    
    // Update variable voltage setting
    VarSettingN[0].value = psctl.statusMap["Var"].levels;
    IDSetNumber(&VarSettingNP, nullptr);
    
    //TODO set MP rate and dew fields and var volts fields and correct MP type switch and autoboot switches (or do this during init)
    
    SetTimer(POLLMS);
}

/**********************************************************/
/*   Focuser Routines                                     */
/**********************************************************/
/**********************************************************/
IPState PSpower::MoveAbsFocuser(uint32_t targetTicks)
{
    if ( ! psctl.MoveAbsFocuser(targetTicks))
        return IPS_ALERT;

    targetPosition = targetTicks;
    FocusAbsPosNP.s = IPS_BUSY;
    
    LOGF_INFO("Set abs position to %l", targetTicks);

    return IPS_BUSY;
}

//************************************************************
IPState PSpower::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int direction = (dir == FOCUS_INWARD) ? -1 : 1;
    int reversed = (FocusReverseS[INDI_ENABLED].s == ISS_ON) ? -1 : 1;
    int relative = static_cast<int>(ticks);

    int targetAbsPosition = FocusAbsPosN[0].value + (relative * direction * reversed);

    targetAbsPosition = std::min(static_cast<uint32_t>(FocusMaxPosN[0].value),static_cast<uint32_t>(std::max(static_cast<int>(FocusAbsPosN[0].min), targetAbsPosition)));

    return MoveAbsFocuser(targetAbsPosition);
}

//************************************************************
bool PSpower::AbortFocuser()
{
    LOG_INFO("Aborting Focus");
    FocusAbsPosNP.s = IPS_OK;
    
    return psctl.AbortFocuser();
}

//************************************************************
bool PSpower::SyncFocuser(uint32_t ticks)
{
    if ( ! psctl.setAbsPosition(ticks))
        return false;

    targetPosition = ticks;
    simPosition = ticks;
    FocusAbsPosNP.s = IPS_OK;
    LOGF_INFO("Set abs position to %l", ticks);

    return psctl.SyncFocuser(ticks);
}

//************************************************************
bool PSpower::SetFocuserMaxPosition(uint32_t ticks)
{
    LOGF_INFO("Set max position to %l", ticks);
    return psctl.SetFocuserMaxPosition(ticks);
}

/**********************************************************/
/*   Handle Faults                                        */
/**********************************************************/
/**********************************************************/
uint32_t PSpower::checkFaults()
{
    uint32_t faultstat = psctl.getFaultStatus(curProfile.faultMask);
    
    //faultstat = 0x00010004;  // this is for testing the fault system
    
    //Update faults field
    if (faultstat & 0xffff0000) {
        uint8_t ffaults = (faultstat & 0xffff0000) >> 16;
        //TODO this isn't working ...
        FaultsN[FFATAL].value = long(ffaults);
        //FaultsN[FFATAL].value = 24;
        IDSetNumber(&FaultsNP, nullptr);
        if (!FatalOccured) {
            LOGF_ERROR("Fatal fault(s) have occurred: %04X, you need to restart Power*Star", ffaults);
            FatalOccured = true;
        }
    }
    
    if (faultstat & 0x0000ffff) {
        uint8_t nfaults = faultstat & 0x0000ffff;
        FaultsN[NFATAL].value = float(nfaults);
        FaultsN[FFATAL].value = 24;
        IDSetNumber(&FaultsNP, nullptr);
        if (!NonFatalOccured) {
            LOGF_ERROR("Non-Fatal Fault(s) have occurred: %04X", nfaults);
            NonFatalOccured = true;
        }
    }
    
    if (!faultstat) {
        FaultsNP.s = IPS_OK;
        FaultsClearSP.s = IPS_OK;
    }
    else {
        FaultsNP.s = IPS_ALERT;
        FaultsClearSP.s = IPS_ALERT;
    }
    IDSetNumber(&FaultsNP, nullptr);
    IDSetSwitch(&FaultsClearSP, nullptr);
    
    // level 1 byte 1 (non fatal) faults
    //Over/Under Voltage on 12V input
    if (faultstat & 0x00000002)
        FaultStatusL[FSOverUnder].s = IPS_BUSY;
    // Over Current on 12V Input
    else if (faultstat & 0x00000004)
        FaultStatusL[FSTotalCurrent].s = IPS_BUSY;
    // Motor temperature sensor
    else if (faultstat & 0x00000008)
        FaultStatusL[FSMotorTemp].s = IPS_BUSY;
    // Bipolar Motor
    else if (faultstat & 0x00000010)
        FaultStatusL[FSMotorDriver].s = IPS_BUSY;
    //Internal voltage out of spec
    else if (faultstat & 0x00000020)
        FaultStatusL[FSInternal].s = IPS_BUSY;
    // Environment sensor
    else if (faultstat & 0x00000040)
        FaultStatusL[FSEnvironment].s = IPS_BUSY;
    // Variable voltage over/under voltage by 6.25%
    else if (faultstat & 0x00000080)
        FaultStatusL[FSVarVoltage].s = IPS_BUSY;
            
    // level 1 byte 2 (non fatal) faults
    // Out1 over 15A
    else if (faultstat & 0x00000100)
        FaultStatusL[FSOut1].s = IPS_BUSY;
    // Out2 over 10A
    else if (faultstat & 0x00000200)
        FaultStatusL[FSOut2].s = IPS_BUSY;
    // Out3 over 6A
    else if (faultstat & 0x00000400)
        FaultStatusL[FSOut3].s = IPS_BUSY;
    // Out4 over 6A
    else if (faultstat & 0x00000800)
        FaultStatusL[FSOut4].s = IPS_BUSY;
    // Dew1 over 6A
    else if (faultstat & 0x00001000)
        FaultStatusL[FSDewA].s = IPS_BUSY;
    // Dew2 over 6A
    else if (faultstat & 0x00002000)
        FaultStatusL[FSDewB].s = IPS_BUSY;
    // MP over 6A
    else if (faultstat & 0x00004000)
        FaultStatusL[FSFMP].s = IPS_BUSY;
    // Position Change
    else if (faultstat & 0x00008000)
        FaultStatusL[FSPosChange].s = IPS_BUSY;
                
    // Fatal faults:
    if (faultstat & 0x00000002) {
        FaultsNP.s = IPS_ALERT;
        FaultsClearSP.s = IPS_ALERT;
    }
            
    // level 2 byte 1 (fatal) faults
    // Fatal: Out1 over 30A
    else if (faultstat & 0x00010000)
        FaultStatusL[FSOut1].s = IPS_ALERT;
    // Fatal: Out2 over 20A
    else if (faultstat & 0x00020000)
        FaultStatusL[FSOut2].s = IPS_ALERT;
    // Fatal: Out3 over 10A
    else if (faultstat & 0x00040000)
        FaultStatusL[FSOut3].s = IPS_ALERT;
    // Fatal: Out4 over 10A
    else if (faultstat & 0x00080000)
        FaultStatusL[FSOut4].s = IPS_ALERT;
    // Fatal: Dew1 over 10A
    else if (faultstat & 0x00100000)
        FaultStatusL[FSDewA].s = IPS_ALERT;
    //Fatal: Dew2 over 10A
    else if (faultstat & 0x00200000)
        FaultStatusL[FSDewB].s = IPS_ALERT;
    // Fatal: VAR over 10A
    else if (faultstat & 0x00400000)
        FaultStatusL[FSVarVoltage].s = IPS_ALERT;
    //Fatal: MP over 10A
    else if (faultstat & 0x00800000)
        FaultStatusL[FSFMP].s = IPS_ALERT;
            
            
    // level 2 byte 2 (fatal) faults
    // Fatal: 12V input under-voltage
    else if (faultstat & 0x01000000)
        FaultStatusL[FSOverUnder].s = IPS_ALERT;
    // Fatal: 12V input over-voltage
    else if (faultstat & 0x02000000)
        FaultStatusL[FSOverUnder].s = IPS_ALERT;
    // Fatal: Total current over-current
    else if (faultstat & 0x04000000)
        FaultStatusL[FSTotalCurrent].s = IPS_ALERT;
    // Fatal: Current sensor
    else if (faultstat & 0x08000000)
        FaultStatusL[FSTotalCurrent].s = IPS_ALERT;
    // Fatal: Internal 3.3V under-voltage
    else if (faultstat & 0x10000000)
        FaultStatusL[FSInternal].s = IPS_ALERT;
    // Fatal: Internal 3.3V over-voltage
    else if (faultstat & 0x20000000)
        FaultStatusL[FSInternal].s = IPS_ALERT;
    // Fatal: Internal 5V under-voltage
    else if (faultstat & 0x40000000)
        FaultStatusL[FSInternal].s = IPS_ALERT;
            
    // 0x80000000 not used
            
    IDSetNumber(&FaultsNP, nullptr);
    IDSetSwitch(&FaultsClearSP, nullptr);
    IDSetLight(&FaultStatusLP, nullptr);

    return faultstat;
}


