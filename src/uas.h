#ifndef _UAS_H_
#define _UAS_H_

#include "linkmanager.h"
#include "linkinterface.h"
#include "uasinterface.h"
#include "mavlinkprotocol.h"
#include <QVector3D>
#include <QDateTime>
class LinkManager;
class LinkInterface;
class MAVLinkProtocol;
/**
 *  A generic MAVLINK-connected MAV/UAV
 *
 * This class represents one vehicle. It can be used like the real vehicle, e.g. a call to halt()
 * will automatically send the appropriate messages to the vehicle. The vehicle state will also be
 * automatically updated by the comm architecture, so when writing code to e.g. control the vehicle
 * no knowledge of the communication infrastructure is needed.
 */
class UAS : public UASInterface
{
    Q_OBJECT
public:
    UAS(MAVLinkProtocol* protocol, int id = 0);
    ~UAS();

    enum BatteryType
    {
        NICD = 0,
        NIMH = 1,
        LIION = 2,
        LIPOLY = 3,
        LIFE = 4,
        AGZN = 5
    }; ///< The type of battery used

    float lipoFull;  ///< 100% charged voltage
    float lipoEmpty; ///< Discharged voltage

    /* MANAGEMENT */

    /**  The name of the robot */
    QString getUASName(void) const;
    /**  Get short state */
    const QString& getShortState() const;
    /**  Get short mode */
    const QString& getShortMode() const;
    /**  Translate from mode id to text */
    static QString getShortModeTextFor(int id);
    /**  Translate from mode id to audio text */
    static QString getAudioModeTextFor(int id);
    /**  Get the unique system id */
    int getUASID() const;
    /**  Get the airframe */
    int getAirframe() const
    {
        return airframe;
    }
    /**  Get the components */
    QMap<int, QString> getComponents();

    /**  The time interval the robot is switched on */
    quint64 getUptime() const;
    /**  Get the status flag for the communication */
    int getCommunicationStatus() const;
    /**  Add one measurement and get low-passed voltage */
    float filterVoltage(float value) const;
    /**  Get the links associated with this robot */
    QList<LinkInterface*>* getLinks();

    double getLocalX() const
    {
        return localX;
    }
    double getLocalY() const
    {
        return localY;
    }
    double getLocalZ() const
    {
        return localZ;
    }
    double getLatitude() const
    {
        return latitude;
    }
    double getLongitude() const
    {
        return longitude;
    }
    double getAltitude() const
    {
        return altitude;
    }
    virtual bool localPositionKnown() const
    {
        return isLocalPositionKnown;
    }
    virtual bool globalPositionKnown() const
    {
        return isGlobalPositionKnown;
    }

    double getRoll() const
    {
        return roll;
    }
    double getPitch() const
    {
        return pitch;
    }
    double getYaw() const
    {
        return yaw;
    }
    bool getSelected() const;
    QVector3D getNedPosGlobalOffset() const
    {
        return nedPosGlobalOffset;
    }

    QVector3D getNedAttGlobalOffset() const
    {
        return nedAttGlobalOffset;
    }

    friend class UASWaypointManager;

protected: //COMMENTS FOR TEST UNIT
    int uasId;                    ///< Unique system ID
    unsigned char type;           ///< UAS type (from type enum)
    quint64 startTime;            ///< The time the UAS was switched on
    CommStatus commStatus;        ///< Communication status
    QString name;                 ///< Human-friendly name of the vehicle, e.g. bravo
    int autopilot;                ///< Type of the Autopilot: -1: None, 0: Generic, 1: PIXHAWK, 2: SLUGS, 3: Ardupilot (up to 15 types), defined in MAV_AUTOPILOT_TYPE ENUM
    int airframe;                 ///< The airframe type
    uint32_t hardwareVersion;     ///< bytes: maj|min|rev|ID
    uint32_t firmwareVersion;     ///< bytes: maj|min|patch(x2B)
    QString hardwareVersionStr;
    QString firmwareVersionStr;

    QList<LinkInterface*>* links; ///< List of links this UAS can be reached by
    QList<int> unknownPackets;    ///< Packet IDs which are unknown and have been received
    MAVLinkProtocol* mavlink;     ///< Reference to the MAVLink instance
    BatteryType batteryType;      ///< The battery type
    int cells;                    ///< Number of cells


    QList<double> actuatorValues;
    QList<QString> actuatorNames;

    QList<double> motorValues;
    QList<QString> motorNames;
    QMap<int, QString> components;  ///< IDs and names of all detected onboard components

    double thrustSum;           ///< Sum of forward/up thrust of all thrust actuators, in Newtons
    double thrustMax;           ///< Maximum forward/up thrust of this vehicle, in Newtons

    // Battery stats
    float fullVoltage;          ///< Voltage of the fully charged battery (100%)
    float emptyVoltage;         ///< Voltage of the empty battery (0%)
    float startVoltage;         ///< Voltage at system start
    float tickVoltage;          ///< Voltage where 0.1 V ticks are told
    float lastTickVoltageValue; ///< The last voltage where a tick was announced
    float tickLowpassVoltage;   ///< Lowpass-filtered voltage for the tick announcement
    float warnVoltage;          ///< Voltage where QGC will start to warn about low battery
    float warnLevelPercent;     ///< Warning level, in percent
    double currentVoltage;      ///< Voltage currently measured
    float lpVoltage;            ///< Low-pass filtered voltage
    double currentCurrent;      ///< Battery current currently measured
    bool batteryRemainingEstimateEnabled; ///< If the estimate is enabled, QGC will try to estimate the remaining battery life
    float chargeLevel;          ///< Charge level of battery, in percent
    int timeRemaining;          ///< Remaining time calculated based on previous and current
    uint8_t mode;              ///< The current mode of the MAV
    uint32_t custom_mode;       ///< The current mode of the MAV
    int status;                 ///< The current status of the MAV
    uint32_t navMode;                ///< The current navigation mode of the MAV
    quint64 onboardTimeOffset;

    bool controlRollManual;     ///< status flag, true if roll is controlled manually
    bool controlPitchManual;    ///< status flag, true if pitch is controlled manually
    bool controlYawManual;      ///< status flag, true if yaw is controlled manually
    bool controlThrustManual;   ///< status flag, true if thrust is controlled manually

    double manualRollAngle;     ///< Roll angle set by human pilot (radians)
    double manualPitchAngle;    ///< Pitch angle set by human pilot (radians)
    double manualYawAngle;      ///< Yaw angle set by human pilot (radians)
    double manualThrust;        ///< Thrust set by human pilot (radians)
    float receiveDropRate;      ///< Percentage of packets that were dropped on the MAV's receiving link (from GCS and other MAVs)
    float sendDropRate;         ///< Percentage of packets that were not received from the MAV by the GCS
    bool lowBattAlarm;          ///< Switch if battery is low
    bool positionLock;          ///< Status if position information is available or not
    double localX;
    double localY;
    double localZ;
    double latitude;
    double longitude;
    double altitude;
    double speedX;              ///< True speed in X axis
    double speedY;              ///< True speed in Y axis
    double speedZ;              ///< True speed in Z axis
    double roll;
    double pitch;
    double yaw;
    quint64 lastHeartbeat;      ///< Time of the last heartbeat message
    QTimer* statusTimeout;      ///< Timer for various status timeouts

    int imageSize;              ///< Image size being transmitted (bytes)
    int imagePackets;           ///< Number of data packets being sent for this image
    int imagePacketsArrived;    ///< Number of data packets recieved
    int imagePayload;           ///< Payload size per transmitted packet (bytes). Standard is 254, and decreases when image resolution increases.
    int imageQuality;           ///< Quality of the transmitted image (percentage)
    int imageType;              ///< Type of the transmitted image (BMP, PNG, JPEG, RAW 8 bit, RAW 32 bit)
    int imageWidth;             ///< Width of the image stream
    int imageHeight;            ///< Width of the image stream
    QByteArray imageRecBuffer;  ///< Buffer for the incoming bytestream
    QImage image;               ///< Image data of last completely transmitted image
    quint64 imageStart;

    QMap<int, QMap<QString, QVariant>* > parameters; ///< All parameters
    bool paramsOnceRequested;       ///< If the parameter list has been read at least once

    bool attitudeKnown;             ///< True if attitude was received, false else
    UASParamManager* paramManager; ///< Parameter manager class
    QString shortStateText;         ///< Short textual state description
    QString shortModeText;          ///< Short textual mode description
    bool attitudeStamped;           ///< Should arriving data be timestamped with the last attitude? This helps with broken system time clocks on the MAV
    quint64 lastAttitude;           ///< Timestamp of last attitude measurement
//    QGCHilLink* simulation;         ///< Hardware in the loop simulation link
    bool isLocalPositionKnown;      ///< If the local position has been received for this MAV
    bool isGlobalPositionKnown;     ///< If the global position has been received for this MAV
    bool systemIsArmed;             ///< If the system is armed
    QVector3D nedPosGlobalOffset;   ///< Offset between the system's NED position measurements and the swarm / global 0/0/0 origin
    QVector3D nedAttGlobalOffset;   ///< Offset between the system's NED position measurements and the swarm / global 0/0/0 origin

public:
    /**  Set the current battery type */
    void setBattery(BatteryType type, int cells);
    /**  Estimate how much flight time is remaining */
    int calculateTimeRemaining();
    /**  Get the current charge level */
    float getChargeLevel();
    /**  Get the human-readable status message for this code */
    void getStatusForCode(int statusCode, QString& uasState, QString& stateDescription);
    /**  Get the human-readable navigation mode translation for this mode */
    QString getNavModeText(int mode);
    /**  Check if vehicle is in autonomous mode */
    bool isAuto();
    /**  Check if vehicle is armed */
    bool isArmed() const { return systemIsArmed; }

//    UASWaypointManager* getWaypointManager() {
//        return &waypointManager;
//    }
    /**  Get reference to the param manager **/
    UASParamManager* getParamManager() const {
        return paramManager;
    }
    /**  Get the HIL simulation */
//    QGCHilLink* getHILSimulation() const {
//        return simulation;
//    }
    // TODO Will be removed
    /**  Set reference to the param manager **/
    void setParamManager(UASParamManager* manager) {
        paramManager = manager;
    }
    int getSystemType();
    QString getSystemTypeName()
    {
        switch(type)
        {
        case MAV_TYPE_GENERIC:
            return "GENERIC";
            break;
        case MAV_TYPE_FIXED_WING:
            return "FIXED_WING";
            break;
        case MAV_TYPE_QUADROTOR:
            return "QUADROTOR";
            break;
        case MAV_TYPE_COAXIAL:
            return "COAXIAL";
            break;
        case MAV_TYPE_HELICOPTER:
            return "HELICOPTER";
            break;
        case MAV_TYPE_ANTENNA_TRACKER:
            return "ANTENNA_TRACKER";
            break;
        case MAV_TYPE_GCS:
            return "GCS";
            break;
        case MAV_TYPE_AIRSHIP:
            return "AIRSHIP";
            break;
        case MAV_TYPE_FREE_BALLOON:
            return "FREE_BALLOON";
            break;
        case MAV_TYPE_ROCKET:
            return "ROCKET";
            break;
        case MAV_TYPE_GROUND_ROVER:
            return "GROUND_ROVER";
            break;
        case MAV_TYPE_SURFACE_BOAT:
            return "BOAT";
            break;
        case MAV_TYPE_SUBMARINE:
            return "SUBMARINE";
            break;
        case MAV_TYPE_HEXAROTOR:
            return "HEXAROTOR";
            break;
        case MAV_TYPE_OCTOROTOR:
            return "OCTOROTOR";
            break;
        case MAV_TYPE_TRICOPTER:
            return "TRICOPTER";
            break;
        case MAV_TYPE_FLAPPING_WING:
            return "FLAPPING_WING";
            break;
        default:
            return "";
            break;
        }
    }

    QImage getImage();
    void requestImage();
    int getAutopilotType(){
        return autopilot;
    }
    QString getAutopilotTypeName()
    {
        switch (autopilot)
        {
        case MAV_AUTOPILOT_GENERIC:
            return "GENERIC";
            break;
        case MAV_AUTOPILOT_PIXHAWK:
            return "PIXHAWK";
            break;
        case MAV_AUTOPILOT_SLUGS:
            return "SLUGS";
            break;
        case MAV_AUTOPILOT_ARDUPILOTMEGA:
            return "ARDUPILOTMEGA";
            break;
        case MAV_AUTOPILOT_OPENPILOT:
            return "OPENPILOT";
            break;
        case MAV_AUTOPILOT_GENERIC_WAYPOINTS_ONLY:
            return "GENERIC_WAYPOINTS_ONLY";
            break;
        case MAV_AUTOPILOT_GENERIC_WAYPOINTS_AND_SIMPLE_NAVIGATION_ONLY:
            return "GENERIC_MISSION_NAVIGATION_ONLY";
            break;
        case MAV_AUTOPILOT_GENERIC_MISSION_FULL:
            return "GENERIC_MISSION_FULL";
            break;
        case MAV_AUTOPILOT_INVALID:
            return "NO AP";
            break;
        case MAV_AUTOPILOT_PPZ:
            return "PPZ";
            break;
        case MAV_AUTOPILOT_UDB:
            return "UDB";
            break;
        case MAV_AUTOPILOT_FP:
            return "FP";
            break;
        case MAV_AUTOPILOT_PX4:
            return "PX4";
            break;
        case MAV_AUTOPILOT_AUTOQUAD:
            return "AUTOQUAD";
            break;
        default:
            return "";
            break;
        }
    }

public slots:
    /**  Set the autopilot type */
    void setAutopilotType(int apType)
    {
        autopilot = apType;
        emit systemSpecsChanged(uasId);
    }
    /**  Set the type of airframe */
    void setSystemType(int systemType);
    /**  Set the specific airframe type */
    void setAirframe(int airframe)
    {
        if((airframe >= QGC_AIRFRAME_GENERIC) && (airframe < QGC_AIRFRAME_END_OF_ENUM))
        {
          this->airframe = airframe;
          emit systemSpecsChanged(uasId);
        }

    }
    /**  Set a new name **/
    void setUASName(const QString& name);
    /**  Executes a command **/
    void executeCommand(MAV_CMD command);
    /**  Executes a command with 7 params */
    void executeCommand(MAV_CMD command, int confirmation, float param1, float param2, float param3, float param4, float param5, float param6, float param7, int component);
    /**  Set the current battery type and voltages */
    void setBatterySpecs(const QString& specs);
    /**  Get the current battery type and specs */
    QString getBatterySpecs();
    float getBatteryEmptyVoltage();
    float getBatteryWarnVoltage();

    /**  Launches the system **/
    void launch();
    /**  Write this waypoint to the list of waypoints */
    //void setWaypoint(Waypoint* wp); FIXME tbd
    /**  Set currently active waypoint */
    //void setWaypointActive(int id); FIXME tbd
    /**  Order the robot to return home **/
    void home();
    /**  Order the robot to land **/
    void land();
    void halt();
    void go();

    /**  Enable / disable HIL */
//    void enableHilFlightGear(bool enable, QString options);
//    void enableHilXPlane(bool enable);


    /**  Send the full HIL state to the MAV */

    void sendHilState(	uint64_t time_us, float roll, float pitch, float yaw, float rollspeed,
                        float pitchspeed, float yawspeed, int32_t lat, int32_t lon, int32_t alt,
                        int16_t vx, int16_t vy, int16_t vz, int16_t xacc, int16_t yacc, int16_t zacc);

    /**  Places the UAV in Hardware-in-the-Loop simulation status **/
    void startHil();

    /**  Stops the UAV's Hardware-in-the-Loop simulation status **/
    void stopHil();


    /**  Stops the robot system. If it is an MAV, the robot starts the emergency landing procedure **/
    void emergencySTOP();

    /**  Kills the robot. All systems are immediately shut down (e.g. the main power line is cut). This might lead to a crash **/
    bool emergencyKILL();

    /**  Shut the system cleanly down. Will shut down any onboard computers **/
    void shutdown();

    /**  Set the target position for the robot to navigate to. */
    void setTargetPosition(float x, float y, float z, float yaw);

    void startLowBattAlarm();
    void stopLowBattAlarm();

    /**  Arm system */
    void armSystem();
    /**  Disable the motors */
    void disarmSystem();

    /**  Set the values for the manual control of the vehicle */
    void setManualControlCommands(double roll, double pitch, double yaw, double thrust, int xHat, int yHat, int buttons);
    /**  Receive a button pressed event from an input device, e.g. joystick */
    void receiveButton(int buttonIndex);

    /**  Set the values for the 6dof manual control of the vehicle */
//    void setManual6DOFControlCommands(double x, double y, double z, double roll, double pitch, double yaw);

    /**  Add a link associated with this robot */
    void addLink(LinkInterface* link);
    /**  Remove a link associated with this robot */
    void removeLink(QObject* object);

    /**  Receive a message from one of the communication links. */
    virtual void receiveMessage(LinkInterface* link, mavlink_message_t message);

    /**  Send a message over this link (to this or to all UAS on this link) */
    void sendMessage(LinkInterface* link, mavlink_message_t message);
    /**  Send a message over all links this UAS can be reached with (!= all links) */
    void sendMessage(mavlink_message_t message);

    /**  Temporary Hack for sending packets to patch Antenna. Send a message over all serial links except for this UAS's */
    void forwardMessage(mavlink_message_t message);

    /**  Set this UAS as the system currently in focus, e.g. in the main display widgets */
    void setSelected();

    /**  Set current mode of operation, e.g. auto or manual */
    void setMode(int mode);

    /**  Request all parameters */
    void requestParameters();

    /**  Request a single parameter by name */
    void requestParameter(int component, const QString& parameter);
    /**  Request a single parameter by index */
    void requestParameter(int component, int id);

    /**  Set a system parameter */
    void setParameter(const int component, const QString& id, const QVariant& value);

    /**  Write parameters to permanent storage */
    void writeParametersToStorage();
    void writeParametersToStorageAQ();
    void writeParametersToSDAQ();
    void writeWaypointsToSDAQ();
    void startStopTelemetry(bool enable, float frequenz, uint8_t dataset = 0);
    void sendCommmandToAq(int command,int confirm, float para1=0,float para2=0,float para3=0,float para4=0,float para5=0,float para6=0,float para7=0);
    void sendCommmandToIMU(int command,int confirm, float para1=0,float para2=0,float para3=0,float para4=0,float para5=0,float para6=0,float para7=0);

    /**  Read parameters from permanent storage */
    void readParametersFromStorage();
    void readParametersFromStorageAQ();
    void readParametersFromSDAQ();
    void readWaypointsFromSDAQ();
    void loadDefaultParametersAQ();


    /**  Get the names of all parameters */
    QList<QString> getParameterNames(int component);

    /**  Get the ids of all components */
    QList<int> getComponentIds();

    void enableDataStream(const int streamId, const int rate);
    void enableAllDataTransmission(int rate);
    void enableRawSensorDataTransmission(int rate);
    void enableExtendedSystemStatusTransmission(int rate);
    void enableRCChannelDataTransmission(int rate);
    void enableRawControllerDataTransmission(int rate);
    //void enableRawSensorFusionTransmission(int rate);
    void enablePositionTransmission(int rate);
    void enableExtra1Transmission(int rate);
    void enableExtra2Transmission(int rate);
    void enableExtra3Transmission(int rate);

    /**  Update the system state */
    void updateState();

    /**  Set world frame origin at current GPS position */
    void setLocalOriginAtCurrentGPSPosition();
    /**  Set world frame origin / home position at this GPS position */
    void setHomePosition(double lat, double lon, double alt);
    /**  Set local position setpoint */
    void setLocalPositionSetpoint(float x, float y, float z, float yaw);
    /**  Add an offset in body frame to the setpoint */
    void setLocalPositionOffset(float x, float y, float z, float yaw);

    void startRadioControlCalibration();
    void startMagnetometerCalibration();
    void startGyroscopeCalibration();
    void startPressureCalibration();

    void startDataRecording();
    void stopDataRecording();
    void deleteSettings();

signals:
    /**  The main/battery voltage has changed/was updated */
    //void voltageChanged(int uasId, double voltage); // Defined in UASInterface already
    /**  An actuator value has changed */
    //void actuatorChanged(UASInterface*, int actId, double value); // Defined in UASInterface already
    /**  An actuator value has changed */
    void actuatorChanged(UASInterface* uas, QString actuatorName, double min, double max, double value);
    void motorChanged(UASInterface* uas, QString motorName, double min, double max, double value);
    /**  The system load (MCU/CPU usage) changed */
    void loadChanged(UASInterface* uas, double load);
    /**  Propagate a heartbeat received from the system */
    //void heartbeat(UASInterface* uas); // Defined in UASInterface already
    void imageStarted(quint64 timestamp);
    /**  A new camera image has arrived */
    void imageReady(UASInterface* uas);
    /**  HIL controls have changed */
    void hilControlsChanged(uint64_t time, float rollAilerons, float pitchElevator, float yawRudder, float throttle, uint8_t systemMode, uint8_t navMode);
    /**  HIL actuator outputs have changed */
    void hilActuatorsChanged(uint64_t time, float act1, float act2, float act3, float act4, float act5, float act6, float act7, float act8);

protected:
    /**  Get the UNIX timestamp in milliseconds, enter microseconds */
    quint64 getUnixTime(quint64 time=0);
    /**  Get the UNIX timestamp in milliseconds, enter milliseconds */
    quint64 getUnixTimeFromMs(quint64 time);
    /**  Get the UNIX timestamp in milliseconds, ignore attitudeStamped mode */
    quint64 getUnixReferenceTime(quint64 time);

    void parseTextMessage(QString *msg, int severity);


    int componentID[256];
    bool componentMulti[256];
    bool connectionLost; ///< Flag indicates a timed out connection
    quint64 connectionLossTime; ///< Time the connection was interrupted
    quint64 lastVoltageWarning; ///< Time at which the last voltage warning occured
    quint64 lastNonNullTime;    ///< The last timestamp from the MAV that was not null
    unsigned int onboardTimeOffsetInvalidCount;     ///< Count when the offboard time offset estimation seemed wrong
    bool hilEnabled;            ///< Set to true if HIL mode is enabled from GCS (UAS might be in HIL even if this flag is not set, this defines the GCS HIL setting)

protected slots:
    /**  Write settings to disk */
    void writeSettings();
    /**  Read settings from disk */
    void readSettings();

//    // MESSAGE RECEPTION
//    /**  Receive a named value message */
//    void receiveMessageNamedValue(const mavlink_message_t& message);

private:
//    unsigned int mode;          ///< The current mode of the MAV
};


#endif // _UAS_H_
