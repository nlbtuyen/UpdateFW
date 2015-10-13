#include "vskyline/vskyline.h"
#include <QList>
#include <QMessageBox>
#include <QTimer>
#include <QSettings>
#include <iostream>
#include <QDebug>
#include <cmath>
#include <qmath.h>
#include "uas.h"
#include "linkinterface.h"
#include "uasmanager.h"
#include "qgc.h"
#include "mavlinkprotocol.h"
#include "linkmanager.h"
#include "seriallink.h"
#include "autoquadmav.h"
#include "vskyline/mavlink.h"

#ifndef isinf
#define isinf(x) ((x)!=(x))
#endif


#ifndef isnan
#define isnan(x) ((x)!=(x))
#endif

/**
* Gets the settings from the previous UAS (name, airframe, autopilot, battery specs)
* by calling readSettings. This means the new UAS will have the same settings
* as the previous one created unless one calls deleteSettings in the code after
* creating the UAS.
*/
UAS::UAS(MAVLinkProtocol* protocol, int id) : UASInterface(),
    lipoFull(4.2f),
    lipoEmpty(3.5f),
    uasId(id),
    startTime(QGC::groundTimeMilliseconds()),
    commStatus(COMM_DISCONNECTED),
    name(""),
    autopilot(-1),
    airframe(QGC_AIRFRAME_AUTOQUAD),
    hardwareVersion(0),
    firmwareVersion(0),
    links(new QList<LinkInterface*>()),
    unknownPackets(),
    mavlink(protocol),
    thrustSum(0),
    thrustMax(10),
    startVoltage(-1.0f),
    tickVoltage(10.5f),
    lastTickVoltageValue(-1.0f),
    tickLowpassVoltage(12.0f),
    warnVoltage(9.5f),
    warnLevelPercent(20.0f),
    currentVoltage(12.6f),
    lpVoltage(12.0f),
    currentCurrent(0.0f),
    batteryRemainingEstimateEnabled(true),
    mode(-1),
    status(-1),
    navMode(-1),
    onboardTimeOffset(0),
    controlRollManual(true),
    controlPitchManual(true),
    controlYawManual(true),
    controlThrustManual(true),
    manualRollAngle(0),
    manualPitchAngle(0),
    manualYawAngle(0),
    manualThrust(0),
    receiveDropRate(0),
    sendDropRate(0),
    lowBattAlarm(false),
    positionLock(false),
    localX(0.0),
    localY(0.0),
    localZ(0.0),
    latitude(0.0),
    longitude(0.0),
    altitude(0.0),
    roll(0.0),
    pitch(0.0),
    yaw(0.0),
    statusTimeout(new QTimer(this)),
    paramsOnceRequested(false),
    attitudeKnown(false),
    paramManager(NULL),
    attitudeStamped(false),
    lastAttitude(0),
    isLocalPositionKnown(false),
    isGlobalPositionKnown(false),
    systemIsArmed(false),
    nedPosGlobalOffset(0,0,0),
    nedAttGlobalOffset(0,0,0),
    connectionLost(false),
    lastVoltageWarning(0),
    lastNonNullTime(0),
    onboardTimeOffsetInvalidCount(0),
    hilEnabled(false)

{
    for (unsigned int i = 0; i<255;++i)
    {
        componentID[i] = -1;
        componentMulti[i] = false;
    }

    color = UASInterface::getNextColor();
    setBatterySpecs(QString("9V,9.5V,12.6V"));
    connect(statusTimeout, SIGNAL(timeout()), this, SLOT(updateState()));
    connect(this, SIGNAL(systemSpecsChanged(int)), this, SLOT(writeSettings()));
    statusTimeout->start(500);
    readSettings();
    type = MAV_TYPE_GENERIC;
    // Initial signals
    emit disarmed();
    emit armingChanged(false);
}

/**
* Saves the settings of name, airframe, autopilot type and battery specifications
* by calling writeSettings.
*/
UAS::~UAS()
{
    writeSettings();
    delete links;
    delete statusTimeout;
}

/**
* Saves the settings of name, airframe, autopilot type and battery specifications
* for the next instantiation of UAS.
*/
void UAS::writeSettings()
{
    QSettings settings;
    settings.beginGroup(QString("MAV%1").arg(uasId));
    settings.setValue("NAME", this->name);
    settings.setValue("AIRFRAME", this->airframe);
    settings.setValue("AP_TYPE", this->autopilot);
    settings.setValue("BATTERY_SPECS", getBatterySpecs());
    settings.endGroup();
    settings.sync();
}

/**
* Reads in the settings: name, airframe, autopilot type, and battery specifications
* for the new UAS.
*/
void UAS::readSettings()
{
    QSettings settings;
    settings.beginGroup(QString("MAV%1").arg(uasId));
    this->name = settings.value("NAME", this->name).toString();
    this->airframe = settings.value("AIRFRAME", this->airframe).toInt();
    this->autopilot = settings.value("AP_TYPE", this->autopilot).toInt();
    if (settings.contains("BATTERY_SPECS"))
    {
        setBatterySpecs(settings.value("BATTERY_SPECS").toString());
    }
    settings.endGroup();
}

/**
*  Deletes the settings origianally read into the UAS by readSettings.
*  This is in case one does not want the old values but would rather
*  start with the values assigned by the constructor.
*/
void UAS::deleteSettings()
{
    this->name = "";
    this->airframe = QGC_AIRFRAME_GENERIC;
    this->autopilot = -1;
    setBatterySpecs(QString("9V,9.5V,12.6V"));
}

/**
* @ return the id of the uas
*/
int UAS::getUASID() const
{
    return uasId;
}

/**
* Update the heartbeat.
*/
void UAS::updateState()
{
    QString audioSystemName = "";
    if (UASManager::instance()->getSystemsCount() > 1)
        audioSystemName = tr("to System %1").arg(uasId);

    // Check if heartbeat timed out
    quint64 heartbeatInterval = QGC::groundTimeUsecs() - lastHeartbeat;
    if (!connectionLost && (heartbeatInterval > timeoutIntervalHeartbeat))
    {
        connectionLost = true;
        commStatus = COMM_TIMEDOUT;
    }

    // Update connection loss time on each iteration
    if (connectionLost && (heartbeatInterval > timeoutIntervalHeartbeat))
    {
        connectionLossTime = heartbeatInterval;
        emit heartbeatTimeout(true, heartbeatInterval/1000);
    } else
        commStatus = COMM_CONNECTED;

    // Connection gained
    if (connectionLost && (heartbeatInterval < timeoutIntervalHeartbeat))
    {
        connectionLost = false;
        connectionLossTime = 0;
        emit heartbeatTimeout(false, 0);
    }
}

/**
* If the acitve UAS (the UAS that was selected) is not the one that is currently
* active, then change the active UAS to the one that was selected.
*/
void UAS::setSelected()
{
    if (UASManager::instance()->getActiveUAS() != this)
    {
        UASManager::instance()->setActiveUAS(this);
        emit systemSelected(true);
    }
}

/**
* @return if the active UAS is the current UAS
**/
bool UAS::getSelected() const
{
    return (UASManager::instance()->getActiveUAS() == this);
}

void UAS::receiveMessage(LinkInterface* link, mavlink_message_t message) //@Leo receive mess and change on UI
{
    if (!link) return;
    if (!links->contains(link))
    {
        addLink(link);
    }

    if (!components.contains(message.compid))
    {
        QString componentName;

        switch (message.compid)
        {
        case MAV_COMP_ID_ALL:
        {
            componentName = tr("ANONYMOUS");
            break;
        }
        case MAV_COMP_ID_IMU:
        {
            componentName = tr("IMU #1");
            break;
        }
        case MAV_COMP_ID_CAMERA:
        {
            componentName = tr("CAMERA");
            break;
        }
        case MAV_COMP_ID_MISSIONPLANNER:
        {
            componentName = tr("MISSIONPLANNER");
            break;
        }
        }

        components.insert(message.compid, componentName);
        emit componentCreated(uasId, message.compid, componentName);
    }

    // Only accept messages from this system (condition 1)
    // and only then if a) attitudeStamped is disabled OR b) attitudeStamped is enabled
    // and we already got one attitude packet
    if (message.sysid == uasId && (!attitudeStamped || (attitudeStamped && (lastAttitude != 0)) || message.msgid == MAVLINK_MSG_ID_ATTITUDE))
    {
        QString uasState;
        QString stateDescription;
        QString audioSystemName = "";

        bool multiComponentSourceDetected = false;
        bool wrongComponent = false;

        if (UASManager::instance()->getSystemsCount() > 1)
            audioSystemName = tr("System %1").arg(uasId);

        switch (message.compid)
        {
        case MAV_COMP_ID_IMU_2:
            // Prefer IMU 2 over IMU 1 (FIXME)
            componentID[message.msgid] = MAV_COMP_ID_IMU_2;
            break;
        default:
            // Do nothing
            break;
        }

        // Store component ID
        if (componentID[message.msgid] == -1)
        {
            // Prefer the first component
            componentID[message.msgid] = message.compid;
        }
        else
        {
            // Got this message already
            if (componentID[message.msgid] != message.compid)
            {
                componentMulti[message.msgid] = true;
                wrongComponent = true;
            }
        }

        if (componentMulti[message.msgid] == true) multiComponentSourceDetected = true;
        switch (message.msgid)
        {

        case MAVLINK_MSG_ID_AQ_TELEMETRY_F:
        {
            break;
        }

        case MAVLINK_MSG_ID_HEARTBEAT:
        {
            if (multiComponentSourceDetected && wrongComponent)
            {
                break;
            }
            lastHeartbeat = QGC::groundTimeUsecs();
            emit heartbeat(this);
            mavlink_heartbeat_t state;
            mavlink_msg_heartbeat_decode(&message, &state);

            // Send the base_mode and system_status values to the plotter. This uses the ground time
            // so the Ground Time checkbox must be ticked for these values to display
            quint64 time = getUnixTime();
            QString name = QString("M%1:HEARTBEAT.%2").arg(message.sysid);
            emit valueChanged(uasId, name.arg("base_mode"), "bits", state.base_mode, time);
            emit valueChanged(uasId, name.arg("custom_mode"), "bits", state.custom_mode, time);
            emit valueChanged(uasId, name.arg("system_status"), "-", state.system_status, time);

            // Set new type if it has changed
            if (this->type != state.type)
            {
                this->type = state.type;
                if (airframe == 0)
                {
                    switch (type)
                    {
                    case MAV_TYPE_FIXED_WING:
                        setAirframe(UASInterface::QGC_AIRFRAME_EASYSTAR);
                        break;
                    case MAV_TYPE_QUADROTOR:
                        setAirframe(UASInterface::QGC_AIRFRAME_CHEETAH);
                        break;
                    case MAV_TYPE_HEXAROTOR:
                        setAirframe(UASInterface::QGC_AIRFRAME_HEXCOPTER);
                        break;
                    default:
                        // Do nothing
                        break;
                    }
                }
                this->autopilot = state.autopilot;
                emit systemTypeSet(this, type);
            }

            QString stateAudio = "";
            QString modeAudio = "";
            QString armModeAudio = "";
            bool statechanged = false;
            bool modechanged = false;
            bool armingchanged = false;

            bool currentlyArmed = state.base_mode & MAV_MODE_FLAG_DECODE_POSITION_SAFETY;

            if (systemIsArmed != currentlyArmed)
            {
                systemIsArmed = currentlyArmed;
                emit armingChanged(systemIsArmed);
                armingchanged = true;
                if (systemIsArmed)
                {
                    emit armed();
                    armModeAudio = tr("armed");
                }
                else
                {
                    emit disarmed();
                    armModeAudio = tr("disarmed");
                }
            }

            if ((state.system_status != this->status) && state.system_status != MAV_STATE_UNINIT)
            {
                statechanged = true;
                this->status = state.system_status;
                getStatusForCode((int)state.system_status, uasState, stateDescription);
                emit statusChanged(this, uasState, stateDescription);
                emit statusChanged(this->status);

                shortStateText = uasState;

                // Adjust for better audio
                if (uasState == QString("STANDBY")) uasState = tr("standing by");
                if (uasState == QString("EMERGENCY")) uasState = tr("emergency condition");
                if (uasState == QString("CRITICAL")) uasState = tr("critical condition");
                if (uasState == QString("SHUTDOWN")) uasState = tr("shutting down");

                stateAudio = uasState;
            }

            if (systemIsArmed && this->mode != static_cast<int>(state.base_mode))
            {
                modechanged = true;
                uint8_t oldMode = this->mode;
                this->mode = static_cast<int>(state.base_mode);
                shortModeText = getShortModeTextFor(this->mode);

                emit modeChanged(this->getUASID(), shortModeText, "");

                if (!(this->mode & MAV_MODE_FLAG_DECODE_POSITION_GUIDED) && !(oldMode & MAV_MODE_FLAG_DECODE_POSITION_GUIDED))
                    modeAudio = tr(" now in ") + getAudioModeTextFor(this->mode);
            }

            if (navMode != state.custom_mode)
            {
                emit navModeChanged(uasId, state.custom_mode, getNavModeText(state.custom_mode));
                navMode = state.custom_mode;
            }
        }

            break;
        case MAVLINK_MSG_ID_SYS_STATUS:
        {
            if (multiComponentSourceDetected && wrongComponent)
            {
                break;
            }
            mavlink_sys_status_t state;
            mavlink_msg_sys_status_decode(&message, &state);

            // Prepare for sending data to the realtime plotter, which is every field excluding onboard_control_sensors_present.
            quint64 time = getUnixTime();
            QString name = QString("M%1:SYS_STATUS.%2").arg(message.sysid);
            emit valueChanged(uasId, name.arg("sensors_enabled"), "bits", state.onboard_control_sensors_enabled, time);
            emit valueChanged(uasId, name.arg("sensors_health"), "bits", state.onboard_control_sensors_health, time);
            emit valueChanged(uasId, name.arg("errors_comm"), "-", state.errors_comm, time);
            emit valueChanged(uasId, name.arg("errors_count1"), "-", state.errors_count1, time);
            emit valueChanged(uasId, name.arg("errors_count2"), "-", state.errors_count2, time);
            emit valueChanged(uasId, name.arg("errors_count3"), "-", state.errors_count3, time);
            emit valueChanged(uasId, name.arg("errors_count4"), "-", state.errors_count4, time);

            // Process CPU load.
            emit loadChanged(this,state.load/10.0f);
            emit valueChanged(uasId, name.arg("load"), "%", state.load/10.0f, time);

            // Battery charge/time remaining/voltage calculations
            bool vwarning = false;
            bool vtick = false;
            currentVoltage = state.voltage_battery/1000.0f;
            lpVoltage = filterVoltage(currentVoltage);

            if (startVoltage == -1.0f && currentVoltage > 0.1f)
                startVoltage = currentVoltage;

            timeRemaining = calculateTimeRemaining();
            if (!batteryRemainingEstimateEnabled && chargeLevel != -1)
                chargeLevel = state.battery_remaining;

            // And if the battery current draw is measured, log that also.
            if (state.current_battery != -1) {
                currentCurrent = ((double)state.current_battery)/100.0f;
                emit valueChanged(uasId, name.arg("battery_current"), "A", ((double)state.current_battery) / 100.0f, time);
            }

            emit batteryChanged(this, lpVoltage, getChargeLevel(), timeRemaining);
            emit valueChanged(uasId, name.arg("battery_remaining"), "%", getChargeLevel(), time);
            emit voltageChanged(message.sysid, currentVoltage);
            emit valueChanged(uasId, name.arg("battery_voltage"), "V", currentVoltage, time);

            // LOW BATTERY ALARM

            // calculate rate of change since last reding (one "tick" is ~ 0.1v change)
            tickLowpassVoltage = tickLowpassVoltage*0.8f + 0.2f*currentVoltage;
            if (lastTickVoltageValue == -1.0f)
                lastTickVoltageValue = tickLowpassVoltage;
            if (fabs(lastTickVoltageValue - tickLowpassVoltage) > 0.1f) {
                vtick = true;
                lastTickVoltageValue = tickLowpassVoltage;
            }

            if (batteryRemainingEstimateEnabled && lpVoltage < warnVoltage && (currentVoltage - 0.2f) < warnVoltage && (currentVoltage > 3.3))
                vwarning = true;
            else if (!batteryRemainingEstimateEnabled && chargeLevel < warnLevelPercent)
                vwarning = true;

            // add  && vtick  to only warn when voltage changes
            if (vwarning && startVoltage > 0.0f && (QGC::groundTimeUsecs() - lastVoltageWarning) > 15e6) {
                lastVoltageWarning = QGC::groundTimeUsecs();
            }

            // control_sensors_enabled:
            // relevant bits: 11: attitude stabilization, 12: yaw position, 13: z/altitude control, 14: x/y position control
            emit attitudeControlEnabled(state.onboard_control_sensors_enabled & (1 << 11));
            emit positionYawControlEnabled(state.onboard_control_sensors_enabled & (1 << 12));
            emit positionZControlEnabled(state.onboard_control_sensors_enabled & (1 << 13));
            emit positionXYControlEnabled(state.onboard_control_sensors_enabled & (1 << 14));

            // Trigger drop rate updates as needed. Here we convert the incoming
            // drop_rate_comm value from 1/100 of a percent in a uint16 to a true
            // percentage as a float. We also cap the incoming value at 100% as defined
            // by the MAVLink specifications.
            if (state.drop_rate_comm > 10000)
            {
                state.drop_rate_comm = 10000;
            }
            emit dropRateChanged(this->getUASID(), state.drop_rate_comm/100.0f);
            emit valueChanged(uasId, name.arg("drop_rate_comm"), "%", state.drop_rate_comm/100.0f, time);
        }
            break;
        case MAVLINK_MSG_ID_ATTITUDE:
        {
            mavlink_attitude_t attitude;

            quint64 time = getUnixReferenceTime(attitude.time_boot_ms);

            emit attitudeChanged(this, message.compid, QGC::limitAngleToPMPIf(attitude.roll), QGC::limitAngleToPMPIf(attitude.pitch), QGC::limitAngleToPMPIf(attitude.yaw), time);

            if (!wrongComponent)
            {
                lastAttitude = time;
                roll = QGC::limitAngleToPMPIf(attitude.roll);
                pitch = QGC::limitAngleToPMPIf(attitude.pitch);
                yaw = QGC::limitAngleToPMPIf(attitude.yaw);
                attitudeKnown = true;
                emit attitudeChanged(this, roll, pitch, yaw, time);
                emit attitudeSpeedChanged(uasId, attitude.rollspeed, attitude.pitchspeed, attitude.yawspeed, time);
            }
            mavlink_aq_telemetry_f_t telemetry_msg; //@trung
            mavlink_msg_aq_telemetry_f_decode(&message, &telemetry_msg);
            emit TelemetryChangedF(uasId, telemetry_msg, attitude);
        }
            break;

        case MAVLINK_MSG_ID_HIL_CONTROLS:
        {
            mavlink_hil_controls_t hil;
            mavlink_msg_hil_controls_decode(&message, &hil);
            emit hilControlsChanged(hil.time_usec, hil.roll_ailerons, hil.pitch_elevator, hil.yaw_rudder, hil.throttle, hil.mode, hil.nav_mode);
        }
            break;
        case MAVLINK_MSG_ID_VFR_HUD:
        {
            mavlink_vfr_hud_t hud;
            mavlink_msg_vfr_hud_decode(&message, &hud);
            quint64 time = getUnixTime();
            // Display updated values
            emit thrustChanged(this, hud.throttle/100.0);

            if (!attitudeKnown)
            {
                yaw = QGC::limitAngleToPMPId((((double)hud.heading-180.0)/360.0)*M_PI);
                emit attitudeChanged(this, roll, pitch, yaw, time);
            }

            emit altitudeChanged(uasId, hud.alt);
            emit hudSpeedChanged(this, hud.airspeed, 0.0f, hud.climb, time);
        }
            break;
        case MAVLINK_MSG_ID_LOCAL_POSITION_NED:
        {
            mavlink_local_position_ned_t pos;
            mavlink_msg_local_position_ned_decode(&message, &pos);
            quint64 time = getUnixTime(pos.time_boot_ms);

            // Emit position always with component ID
            emit localPositionChanged(this, message.compid, pos.x, pos.y, pos.z, time);


            if (!wrongComponent)
            {
                localX = pos.x;
                localY = pos.y;
                localZ = pos.z;
                emit localPositionChanged(this, pos.x, pos.y, pos.z, time);
                emit localSpeedChanged(this, pos.vx, pos.vy, pos.vz, time);
                isLocalPositionKnown = true;
            }
        }
            break;
        case MAVLINK_MSG_ID_GLOBAL_VISION_POSITION_ESTIMATE:
        {
            mavlink_global_vision_position_estimate_t pos;
            mavlink_msg_global_vision_position_estimate_decode(&message, &pos);
            quint64 time = getUnixTime(pos.usec);
            emit localPositionChanged(this, message.compid, pos.x, pos.y, pos.z, time);
            emit attitudeChanged(this, message.compid, pos.roll, pos.pitch, pos.yaw, time);
        }
            break;
        case MAVLINK_MSG_ID_GLOBAL_POSITION_INT:
        {
            mavlink_global_position_int_t pos;
            mavlink_msg_global_position_int_decode(&message, &pos);
            quint64 time = getUnixTime();
            latitude = pos.lat/(double)1E7;
            longitude = pos.lon/(double)1E7;
            altitude = pos.alt/1000.0;
            speedX = pos.vx/100.0;
            speedY = pos.vy/100.0;
            speedZ = pos.vz/100.0;
            emit globalPositionChanged(this, latitude, longitude, altitude, time);
            emit gpsSpeedChanged(this, speedX, speedY, speedZ, time);

            positionLock = true;
            isGlobalPositionKnown = true;
            //TODO fix this hack for forwarding of global position for patch antenna tracking
            forwardMessage(message);
        }
            break;
        case MAVLINK_MSG_ID_GPS_RAW_INT:
        {
            mavlink_gps_raw_int_t pos;
            mavlink_msg_gps_raw_int_decode(&message, &pos);

            // SANITY CHECK
            // only accept values in a realistic range
            // quint64 time = getUnixTime(pos.time_usec);
            quint64 time = getUnixTime(pos.time_usec);

            emit gpsLocalizationChanged(this, pos.fix_type);
            // TODO: track localization state not only for gps but also for other loc. sources
            int loc_type = pos.fix_type;
            if (loc_type == 1)
            {
                loc_type = 0;
            }
            emit localizationChanged(this, loc_type);

            emit valueChanged(uasId, "GPS_RAW_INT.eph", "m", pos.eph, time);
            emit valueChanged(uasId, "GPS_RAW_INT.epv", "m", pos.epv, time);

            if ((float)pos.eph/100.0f < 20)
            {
                latitude = pos.lat/(double)1E7;
                longitude = pos.lon/(double)1E7;
                altitude = pos.alt/1000.0;
                emit globalPositionChanged(this, latitude, longitude, altitude, time);
                emit valueChanged(uasId, "altitude", "m", altitude, time);

                if (pos.fix_type > 2) {
                    positionLock = true;
                    isGlobalPositionKnown = true;
                } else {
                    positionLock = false;
                    isGlobalPositionKnown = false;
                }

                // Smaller than threshold and not NaN

                float vel = (float)pos.vel/100.0f;
                if (vel < 1000000.0f && !isnan(vel) && !isinf(vel))
                    emit gpsSpeedChanged(this, vel, time);
                else
                    emit textMessageReceived(uasId, message.compid, 255, tr("GCS ERROR: RECEIVED INVALID SPEED OF %1 m/s").arg(vel));
            }
        }
            break;
        case MAVLINK_MSG_ID_GPS_STATUS:
        {
            mavlink_gps_status_t pos;
            mavlink_msg_gps_status_decode(&message, &pos);
            for(int i = 0; i < (int)pos.satellites_visible; i++)
            {
                emit gpsSatelliteStatusChanged(uasId, (unsigned char)pos.satellite_prn[i], (unsigned char)pos.satellite_elevation[i], (unsigned char)pos.satellite_azimuth[i], (unsigned char)pos.satellite_snr[i], static_cast<bool>(pos.satellite_used[i]));
            }
        }
            break;
        case MAVLINK_MSG_ID_GPS_GLOBAL_ORIGIN:
        {
            mavlink_gps_global_origin_t pos;
            mavlink_msg_gps_global_origin_decode(&message, &pos);
            emit homePositionChanged(uasId, pos.latitude / 10000000.0, pos.longitude / 10000000.0, pos.altitude / 1000.0);
        }
            break;
        case MAVLINK_MSG_ID_RADIO_STATUS:
        {
            emit remoteControlRSSIChanged(mavlink_msg_radio_status_get_rssi(&message));
        }
            break;
        case MAVLINK_MSG_ID_RC_CHANNELS_RAW:
        {
            mavlink_rc_channels_raw_t channels;
            mavlink_msg_rc_channels_raw_decode(&message, &channels);
            emit remoteControlRSSIChanged(channels.rssi);
            emit remoteControlChannelRawChanged(0 + (channels.port * 8), channels.chan1_raw);
            emit remoteControlChannelRawChanged(1 + (channels.port * 8), channels.chan2_raw);
            emit remoteControlChannelRawChanged(2 + (channels.port * 8), channels.chan3_raw);
            emit remoteControlChannelRawChanged(3 + (channels.port * 8), channels.chan4_raw);
            emit remoteControlChannelRawChanged(4 + (channels.port * 8), channels.chan5_raw);
            emit remoteControlChannelRawChanged(5 + (channels.port * 8), channels.chan6_raw);
            emit remoteControlChannelRawChanged(6 + (channels.port * 8), channels.chan7_raw);
            emit remoteControlChannelRawChanged(7 + (channels.port * 8), channels.chan8_raw);
        }
            break;
        case MAVLINK_MSG_ID_RC_CHANNELS_SCALED:
        {
            mavlink_rc_channels_scaled_t channels;
            mavlink_msg_rc_channels_scaled_decode(&message, &channels);
            emit remoteControlRSSIChanged(channels.rssi);
            emit remoteControlChannelScaledChanged(0 + (channels.port * 8), channels.chan1_scaled/10000.0f);
            emit remoteControlChannelScaledChanged(1 + (channels.port * 8), channels.chan2_scaled/10000.0f);
            emit remoteControlChannelScaledChanged(2 + (channels.port * 8), channels.chan3_scaled/10000.0f);
            emit remoteControlChannelScaledChanged(3 + (channels.port * 8), channels.chan4_scaled/10000.0f);
            emit remoteControlChannelScaledChanged(4 + (channels.port * 8), channels.chan5_scaled/10000.0f);
            emit remoteControlChannelScaledChanged(5 + (channels.port * 8), channels.chan6_scaled/10000.0f);
            emit remoteControlChannelScaledChanged(6 + (channels.port * 8), channels.chan7_scaled/10000.0f);
            emit remoteControlChannelScaledChanged(7 + (channels.port * 8), channels.chan8_scaled/10000.0f);
        }
            break;
        case MAVLINK_MSG_ID_PARAM_VALUE:
        {
            mavlink_param_value_t value;
            mavlink_msg_param_value_decode(&message, &value);
            QByteArray bytes(value.param_id, MAVLINK_MSG_PARAM_VALUE_FIELD_PARAM_ID_LEN);
            QString parameterName(bytes);
            QVariant param;
            int component = message.compid;
            mavlink_param_union_t val;
            val.param_float = value.param_value;
            val.type = value.param_type;

            // Insert component if necessary
            if (!parameters.contains(component))
            {
                parameters.insert(component, new QMap<QString, QVariant>());
            }

            // Insert parameter into registry
            if (parameters.value(component)->contains(parameterName))
                parameters.value(component)->remove(parameterName);

            // Insert with correct type
            switch (value.param_type)
            {
            case MAV_PARAM_TYPE_REAL32:
                param = QVariant(val.param_float);
                break;
            case MAV_PARAM_TYPE_UINT32:
                param = QVariant(val.param_uint32);
                break;
            case MAV_PARAM_TYPE_INT32:
                param = QVariant(val.param_int32);
                break;
            default:
                qCritical() << "INVALID DATA TYPE USED AS PARAMETER VALUE: " << value.param_type;
                return;
            }

            parameters.value(component)->insert(parameterName, param);
            // Emit change
            emit parameterChanged(uasId, message.compid, parameterName, param);
            emit parameterChanged(uasId, message.compid, value.param_count, value.param_index, parameterName, param);
        }
            break;
        case MAVLINK_MSG_ID_COMMAND_ACK:
        {
            mavlink_command_ack_t ack;
            mavlink_msg_command_ack_decode(&message, &ack);

            emit commandAcked(uasId, message.compid, ack.command, ack.result);

            switch (ack.result)
            {
            case MAV_CMD_ACK_OK:
            {
                emit textMessageReceived(uasId, message.compid, 0, tr("SUCCESS: Executed CMD: %1").arg(ack.command));
            }
                break;
            case MAV_CMD_ACK_ERR_FAIL:
            {
                emit textMessageReceived(uasId, message.compid, 0, tr("FAILURE: Temporarily rejected CMD: %1").arg(ack.command));
            }
                break;
            case MAV_CMD_ACK_ERR_NOT_SUPPORTED:
            {
                emit textMessageReceived(uasId, message.compid, 0, tr("FAILURE: Unsupported CMD: %1").arg(ack.command));
            }
                break;
            }
        }
            break;
        case MAVLINK_MSG_ID_DATA_STREAM:
        {
            mavlink_data_stream_t ds;
            mavlink_msg_data_stream_decode(&message, &ds);

            emit dataStreamAnnounced(uasId, ds.stream_id, ds.message_rate, ds.on_off);
        }
            break;
        case MAVLINK_MSG_ID_MISSION_COUNT:
        {
            mavlink_mission_count_t wpc;
            mavlink_msg_mission_count_decode(&message, &wpc);
        }
            break;

        case MAVLINK_MSG_ID_MISSION_ITEM:
        {
            mavlink_mission_item_t wp;
            mavlink_msg_mission_item_decode(&message, &wp);
        }
            break;

        case MAVLINK_MSG_ID_MISSION_ACK:
        {
            mavlink_mission_ack_t wpa;
            mavlink_msg_mission_ack_decode(&message, &wpa);
        }
            break;

        case MAVLINK_MSG_ID_MISSION_REQUEST:
        {
            mavlink_mission_request_t wpr;
            mavlink_msg_mission_request_decode(&message, &wpr);
        }
            break;

        case MAVLINK_MSG_ID_MISSION_ITEM_REACHED:
        {
            mavlink_mission_item_reached_t wpr;
            mavlink_msg_mission_item_reached_decode(&message, &wpr);
            //            emit textMessageReceived(message.sysid, message.compid, 0, text);
        }
            break;

        case MAVLINK_MSG_ID_MISSION_CURRENT:
        {
            mavlink_mission_current_t wpc;
            mavlink_msg_mission_current_decode(&message, &wpc);
        }
            break;
        case MAVLINK_MSG_ID_STATUSTEXT:
        {
            QByteArray b;
            b.resize(MAVLINK_MSG_STATUSTEXT_FIELD_TEXT_LEN);
            mavlink_msg_statustext_get_text(&message, b.data());
            QString text = QString(b);
            int severity = mavlink_msg_statustext_get_severity(&message);
            emit textMessageReceived(uasId, message.compid, severity, text);
            parseTextMessage(&text, severity);
        }
            break;
        case MAVLINK_MSG_ID_SERVO_OUTPUT_RAW:
        {
            mavlink_servo_output_raw_t raw;
            mavlink_msg_servo_output_raw_decode(&message, &raw);

            if (hilEnabled)
            {
                emit hilActuatorsChanged(static_cast<uint64_t>(getUnixTimeFromMs(raw.time_usec)), static_cast<float>(raw.servo1_raw),
                                         static_cast<float>(raw.servo2_raw), static_cast<float>(raw.servo3_raw),
                                         static_cast<float>(raw.servo4_raw), static_cast<float>(raw.servo5_raw), static_cast<float>(raw.servo6_raw),
                                         static_cast<float>(raw.servo7_raw), static_cast<float>(raw.servo8_raw));
            }
        }
            break;
#ifdef MAVLINK_MSG_ID_AQ_ESC_TELEMETRY
        case MAVLINK_MSG_ID_AQ_ESC_TELEMETRY :
        {
            using namespace AUTOQUADMAV;

            uint8_t i;
            uint32_t buf[2];
            esc32CanStatus_t *stat;
            mavlink_aq_esc_telemetry_t esc;

            mavlink_msg_aq_esc_telemetry_decode(&message, &esc);

            for (i=0; i < esc.num_in_seq; ++i) {
                memset(buf, 0, sizeof(buf));
                buf[0] = esc.data0[i];
                buf[1] = esc.data1[i];
                stat = (esc32CanStatus_t *)&buf;
                emit escTelemetryUpdate(
                            esc.escid[i],
                            esc.data_version[i],
                            esc.status_age[i],
                            stat->state,
                            (float)stat->vin / 100.0f,
                            (float)stat->amps / 100.0f,
                            stat->rpm,
                            (float)stat->duty / 255.0f * 100.0f,
                            (float)stat->temp / 4.0f - 32.0f,   // temp in data v3
                            stat->temp,                         // errCount in data v2
                            stat->errCode
                            );
            }
        }
            break;
#endif
#ifdef MAVLINK_ENABLED_PIXHAWK
        case MAVLINK_MSG_ID_DATA_TRANSMISSION_HANDSHAKE:
        {
            mavlink_data_transmission_handshake_t p;
            mavlink_msg_data_transmission_handshake_decode(&message, &p);
            imageSize = p.size;
            imagePackets = p.packets;
            imagePayload = p.payload;
            imageQuality = p.jpg_quality;
            imageType = p.type;
            imageWidth = p.width;
            imageHeight = p.height;
            imageStart = QGC::groundTimeMilliseconds();
        }
            break;

        case MAVLINK_MSG_ID_ENCAPSULATED_DATA:
        {
            mavlink_encapsulated_data_t img;
            mavlink_msg_encapsulated_data_decode(&message, &img);
            int seq = img.seqnr;
            int pos = seq * imagePayload;

            // Check if we have a valid transaction
            if (imagePackets == 0)
            {
                // NO VALID TRANSACTION - ABORT
                // Restart statemachine
                imagePacketsArrived = 0;
            }

            for (int i = 0; i < imagePayload; ++i)
            {
                if (pos <= imageSize) {
                    imageRecBuffer[pos] = img.data[i];
                }
                ++pos;
            }

            ++imagePacketsArrived;

            // emit signal if all packets arrived
            if ((imagePacketsArrived >= imagePackets))
            {
                // Restart statemachine
                imagePacketsArrived = 0;
                emit imageReady(this);
                //qDebug() << "imageReady emitted. all packets arrived";
            }
        }
            break;



#endif
            // Messages to ignore
        case MAVLINK_MSG_ID_RAW_IMU:
        case MAVLINK_MSG_ID_SCALED_IMU:
        case MAVLINK_MSG_ID_NAV_CONTROLLER_OUTPUT:
        case MAVLINK_MSG_ID_RAW_PRESSURE:
        case MAVLINK_MSG_ID_SCALED_PRESSURE:
        case MAVLINK_MSG_ID_OPTICAL_FLOW:
        case MAVLINK_MSG_ID_DEBUG_VECT:
        case MAVLINK_MSG_ID_DEBUG:
        case MAVLINK_MSG_ID_NAMED_VALUE_FLOAT:
        case MAVLINK_MSG_ID_NAMED_VALUE_INT:
        case MAVLINK_MSG_ID_MANUAL_CONTROL:
        case MAVLINK_MSG_ID_HIGHRES_IMU:
            break;
        default:
        {
            if (!unknownPackets.contains(message.msgid))
            {
                unknownPackets.append(message.msgid);
                //                emit textMessageReceived(uasId, message.compid, 255, errString);
            }
        }
            break;
        }
    }
}

/**
* Set the home position of the UAS.
* @param lat The latitude fo the home position
* @param lon The longitute of the home position
* @param alt The altitude of the home position
*/
void UAS::setHomePosition(double lat, double lon, double alt)
{
    Q_UNUSED(lat);
    Q_UNUSED(lon);
    Q_UNUSED(alt);

}

/**
* Set the origin to the current GPS location.
**/
void UAS::setLocalOriginAtCurrentGPSPosition()
{
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText(tr("Setting new World Coordinate Frame Origin"));
    msgBox.setInformativeText(tr("Do you want to set a new origin? Waypoints defined in the local frame will be shifted in their physical location"));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);
    int ret = msgBox.exec();

    // Close the message box shortly after the click to prevent accidental clicks
    QTimer::singleShot(5000, &msgBox, SLOT(reject()));


    if (ret == QMessageBox::Yes)
    {
        mavlink_message_t msg;
        mavlink_msg_command_long_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, this->getUASID(), 0, MAV_CMD_DO_SET_HOME, 1, 1, 0, 0, 0, 0, 0, 0);
        // Send message twice to increase chance that it reaches its goal
        sendMessage(msg);
    }
}

/**
* Set a local position setpoint.
* @param x postion
* @param y position
* @param z position
*/
void UAS::setLocalPositionSetpoint(float x, float y, float z, float yaw)
{
#ifdef MAVLINK_ENABLED_PIXHAWK
    mavlink_message_t msg;
    mavlink_msg_set_local_position_setpoint_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, uasId, 0, MAV_FRAME_LOCAL_NED, x, y, z, yaw/M_PI*180.0);
    sendMessage(msg);
#else
    Q_UNUSED(x);
    Q_UNUSED(y);
    Q_UNUSED(z);
    Q_UNUSED(yaw);
#endif
}

/**
* Set a offset of the local position.
* @param x position
* @param y position
* @param z position
* @param yaw
*/
void UAS::setLocalPositionOffset(float x, float y, float z, float yaw)
{
#ifdef MAVLINK_ENABLED_PIXHAWK
    mavlink_message_t msg;
    mavlink_msg_set_position_control_offset_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, uasId, 0, x, y, z, yaw);
    sendMessage(msg);
#else
    Q_UNUSED(x);
    Q_UNUSED(y);
    Q_UNUSED(z);
    Q_UNUSED(yaw);
#endif
}

void UAS::startRadioControlCalibration()
{
    mavlink_message_t msg;
    // Param 1: gyro cal, param 2: mag cal, param 3: pressure cal, Param 4: radio
    mavlink_msg_command_long_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, uasId, MAV_COMP_ID_IMU, MAV_CMD_PREFLIGHT_CALIBRATION, 1, 0, 0, 0, 1, 0, 0, 0);
    sendMessage(msg);
}

void UAS::startDataRecording()
{
    mavlink_message_t msg;
    mavlink_msg_command_long_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, uasId, 0, MAV_CMD_DO_CONTROL_VIDEO, 1, -1, -1, -1, 2, 0, 0, 0);
    sendMessage(msg);
}

void UAS::stopDataRecording()
{
    mavlink_message_t msg;
    mavlink_msg_command_long_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, uasId, 0, MAV_CMD_DO_CONTROL_VIDEO, 1, -1, -1, -1, 0, 0, 0, 0);
    sendMessage(msg);
}

void UAS::startMagnetometerCalibration()
{
    mavlink_message_t msg;
    // Param 1: gyro cal, param 2: mag cal, param 3: pressure cal, Param 4: radio
    mavlink_msg_command_long_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, uasId, MAV_COMP_ID_IMU, MAV_CMD_PREFLIGHT_CALIBRATION, 1, 0, 1, 0, 0, 0, 0, 0);
    sendMessage(msg);
}

void UAS::startGyroscopeCalibration()
{
    mavlink_message_t msg;
    // Param 1: gyro cal, param 2: mag cal, param 3: pressure cal, Param 4: radio
    mavlink_msg_command_long_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, uasId, MAV_COMP_ID_IMU, MAV_CMD_PREFLIGHT_CALIBRATION, 1, 1, 0, 0, 0, 0, 0, 0);
    sendMessage(msg);
}

void UAS::startPressureCalibration()
{
    mavlink_message_t msg;
    // Param 1: gyro cal, param 2: mag cal, param 3: pressure cal, Param 4: radio
    mavlink_msg_command_long_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, uasId, MAV_COMP_ID_IMU, MAV_CMD_PREFLIGHT_CALIBRATION, 1, 0, 0, 1, 0, 0, 0, 0);
    sendMessage(msg);
}

void UAS::parseTextMessage(QString *msg, int severity)
{
    Q_UNUSED(severity);
    if (msg->contains(QRegExp("^(#audio:|Warning|Error)", Qt::CaseInsensitive))) {
        msg->remove("#audio:");
    } else {

#ifdef MAV_CUSTOM_VERSION_PARSE_REGEX
        QRegExp versionRe(MAV_CUSTOM_VERSION_PARSE_REGEX);
        QStringList vlist;
        uint8_t fwMaj = 0, fwMin = 0, hwMaj = 0, hwMin = 0, hwRev = 0, hwId = 0, i = 1;
        uint16_t fwBld = 0;
        bool ok;

        // parse version number
        if (msg->contains(versionRe)) {
            firmwareVersionStr = "";
            hardwareVersionStr = "";
            vlist = versionRe.capturedTexts();
            if (vlist.length() > i && vlist.at(i).length()) {
                firmwareVersionStr = vlist.at(i);
                if (vlist.length() > ++i && vlist.at(i).length()) {
                    fwMaj = vlist.at(i).toUInt(&ok);
                    if (!ok) fwMaj = 0;
                }
                if (vlist.length() > ++i && vlist.at(i).length()) {
                    fwMin = vlist.at(i).toUInt(&ok);
                    if (!ok) fwMin = 0;
                }
                if (vlist.length() > ++i && vlist.at(i).length()) {
                    fwBld = vlist.at(i).toUInt(&ok);
                    if (!ok) fwBld = 0;
                }
                if (vlist.length() > ++i && vlist.at(i).length()) {
                    hardwareVersionStr = vlist.at(i);
                    if (vlist.length() > ++i && vlist.at(i).length()) {
                        hwMaj = vlist.at(i).toUInt(&ok);
                        if (!ok) hwMaj = 0;
                    }
                    if (vlist.length() > ++i && vlist.at(i).length()) {
                        hwMin = vlist.at(i).toUInt(&ok);
                        if (!ok) hwMin = 0;
                    }
                    if (vlist.length() > ++i && vlist.at(i).length()) {
                        hwRev = vlist.at(i).toUInt(&ok);
                        if (!ok) hwRev = 0;
                    }
                    if (vlist.length() > ++i && vlist.at(i).length()) {
                        hwId = vlist.at(i).toUInt(&ok);
                        if (!ok) hwId = 0;
                    }
                }
                firmwareVersion = (fwMaj << 24) | (fwMin << 16) | fwBld;
                hardwareVersion = (hwMaj << 24) | (hwMin << 16) | (hwRev << 8) | hwId;;

                emit systemVersionChanged(uasId, firmwareVersion, hardwareVersion, firmwareVersionStr, hardwareVersionStr);
            }
        }
#endif
    }
}

/**
* Check if time is smaller than 40 years, assuming no system without Unix
* timestamp runs longer than 40 years continuously without reboot. In worst case
* this will add/subtract the communication delay between GCS and MAV, it will
* never alter the timestamp in a safety critical way.
*/
quint64 UAS::getUnixReferenceTime(quint64 time)
{
    // Same as getUnixTime, but does not react to attitudeStamped mode
    if (time == 0)
    {
        return QGC::groundTimeMilliseconds();
    }
    // Check if time is smaller than 40 years,
    // assuming no system without Unix timestamp
    // runs longer than 40 years continuously without
    // reboot. In worst case this will add/subtract the
    // communication delay between GCS and MAV,
    // it will never alter the timestamp in a safety
    // critical way.
    //
    // Calculation:
    // 40 years
    // 365 days
    // 24 hours
    // 60 minutes
    // 60 seconds
    // 1000 milliseconds
    // 1000 microseconds
#ifndef _MSC_VER
    else if (time < 1261440000000000LLU)
#else
    else if (time < 1261440000000000)
#endif
    {
        //        qDebug() << "GEN time:" << time/1000 + onboardTimeOffset;
        if (onboardTimeOffset == 0)
        {
            onboardTimeOffset = QGC::groundTimeMilliseconds() - time/1000;
        }
        return time/1000 + onboardTimeOffset;
    }
    else
    {
        // Time is not zero and larger than 40 years -> has to be
        // a Unix epoch timestamp. Do nothing.
        return time/1000;
    }
}

/**
* @warning If attitudeStamped is enabled, this function will not actually return
* the precise time stamp of this measurement augmented to UNIX time, but will
* MOVE the timestamp IN TIME to match the last measured attitude. There is no
* reason why one would want this, except for system setups where the onboard
* clock is not present or broken and datasets should be collected that are still
* roughly synchronized. PLEASE NOTE THAT ENABLING ATTITUDE STAMPED RUINS THE
* SCIENTIFIC NATURE OF THE CORRECT LOGGING FUNCTIONS OF QGROUNDCONTROL!
*/
quint64 UAS::getUnixTimeFromMs(quint64 time)
{
    return getUnixTime(time*1000);
}

/**
* @warning If attitudeStamped is enabled, this function will not actually return
* the precise time stam of this measurement augmented to UNIX time, but will
* MOVE the timestamp IN TIME to match the last measured attitude. There is no
* reason why one would want this, except for system setups where the onboard
* clock is not present or broken and datasets should be collected that are
* still roughly synchronized. PLEASE NOTE THAT ENABLING ATTITUDE STAMPED
* RUINS THE SCIENTIFIC NATURE OF THE CORRECT LOGGING FUNCTIONS OF QGROUNDCONTROL!
*/
quint64 UAS::getUnixTime(quint64 time)
{
    quint64 ret = 0;
    if (attitudeStamped)
    {
        ret = lastAttitude;
    }

    if (time == 0)
    {
        ret = QGC::groundTimeMilliseconds();
    }
    // Check if time is smaller than 40 years,
    // assuming no system without Unix timestamp
    // runs longer than 40 years continuously without
    // reboot. In worst case this will add/subtract the
    // communication delay between GCS and MAV,
    // it will never alter the timestamp in a safety
    // critical way.
    //
    // Calculation:
    // 40 years
    // 365 days
    // 24 hours
    // 60 minutes
    // 60 seconds
    // 1000 milliseconds
    // 1000 microseconds
#ifndef _MSC_VER
    else if (time < 1261440000000000LLU)
#else
    else if (time < 1261440000000000)
#endif
    {
        //        qDebug() << "GEN time:" << time/1000 + onboardTimeOffset;
        if (onboardTimeOffset == 0 || time < (lastNonNullTime - 100))
        {
            lastNonNullTime = time;
            onboardTimeOffset = QGC::groundTimeMilliseconds() - time/1000;
        }
        if (time > lastNonNullTime) lastNonNullTime = time;

        ret = time/1000 + onboardTimeOffset;
    }
    else
    {
        // Time is not zero and larger than 40 years -> has to be
        // a Unix epoch timestamp. Do nothing.
        ret = time/1000;
    }

    return ret;
}

/**
* @param component that will be searched for in the map of parameters.
*/
QList<QString> UAS::getParameterNames(int component)
{
    if (parameters.contains(component))
    {
        return parameters.value(component)->keys();
    }
    else
    {
        return QList<QString>();
    }
}

QList<int> UAS::getComponentIds()
{
    return parameters.keys();
}

/**
* @param mode that UAS is to be set to.
*/
void UAS::setMode(int mode)
{
    mavlink_message_t msg;
    mavlink_msg_set_mode_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, (uint8_t)uasId, (uint8_t)mode, (uint16_t)navMode);
    sendMessage(msg);
    qDebug() << "SENDING REQUEST TO SET MODE TO SYSTEM" << uasId << ", REQUEST TO SET MODE " << (uint8_t)mode;
}

/**
* Send a message to every link that is connected.
* @param message that is to be sent
*/
void UAS::sendMessage(mavlink_message_t message)
{
    if (!LinkManager::instance()) return;
    // Emit message on all links that are currently connected
    foreach (LinkInterface* link, *links)
    {
        if (LinkManager::instance()->getLinks().contains(link))
        {
            sendMessage(link, message);
        }
        else
        {
            // Remove from list
            links->removeAt(links->indexOf(link));
        }
    }
}

/**
* Forward a message to all links that are currently connected.
* @param message that is to be forwarded
*/
void UAS::forwardMessage(mavlink_message_t message)
{
    // Emit message on all links that are currently connected
    QList<LinkInterface*>link_list = LinkManager::instance()->getLinksForProtocol(mavlink);

    foreach(LinkInterface* link, link_list)
    {
        if (link)
        {

            sendMessage(link, message);
        }
    }
}

/**
* Send a message to the link that is connected.
* @param link that the message will be sent to
* @message that is to be sent
*/
void UAS::sendMessage(LinkInterface* link, mavlink_message_t message)
{
    if(!link) return;
    // Create buffer
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    // Write message into buffer, prepending start sign
    int len = mavlink_msg_to_send_buffer(buffer, &message);
    static uint8_t messageKeys[256] = MAVLINK_MESSAGE_CRCS;
    mavlink_finalize_message_chan(&message, mavlink->getSystemId(), mavlink->getComponentId(), link->getId(), message.len, messageKeys[message.msgid]);
    // If link is connected
    if (link->isConnected())
    {
        // Send the portion of the buffer now occupied by the message
        link->writeBytes((const char*)buffer, len);
    }
}

/**
 * @param value battery voltage
 */
float UAS::filterVoltage(float value) const
{
    return lpVoltage * 0.7f + value * 0.3f;
}

/**
* The mode can be preflight or unknown.
* @Return the mode of the autopilot
*/
QString UAS::getNavModeText(int mode)
{
    QString ret = tr("UNKNOWN");
    if (autopilot == MAV_AUTOPILOT_PIXHAWK)
    {
        switch (mode)
        {
        case 0:
            ret = tr("PREFLIGHT");
            break;
        }
    }

    // If nothing matches, return unknown
    return ret;
}

/**
* Get the status of the code and a description of the status.
* Status can be unitialized, booting up, calibrating sensors, active
* standby, cirtical, emergency, shutdown or unknown.
*/
void UAS::getStatusForCode(int statusCode, QString& uasState, QString& stateDescription)
{
    switch (statusCode)
    {
    case MAV_STATE_UNINIT:
        uasState = tr("UNINIT");
        stateDescription = tr("Unitialized, booting up.");
        break;
    case MAV_STATE_BOOT:
        uasState = tr("BOOT");
        stateDescription = tr("Booting system, please wait.");
        break;
    case MAV_STATE_CALIBRATING:
        uasState = tr("CALIBRATING");
        stateDescription = tr("Calibrating sensors, please wait.");
        break;
    case MAV_STATE_ACTIVE:
        uasState = tr("ACTIVE");
        stateDescription = tr("Active, normal operation.");
        break;
    case MAV_STATE_STANDBY:
        uasState = tr("STANDBY");
        stateDescription = tr("Standby mode, ready for launch.");
        break;
    case MAV_STATE_CRITICAL:
        uasState = tr("CRITICAL");
        stateDescription = tr("FAILURE: Continuing operation.");
        break;
    case MAV_STATE_EMERGENCY:
        uasState = tr("EMERGENCY");
        stateDescription = tr("EMERGENCY: Land Immediately!");
        break;
        //case MAV_STATE_HILSIM:
        //uasState = tr("HIL SIM");
        //stateDescription = tr("HIL Simulation, Sensors read from SIM");
        //break;

    case MAV_STATE_POWEROFF:
        uasState = tr("SHUTDOWN");
        stateDescription = tr("Powering off system.");
        break;

    default:
        uasState = tr("UNKNOWN");
        stateDescription = tr("Unknown system state");
        break;
    }
}

QImage UAS::getImage()
{
#ifdef MAVLINK_ENABLED_PIXHAWK

    //    qDebug() << "IMAGE TYPE:" << imageType;

    // RAW greyscale
    if (imageType == MAVLINK_DATA_STREAM_IMG_RAW8U)
    {
        // TODO FIXME
        int imgColors = 255;//imageSize/(imageWidth*imageHeight);
        //const int headerSize = 15;

        // Construct PGM header
        QString header("P5\n%1 %2\n%3\n");
        header = header.arg(imageWidth).arg(imageHeight).arg(imgColors);

        QByteArray tmpImage(header.toStdString().c_str(), header.toStdString().size());
        tmpImage.append(imageRecBuffer);

        //qDebug() << "IMAGE SIZE:" << tmpImage.size() << "HEADER SIZE: (15):" << header.size() << "HEADER: " << header;

        if (imageRecBuffer.isNull())
        {
            qDebug()<< "could not convertToPGM()";
            return QImage();
        }

        if (!image.loadFromData(tmpImage, "PGM"))
        {
            qDebug()<< "could not create extracted image";
            return QImage();
        }

    }
    // BMP with header
    else if (imageType == MAVLINK_DATA_STREAM_IMG_BMP ||
             imageType == MAVLINK_DATA_STREAM_IMG_JPEG ||
             imageType == MAVLINK_DATA_STREAM_IMG_PGM ||
             imageType == MAVLINK_DATA_STREAM_IMG_PNG)
    {
        if (!image.loadFromData(imageRecBuffer))
        {
            qDebug() << "Loading data from image buffer failed!";
        }
    }
    // Restart statemachine
    imagePacketsArrived = 0;
    //imageRecBuffer.clear();
    return image;
#else
    return QImage();
#endif

}

void UAS::requestImage()
{
#ifdef MAVLINK_ENABLED_PIXHAWK
    qDebug() << "trying to get an image from the uas...";

    // check if there is already an image transmission going on
    if (imagePacketsArrived == 0)
    {
        mavlink_message_t msg;
        mavlink_msg_data_transmission_handshake_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, DATA_TYPE_JPEG_IMAGE, 0, 0, 0, 0, 0, 50);
        sendMessage(msg);
    }
#endif
}


/* MANAGEMENT */

/**
 *
 * @return The uptime in milliseconds
 *
 */
quint64 UAS::getUptime() const
{
    if(startTime == 0)
    {
        return 0;
    }
    else
    {
        return QGC::groundTimeMilliseconds() - startTime;
    }
}

int UAS::getCommunicationStatus() const
{
    return commStatus;
}

void UAS::requestParameters()
{
    mavlink_message_t msg;
    mavlink_msg_param_request_list_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, this->getUASID(), MAV_COMP_ID_ALL);
    sendMessage(msg);
}

void UAS::writeParametersToStorage()
{
    mavlink_message_t msg;
    mavlink_msg_command_long_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, uasId, 0, MAV_CMD_PREFLIGHT_STORAGE, 1, 1, -1, -1, -1, 0, 0, 0);
    sendMessage(msg);
}

void UAS::readParametersFromStorage()
{
    mavlink_message_t msg;
    mavlink_msg_command_long_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, uasId, 0, MAV_CMD_PREFLIGHT_STORAGE, 1, 0, -1, -1, -1, 0, 0, 0);
    sendMessage(msg);
}

void UAS::readParametersFromStorageAQ()
{
    sendCommmandToAq(MAV_CMD_PREFLIGHT_STORAGE, 1, 0.0f);
}

void UAS::writeParametersToStorageAQ()
{
    sendCommmandToAq(MAV_CMD_PREFLIGHT_STORAGE, 1, 1.0f);
}

void UAS::readParametersFromSDAQ()
{
    sendCommmandToAq(MAV_CMD_PREFLIGHT_STORAGE, 1, 2.0f);
}

void UAS::writeParametersToSDAQ()
{
    sendCommmandToAq(MAV_CMD_PREFLIGHT_STORAGE, 1, 3.0f);
}

void UAS::loadDefaultParametersAQ()
{
    sendCommmandToAq(MAV_CMD_PREFLIGHT_STORAGE, 1, 4.0f);
}

void UAS::readWaypointsFromSDAQ()
{
    sendCommmandToAq(MAV_CMD_PREFLIGHT_STORAGE, 1, -1.0f, 0.0f);
}

void UAS::writeWaypointsToSDAQ()
{
    sendCommmandToAq(MAV_CMD_PREFLIGHT_STORAGE, 1, -1.0f, 1.0f);
}

void UAS::startStopTelemetry(bool enable, float frequenz, uint8_t dataset){
    sendCommmandToAq(MAV_CMD_AQ_TELEMETRY, 1, enable, frequenz, dataset);
}

void UAS::sendCommmandToAq(int command,int confirm, float para1,float para2,float para3,float para4,float para5,float para6,float para7){
    mavlink_message_t msg;
    mavlink_msg_command_long_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, uasId, 0, command, confirm, para1, para2, para3, para4, para5, para6, para7);
    sendMessage(msg);
}

void UAS::sendCommmandToIMU(int command,int confirm, float para1,float para2,float para3,float para4,float para5,float para6,float para7){
    mavlink_message_t msg;
    mavlink_msg_command_long_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, uasId, MAV_COMP_ID_IMU, command, confirm, para1, para2, para3, para4, para5, para6, para7);
    sendMessage(msg);
}


/**
* @param rate The update rate in Hz the message should be sent
*/
void UAS::enableDataStream(const int streamId, const int rate)
{
    if (streamId < 0 || streamId >= MAV_DATA_STREAM_ENUM_END)
        return;

    // Buffers to write data to
    mavlink_message_t msg;
    mavlink_request_data_stream_t stream;
    // Select the message to request from now on
    stream.req_stream_id = streamId;
    // Select the update rate in Hz the message should be send
    stream.req_message_rate = rate;
    // Start / stop the message
    stream.start_stop = (rate) ? 1 : 0;
    // The system which should take this command
    stream.target_system = uasId;
    // The component / subsystem which should take this command
    stream.target_component = 0;
    // Encode and send the message
    mavlink_msg_request_data_stream_encode(mavlink->getSystemId(), mavlink->getComponentId(), &msg, &stream);
    sendMessage(msg);
}



/**
* @param rate The update rate in Hz the message should be sent
*/
void UAS::enableAllDataTransmission(int rate)
{
    enableDataStream(MAV_DATA_STREAM_ALL, rate);
}

/**
* @param rate The update rate in Hz the message should be sent
*/
void UAS::enableRawSensorDataTransmission(int rate)
{
    enableDataStream(MAV_DATA_STREAM_RAW_SENSORS, rate);
}

/**
* @param rate The update rate in Hz the message should be sent
*/
void UAS::enableExtendedSystemStatusTransmission(int rate)
{
    enableDataStream(MAV_DATA_STREAM_EXTENDED_STATUS, rate);
}

/**
* @param rate The update rate in Hz the message should be sent
*/
void UAS::enableRCChannelDataTransmission(int rate)
{
#if defined(MAVLINK_ENABLED_UALBERTA_MESSAGES)
    mavlink_message_t msg;
    mavlink_msg_request_rc_channels_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, enabled);
    sendMessage(msg);
#else
    enableDataStream(MAV_DATA_STREAM_RC_CHANNELS, rate);
#endif
}

/**
* @param rate The update rate in Hz the message should be sent
*/
void UAS::enableRawControllerDataTransmission(int rate)
{
    enableDataStream(MAV_DATA_STREAM_RAW_CONTROLLER, rate);
}

/**
* @param rate The update rate in Hz the message should be sent
*/
void UAS::enablePositionTransmission(int rate)
{
    enableDataStream(MAV_DATA_STREAM_POSITION, rate);
}

/**
* @param rate The update rate in Hz the message should be sent
*/
void UAS::enableExtra1Transmission(int rate)
{
    enableDataStream(MAV_DATA_STREAM_EXTRA1, rate);
}

/**
* @param rate The update rate in Hz the message should be sent
*/
void UAS::enableExtra2Transmission(int rate)
{
    enableDataStream(MAV_DATA_STREAM_EXTRA2, rate);
}

/**
* @param rate The update rate in Hz the message should be sent
*/
void UAS::enableExtra3Transmission(int rate)
{
    enableDataStream(MAV_DATA_STREAM_EXTRA3, rate);
}

/**
 * Set a parameter value onboard
 *
 * @param component The component to set the parameter
 * @param id Name of the parameter
 */
void UAS::setParameter(const int component, const QString& id, const QVariant& value)
{
    if (!id.isNull())
    {
        mavlink_message_t msg;
        mavlink_param_set_t p;
        mavlink_param_union_t union_value;

        // Assign correct value based on QVariant
        switch (value.type())
        {
        case QVariant::Int:
            union_value.param_int32 = value.toInt();
            p.param_type = MAV_PARAM_TYPE_INT32;
            break;
        case QVariant::UInt:
            union_value.param_uint32 = value.toUInt();
            p.param_type = MAV_PARAM_TYPE_UINT32;
            break;
        case QMetaType::Float:
        case QMetaType::Double:
            union_value.param_float = value.toFloat();
            p.param_type = MAV_PARAM_TYPE_REAL32;
            break;
        default:
            qCritical() << "ABORTED PARAM SEND, NO VALID QVARIANT TYPE";
            return;
        }

        p.param_value = union_value.param_float;
        p.target_system = (uint8_t)uasId;
        p.target_component = (uint8_t)component;


        // Copy string into buffer, ensuring not to exceed the buffer size
        for (unsigned int i = 0; i < sizeof(p.param_id); i++)
        {
            // String characters
            if ((int)i < id.length())
            {
                p.param_id[i] = id.toLatin1()[i];
            }
            else
            {
                // Fill rest with zeros
                p.param_id[i] = 0;
            }
        }
        mavlink_msg_param_set_encode(mavlink->getSystemId(), mavlink->getComponentId(), &msg, &p);
        sendMessage(msg);
    }
}

/**
* Request parameter, use parameter name to request it.
*/
void UAS::requestParameter(int component, int id)
{
    // Request parameter, use parameter name to request it
    mavlink_message_t msg;
    mavlink_param_request_read_t read;
    read.param_index = id;
    read.param_id[0] = '\0'; // Enforce null termination
    read.target_system = uasId;
    read.target_component = component;
    mavlink_msg_param_request_read_encode(mavlink->getSystemId(), mavlink->getComponentId(), &msg, &read);
    sendMessage(msg);
}

/**
* Request a parameter, use parameter name to request it.
*/
void UAS::requestParameter(int component, const QString& parameter)
{
    // Request parameter, use parameter name to request it
    mavlink_message_t msg;
    mavlink_param_request_read_t read;
    read.param_index = -1;
    // Copy full param name or maximum max field size
    if (parameter.length() > MAVLINK_MSG_PARAM_REQUEST_READ_FIELD_PARAM_ID_LEN)
        emit textMessageReceived(uasId, 0, 255, tr("QGC WARNING: Parameter name %1 is more than %2 bytes long. This might lead to errors and mishaps!").arg(parameter).arg(MAVLINK_MSG_PARAM_REQUEST_READ_FIELD_PARAM_ID_LEN-1));

    memcpy(read.param_id, parameter.toStdString().c_str(), qMax(parameter.length(), MAVLINK_MSG_PARAM_REQUEST_READ_FIELD_PARAM_ID_LEN));
    read.param_id[15] = '\0'; // Enforce null termination
    read.target_system = uasId;
    read.target_component = component;
    mavlink_msg_param_request_read_encode(mavlink->getSystemId(), mavlink->getComponentId(), &msg, &read);
    sendMessage(msg);
    qDebug() << __FILE__ << __LINE__ << "REQUESTING PARAM RETRANSMISSION FROM COMPONENT" << component << "FOR PARAM NAME" << parameter;
}

/**
* @param systemType Type of MAV.
*/
void UAS::setSystemType(int systemType)
{
    if((systemType >= MAV_TYPE_GENERIC) && (systemType < MAV_TYPE_ENUM_END))
    {
        type = systemType;

        // If the airframe is still generic, change it to a close default type
        if (airframe == 0)
        {
            switch (systemType)
            {
            case MAV_TYPE_FIXED_WING:
                airframe = QGC_AIRFRAME_EASYSTAR;
                break;
            case MAV_TYPE_QUADROTOR:
                airframe = QGC_AIRFRAME_MIKROKOPTER;
                break;
            }
        }
        emit systemSpecsChanged(uasId);
    }
}

void UAS::setUASName(const QString& name)
{
    if (name != "")
    {
        this->name = name;
        writeSettings();
        emit nameChanged(name);
        emit systemSpecsChanged(uasId);
    }
}

void UAS::executeCommand(MAV_CMD command)
{
    mavlink_message_t msg;
    mavlink_command_long_t cmd;
    cmd.command = (uint16_t)command;
    cmd.confirmation = 0;
    cmd.param1 = 0.0f;
    cmd.param2 = 0.0f;
    cmd.param3 = 0.0f;
    cmd.param4 = 0.0f;
    cmd.param5 = 0.0f;
    cmd.param6 = 0.0f;
    cmd.param7 = 0.0f;
    cmd.target_system = uasId;
    cmd.target_component = 0;
    mavlink_msg_command_long_encode(mavlink->getSystemId(), mavlink->getComponentId(), &msg, &cmd);
    sendMessage(msg);
}

void UAS::executeCommand(MAV_CMD command, int confirmation, float param1, float param2, float param3, float param4, float param5, float param6, float param7, int component)
{
    mavlink_message_t msg;
    mavlink_command_long_t cmd;
    cmd.command = (uint16_t)command;
    cmd.confirmation = confirmation;
    cmd.param1 = param1;
    cmd.param2 = param2;
    cmd.param3 = param3;
    cmd.param4 = param4;
    cmd.param5 = param5;
    cmd.param6 = param6;
    cmd.param7 = param7;
    cmd.target_system = uasId;
    cmd.target_component = component;
    mavlink_msg_command_long_encode(mavlink->getSystemId(), mavlink->getComponentId(), &msg, &cmd);
    sendMessage(msg);
}

/**
 * Launches the system
 *
 */
void UAS::launch()
{
    mavlink_message_t msg;
    mavlink_msg_command_long_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, this->getUASID(), 0, MAV_CMD_NAV_TAKEOFF, 1, 0, 0, 0, 0, 0, 0, 0);
    sendMessage(msg);
}

/**
 * @warning Depending on the UAS, this might make the rotors of a helicopter spinning
 *
 */
void UAS::armSystem()
{
    mavlink_message_t msg;
    mavlink_msg_set_mode_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, this->getUASID(), mode | MAV_MODE_FLAG_SAFETY_ARMED, navMode);
    sendMessage(msg);
}

/**
 * @warning Depending on the UAS, this might completely stop all motors.
 *
 */
void UAS::disarmSystem()
{
    mavlink_message_t msg;
    mavlink_msg_set_mode_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, this->getUASID(), mode & ~MAV_MODE_FLAG_SAFETY_ARMED, navMode);
    sendMessage(msg);
}

/**
* Set the manual control commands.
* This can only be done if the system has manual inputs enabled and is armed.
*/
void UAS::setManualControlCommands(double roll, double pitch, double yaw, double thrust, int xHat, int yHat, int buttons)
{
    Q_UNUSED(xHat);
    Q_UNUSED(yHat);

    // Scale values
    double rollPitchScaling = 1.0f * 1000.0f;
    double yawScaling = 1.0f * 1000.0f;
    double thrustScaling = 1.0f * 1000.0f;

    manualRollAngle = roll * rollPitchScaling;
    manualPitchAngle = pitch * rollPitchScaling;
    manualYawAngle = yaw * yawScaling;
    manualThrust = thrust * thrustScaling;

    // If system has manual inputs enabled and is armed
    if(((mode & MAV_MODE_FLAG_DECODE_POSITION_MANUAL) && (mode & MAV_MODE_FLAG_DECODE_POSITION_SAFETY)) || (mode & MAV_MODE_FLAG_HIL_ENABLED))
    {
        mavlink_message_t message;
        mavlink_msg_manual_control_pack(mavlink->getSystemId(), mavlink->getComponentId(), &message, this->uasId, (float)manualPitchAngle, (float)manualRollAngle, (float)manualThrust, (float)manualYawAngle, buttons);
        sendMessage(message);

        emit attitudeThrustSetPointChanged(this, roll, pitch, yaw, thrust, QGC::groundTimeMilliseconds());
    }
}

/**
* @return the type of the system
*/
int UAS::getSystemType()
{
    return this->type;
}

/**
* @param buttonIndex
*/
void UAS::receiveButton(int buttonIndex)
{
    switch (buttonIndex)
    {
    case 0:

        break;
    case 1:

        break;
    default:

        break;
    }

}

/**
* Halt the uas.
*/
void UAS::halt()
{
    mavlink_message_t msg;
    mavlink_msg_command_long_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, uasId, MAV_COMP_ID_ALL, MAV_CMD_OVERRIDE_GOTO, 1, MAV_GOTO_DO_HOLD, MAV_GOTO_HOLD_AT_CURRENT_POSITION, 0, 0, 0, 0, 0);
    sendMessage(msg);
}

/**
* Make the UAS move.
*/
void UAS::go()
{
    mavlink_message_t msg;
    mavlink_msg_command_long_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, uasId, MAV_COMP_ID_ALL, MAV_CMD_OVERRIDE_GOTO, 1, MAV_GOTO_DO_CONTINUE, MAV_GOTO_HOLD_AT_CURRENT_POSITION, 0, 0, 0, 0, 0);
    sendMessage(msg);
}

/**
* Order the robot to return home
*/
void UAS::home()
{
    mavlink_message_t msg;

    double latitude = UASManager::instance()->getHomeLatitude();
    double longitude = UASManager::instance()->getHomeLongitude();
    double altitude = UASManager::instance()->getHomeAltitude();
    int frame = UASManager::instance()->getHomeFrame();

    mavlink_msg_command_long_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, uasId, MAV_COMP_ID_ALL, MAV_CMD_OVERRIDE_GOTO, 1, MAV_GOTO_DO_CONTINUE, MAV_GOTO_HOLD_AT_CURRENT_POSITION, frame, 0, latitude, longitude, altitude);
    sendMessage(msg);
}

/**
* Order the robot to land on the runway
*/
void UAS::land()
{
    mavlink_message_t msg;

    mavlink_msg_command_long_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, uasId, MAV_COMP_ID_ALL, MAV_CMD_NAV_LAND, 1, 0, 0, 0, 0, 0, 0, 0);
    sendMessage(msg);
}

/**
 * The MAV starts the emergency landing procedure. The behaviour depends on the onboard implementation
 * and might differ between systems.
 */
void UAS::emergencySTOP()
{
    // FIXME MAVLINKV10PORTINGNEEDED
    halt();
}

/**
 * Shut down this mav - All onboard systems are immediately shut down (e.g. the
 *  main power line is cut).
 * @warning This might lead to a crash.
 *
 * The command will not be executed until emergencyKILLConfirm is issues immediately afterwards
 */
bool UAS::emergencyKILL()
{
    halt();
    return false;
}

/**
* @param time_us Timestamp (microseconds since UNIX epoch or microseconds since system boot)
* @param roll Roll angle (rad)
* @param pitch Pitch angle (rad)
* @param yaw Yaw angle (rad)
* @param rollspeed Roll angular speed (rad/s)
* @param pitchspeed Pitch angular speed (rad/s)
* @param yawspeed Yaw angular speed (rad/s)
* @param lat Latitude, expressed as * 1E7
* @param lon Longitude, expressed as * 1E7
* @param alt Altitude in meters, expressed as * 1000 (millimeters)
* @param vx Ground X Speed (Latitude), expressed as m/s * 100
* @param vy Ground Y Speed (Longitude), expressed as m/s * 100
* @param vz Ground Z Speed (Altitude), expressed as m/s * 100
* @param xacc X acceleration (mg)
* @param yacc Y acceleration (mg)
* @param zacc Z acceleration (mg)
*/
void UAS::sendHilState(uint64_t time_us, float roll, float pitch, float yaw, float rollspeed,
                       float pitchspeed, float yawspeed, int32_t lat, int32_t lon, int32_t alt,
                       int16_t vx, int16_t vy, int16_t vz, int16_t xacc, int16_t yacc, int16_t zacc)
{
    if (this->mode & MAV_MODE_FLAG_HIL_ENABLED)
    {
        mavlink_message_t msg;
        mavlink_msg_hil_state_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg,
                                   time_us, roll, pitch, yaw, rollspeed, pitchspeed, yawspeed,
                                   lat, lon, alt, vx, vy, vz, xacc, yacc, zacc);
        sendMessage(msg);
    }
    else
    {
        // Attempt to set HIL mode
        mavlink_message_t msg;
        mavlink_msg_set_mode_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, this->getUASID(), mode | MAV_MODE_FLAG_HIL_ENABLED, navMode);
        sendMessage(msg);
        qDebug() << __FILE__ << __LINE__ << "HIL is onboard not enabled, trying to enable.";
    }
}

/**
* Connect flight gear link.
**/
void UAS::startHil()
{
}

/**
* disable flight gear link.
*/
void UAS::stopHil()
{
}

void UAS::shutdown()
{
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Critical);
    msgBox.setText(tr("Shutting down the UAS"));
    msgBox.setInformativeText(tr("Do you want to shut down the onboard computer?"));

    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Cancel);
    int ret = msgBox.exec();

    // Close the message box shortly after the click to prevent accidental clicks
    QTimer::singleShot(5000, &msgBox, SLOT(reject()));

    if (ret == QMessageBox::Yes)
    {
        // If the active UAS is set, execute command
        mavlink_message_t msg;
        mavlink_msg_command_long_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, uasId, MAV_COMP_ID_ALL, MAV_CMD_PREFLIGHT_REBOOT_SHUTDOWN, 1, 0, 2, 0, 0, 0, 0, 0);
        sendMessage(msg);
    }
}

/**
* @param x position
* @param y position
* @param z position
* @param yaw
*/
void UAS::setTargetPosition(float x, float y, float z, float yaw)
{
    mavlink_message_t msg;
    mavlink_msg_command_long_pack(mavlink->getSystemId(), mavlink->getComponentId(), &msg, uasId, MAV_COMP_ID_ALL, MAV_CMD_NAV_PATHPLANNING, 1, 1, 1, 0, yaw, x, y, z);
    sendMessage(msg);
}

/**
 * @return The name of this system as string in human-readable form
 */
QString UAS::getUASName(void) const
{
    QString result;
    if (name == "")
    {
        result = tr("MAV ") + result.sprintf("%03d", getUASID());
    }
    else
    {
        result = name;
    }
    return result;
}

/**
* @return the state of the uas as a short text.
*/
const QString& UAS::getShortState() const
{
    return shortStateText;
}

/**
* The mode can be autonomous, guided, manual or armed. It will also return if
* hardware in the loop is being used.
* @return the audio mode text for the id given.
*/
QString UAS::getAudioModeTextFor(int id)
{
    QString mode;
    uint8_t modeid = id;

    // BASE MODE DECODING
    if (modeid & (uint8_t)MAV_MODE_FLAG_DECODE_POSITION_AUTO)
    {
        mode += tr("autonomous");
    }
    else if (modeid & (uint8_t)MAV_MODE_FLAG_DECODE_POSITION_GUIDED)
    {
        mode += tr("guided");
    }
    else if (modeid & (uint8_t)MAV_MODE_FLAG_DECODE_POSITION_STABILIZE)
    {
        mode += tr("stabilized");
    }
    else if (modeid & (uint8_t)MAV_MODE_FLAG_DECODE_POSITION_MANUAL)
    {
        mode += tr("manual");
    }
    else
    {
        // Nothing else applies, we're in preflight
        mode += tr("preflight");
    }

    if (modeid != 0)
    {
        mode += tr(" mode");
    }

    // ARMED STATE DECODING
    //    if (modeid & (uint8_t)MAV_MODE_FLAG_DECODE_POSITION_SAFETY)
    //    {
    //        mode.append(tr(" and armed"));
    //    }

    // HARDWARE IN THE LOOP DECODING
    if (modeid & (uint8_t)MAV_MODE_FLAG_DECODE_POSITION_HIL)
    {
        mode.append(tr(" using hardware in the loop simulation"));
    }

    return mode;
}

/**
* The mode returned can be auto, stabilized, test, manual, preflight or unknown.
* @return the short text of the mode for the id given.
*/
QString UAS::getShortModeTextFor(int id)
{
    QString mode;
    uint8_t modeid = id;


    mode += "|";
    // BASE MODE DECODING
    if (modeid & (uint8_t)MAV_MODE_FLAG_DECODE_POSITION_AUTO)
        mode += tr("AUTO");
    else if (modeid & (uint8_t)MAV_MODE_FLAG_DECODE_POSITION_GUIDED)
        mode += tr("VECTOR");
    mode += "|";
    if (modeid & (uint8_t)MAV_MODE_FLAG_DECODE_POSITION_STABILIZE)
        mode += tr("STABILIZED");
    else if (modeid & (uint8_t)MAV_MODE_FLAG_DECODE_POSITION_TEST)
        mode += tr("TEST");
    else if (modeid & (uint8_t)MAV_MODE_FLAG_DECODE_POSITION_MANUAL)
        mode += tr("MANUAL");
    else if (modeid == 0)
        mode = tr("PREFLIGHT");
    else
        mode = tr("UNKNOWN");

    // ARMED STATE DECODING
    if (modeid & (uint8_t)MAV_MODE_FLAG_DECODE_POSITION_SAFETY)
    {
        mode.prepend("A");
    }
    else
    {
        mode.prepend("D");
    }

    // HARDWARE IN THE LOOP DECODING
    if (modeid & (uint8_t)MAV_MODE_FLAG_DECODE_POSITION_HIL)
    {
        mode.prepend("HIL:");
    }

    return mode;
}

const QString& UAS::getShortMode() const
{
    return shortModeText;
}

/**
* Add the link and connect a signal to it which will be set off when it is destroyed.
*/
void UAS::addLink(LinkInterface* link)
{
    if (!links->contains(link))
    {
        links->append(link);
        connect(link, SIGNAL(destroyed(QObject*)), this, SLOT(removeLink(QObject*)));
        //        if (getLinks()->size() == 1)
        //            emit connected();
    }
}

void UAS::removeLink(QObject* object)
{
    LinkInterface* link = dynamic_cast<LinkInterface*>(object);
    if (link)
    {
        links->removeAt(links->indexOf(link));
    }
    if (!getLinks()->size()) {
        commStatus = COMM_DISCONNECTED;
    }
}

/**
* @return the list of links
*/
QList<LinkInterface*>* UAS::getLinks()
{
    return links;
}

/**
* @rerturn the map of the components
*/
QMap<int, QString> UAS::getComponents()
{
    return components;
}

/**
* Set the battery type and the  number of cells.
* @param type of the battery
* @param cells Number of cells.
*/
void UAS::setBattery(BatteryType type, int cells)
{
    this->batteryType = type;
    this->cells = cells;
    switch (batteryType)
    {
    case NICD:
        break;
    case NIMH:
        break;
    case LIION:
        break;
    case LIPOLY:
        fullVoltage = this->cells * UAS::lipoFull;
        emptyVoltage = this->cells * UAS::lipoEmpty;
        break;
    case LIFE:
        break;
    case AGZN:
        break;
    }
}

/**
* Set the battery specificaitons: empty voltage, warning voltage, and full voltage.
* @param specifications of the battery
*/
void UAS::setBatterySpecs(const QString& specs)
{
    if (specs.length() == 0 || specs.contains("%"))
    {
        batteryRemainingEstimateEnabled = false;
        bool ok;
        QString percent = specs;
        percent = percent.remove("%");
        float temp = percent.toFloat(&ok);
        if (ok)
        {
            warnLevelPercent = temp;
        }
        else
        {
            emit textMessageReceived(0, 0, 0, tr("Could not set battery options, format is wrong"));
        }
    }
    else
    {
        batteryRemainingEstimateEnabled = true;
        QString stringList = specs;
        stringList = stringList.remove(QRegExp("[^\\d,\\.]", Qt::CaseInsensitive));
        QStringList parts = stringList.split(",");
        if (parts.length() == 3)
        {
            float temp;
            bool ok;
            // Get the empty voltage
            temp = parts.at(0).toFloat(&ok);
            if (ok) emptyVoltage = temp;
            // Get the warning voltage
            temp = parts.at(1).toFloat(&ok);
            if (ok) warnVoltage = temp;
            // Get the full voltage
            temp = parts.at(2).toFloat(&ok);
            if (ok) fullVoltage = temp;
        }
        else
        {
            emit textMessageReceived(0, 0, 0, tr("Could not set battery options, format is wrong"));
        }
    }
}

/**
* @return the battery specifications(empty voltage, warning voltage, full voltage)
*/
QString UAS::getBatterySpecs()
{
    if (batteryRemainingEstimateEnabled)
    {
        return QString("%1V, %2V, %3V").arg(emptyVoltage).arg(warnVoltage).arg(fullVoltage);
    }
    else
    {
        return QString("%1%").arg(warnLevelPercent);
    }
}

float UAS::getBatteryEmptyVoltage()
{
    if (batteryRemainingEstimateEnabled)
        return emptyVoltage;
    else
        return -1.0;
}

float UAS::getBatteryWarnVoltage()
{
    if (batteryRemainingEstimateEnabled)
        return warnVoltage;
    else
        return -1.0;
}

/**
* @return the time remaining.
*/
int UAS::calculateTimeRemaining()
{
    quint64 dt = QGC::groundTimeMilliseconds() - startTime;
    double seconds = dt / 1000.0f;
    double voltDifference = startVoltage - currentVoltage;
    if (voltDifference <= 0) voltDifference = 0.00000000001f;
    double dischargePerSecond = voltDifference / seconds;
    int remaining = static_cast<int>((currentVoltage - emptyVoltage) / dischargePerSecond);
    // Can never be below 0
    if (remaining < 0) remaining = 0;
    return remaining;
}

/**
 * @return charge level in percent - 0 - 100
 */
float UAS::getChargeLevel()
{
    if (batteryRemainingEstimateEnabled)
    {
        if (lpVoltage < emptyVoltage)
        {
            chargeLevel = 0.0f;
        }
        else if (lpVoltage > fullVoltage)
        {
            chargeLevel = 100.0f;
        }
        else
        {
            chargeLevel = 100.0f * ((lpVoltage - emptyVoltage)/(fullVoltage - emptyVoltage));
        }
    }
    return chargeLevel;
}

void UAS::startLowBattAlarm()
{
    QString audioSystemName = "";
    if (UASManager::instance()->getSystemsCount() > 1)
        audioSystemName = tr("on System %1").arg(uasId);

    if (!lowBattAlarm)
    {
        lowBattAlarm = true;
    }
}

void UAS::stopLowBattAlarm()
{
    if (lowBattAlarm)
    {
        lowBattAlarm = false;
    }
}
