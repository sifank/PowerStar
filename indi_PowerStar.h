/***********************************************************************
*  Program:      indi_PowerStar.h
*  Version:      20210305
*  Author:       Sifan S. Kahale
*  Description:  PowerStar.h file

***********************************************************************/
#pragma once

//#include <iostream>
//#include <memory>
//#include <map>
//#include <cmath>
#include <string.h>
#include <stdio.h>
#include <defaultdevice.h>
#include "indifocuserinterface.h"
#include "indiweatherinterface.h"
#include <cstring>
#include "PScontrol.h"

using namespace std;

typedef enum {     PS_NOT_MOVING,
                   PS_MOVING_IN,
                   PS_MOVING_OUT,
                   PS_BUSY,
                   PS_UNK,
                   PS_LOCKED
} PS_MOTOR;

class PSpower : public INDI::DefaultDevice, public INDI::FocuserInterface, public INDI::WeatherInterface
{
protected:
    // Weather Overrides
        virtual IPState updateWeather() override
        {
            return IPS_OK;
        }
    
public:
    PSCTL psctl;
    
    PSpower();
    
    virtual ~PSpower() = default;
    virtual bool initProperties() override;
    virtual const char *getDefaultName() override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n) override;
    virtual void ISGetProperties(const char *dev) override;
    virtual bool updateProperties() override;
    virtual void TimerHit() override;
    virtual bool   Connect() override;
	virtual bool   Disconnect() override;

    // focus
    virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
    virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
    virtual bool AbortFocuser() override;
    virtual bool SyncFocuser(uint32_t ticks) override;
    virtual bool SetFocuserMaxPosition(uint32_t ticks) override;
    
private:
    virtual bool saveConfigItems(FILE *fp) override;
    
    bool setAbsPosition(uint32_t ticks);
    bool getAbsPosition(uint32_t *ticks);
    bool setMaxPosition(uint32_t ticks);
    bool getMaxPosition(uint32_t *ticks);
    bool setPosition(uint32_t ticks, uint8_t cmdCode);
    bool getPosition(uint32_t *ticks, uint8_t cmdCode);
    uint32_t checkFaults();
    
    float   Temp;
    float   Hum;
    float   Dp;
    float   Dep;
    float   VoltsIn;
    float   AmpsIn;
    float   AmpHrs;
    float   WattHrs;
    uint32_t faults;
    uint32_t faultMask;
    
    PS_MOTOR m_Motor { PS_NOT_MOVING };
    int32_t  simPosition { 0 };
    uint32_t maximumPosition = 0;
    uint32_t relitivePosition = 0;
    uint32_t targetPosition { 0 };
    uint8_t* response = {0};
    uint32_t currentTicks = 0;
    
    // Focus
    enum {
            HSM,
            PDMS,
            UNI12,
            Template_N,
        };
        ISwitch TemplateS[Template_N];
        ISwitchVectorProperty TemplateSP;
        
        enum {
            Unipolar,
            Bipolar,
            MtrType_N,
        };
        ISwitch MtrTypeS[MtrType_N];
        ISwitchVectorProperty MtrTypeSP;
        
        enum {
            In,
            Out,
            PrefDir_N,
        };
        ISwitch PrefDirS[PrefDir_N];
        ISwitchVectorProperty PrefDirSP;
        
        enum {
            Yes,
            No,
            RevMtr_N,
        };
        ISwitch RevMtrS[RevMtr_N];
        ISwitchVectorProperty RevMtrSP;
        
        ISwitch PermFocS[RevMtr_N];
        ISwitchVectorProperty PermFocSP;
        
        enum {
            None,
            Motor,
            Env,
            TempComp_N,
        };
        ISwitch TempCompS[TempComp_N];
        ISwitchVectorProperty TempCompSP;
        
        enum {
            Backlash,
            IdleCur,
            StepPer,
            TempCoef,
            DrvCur,
            TempHys,
            MtrProf_N,
        };            
        INumber MtrProfN[MtrProf_N];
        INumberVectorProperty MtrProfNP;
    
    // Power Usage
    enum
    {
        SENSOR_VOLTAGE,
        SENSOR_CURRENT,
        SENSOR_POWER,
        SENSOR_AMP_HOURS,
        SENSOR_WATT_HOURS,        
        SENSOR_N,
    };
    INumber PowerSensorsN[SENSOR_N];
    INumberVectorProperty PowerSensorsNP;
        
    ISwitch PowerClearS[0] {};
    ISwitchVectorProperty PowerClearSP;
    
    // Power LED
    INumber PowerLEDN[1];
    INumberVectorProperty PowerLEDNP;
    
    // Weather
    enum
    {
        TEMP,
        HUM,
        DP,
        DEP,
        WEATHER_N,
    };
    INumber WEATHERN[WEATHER_N];
    INumberVectorProperty WEATHERNP;
    
    // Firmware
    enum
    {
        FIRMWARE_VERSION,
        DRIVER_VERSION,
        FIRMWARE_N,
    };
    IText FirmwareT[FIRMWARE_N];
    ITextVectorProperty FirmwareTP;
    
    // Power
    enum
    {
        OUT1,
        OUT2,
        OUT3,
        OUT4,
        VAR,
        MP,
        POWER_N,
    };
    ISwitch PortCtlS[POWER_N];
    ISwitchVectorProperty PortCtlSP;
    
    IText PortLabelsT[POWER_N];
    ITextVectorProperty PortLabelsTP;
    
    INumber PortCurrentN[POWER_N];
    INumberVectorProperty PortCurrentNP;
    
    ILight PORTlightsL[POWER_N];
    ILightVectorProperty PORTlightsLP;
    
     enum
    {
        ABOUT1,
        ABOUT2,
        ABOUT3,
        ABOUT4,
        ABVAR,
        ABMP,
        ABDEWA,
        ABDEWB,
        ABUSB2,
        ABUSB3,
        ABUSB6,
        AutoBoot_N,
    };
    ISwitch AutoBootS[AutoBoot_N];
    ISwitchVectorProperty AutoBootSP;
    
    enum
    {
        DEW1,
        DEW2,
        DEW_N,
    };
    INumber DewPowerN[DEW_N];
    INumberVectorProperty DewPowerNP;
    
    IText DewLabelsT[DEW_N];
    ITextVectorProperty DewLabelsTP;
    
    ILight DEWlightsL[DEW_N];
    ILightVectorProperty DEWlightsLP;
    
    INumber DewCurrentN[DEW_N];
    INumberVectorProperty DewCurrentNP;
    
    enum
    {
        USB1,
        USB2,
        USB3,
        USB4,
        USB5,
        USB6,
        USB_N,
    };
    IText USBLabelsT[USB_N];
    ITextVectorProperty USBLabelsTP;
    
    ILight USBlightsL[USB_N];
    ILightVectorProperty USBlightsLP;
    
    enum
    {
        PUSB2,
        PUSB3,
        PUSB6,
        USBPW_N,
    };
    ISwitch USBpwS[USBPW_N];
    ISwitchVectorProperty USBpwSP;
    
    // Faults    
    enum{
        NFATAL,
        FFATAL,
        Fatal_N,
    };
    INumber FaultsN[Fatal_N];
    INumberVectorProperty FaultsNP;
    
    INumber FaultMaskN[1];
    INumberVectorProperty FaultMaskNP;
    
    ISwitch FaultsClearS[1] {};
    ISwitchVectorProperty FaultsClearSP;
    
    bool FatalOccured = false;
    bool NonFatalOccured = false;
    
    enum {
        FSOverUnder,
        FSTotalCurrent,
        FSOut1,
        FSOut2,
        FSOut3,
        FSOut4,
        FSDewA,
        FSDewB,   
        FSVarVoltage,
        FSFMP,
        FSPosChange,
        FSMotorTemp,
        FSMotorDriver,
        FSInternal,
        FSEnvironment,
        FaultStatus_N,
    };
    ILight FaultStatusL[FaultStatus_N];
    ILightVectorProperty FaultStatusLP;
    
    enum {
       FS1OverUnder,
        FS1TotalCurrent,
        FS1Out1,
        FS1Out2,
        FS1Out3,
        FS1Out4,
        FS1DewA,
        FS1DewB,
        FaultStatus1_N,
    };
    ISwitch FaultMask1S[FaultStatus1_N];
    ISwitchVectorProperty FaultMask1SP;
    
    enum {     
        FS2VarVoltage,
        FS2FMP,
        FS2PosChange,
        FS2MotorTemp,
        FS2MotorDriver,
        FS2Internal,
        FS2Environment,
        FaultStatus2_N,
    };
    ISwitch FaultMask2S[FaultStatus2_N];
    ISwitchVectorProperty FaultMask2SP;
    
    // User Limits
    enum {
        InLowerLim,
        InUpperLim,
        VarLowerLim,
        VarUpperLim,
        TotalCurrLim,
        Out1CurrLim,
        Out2CurrLim,
        Out3CurrLim,
        Out4CurrLim,
        DewACurrLim,
        DewBCurrLim,
        MPCurrLim,
        UserLimits_N
    };
    INumber UserLimitsN[UserLimits_N];
    INumberVectorProperty UserLimitsNP;
    
    // Options
    INumber VarSettingN[1];
    INumberVectorProperty VarSettingNP;
    
    enum{
        DC,
        PWM,
        DEW,
        MP_N,
    };
    ISwitch MPtypeS[MP_N];
    ISwitchVectorProperty MPtypeSP;
    
    INumber MPpwmN[1];
    INumberVectorProperty MPpwmNP;
    
    INumber MPdewN[1];
    INumberVectorProperty MPdewNP;
    
    enum {
        ALLON,
        ALLOFF,
        AUTON,
        REBOOT,
        All_N,
    };
    ISwitch AllS[All_N];
    ISwitchVectorProperty AllSP;
    
    ISwitch RebootS[0];
    ISwitchVectorProperty RebootSP;
    
    PowerStarProfile curProfile;
    
    static constexpr const char *POWER_TAB {"Power"};
    static constexpr const char *USB_TAB {"USB"};
    static constexpr const char *DEW_TAB {"DEW"};
    static constexpr const char *FIRMWARE_TAB {"Firmware"};
    static constexpr const char *FAULTS_TAB {"Faults"};
    static constexpr const char *FAULTMASK_TAB {"Fault Mask"};
    static constexpr const char *USRLIMIT_TAB {"User Limits"};
    static constexpr const char *SETTINGS_TAB {"Naming"};
    static constexpr const char *FOCUS {"Focus"};
    static constexpr const char *ENVIRONMENT_TAB {"Environment"};
};

