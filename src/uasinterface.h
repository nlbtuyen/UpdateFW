#ifndef _UASINTERFACE_H_
#define _UASINTERFACE_H_

#include <QObject>
#include <QList>
#include <QAction>
#include <QColor>
#include <QPointer>
#include "linkinterface.h"
#include "protocolinterface.h"
#include "vskyline/mavlink_msg_aq_telemetry_f.h"
#include "uasparammanager.h"
#include "vskyline/vskyline.h"


/**
 *  Interface for all robots.
 *
 * This interface is abstract and thus cannot be instantiated. It serves only as type definition.
 * It represents an unmanned aerial vehicle, e.g. a micro air vehicle.
 **/
class UASInterface : public QObject
{
    Q_OBJECT
public:
    virtual ~UASInterface() {}

    /* MANAGEMENT */

    /**  The name of the robot **/
    virtual QString getUASName() const = 0;
    /**  Get short state */
    virtual const QString& getShortState() const = 0;
    /**  Get short mode */
    virtual const QString& getShortMode() const = 0;
    /**  Translate mode id into text */
    static QString getShortModeTextFor(int id);
    virtual int getUASID() const = 0; ///< Get the ID of the connected UAS
    /**  The time interval the robot is switched on **/
    virtual quint64 getUptime() const = 0;
    /**  Get the status flag for the communication **/
    virtual int getCommunicationStatus() const = 0;

    virtual double getLocalX() const = 0;
    virtual double getLocalY() const = 0;
    virtual double getLocalZ() const = 0;
    virtual bool localPositionKnown() const = 0;

    virtual double getLatitude() const = 0;
    virtual double getLongitude() const = 0;
    virtual double getAltitude() const = 0;
    virtual bool globalPositionKnown() const = 0;

    virtual double getRoll() const = 0;
    virtual double getPitch() const = 0;
    virtual double getYaw() const = 0;

    virtual bool getSelected() const = 0;

    virtual bool isArmed() const = 0;

    /**  Set the airframe of this MAV */
    virtual int getAirframe() const = 0;


    // TODO Will be removed
    /**  Set reference to the param manager **/
    virtual void setParamManager(UASParamManager* manager) = 0;

    /* COMMUNICATION FLAGS */

    enum CommStatus {
        /** Unit is disconnected, no failure state reached so far **/
        COMM_DISCONNECTED = 0,
        /** The communication is established **/
        COMM_CONNECTING = 1,
        /** The communication link is up **/
        COMM_CONNECTED = 2,
        /** The connection is closed **/
        COMM_DISCONNECTING = 3,
        COMM_FAIL = 4,
        COMM_TIMEDOUT = 5///< Communication link failed
    };

    enum Airframe {
        QGC_AIRFRAME_GENERIC = 0,
        QGC_AIRFRAME_AUTOQUAD,
        QGC_AIRFRAME_EASYSTAR,
        QGC_AIRFRAME_TWINSTAR,
        QGC_AIRFRAME_MERLIN,
        QGC_AIRFRAME_CHEETAH,
        QGC_AIRFRAME_MIKROKOPTER,
        QGC_AIRFRAME_REAPER,
        QGC_AIRFRAME_PREDATOR,
        QGC_AIRFRAME_COAXIAL,
        QGC_AIRFRAME_PTERYX,
        QGC_AIRFRAME_TRICOPTER,
        QGC_AIRFRAME_HEXCOPTER,
        QGC_AIRFRAME_X8,
        QGC_AIRFRAME_VIPER_2_0,
        QGC_AIRFRAME_CAMFLYER_Q,
        QGC_AIRFRAME_END_OF_ENUM
    };

    /**
         *  Get the links associated with this robot
         *
         * @return List with all links this robot is associated with. Association is usually
         *         based on the fact that a message for this robot has been received through that
         *         interface. The LinkInterface can support multiple protocols.
         **/
    virtual QList<LinkInterface*>* getLinks() = 0;

    /**
     *  Get the color for this UAS
     *
     * This static function holds a color map that allows to draw a new color for each robot
     *
     * @return The next color in the color map. The map holds 20 colors and starts from the beginning
     *         if the colors are exceeded.
     */
    static QColor getNextColor() {
        /* Create color map */
        static QList<QColor> colors = QList<QColor>()
            << QColor(215,75,6)  // burnt orange
            << QColor(66,35,174) // dk blue/purple
            << QColor(43,144,38) // dk green
            << QColor(125,7,107) // dk eggplant
            << QColor(232,33,47)
            << QColor(116,251,110)
            << QColor(234,38,107)
            << QColor(104,250,138)
            << QColor(235,43,165)
            << QColor(98,248,176)
            << QColor(236,48,221)
            << QColor(92,247,217)
            << QColor(200,54,238)
            << QColor(87,231,246)
            << QColor(151,59,239)
            << QColor(81,183,244)
            << QColor(75,133,243)
            << QColor(242,255,128)
            << QColor(230,126,23)
            << QColor(231,72,28)
            << QColor(104,64,240)
            << QColor(203,254,121)
            << QColor(161,252,116);

        static int nextColor = -1;
        if(nextColor == 18){//if at the end of the list
            nextColor = -1;//go back to the beginning
        }
        nextColor++;
        return colors[nextColor];//return the next color
   }

    /**  Get the type of the system (airplane, quadrotor, helicopter,..)*/
    virtual int getSystemType() = 0;
    virtual QString getSystemTypeName() = 0;
    /**  Get the type of the autopilot (PIXHAWK, APM, UDB, PPZ,..) */
    virtual int getAutopilotType() = 0;
    virtual QString getAutopilotTypeName() = 0;
    virtual void setAutopilotType(int apType) = 0;

    virtual QMap<int, QString> getComponents() = 0;

    QColor getColor()
    {
        return color;
    }

public slots:

    /**  Set a new name for the system */
    virtual void setUASName(const QString& name) = 0;
    /**  Execute command immediately **/
    virtual void executeCommand(MAV_CMD command) = 0;
    /**  Executes a command **/
    virtual void executeCommand(MAV_CMD command, int confirmation, float param1, float param2, float param3, float param4, float param5, float param6, float param7, int component) = 0;

    /**  Selects the airframe */
    virtual void setAirframe(int airframe) = 0;

    /**  Launches the system/Liftof **/
    virtual void launch() = 0;
    /**  Set a new waypoint **/
    //virtual void setWaypoint(Waypoint* wp) = 0;
    /**  Set this waypoint as next waypoint to fly to */
    //virtual void setWaypointActive(int wp) = 0;
    /**  Order the robot to return home / to land on the runway **/
    virtual void home() = 0;
    /**  Order the robot to land **/
    virtual void land() = 0;
    /**  Halt the system */
    virtual void halt() = 0;
    /**  Start/continue the current robot action */
    virtual void go() = 0;
    /**  Set the current mode of operation */
    virtual void setMode(int mode) = 0;
    /** Stops the robot system. If it is an MAV, the robot starts the emergency landing procedure **/
    virtual void emergencySTOP() = 0;
    /** Kills the robot. All systems are immediately shut down (e.g. the main power line is cut). This might lead to a crash **/
    virtual bool emergencyKILL() = 0;
    /**
     *  Shut down the system's computers
     *
     * Works only if already landed and will cleanly shut down all onboard computers.
     */
    virtual void shutdown() = 0;
    /**  Set the target position for the robot to navigate to.
     *  @param x x-coordinate of the target position
     *  @param y y-coordinate of the target position
     *  @param z z-coordinate of the target position
     *  @param yaw heading of the target position
     */
    virtual void setTargetPosition(float x, float y, float z, float yaw) = 0;
    /**  Request the list of stored waypoints from the robot */
    //virtual void requestWaypoints() = 0;
    /**  Clear all existing waypoints on the robot */
    //virtual void clearWaypointList() = 0;
    /**  Set world frame origin at current GPS position */
    virtual void setLocalOriginAtCurrentGPSPosition() = 0;
    /**  Set world frame origin / home position at this GPS position */
    virtual void setHomePosition(double lat, double lon, double alt) = 0;
    /**  Request all onboard parameters of all components */
    virtual void requestParameters() = 0;
    /**  Request one specific onboard parameter */
    virtual void requestParameter(int component, const QString& parameter) = 0;
    /**  Write parameter to permanent storage */
    virtual void writeParametersToStorage() = 0;
    virtual void writeParametersToStorageAQ() = 0;
    virtual void writeParametersToSDAQ() = 0;
    virtual void writeWaypointsToSDAQ() = 0;
    virtual void startStopTelemetry(bool enable, float frequenz, uint8_t dataset = 0) =0;
    /**  Read parameter from permanent storage */
    virtual void readParametersFromStorage() = 0;
    virtual void readParametersFromStorageAQ() = 0;
    virtual void readParametersFromSDAQ() = 0;
    virtual void readWaypointsFromSDAQ() = 0;
    virtual void loadDefaultParametersAQ() = 0;
    virtual void sendCommmandToAq(int command,int confirm, float para1=0,float para2=0,float para3=0,float para4=0,float para5=0,float para6=0,float para7=0) = 0;
    virtual void sendCommmandToIMU(int command,int confirm, float para1=0,float para2=0,float para3=0,float para4=0,float para5=0,float para6=0,float para7=0) = 0;

    /**  Set a system parameter
     * @param component ID of the system component to write the parameter to
     * @param id String identifying the parameter
     * @warning The length of the ID string is limited by the MAVLink format! Take care to not exceed it
     * @param value Value of the parameter, IEEE 754 single precision floating point
     */
    virtual void setParameter(const int component, const QString& id, const QVariant& value) = 0;

    /**
     *  Add a link to the list of current links
     *
     * Adding the link to the robot's internal link list will allow him so send its own messages
     * over all registered links. Usually a link is added once a message for this particular robot
     * has been received over a link, but adding could also be done manually.
     * @warning Not all links should be added to all robots by default, because else all robots will
     *          attempt to send over all links, typically choking wireless serial links.
     */
    virtual void addLink(LinkInterface* link) = 0;

    /**
     *  Set the current robot as focused in the user interface
     */
    virtual void setSelected() = 0;

    virtual void enableDataStream(const int streamId, const int rate) = 0;
    virtual void enableAllDataTransmission(int rate) = 0;
    virtual void enableRawSensorDataTransmission(int rate) = 0;
    virtual void enableExtendedSystemStatusTransmission(int rate) = 0;
    virtual void enableRCChannelDataTransmission(int rate) = 0;
    virtual void enableRawControllerDataTransmission(int rate) = 0;
    //virtual void enableRawSensorFusionTransmission(int rate) = 0;
    virtual void enablePositionTransmission(int rate) = 0;
    virtual void enableExtra1Transmission(int rate) = 0;
    virtual void enableExtra2Transmission(int rate) = 0;
    virtual void enableExtra3Transmission(int rate) = 0;

    virtual void setLocalPositionSetpoint(float x, float y, float z, float yaw) = 0;
    virtual void setLocalPositionOffset(float x, float y, float z, float yaw) = 0;

    virtual void startRadioControlCalibration() = 0;
    virtual void startMagnetometerCalibration() = 0;
    virtual void startGyroscopeCalibration() = 0;
    virtual void startPressureCalibration() = 0;

    /**  Set the current battery type and voltages */
    virtual void setBatterySpecs(const QString& specs) = 0;
    /**  Get the current battery type and specs */
    virtual QString getBatterySpecs() = 0;
    virtual float getBatteryEmptyVoltage() = 0;
    virtual float getBatteryWarnVoltage() = 0;

protected:
    QColor color;

signals:
    /**  The robot state has changed */
    void statusChanged(int stateFlag);
    /**  A new component was detected or created */
    void componentCreated(int uas, int component, const QString& name);
    /**  The robot state has changed
     *
     * @param uas this robot
     * @param status short description of status, e.g. "connected"
     * @param description longer textual description. Should be however limited to a short text, e.g. 200 chars.
     */
    void statusChanged(UASInterface* uas, QString status, QString description);
    /**  System has been removed / disconnected / shutdown cleanly, remove */
    void systemRemoved(UASInterface* uas);
    void systemRemoved();
    /**
     *  Received a plain text message from the robot
     * This signal should NOT be used for standard communication, but rather for VERY IMPORTANT
     * messages like critical errors.
     *
     * @param uasid ID of the sending system
     * @param compid ID of the sending component
     * @param text the status text
     * @param severity The severity of the message, 0 for plain debug messages, 10 for very critical messages
     */

    void poiFound(UASInterface* uas, int type, int colorIndex, QString message, float x, float y, float z);
    void poiConnectionFound(UASInterface* uas, int type, int colorIndex, QString message, float x1, float y1, float z1, float x2, float y2, float z2);

    /**  A text message from the system has been received */
    void textMessageReceived(int uasid, int componentid, int severity, QString text);

    /**  A command to the system has been acknowledged */
    void commandAcked(int uasid, int componentid, uint16_t command, uint8_t result);

    void navModeChanged(int uasid, int mode, const QString& text);

    /**  System is now armed */
    void armed();
    /**  System is now disarmed */
    void disarmed();
    /**  Arming mode changed */
    void armingChanged(bool armed);

    /**
     *  Update the error count of a device
     *
     * The error count indicates how many errors occured during the use of a device.
     * Usually a random error from time to time is acceptable, e.g. through electromagnetic
     * interferences on device lines like I2C and SPI. A constantly and rapidly increasing
     * error count however can help to identify broken cables or misbehaving drivers.
     *
     * @param uasid System ID
     * @param component Name of the component, e.g. "IMU"
     * @param device Name of the device, e.g. "SPI0" or "I2C1"
     * @param count Errors occured since system startup
     */
    void errCountChanged(int uasid, QString component, QString device, int count);

    /**
     *  Drop rate of communication link updated
     *
     * @param systemId id of the air system
     * @param receiveDrop drop rate of packets this MAV receives (sent from GCS or other MAVs)
     */
    void dropRateChanged(int systemId,  float receiveDrop);
    /**  Robot mode has changed */
    void modeChanged(int sysId, QString status, QString description);
    /**  Robot armed state has changed */
    void armingChanged(int sysId, QString armingState);
    /**  A command has been issued **/
    void commandSent(int command);
    /**  The connection status has changed **/
    void connectionChanged(CommStatus connectionFlag);
    /**  The robot is connecting **/
    void connecting();
    /**  The robot is connected **/
    void connected();
    /**  The robot is disconnected **/
    void disconnected();
    /**  The robot is active **/
    void activated();
    /**  The robot is inactive **/
    void deactivated();
    /**  The robot is manually controlled **/
    void manualControl();

    /**  DATA_STREAM message received from UAS **/
    void dataStreamAnnounced(const int uasId, const uint8_t stream_id, const uint16_t rate, const bool on_off);

    /**  A value of the robot has changed.
      *
      * Typically this is used to send lowlevel information like the battery voltage to the plotting facilities of
      * the groundstation. The data here should be converted to human-readable values before being passed, so ideally
      * SI units.
      *
      * @param uasId ID of this system
      * @param name name of the value, e.g. "battery voltage"
      * @param unit The units this variable is in as an abbreviation. For system-dependent (such as raw ADC values) use "raw", for bitfields use "bits", for true/false or on/off use "bool", for unitless values use "-".
      * @param value the value that changed
      * @param msec the timestamp of the message, in milliseconds
      */
    void valueChanged(const int uasId, const QString& name, const QString& unit, const QVariant& value, const quint64 msec);

    void voltageChanged(int uasId, double voltage);
    void waypointUpdated(int uasId, int id, double x, double y, double z, double yaw, bool autocontinue, bool active);
    void waypointSelected(int uasId, int id);
    void waypointReached(UASInterface* uas, int id);
    void autoModeChanged(bool autoMode);
    void parameterChanged(int uas, int component, QString parameterName, QVariant value);
    void parameterChanged(int uas, int component, int parameterCount, int parameterId, QString parameterName, QVariant value);
    void patternDetected(int uasId, QString patternPath, float confidence, bool detected);
    void letterDetected(int uasId, QString letter, float confidence, bool detected);
    /**
     *  The battery status has been updated
     *
     * @param uas sending system
     * @param voltage battery voltage
     * @param percent remaining capacity in percent
     * @param seconds estimated remaining flight time in seconds
     */
    void batteryChanged(UASInterface* uas, double voltage, double percent, int seconds);
    void statusChanged(UASInterface* uas, QString status);
    void actuatorChanged(UASInterface*, int actId, double value);
    void thrustChanged(UASInterface*, double thrust);
    void heartbeat(UASInterface* uas);
    void attitudeChanged(UASInterface*, double roll, double pitch, double yaw, quint64 usec);
    void attitudeChanged(UASInterface*, int component, double roll, double pitch, double yaw, quint64 usec);
    void attitudeSpeedChanged(int uas, double rollspeed, double pitchspeed, double yawspeed, quint64 usec);
    void attitudeThrustSetPointChanged(UASInterface*, double rollDesired, double pitchDesired, double yawDesired, double thrustDesired, quint64 usec);
    /**  The MAV set a new setpoint in the local (not body) NED X, Y, Z frame */
    void positionSetPointsChanged(int uasid, float xDesired, float yDesired, float zDesired, float yawDesired, quint64 usec);
    /**  A user (or an autonomous mission or obstacle avoidance planner) requested to set a new setpoint */
    void userPositionSetPointsChanged(int uasid, float xDesired, float yDesired, float zDesired, float yawDesired);
    void localPositionChanged(UASInterface*, double x, double y, double z, quint64 usec);
    void localPositionChanged(UASInterface*, int component, double x, double y, double z, quint64 usec);
    void globalPositionChanged(UASInterface*, double lat, double lon, double alt, quint64 usec);
    void altitudeChanged(int uasid, double altitude);
    /**  Update the status of one satellite used for localization */
    void gpsSatelliteStatusChanged(int uasid, int satid, float azimuth, float direction, float snr, bool used);
    void localSpeedChanged(UASInterface*, double x, double y, double z, quint64 usec);
    void gpsSpeedChanged(UASInterface*, double x, double y, double z, quint64 usec);
    void hudSpeedChanged(UASInterface*, double x, double y, double z, quint64 usec);
    void gpsSpeedChanged(UASInterface*, double grndspd, quint64 usec);
    void imageStarted(int imgid, int width, int height, int depth, int channels);
    void imageDataReceived(int imgid, const unsigned char* imageData, int length, int startIndex);
    /**  Emit the new system type */
    void systemTypeSet(UASInterface* uas, unsigned int type);

    /**  Attitude control enabled/disabled */
    void attitudeControlEnabled(bool enabled);
    /**  Position 2D control enabled/disabled */
    void positionXYControlEnabled(bool enabled);
    /**  Altitude control enabled/disabled */
    void positionZControlEnabled(bool enabled);
    /**  Heading control enabled/disabled */
    void positionYawControlEnabled(bool enabled);
    /**  Optical flow status changed */
    void opticalFlowStatusChanged(bool supported, bool enabled, bool ok);
    /**  Vision based localization status changed */
    void visionLocalizationStatusChanged(bool supported, bool enabled, bool ok);
    /**  Infrared / Ultrasound status changed */
    void distanceSensorStatusChanged(bool supported, bool enabled, bool ok);
    /**  Gyroscope status changed */
    void gyroStatusChanged(bool supported, bool enabled, bool ok);
    /**  Accelerometer status changed */
    void accelStatusChanged(bool supported, bool enabled, bool ok);
    /**  Magnetometer status changed */
    void magSensorStatusChanged(bool supported, bool enabled, bool ok);
    /**  Barometer status changed */
    void baroStatusChanged(bool supported, bool enabled, bool ok);
    /**  Differential pressure / airspeed status changed */
    void airspeedStatusChanged(bool supported, bool enabled, bool ok);
    /**  Actuator status changed */
    void actuatorStatusChanged(bool supported, bool enabled, bool ok);
    /**  Laser scanner status changed */
    void laserStatusChanged(bool supported, bool enabled, bool ok);
    /**  Vicon / Leica Geotracker status changed */
    void groundTruthSensorStatusChanged(bool supported, bool enabled, bool ok);


    /**  Value of a remote control channel (raw) */
    void remoteControlChannelRawChanged(int channelId, float raw);
    /**  Value of a remote control channel (scaled)*/
    void remoteControlChannelScaledChanged(int channelId, float normalized);
    /**  Remote control RSSI changed */
    void remoteControlRSSIChanged(float rssi);

    /**
     *  Localization quality changed
     * @param fix 0: lost, 1: 2D local position hold, 2: 2D localization, 3: 3D localization
     */
    void localizationChanged(UASInterface* uas, int fix);
    /**
     *  GPS localization quality changed
     * @param fix 0: lost, 1: at least one satellite, but no GPS fix, 2: 2D localization, 3: 3D localization
     */
    void gpsLocalizationChanged(UASInterface* uas, int fix);
    /**
     *  Vision localization quality changed
     * @param fix 0: lost, 1: 2D local position hold, 2: 2D localization, 3: 3D localization
     */
    void visionLocalizationChanged(UASInterface* uas, int fix);
    /**
     *  IR/U localization quality changed
     * @param fix 0: No IR/Ultrasound sensor, N > 0: Found N active sensors
     */
    void irUltraSoundLocalizationChanged(UASInterface* uas, int fix);

    /**  AutoQuad custom telemetry messages */
    void TelemetryChangedF(const int uasId, mavlink_aq_telemetry_f_t values, mavlink_attitude_t value);

    /**  Telemetry message from ESC */
    void escTelemetryUpdate(uint8_t escId, uint8_t version, uint16_t age, uint8_t state, float volts, float amps, uint16_t rpm, float duty, float temp, uint16_t errCount, uint8_t errCode);

    // ERROR AND STATUS SIGNALS
    /**  Heartbeat timed out or was regained */
    void heartbeatTimeout(bool timeout, unsigned int ms);
    /**  Name of system changed */
    void nameChanged(QString newName);
    /**  System has been selected as focused system */
    void systemSelected(bool selected);
    /**  Core specifications have changed */
    void systemSpecsChanged(int uasId);

    void systemVersionChanged(int uasId, uint32_t fwVer, uint32_t hwVer, QString fwVerStr, QString hwVerStr);

    /**  Object detected */
    void objectDetected(unsigned int time, int id, int type, const QString& name, int quality, float bearing, float distance);


    // HOME POSITION / ORIGIN CHANGES
    void homePositionChanged(int uas, double lat, double lon, double alt);

protected:

    // TIMEOUT CONSTANTS
    static const unsigned int timeoutIntervalHeartbeat = 2500 * 1000; ///< @Leo Heartbeat timeout is 2.5 seconds

};

#endif // _UASINTERFACE_H_
