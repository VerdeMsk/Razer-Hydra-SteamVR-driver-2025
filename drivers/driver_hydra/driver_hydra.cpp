//============ Copyright (c) Valve Corporation, All rights reserved. ============

#define WIN32_LEAN_AND_MEAN

#include "driver_hydra.h"
#include "driverlog.h"
#include <sixense.h> // base station

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h> // for timeBeginPeriod()
#pragma comment(lib, "winmm.lib")
#endif

using namespace vr;

#if defined(_WIN32)
#define HMD_DLL_EXPORT extern "C" __declspec( dllexport )
#define HMD_DLL_IMPORT extern "C" __declspec( dllimport )
#elif defined(__GNUC__) || defined(COMPILER_GCC) || defined(__APPLE__)
#define HMD_DLL_EXPORT extern "C" __attribute__((visibility("default")))
#define HMD_DLL_IMPORT extern "C" 
#else
#error "Unsupported Platform."
#endif

// base station
class CHydraTracker; 
CHydraTracker* g_pHydraTracker = nullptr;
vr::TrackedDeviceIndex_t g_hydraTrackerIndex = vr::k_unTrackedDeviceIndexInvalid;
bool g_bHydraTrackerAdded = false;
bool bShowBaseStation;
vr::HmdVector3_t g_vecBaseEstimate = { 0.0f, 0.0f, 0.0f }; //
// base station

CServerDriver_Hydra g_serverDriverHydra;

//-----------------------------------------------------------------------------
// Purpose: HmdDriverFactory
//-----------------------------------------------------------------------------
HMD_DLL_EXPORT void *HmdDriverFactory(const char *pInterfaceName, int *pReturnCode)
{
    if (0 == strcmp(IServerTrackedDeviceProvider_Version, pInterfaceName))
    {
        return &g_serverDriverHydra;
    }

    // watchdog causes problems with steamvr (endless startup loop), so it's disabled for now
    /*
    if (0 == strcmp(IVRWatchdogProvider_Version, pInterfaceName))
    {
    return &g_watchdogDriverHydra;
    }
    */

    if (pReturnCode)
        *pReturnCode = VRInitError_Init_InterfaceNotFound;

    return NULL;
}


inline HmdQuaternion_t HmdQuaternion_Init(double w, double x, double y, double z)
{
    HmdQuaternion_t quat;
    quat.w = w;
    quat.x = x;
    quat.y = y;
    quat.z = z;
    return quat;
}

// keys for use with the settings API
static const char * const k_pch_Hydra_Section = "hydra";
//static const char * const k_pch_Hydra_RenderModel_String = "rendermodel";
static const char * const k_pch_Hydra_EnableIMU_Bool = "EnableImu";
static const char * const k_pch_Hydra_AlternativeImuVersion_Bool = "AlternativeImuVersion";
static const char * const k_pch_Hydra_GripToggle_Bool = "GripToggle";
static const char * const k_pch_Hydra_JoystickDeadzone_Float = "JoyStickDeadZone";
static const char * const k_pch_Hydra_CrouchPressKey_String = "CrouchPressKey";
static const char * const k_pch_Hydra_CustomPressKey_String = "CustomPressKey";
static const char * const k_pch_Hydra_CrouchOffset_Float = "CrouchOffset";
static const char * const k_pch_Hydra_RecognizeAsIndexControllers_Bool = "IndexControllers";
static const char * const k_pch_Hydra_EnableCustomKey_Bool = "EnableCustomKey";
static const char * const k_pch_Hydra_ShowBaseStation_Bool = "ShowBaseStation";
static const char * const k_pch_Hydra_SixenseFilterEnabled_Bool = "SixenseFilterEnabled";
static const char * const k_pch_Hydra_DynamicFilterPower_Float = "DynamicFilterPower"; 
static const char * const k_pch_Hydra_MinFilteringVal_Float = "MinFilteringValue";
static const char * const k_pch_Hydra_MaxFilteringVal_Float = "MaxFilteringValue";
static const char * const k_pch_Hydra_ThrowMultiplier_Float = "ThrowMultiplier";
float PosZOffset = 0;
float deltaTime = 0.0166667f; // Verde_msk. Initially 0.016f. Not much of a difference

// base station
bool g_bBasePoseWasSet = false; // 

class CHydraTracker : public vr::ITrackedDeviceServerDriver {
public:
	CHydraTracker() {}
	virtual ~CHydraTracker() {}

	virtual vr::EVRInitError Activate(vr::TrackedDeviceIndex_t unObjectId) override {
		m_unObjectId = unObjectId;
		m_ulPropertyContainer = vr::VRProperties()->TrackedDeviceToPropertyContainer(unObjectId);

		vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_ModelNumber_String, "Hydra Tracker");
		vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_SerialNumber_String, "HYDRA-TRACKER-001");
		vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_RegisteredDeviceType_String, "hydra/tracker");
		vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_RenderModelName_String, "{hydra}hydra_base_station"); // lh_basestation_vive {htc}vr_tracker_vive_1_0
		vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, vr::Prop_WillDriftInYaw_Bool, false);
		vr::VRProperties()->SetFloatProperty(m_ulPropertyContainer, vr::Prop_TrackingRangeMinimumMeters_Float, 0.01f);
		vr::VRProperties()->SetFloatProperty(m_ulPropertyContainer, vr::Prop_TrackingRangeMaximumMeters_Float, 2.0f);
		vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, vr::Prop_IsOnDesktop_Bool, false);
		vr::VRProperties()->SetInt32Property(m_ulPropertyContainer, vr::Prop_ControllerRoleHint_Int32, vr::TrackedControllerRole_Invalid);
		return vr::VRInitError_None;
	}

	virtual void Deactivate() override {}
	virtual void EnterStandby() override {}
	virtual void* GetComponent(const char* pchComponentNameAndVersion) override { return nullptr; }
	virtual void DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize) override {}

	virtual vr::DriverPose_t GetPose() override {
		vr::DriverPose_t pose = { 0 };
		pose.poseIsValid = true;
		pose.deviceIsConnected = true;
		pose.result = vr::TrackingResult_Running_OK;
		pose.qRotation.w = 1.0f;

		pose.vecPosition[0] = g_vecBaseEstimate.v[0];
		pose.vecPosition[1] = g_vecBaseEstimate.v[1];
		pose.vecPosition[2] = g_vecBaseEstimate.v[2];

		//DriverLog("hydra: Tracker GetPose() at (%f, %f, %f)\n", pose.vecPosition[0], pose.vecPosition[1], pose.vecPosition[2]);
		return pose;
	}

private:
	vr::TrackedDeviceIndex_t m_unObjectId;
	vr::PropertyContainerHandle_t m_ulPropertyContainer;
};
// base station

//Verde_msk. Dynamic filter
float closeMaxSpeed = 0.01f;
float MaxDist = 1.5f;
float m_fDynamicFilterPower; 
float m_fMinFilteringVal;
float m_fMaxFilteringVal;

static void GenerateSerialNumber(char *p, int psize, int base, int controller)
{
    _snprintf(p, psize, "hydra%d_controller%d", base, controller);
}

int KeyNameToKeyCode(std::string KeyName) {
	std::transform(KeyName.begin(), KeyName.end(), KeyName.begin(), ::toupper);

	if (KeyName == "NONE") return 0;

	else if (KeyName == "MOUSE-LEFT-BTN") return VK_LBUTTON;
	else if (KeyName == "MOUSE-RIGHT-BTN") return VK_RBUTTON;
	else if (KeyName == "MOUSE-MIDDLE-BTN") return VK_MBUTTON;
	else if (KeyName == "MOUSE-SIDE1-BTN") return VK_XBUTTON1;
	else if (KeyName == "MOUSE-SIDE2-BTN") return VK_XBUTTON2;

	else if (KeyName == "ESCAPE") return VK_ESCAPE;
	else if (KeyName == "F1") return VK_F1;
	else if (KeyName == "F2") return VK_F2;
	else if (KeyName == "F3") return VK_F3;
	else if (KeyName == "F4") return VK_F4;
	else if (KeyName == "F5") return VK_F5;
	else if (KeyName == "F6") return VK_F6;
	else if (KeyName == "F7") return VK_F7;
	else if (KeyName == "F8") return VK_F8;
	else if (KeyName == "F9") return VK_F9;
	else if (KeyName == "F10") return VK_F10;
	else if (KeyName == "F11") return VK_F11;
	else if (KeyName == "F12") return VK_F12;

	else if (KeyName == "~") return 192;
	else if (KeyName == "1") return '1';
	else if (KeyName == "2") return '2';
	else if (KeyName == "3") return '3';
	else if (KeyName == "4") return '4';
	else if (KeyName == "5") return '5';
	else if (KeyName == "6") return '6';
	else if (KeyName == "7") return '7';
	else if (KeyName == "8") return '8';
	else if (KeyName == "9") return '9';
	else if (KeyName == "0") return '0';
	else if (KeyName == "-") return 189;
	else if (KeyName == "=") return 187;

	else if (KeyName == "TAB") return VK_TAB;
	else if (KeyName == "CAPS-LOCK") return VK_CAPITAL;
	else if (KeyName == "SHIFT") return VK_SHIFT;
	else if (KeyName == "CTRL") return VK_CONTROL;
	else if (KeyName == "WIN") return VK_LWIN;
	else if (KeyName == "ALT") return VK_MENU;
	else if (KeyName == "SPACE") return VK_SPACE;
	else if (KeyName == "ENTER") return VK_RETURN;
	else if (KeyName == "BACKSPACE") return VK_BACK;

	else if (KeyName == "Q") return 'Q';
	else if (KeyName == "W") return 'W';
	else if (KeyName == "E") return 'E';
	else if (KeyName == "R") return 'R';
	else if (KeyName == "T") return 'T';
	else if (KeyName == "Y") return 'Y';
	else if (KeyName == "U") return 'U';
	else if (KeyName == "I") return 'I';
	else if (KeyName == "O") return 'O';
	else if (KeyName == "P") return 'P';
	else if (KeyName == "[") return '[';
	else if (KeyName == "]") return ']';
	else if (KeyName == "A") return 'A';
	else if (KeyName == "S") return 'S';
	else if (KeyName == "D") return 'D';
	else if (KeyName == "F") return 'F';
	else if (KeyName == "G") return 'G';
	else if (KeyName == "H") return 'H';
	else if (KeyName == "J") return 'J';
	else if (KeyName == "K") return 'K';
	else if (KeyName == "L") return 'L';
	else if (KeyName == ";") return 186;
	else if (KeyName == "'") return 222;
	else if (KeyName == "\\") return 220;
	else if (KeyName == "Z") return 'Z';
	else if (KeyName == "X") return 'X';
	else if (KeyName == "C") return 'C';
	else if (KeyName == "V") return 'V';
	else if (KeyName == "B") return 'B';
	else if (KeyName == "N") return 'N';
	else if (KeyName == "M") return 'M';
	else if (KeyName == "<") return 188;
	else if (KeyName == ">") return 190;
	else if (KeyName == "?") return 191;

	else if (KeyName == "PRINTSCREEN") return VK_SNAPSHOT;
	else if (KeyName == "SCROLL-LOCK") return VK_SCROLL;
	else if (KeyName == "PAUSE") return VK_PAUSE;
	else if (KeyName == "INSERT") return VK_INSERT;
	else if (KeyName == "HOME") return VK_HOME;
	else if (KeyName == "PAGE-UP") return VK_NEXT;
	else if (KeyName == "DELETE") return VK_DELETE;
	else if (KeyName == "END") return VK_END;
	else if (KeyName == "PAGE-DOWN") return VK_PRIOR;

	else if (KeyName == "UP") return VK_UP;
	else if (KeyName == "DOWN") return VK_DOWN;
	else if (KeyName == "LEFT") return VK_LEFT;
	else if (KeyName == "RIGHT") return VK_RIGHT;

	else if (KeyName == "NUM-LOCK") return VK_NUMLOCK;
	else if (KeyName == "NUMPAD0") return VK_NUMPAD0;
	else if (KeyName == "NUMPAD1") return VK_NUMPAD1;
	else if (KeyName == "NUMPAD2") return VK_NUMPAD2;
	else if (KeyName == "NUMPAD3") return VK_NUMPAD3;
	else if (KeyName == "NUMPAD4") return VK_NUMPAD4;
	else if (KeyName == "NUMPAD5") return VK_NUMPAD5;
	else if (KeyName == "NUMPAD6") return VK_NUMPAD6;
	else if (KeyName == "NUMPAD7") return VK_NUMPAD7;
	else if (KeyName == "NUMPAD8") return VK_NUMPAD8;
	else if (KeyName == "NUMPAD9") return VK_NUMPAD9;

	else if (KeyName == "NUMPAD-DIVIDE") return VK_DIVIDE;
	else if (KeyName == "NUMPAD-MULTIPLY") return VK_MULTIPLY;
	else if (KeyName == "NUMPAD-MINUS") return VK_SUBTRACT;
	else if (KeyName == "NUMPAD-PLUS") return VK_ADD;
	else if (KeyName == "NUMPAD-DEL") return VK_DECIMAL;

	else return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Watchdog
//-----------------------------------------------------------------------------
/*
class CWatchdogDriver_Hydra : public IVRWatchdogProvider
{
public:
    CWatchdogDriver_Hydra()
    {
        m_pWatchdogThread = nullptr;
    }

    virtual EVRInitError Init(vr::IVRDriverContext *pDriverContext);
    virtual void Cleanup();

private:
    std::thread *m_pWatchdogThread;
};

CWatchdogDriver_Hydra g_watchdogDriverHydra;

bool g_bExiting = false;

void WatchdogThreadFunction()
{
    while (!g_bExiting)
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        vr::VRWatchdogHost()->WatchdogWakeUp();
    }
}

EVRInitError CWatchdogDriver_Hydra::Init(vr::IVRDriverContext *pDriverContext)
{
    VR_INIT_WATCHDOG_DRIVER_CONTEXT(pDriverContext);
    InitDriverLog(vr::VRDriverLog());

    // Watchdog mode on Windows starts a thread that listens for the 'Y' key on the keyboard to 
    // be pressed. A real driver should wait for a system button event or something else from the 
    // the hardware that signals that the VR system should start up.
    g_bExiting = false;
    m_pWatchdogThread = new std::thread(WatchdogThreadFunction);
    if (!m_pWatchdogThread)
    {
        DriverLog("Unable to create watchdog thread\n");
        return VRInitError_Driver_Failed;
    }

    return VRInitError_None;
}

void CWatchdogDriver_Hydra::Cleanup()
{
    g_bExiting = true;
    if (m_pWatchdogThread)
    {
        m_pWatchdogThread->join();
        delete m_pWatchdogThread;
        m_pWatchdogThread = nullptr;
    }

    CleanupDriverLog();
}
*/

//-----------------------------------------------------------------------------
// Purpose: Controller Driver
//-----------------------------------------------------------------------------
class CHydraControllerDriver : public vr::ITrackedDeviceServerDriver
{
public:

	// base station
	int GetControllerId() const { return m_nId; } 
	sixenseControllerData m_LastData;
	sixenseMath::Vector3 m_WorldFromDriverTranslation;
	sixenseMath::Quat m_WorldFromDriverRotation;
	// base station

	int32_t sixenceControllerRole;
    bool IsActivated() const
    {
        return m_unObjectId != vr::k_unTrackedDeviceIndexInvalid;
    }

    bool HasControllerId(int nBase, int nId)
    {
        return nBase == m_nBase && nId == m_nId;
    }

    /** Process sixenseControllerData.  Return true if it's new to help caller manage sleep durations */
    bool Update(sixenseControllerData & cd)
    {
		m_LastData = cd; // base station

        if (m_ucPoseSequenceNumber == cd.sequence_number || !IsActivated())
            return false;
        m_ucPoseSequenceNumber = cd.sequence_number;

        UpdateTrackingState(cd);

        // Block all buttons until initial press confirms hemisphere
        if (WaitingForHemisphereTracking(cd))
            return true;

        DelaySystemButtonForChording(cd);

		if ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0) {
			if (RecognizeAsIndexCtrls) {
				if ((GetAsyncKeyState('1') & 0x8000) != 0) IndexStickMode = 0;
				if ((GetAsyncKeyState('2') & 0x8000) != 0) IndexStickMode = 1;
				if ((GetAsyncKeyState('3') & 0x8000) != 0) IndexStickMode = 2;
				if ((GetAsyncKeyState('4') & 0x8000) != 0) IndexStickMode = 3;
				if ((GetAsyncKeyState('5') & 0x8000) != 0) IndexStickMode = 4;
			} /*else { // legacy
				if ((GetAsyncKeyState('1') & 0x8000) != 0) ViveStickMode = 0;
				if ((GetAsyncKeyState('2') & 0x8000) != 0) ViveStickMode = 1;
				if ((GetAsyncKeyState('3') & 0x8000) != 0) ViveStickMode = 2;
				if ((GetAsyncKeyState('4') & 0x8000) != 0) ViveStickMode = 3;
			}*/
			if ((GetAsyncKeyState('9') & 0x8000) != 0) m_bCrouchEnable = true;
			if ((GetAsyncKeyState('0') & 0x8000) != 0) m_bCrouchEnable = false;
		}

        UpdateControllerState(cd);

        return true;
    }

	// base station
	bool IsCalibrated() const
	{
		return m_bCalibrated;
	}
	// base station

    bool IsHoldingSystemButton() const
    {
        return m_eSystemButtonState == k_eWaiting;
    }

    void ConsumeSystemButtonPress()
    {
        if (m_eSystemButtonState == k_eWaiting)
        {
            m_eSystemButtonState = k_eBlocked;
        }
    }

    // User initiated manual alignment of the coordinate system of driver_hydra with the HMD:
    //
    // The user has put two controllers on either side of her head, near the shoulders.  We
    // assume that the HMD is roughly in between them (so the exact distance apart doesn't
    // matter as long as the pose is symmetrical) and we align the HMD's coordinate system
    // using the line between the controllers (again, exact position is not important, only
    // symmetry).
    static void RealignCoordinates(CHydraControllerDriver * pHydraA, CHydraControllerDriver * pHydraB)
    {
        if (pHydraA->m_unObjectId == vr::k_unTrackedDeviceIndexInvalid)
            return;

        pHydraA->m_pAlignmentPartner = pHydraB;
        pHydraB->m_pAlignmentPartner = pHydraA;
		pHydraB->sixenceControllerRole = HydraLeftRole;
		pHydraA->sixenceControllerRole = HydraRightRole;

        // Ask hydra_monitor to tell us HMD pose
        static vr::VREvent_Data_t nodata = { 0 };
        vr::VRServerDriverHost()->VendorSpecificEvent(pHydraA->m_unObjectId,
            (vr::EVREventType) (vr::VREvent_VendorSpecific_Reserved_Start + 0), nodata,
            -std::chrono::duration_cast<std::chrono::seconds>(k_SystemButtonChordingDelay).count());
    }

    // hydra_monitor called us back with the HMD information
    // (Note we should probably cache pose at the moment of the chording, but we just use current here)
    void FinishRealignCoordinates(sixenseMath::Matrix3 & matHmdRotation, sixenseMath::Vector3 & vecHmdPosition)
    {
        using namespace sixenseMath;

        CHydraControllerDriver * pHydraA = this;
        CHydraControllerDriver * pHydraB = m_pAlignmentPartner;

        if (!pHydraA || !pHydraB)
            return;

        // Assign left/right arbitrarily for a second
        Vector3 posLeft(pHydraA->m_Pose.vecPosition[0], pHydraA->m_Pose.vecPosition[1], pHydraA->m_Pose.vecPosition[2]);
        Vector3 posRight(pHydraB->m_Pose.vecPosition[0], pHydraB->m_Pose.vecPosition[1], pHydraB->m_Pose.vecPosition[2]);

        Vector3 posCenter = (posLeft + posRight) * 0.5f;
        Vector3 posDiff = posRight - posLeft;

        // Choose arbitrary controller for hint about which one is on the right:
        // Assume controllers are roughly upright, so +X vector points across body.
        Quat q1(pHydraA->m_Pose.qRotation.x, pHydraA->m_Pose.qRotation.y, pHydraA->m_Pose.qRotation.z, pHydraA->m_Pose.qRotation.w);
        Vector3 rightProbe = q1 * Vector3(1, 0, 0);
        if (rightProbe * posDiff < 0) // * is dot product
        {
            std::swap(posLeft, posRight);
            posDiff = posDiff * -1.0f;
        }

        // Find a vector pointing forward relative to the hands, so we can rotate
        // that to match forward for the head.  Use -Y by right hand rule.
        Vector3 hydraFront = posDiff ^ Vector3(0, -1, 0); // ^ is cross product
        Vector3 hmdFront = matHmdRotation * Vector3(0, 0, -1); // -Z implicitly forward

        // Project both "front" vectors onto the XZ plane (we only care about yaw,
        // because we assume the HMD space is Y up, and hydra space is also Y up,
        // assuming the base is level).
        hydraFront[1] = 0.0f;
        hmdFront[1] = 0.0f;

        // Rotation is what makes the hydraFront point toward hmdFront
        Quat rotation = Quat::rotation(hydraFront, hmdFront);

        // Adjust for the natural pose of HMD vs controllers
        Vector3 vecAlignPosition = vecHmdPosition + Vector3(0, -0.100f, -0.100f);
        Vector3 translation = vecAlignPosition - rotation * posCenter;

        // Note that it is very common for all objects from a given driver to share
        // the same world transforms, because they are in the same driver space and
        // the same world space.
        pHydraA->m_WorldFromDriverTranslation = translation;
        pHydraA->m_WorldFromDriverRotation = rotation;
        pHydraA->m_bCalibrated = true;
        pHydraB->m_WorldFromDriverTranslation = translation;
        pHydraB->m_WorldFromDriverRotation = rotation;
        pHydraB->m_bCalibrated = true;
		// base station
		g_bBasePoseWasSet = false; // reset for the next RunFrame()
		//DriverLog("Hydra: Base pose reset due to controller recalibration\n");
		if (!m_bCalibrated && m_eHemisphereTrackingState == k_eHemisphereTrackingEnabled) {
			m_bCalibrated = true;
			g_bBasePoseWasSet = false; // Allow base position to auto-update again
		}
		// base station
    }

    void DebugRequest(const char * pchRequest, char * pchResponseBuffer, uint32_t unResponseBufferSize)
    {
        std::istringstream ss(pchRequest);
        std::string strCmd;

        ss >> strCmd;
        if (strCmd == "hydra:realign_coordinates")
        {
            // hydra_monitor is calling us back with HMD tracking information so we can
            // finish realigning our coordinate system to the HMD's
            float m[3][3], v[3];
            for (int i = 0; i < 3; ++i)
            {
                for (int j = 0; j < 3; ++j)
                {
                    // Note the transpose, because sixenseMath::Matrix3 and vr::HmdMatrix34_t disagree on row/col major
                    ss >> m[j][i];
                }
                ss >> v[i];
            }
            sixenseMath::Matrix3 matRot(m);
            sixenseMath::Vector3 matPos(v);

            FinishRealignCoordinates(matRot, matPos);
        }

        /*
        if (unResponseBufferSize >= 1)
            pchResponseBuffer[0] = 0;
        */
    }

    CHydraControllerDriver(int base, int n):
        m_nBase(base),
        m_nId(n),
        m_ucPoseSequenceNumber(0),
        m_eHemisphereTrackingState(k_eHemisphereTrackingDisabled),
        m_bCalibrated(false),
        m_pAlignmentPartner(NULL),
        m_eSystemButtonState(k_eIdle),
        m_unObjectId(vr::k_unTrackedDeviceIndexInvalid),
        m_ulPropertyContainer(vr::k_ulInvalidPropertyContainer)
    {
        char buf[1024];
        GenerateSerialNumber(buf, sizeof(buf), base, n);
        m_sSerialNumber = buf;
        m_sModelNumber = "Hydra";
        m_sManufacturerName = "Razer";

        memset(&m_ControllerState, 0, sizeof(m_ControllerState));
        memset(&m_Pose, 0, sizeof(m_Pose));
        m_Pose.result = vr::TrackingResult_Calibrating_InProgress;

        sixenseControllerData cd;
        sixenseGetNewestData(m_nId, &cd);
        m_firmware_revision = cd.firmware_revision;
        m_hardware_revision = cd.hardware_revision;

        DriverLog("Using settings values\n");

        // assign rendermodel
        //vr::VRSettings()->GetString(k_pch_Hydra_Section, k_pch_Hydra_RenderModel_String, buf, sizeof(buf));
        //m_sRenderModel = buf;

		// Controller type
		RecognizeAsIndexCtrls = vr::VRSettings()->GetBool(k_pch_Hydra_Section, k_pch_Hydra_RecognizeAsIndexControllers_Bool);

		// Grip toggle mode. Verde_msk
		m_bGripToggle = vr::VRSettings()->GetBool(k_pch_Hydra_Section, k_pch_Hydra_GripToggle_Bool);

        // Enable IMU emulation
        m_bEnableIMUEmulation = vr::VRSettings()->GetBool(k_pch_Hydra_Section, k_pch_Hydra_EnableIMU_Bool);
        m_bEnableAngularVelocity = true;

		// IMU emulation versions. Verde_msk
		m_bAlternativeImuVersion = vr::VRSettings()->GetBool(k_pch_Hydra_Section, k_pch_Hydra_AlternativeImuVersion_Bool);

		// Throw (velocity) multiplier. Verde_msk
		m_fThrowMultiplier = vr::VRSettings()->GetFloat(k_pch_Hydra_Section, k_pch_Hydra_ThrowMultiplier_Float);

        // Set joystick deadzone
        m_fJoystickDeadzone = vr::VRSettings()->GetFloat(k_pch_Hydra_Section, k_pch_Hydra_JoystickDeadzone_Float);

        // "Hold Thumbpad" mode (not user configurable)
        m_bEnableHoldThumbpad = true;

		// Crouch key code (for send to another HMD drivers)
		vr::VRSettings()->GetString(k_pch_Hydra_Section, k_pch_Hydra_CrouchPressKey_String, buf, sizeof(buf));
		m_nCrouchPressKey = KeyNameToKeyCode(buf);

		// Crouch offset Z
		m_fCrouchOffset = vr::VRSettings()->GetFloat(k_pch_Hydra_Section, k_pch_Hydra_CrouchOffset_Float);

		// Cutom key code (for send to another apps)
		vr::VRSettings()->GetString(k_pch_Hydra_Section, k_pch_Hydra_CustomPressKey_String, buf, sizeof(buf));
		m_nCustomPressKey = KeyNameToKeyCode(buf);

		// Enable custom key on left Index controller
		EnabledCustomKey = vr::VRSettings()->GetBool(k_pch_Hydra_Section, k_pch_Hydra_EnableCustomKey_Bool);
    }

    virtual ~CHydraControllerDriver()
    {
    }

	virtual EVRInitError Activate(vr::TrackedDeviceIndex_t unObjectId)
	{
		DriverLog("Activated device: %s (object id %d)\n", GetSerialNumber().c_str(), unObjectId);
		m_unObjectId = unObjectId;

		g_serverDriverHydra.LaunchHydraMonitor();

		// Set properties
		m_ulPropertyContainer = vr::VRProperties()->TrackedDeviceToPropertyContainer(m_unObjectId);

		// Index controllers
		if (RecognizeAsIndexCtrls) { 
			// Parameters taken from here https://github.com/HadesVR/HadesVR/blob/main/Software/Driver/src/samples/driver_HadesVR/include/devices.hpp
			if (sixenceControllerRole == HydraLeftRole) {
				vr::VRProperties()->SetInt32Property(m_ulPropertyContainer, vr::Prop_ControllerRoleHint_Int32, vr::TrackedControllerRole_LeftHand);
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_ModelNumber_String, "Knuckles Left");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_RenderModelName_String, "{indexcontroller}valve_controller_knu_1_0_Left");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_Firmware_ProgrammingTarget_String, "LHR-E217CD00");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_RegisteredDeviceType_String, "valve/index_controllerLHR-E217CD00");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceOff_String, "{indexcontroller}/icons/left_controller_status_off.png");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceSearching_String, "{indexcontroller}/icons/left_controller_status_searching.gif");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceSearchingAlert_String, "{indexcontroller}/icons/left_controller_status_searching_alert.gif");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceReady_String, "{indexcontroller}/icons//left_controller_status_ready.png");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceReadyAlert_String, "{indexcontroller}/icons/left_controller_status_ready_alert.png");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceNotReady_String, "{indexcontroller}/icons/left_controller_status_error.png");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceStandby_String, "{indexcontroller}/icons/left_controller_status_off.png");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceAlertLow_String, "{indexcontroller}/icons/left_controller_status_ready_low.png");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_SerialNumber_String, "LHR-E217CD00");
				vr::VRDriverInput()->CreateSkeletonComponent(m_ulPropertyContainer, "/input/skeleton/left", "/skeleton/hand/left", "/pose/raw", vr::VRSkeletalTracking_Partial, nullptr, 0U, &m_skeletonHandle); // Skeleton not work???
			}
			else {
				vr::VRProperties()->SetInt32Property(m_ulPropertyContainer, vr::Prop_ControllerRoleHint_Int32, vr::TrackedControllerRole_RightHand);
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_ModelNumber_String, "Knuckles Right");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_RenderModelName_String, "{indexcontroller}valve_controller_knu_1_0_right");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_Firmware_ProgrammingTarget_String, "LHR-E217CD01");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_RegisteredDeviceType_String, "valve/index_controllerLHR-E217CD01");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceOff_String, "{indexcontroller}/icons/right_controller_status_off.png");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceSearching_String, "{indexcontroller}/icons/right_controller_status_searching.gif");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceSearchingAlert_String, "{indexcontroller}/icons/right_controller_status_searching_alert.gif");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceReady_String, "{indexcontroller}/icons/right_controller_status_ready.png");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceReadyAlert_String, "{indexcontroller}/icons/right_controller_status_ready_alert.png");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceNotReady_String, "{indexcontroller}/icons/right_controller_status_error.png");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceStandby_String, "{indexcontroller}/icons/right_controller_status_off.png");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceAlertLow_String, "{indexcontroller}/icons/right_controller_status_ready_low.png");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_SerialNumber_String, "LHR-E217CD01");
				vr::VRDriverInput()->CreateSkeletonComponent(m_ulPropertyContainer, "/input/skeleton/right", "/skeleton/hand/right", "/pose/raw", vr::VRSkeletalTracking_Partial, nullptr, 0U, &m_skeletonHandle); // Skeleton not work???
			}

			vr::VRDriverInput()->CreateScalarComponent(m_ulPropertyContainer, "/input/thumbstick/x", &m_thumbstickX, vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedTwoSided);
			vr::VRDriverInput()->CreateScalarComponent(m_ulPropertyContainer, "/input/thumbstick/y", &m_thumbstickY, vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedTwoSided);
			vr::VRDriverInput()->CreateScalarComponent(m_ulPropertyContainer, "/input/trigger/value", &m_triggerValue, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided);
			vr::VRDriverInput()->CreateScalarComponent(m_ulPropertyContainer, "/input/trackpad/x", &m_trackpadX, vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedTwoSided);
			vr::VRDriverInput()->CreateScalarComponent(m_ulPropertyContainer, "/input/trackpad/y", &m_trackpadY, vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedTwoSided);
			vr::VRDriverInput()->CreateScalarComponent(m_ulPropertyContainer, "/input/trackpad/force", &m_trackpadForce, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided);
			vr::VRDriverInput()->CreateScalarComponent(m_ulPropertyContainer, "/input/finger/index", &m_fingerIndex, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided);
			vr::VRDriverInput()->CreateScalarComponent(m_ulPropertyContainer, "/input/finger/middle", &m_fingerMiddle, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided);
			vr::VRDriverInput()->CreateScalarComponent(m_ulPropertyContainer, "/input/finger/ring", &m_fingerRing, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided);
			vr::VRDriverInput()->CreateScalarComponent(m_ulPropertyContainer, "/input/finger/pinky", &m_fingerPinky, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided);
			vr::VRDriverInput()->CreateScalarComponent(m_ulPropertyContainer, "/input/grip/force", &m_gripForce, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided);
			vr::VRDriverInput()->CreateScalarComponent(m_ulPropertyContainer, "/input/grip/value", &m_gripValue, vr::EVRScalarType::VRScalarType_Absolute, vr::EVRScalarUnits::VRScalarUnits_NormalizedOneSided);

			//  Buttons handles
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/a/click", &m_aClick);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/a/touch", &m_aTouch);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/b/click", &m_bClick);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/b/touch", &m_bTouch);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/system/click", &m_systemClick);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/system/touch", &m_systemTouch);

			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/trackpad/touch", &m_trackpadTouch);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/trigger/click", &m_triggerClick);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/grip/touch", &m_gripTouch);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/thumbstick/click", &m_thumbstickClick);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/thumbstick/touch", &m_thumbstickTouch);


			vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, vr::Prop_Firmware_UpdateAvailable_Bool, false);
			vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, vr::Prop_DeviceProvidesBatteryStatus_Bool, false);
			vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, vr::Prop_DeviceCanPowerOff_Bool, true);
			vr::VRProperties()->SetInt32Property(m_ulPropertyContainer, vr::Prop_DeviceClass_Int32, vr::TrackedDeviceClass_Controller);
			vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, vr::Prop_Firmware_ForceUpdateRequired_Bool, false);
			vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, vr::Prop_Identifiable_Bool, true);
			vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, vr::Prop_Firmware_RemindUpdate_Bool, false);
			vr::VRProperties()->SetInt32Property(m_ulPropertyContainer, vr::Prop_Axis0Type_Int32, vr::k_eControllerAxis_TrackPad);
			vr::VRProperties()->SetInt32Property(m_ulPropertyContainer, vr::Prop_Axis1Type_Int32, vr::k_eControllerAxis_Trigger);
			vr::VRProperties()->SetInt32Property(m_ulPropertyContainer, vr::Prop_Axis2Type_Int32, vr::k_eControllerAxis_Trigger);
			vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, vr::Prop_HasDisplayComponent_Bool, false);
			vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, vr::Prop_HasCameraComponent_Bool, false);
			vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, vr::Prop_HasDriverDirectModeComponent_Bool, false);
			vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, vr::Prop_HasVirtualDisplayComponent_Bool, false);
			vr::VRProperties()->SetInt32Property(m_ulPropertyContainer, vr::Prop_ControllerHandSelectionPriority_Int32, 0);
			vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_ManufacturerName_String, "Valve");
			vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_ResourceRoot_String, "indexcontroller");
			vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_InputProfilePath_String, "{indexcontroller}/input/index_controller_profile.json");
			vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_ControllerType_String, "knuckles");
			//vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_TrackingSystemName_String, "lighthouse");
		} else { 
			// Oculus Touch controller (replaces Vive Wands). Verde_msk. Parameters from OpenVR
			if (sixenceControllerRole == HydraLeftRole) {
				vr::VRProperties()->SetInt32Property(m_ulPropertyContainer, vr::Prop_ControllerRoleHint_Int32, vr::TrackedControllerRole_LeftHand);
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_ModelNumber_String, "Oculus Quest2 (Left Controller)"); //
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_RenderModelName_String, "oculus_quest_pro_controller_left"); // oculus_quest2_controller_left
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_Firmware_ProgrammingTarget_String, "WMHD315M3010GV_Controller_Left"); //
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_RegisteredDeviceType_String, "oculus/WMHD315M3010GV_Controller_Left"); //
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceOff_String, "{oculus}/icons/rifts_left_controller_off.png"); //
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceSearching_String, "{oculus}/icons/rifts_left_controller_searching.gif"); // 
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceSearchingAlert_String, "{oculus}/icons/rifts_left_controller_searching_alert.gif"); // 
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceReady_String, "{oculus}/icons//rifts_left_controller_ready.png"); // 
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceReadyAlert_String, "{oculus}/icons/rifts_left_controller_ready_alert.png"); // 
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceNotReady_String, "{oculus}/icons/rifts_left_controller_error.png"); // 
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceStandby_String, "{oculus}/icons/rifts_left_controller_off.png"); // 
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceAlertLow_String, "{oculus}/icons/rifts_left_controller_ready_low.png"); // 
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_SerialNumber_String, "WMHD315M3010GV_Controller_Left"); // 
				vr::VRDriverInput()->CreateSkeletonComponent(m_ulPropertyContainer, "/input/skeleton/left", "/skeleton/hand/left", "/pose/raw", vr::VRSkeletalTracking_Partial, nullptr, 0U, &m_skeletonHandle); // not needed?
				//vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_TrackingSystemName_String, "oculus"); // not needed?
			}
			else {
				vr::VRProperties()->SetInt32Property(m_ulPropertyContainer, vr::Prop_ControllerRoleHint_Int32, vr::TrackedControllerRole_RightHand);
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_ModelNumber_String, "Oculus Quest2 (Right Controller)"); //
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_RenderModelName_String, "oculus_quest_pro_controller_right"); // oculus_quest2_controller_right
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_Firmware_ProgrammingTarget_String, "WMHD315M3010GV_Controller_Right"); //
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_RegisteredDeviceType_String, "oculus/WMHD315M3010GV_Controller_Right"); // 
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceOff_String, "{oculus}/icons/rifts_right_controller_off.png"); //
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceSearching_String, "{oculus}/icons/rifts_right_controller_searching.gif"); // 
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceSearchingAlert_String, "{oculus}/icons/rifts_right_controller_searching_alert.gif"); // 
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceReady_String, "{oculus}/icons//rifts_right_controller_ready.png"); // 
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceReadyAlert_String, "{oculus}/icons/rifts_right_controller_ready_alert.png"); // 
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceNotReady_String, "{oculus}/icons/rifts_right_controller_error.png"); // 
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceStandby_String, "{oculus}/icons/rifts_right_controller_off.png"); // 
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceAlertLow_String, "{oculus}/icons/rifts_right_controller_ready_low.png"); // 
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_SerialNumber_String, "WMHD315M3010GV_Controller_Right"); // 
				vr::VRDriverInput()->CreateSkeletonComponent(m_ulPropertyContainer, "/input/skeleton/right", "/skeleton/hand/right", "/pose/raw", vr::VRSkeletalTracking_Partial, nullptr, 0U, &m_skeletonHandle); // not needed?
				//vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_TrackingSystemName_String, "oculus"); // not needed?
			}
			vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_ManufacturerName_String, "oculus"); //m_sManufacturerName.c_str()
			vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, vr::Prop_Firmware_UpdateAvailable_Bool, false);
			vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, vr::Prop_DeviceProvidesBatteryStatus_Bool, false);
			vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, vr::Prop_DeviceCanPowerOff_Bool, true);
			vr::VRProperties()->SetInt32Property(m_ulPropertyContainer, vr::Prop_DeviceClass_Int32, vr::TrackedDeviceClass_Controller);
			vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, vr::Prop_Firmware_ForceUpdateRequired_Bool, false);
			vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, vr::Prop_Identifiable_Bool, true);
			vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, vr::Prop_Firmware_RemindUpdate_Bool, false);
			vr::VRProperties()->SetInt32Property(m_ulPropertyContainer, vr::Prop_Axis1Type_Int32, vr::k_eControllerAxis_Trigger);
			vr::VRProperties()->SetInt32Property(m_ulPropertyContainer, vr::Prop_Axis2Type_Int32, vr::k_eControllerAxis_Trigger);
			vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, vr::Prop_HasDisplayComponent_Bool, false);
			vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, vr::Prop_HasCameraComponent_Bool, false);
			vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, vr::Prop_HasDriverDirectModeComponent_Bool, false);
			vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, vr::Prop_HasVirtualDisplayComponent_Bool, false);
			vr::VRProperties()->SetInt32Property(m_ulPropertyContainer, vr::Prop_ControllerHandSelectionPriority_Int32, 0);
			vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_ResourceRoot_String, "oculus");
			vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_ControllerType_String, "oculus_touch");

			// probably not needed
			//vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, Prop_TrackingFirmwareVersion_String, "cd.firmware_revision=" + m_firmware_revision);
			//vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, Prop_HardwareRevision_String, "cd.hardware_revision=" + m_hardware_revision);
			//vr::VRProperties()->SetUint64Property(m_ulPropertyContainer, Prop_FirmwareVersion_Uint64, m_firmware_revision);
			//vr::VRProperties()->SetUint64Property(m_ulPropertyContainer, Prop_HardwareRevision_Uint64, m_hardware_revision);

			//vr::VRProperties()->SetUint64Property(m_ulPropertyContainer, Prop_CurrentUniverseId_Uint64, 2);
			//vr::VRProperties()->SetInt32Property(m_ulPropertyContainer, Prop_DeviceClass_Int32, TrackedDeviceClass_Controller);
			//vr::VRProperties()->SetInt32Property(m_ulPropertyContainer, Prop_Axis0Type_Int32, k_eControllerAxis_TrackPad);
			//vr::VRProperties()->SetInt32Property(m_ulPropertyContainer, Prop_Axis1Type_Int32, k_eControllerAxis_Trigger);
			//vr::VRProperties()->SetBoolProperty(m_ulPropertyContainer, Prop_IsOnDesktop_Bool, false); // avoid "not fullscreen" warnings from vrmonitor
			//vr::VRProperties()->SetInt32Property(m_ulPropertyContainer, Prop_ControllerRoleHint_Int32, TrackedControllerRole_RightHand);

			vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, Prop_InputProfilePath_String, "{oculus}/input/touch_profile.json"); //

			//  Buttons handles
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/a/click", &m_aClick);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/a/touch", &m_aTouch);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/b/click", &m_bClick);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/b/touch", &m_bTouch);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/x/click", &m_xClick);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/x/touch", &m_xTouch);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/y/click", &m_yClick);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/y/touch", &m_yTouch);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/system/click", &m_systemClick);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/system/touch", &m_systemTouch);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/thumbrest/touch", &m_thumbrestTouch);
			vr::VRDriverInput()->CreateScalarComponent(m_ulPropertyContainer, "/input/trigger/value", &m_triggerValue, vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/trigger/touch", &m_triggerTouch);
			vr::VRDriverInput()->CreateScalarComponent(m_ulPropertyContainer, "/input/grip/value", &m_gripValue, vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/grip/touch", &m_gripTouch);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/joystick/click", &m_thumbstickClick);
			vr::VRDriverInput()->CreateBooleanComponent(m_ulPropertyContainer, "/input/joystick/touch", &m_thumbstickTouch);
			vr::VRDriverInput()->CreateScalarComponent(m_ulPropertyContainer, "/input/joystick/x", &m_thumbstickX, vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedTwoSided);
			vr::VRDriverInput()->CreateScalarComponent(m_ulPropertyContainer, "/input/joystick/y", &m_thumbstickY, vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedTwoSided);
			vr::VRDriverInput()->CreateHapticComponent(m_ulPropertyContainer, "/output/haptic", &m_compHaptic);
		}

		// create our haptic component
		//vr::VRDriverInput()->CreateHapticComponent(m_ulPropertyContainer, "/output/haptic", &haptic);

        return VRInitError_None;
    }

    virtual void Deactivate()
    {
        DriverLog("Deactivated device: %s (object id %d)\n", GetSerialNumber().c_str(), m_unObjectId);
        m_unObjectId = vr::k_unTrackedDeviceIndexInvalid;
    }

    virtual void EnterStandby()
    {
    }

    void *GetComponent(const char *pchComponentNameAndVersion)
    {
        // override this to add a component to a driver
        return NULL;
    }

    virtual void PowerOff()
    {
    }

    virtual DriverPose_t GetPose()
    {
        DriverPose_t pose = { 0 };
        pose.poseIsValid = false;
        pose.result = TrackingResult_Calibrating_OutOfRange;
        pose.deviceIsConnected = true;

        pose.qWorldFromDriverRotation = HmdQuaternion_Init(1, 0, 0, 0);
        pose.qDriverFromHeadRotation = HmdQuaternion_Init(1, 0, 0, 0);

        return pose;
    }


    void RunFrame()
    {
#if defined( _WIN32 )
        // Your driver would read whatever hardware state is associated with its input components and pass that
        // in to UpdateBooleanComponent. This could happen in RunFrame or on a thread of your own that's reading USB
        // state. There's no need to update input state unless it changes, but it doesn't do any harm to do so.
#endif
    }

    void ProcessEvent(const vr::VREvent_t & vrEvent)
    {
        switch (vrEvent.eventType)
        {
        case vr::VREvent_Input_HapticVibration:
        {
            if (vrEvent.data.hapticVibration.componentHandle == m_compHaptic)
            {
                // This is where you would send a signal to your hardware to trigger actual haptic feedback
            }
        }
        break;
        }
    }


    std::string GetSerialNumber() const { return m_sSerialNumber; }

    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
private:
    vr::TrackedDeviceIndex_t m_unObjectId;
    vr::PropertyContainerHandle_t m_ulPropertyContainer;
    vr::ETrackedControllerRole m_eControllerRole;

	// Index controller
	vr::VRInputComponentHandle_t m_thumbstickX;
	vr::VRInputComponentHandle_t m_thumbstickY;
	vr::VRInputComponentHandle_t m_triggerValue;
	vr::VRInputComponentHandle_t m_trackpadX;
	vr::VRInputComponentHandle_t m_trackpadY;
	vr::VRInputComponentHandle_t m_trackpadForce;
	vr::VRInputComponentHandle_t m_fingerIndex;
	vr::VRInputComponentHandle_t m_fingerMiddle;
	vr::VRInputComponentHandle_t m_fingerRing;
	vr::VRInputComponentHandle_t m_fingerPinky;
	vr::VRInputComponentHandle_t m_gripForce;
	vr::VRInputComponentHandle_t m_gripValue;

	vr::VRInputComponentHandle_t m_aClick;
	vr::VRInputComponentHandle_t m_aTouch;
	vr::VRInputComponentHandle_t m_bClick;
	vr::VRInputComponentHandle_t m_bTouch;
	vr::VRInputComponentHandle_t m_systemClick;
	vr::VRInputComponentHandle_t m_systemTouch;

	vr::VRInputComponentHandle_t m_trackpadTouch;
	vr::VRInputComponentHandle_t m_triggerClick;
	vr::VRInputComponentHandle_t m_gripTouch;
	vr::VRInputComponentHandle_t m_thumbstickClick;
	vr::VRInputComponentHandle_t m_thumbstickTouch;
	vr::VRInputComponentHandle_t m_skeletonHandle; //??

	// Some more for Oculus Touch (replaces Vive controller)
	vr::VRInputComponentHandle_t m_xClick;
	vr::VRInputComponentHandle_t m_xTouch;
	vr::VRInputComponentHandle_t m_yClick;
	vr::VRInputComponentHandle_t m_yTouch;
	vr::VRInputComponentHandle_t m_thumbrestTouch;
	vr::VRInputComponentHandle_t m_triggerTouch;

	// Etc
	vr::VRInputComponentHandle_t m_compHaptic;

    std::string m_sSerialNumber;
    std::string m_sModelNumber;
    std::string m_sManufacturerName;
    //std::string m_sRenderModel;

    // Which Hydra controller
    int m_nBase;
    int m_nId;

    // Used to deduplicate state data from the sixense driver
    uint8_t m_ucPoseSequenceNumber;

    // To main structures for passing state to vrserver
    vr::DriverPose_t m_Pose;
    vr::VRControllerState_t m_ControllerState;

    // Ancillary tracking state
    //sixenseMath::Vector3 m_WorldFromDriverTranslation; // moved to public
    //sixenseMath::Quat m_WorldFromDriverRotation; // moved to public
    sixenseUtils::Derivatives m_Deriv;
    enum { k_eHemisphereTrackingDisabled, k_eHemisphereTrackingButtonDown, k_eHemisphereTrackingEnabled } m_eHemisphereTrackingState;
    bool m_bCalibrated;

    // Other controller with from the last realignment
    CHydraControllerDriver *m_pAlignmentPartner;

    // Timeout for system button chording
    std::chrono::steady_clock::time_point m_SystemButtonDelay;
    enum { k_eIdle, k_eWaiting, k_eSent, k_ePulsed, k_eBlocked } m_eSystemButtonState;

    // Cached for answering version queries from vrserver
    unsigned short m_firmware_revision;
    unsigned short m_hardware_revision;

    static const float k_fScaleSixenseToMeters;
    static const std::chrono::milliseconds k_SystemButtonChordingDelay;
    static const std::chrono::milliseconds k_SystemButtonPulsingDuration;

    typedef void (vr::IVRServerDriverHost::*ButtonUpdate)(uint32_t unWhichDevice, vr::EVRButtonId eButtonId, double eventTimeOffset);
    //void SendButtonUpdates(ButtonUpdate ButtonEvent, uint64_t ulMask);


    // IMU emulation things
    std::chrono::steady_clock::time_point m_ControllerLastUpdateTime;
    Eigen::Quaternionf m_ControllerLastRotation;
    sixenseMath::Vector3 m_LastVelocity;
    sixenseMath::Vector3 m_LastAcceleration;
    Eigen::Vector3f m_LastAngularVelocity;
    bool m_bHasUpdateHistory;
    bool m_bEnableAngularVelocity;
    bool m_bEnableHoldThumbpad;

    // steamvr.vrsettings config values
	float m_fJoystickDeadzone;
    bool m_bEnableIMUEmulation;
	bool m_bAlternativeImuVersion; //Verde_msk
	bool m_bGripToggle; //Verde_msk
	float m_fThrowMultiplier; //Verde_msk

	int32_t m_nCrouchPressKey;
	int32_t m_nCustomPressKey;
	float m_fCrouchOffset;

	void UpdateControllerState(sixenseControllerData & cd)
	{
		/**
		* Handling button presses for the user switchable features go here,
		* before the driver state is updated.
		*
		* 18/08/18 - Developer mode removed, because we want to forward all
		* button presses to the new input system from now on
		*/
		/*
		// Developer mode
		if (m_bEnableDeveloperMode) {
			// Button 1 toggles imu emulation
			if (cd.buttons & SIXENSE_BUTTON_1) {
				m_bEnableIMUEmulation = !m_bEnableIMUEmulation;
				m_bHasUpdateHistory = false;
			}
			// Button 2 toggles angular velocity emulation
			if (cd.buttons & SIXENSE_BUTTON_2) {
				m_bEnableAngularVelocity = !m_bEnableAngularVelocity;
			}
		}
		*/

		// Update input components
		
		// Crouch
		if (m_bCrouchEnable && sixenceControllerRole == HydraRightRole)
			m_bCrouchPressed = cd.buttons & SIXENSE_BUTTON_3;
		if (m_bCrouchPressed) {
			PosZOffset = m_fCrouchOffset;
			keybd_event(m_nCrouchPressKey, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 0); // Key down
		} else if (PosZOffset != 0) {
			keybd_event(m_nCrouchPressKey, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0); // Key up
			PosZOffset = 0;
		}

		// === Sixense filter on/off. Buttons === Verde_msk
		if (sixenceControllerRole == HydraRightRole && cd.buttons & SIXENSE_BUTTON_1) {
			sixenseSetFilterEnabled(1);
			//DriverLog("Sixense filter ENABLED via button 1\n");
		}
		if (sixenceControllerRole == HydraLeftRole && cd.buttons & SIXENSE_BUTTON_2) {
			sixenseSetFilterEnabled(0);
			//DriverLog("Sixense filter DISABLED via button 2\n");
		}

		// Bool for a toggle grip mode. Verde_msk
		static bool bumperPressedLeft = false;
		static bool gripStateLeft = false;
		static bool bumperPressedRight = false;
		static bool gripStateRight = false;
		bool pressed = (cd.buttons & SIXENSE_BUTTON_BUMPER) != 0;
		//

		// Index controllers 
		if (RecognizeAsIndexCtrls) {

			// System button
			vr::VRDriverInput()->UpdateBooleanComponent(m_systemTouch, cd.buttons & SIXENSE_BUTTON_START, 0);
			vr::VRDriverInput()->UpdateBooleanComponent(m_systemClick, cd.buttons & SIXENSE_BUTTON_START, 0);

			if (sixenceControllerRole == HydraLeftRole) {

				// A button
				vr::VRDriverInput()->UpdateBooleanComponent(m_aTouch, cd.buttons & SIXENSE_BUTTON_1, 0);
				vr::VRDriverInput()->UpdateBooleanComponent(m_aClick, cd.buttons & SIXENSE_BUTTON_1, 0);

				// B button
				vr::VRDriverInput()->UpdateBooleanComponent(m_bTouch, cd.buttons & SIXENSE_BUTTON_3, 0);
				vr::VRDriverInput()->UpdateBooleanComponent(m_bClick, cd.buttons & SIXENSE_BUTTON_3, 0);
			} else { // Right controller

				// A button
				vr::VRDriverInput()->UpdateBooleanComponent(m_aTouch, cd.buttons & SIXENSE_BUTTON_2, 0);
				vr::VRDriverInput()->UpdateBooleanComponent(m_aClick, cd.buttons & SIXENSE_BUTTON_2, 0);

				// B button
				vr::VRDriverInput()->UpdateBooleanComponent(m_bTouch, cd.buttons & SIXENSE_BUTTON_4, 0);
				vr::VRDriverInput()->UpdateBooleanComponent(m_bClick, cd.buttons & SIXENSE_BUTTON_4, 0);
			}
		
			// Joystick axis
			vr::VRDriverInput()->UpdateBooleanComponent(m_thumbstickTouch, false, 0);
			vr::VRDriverInput()->UpdateBooleanComponent(m_trackpadTouch, false, 0);

			float joyStickX = 0, joyStickY = 0;
			if (fabsf(cd.joystick_x) > m_fJoystickDeadzone || fabsf(cd.joystick_y) > m_fJoystickDeadzone)
			{
				joyStickX = cd.joystick_x;
				joyStickY = cd.joystick_y;
				if (IndexStickMode == 0)
					vr::VRDriverInput()->UpdateBooleanComponent(m_thumbstickTouch, true, 0);
				else if (IndexStickMode == 1)
					vr::VRDriverInput()->UpdateBooleanComponent(m_trackpadTouch, true, 0);
				else if (IndexStickMode == 2 || IndexStickMode == 3 || IndexStickMode == 4) {
					vr::VRDriverInput()->UpdateBooleanComponent(m_thumbstickTouch, true, 0);
					vr::VRDriverInput()->UpdateBooleanComponent(m_trackpadTouch, true, 0);
				}
			}

			if (IndexStickMode == 0) {
				vr::VRDriverInput()->UpdateScalarComponent(m_thumbstickX, joyStickX, 0);
				vr::VRDriverInput()->UpdateScalarComponent(m_thumbstickY, joyStickY, 0);
			} else if (IndexStickMode == 1) {
				vr::VRDriverInput()->UpdateScalarComponent(m_trackpadX, joyStickX, 0);
				vr::VRDriverInput()->UpdateScalarComponent(m_trackpadY, joyStickY, 0);
			} else if (IndexStickMode == 2 || IndexStickMode == 3 || IndexStickMode == 4) {
				vr::VRDriverInput()->UpdateScalarComponent(m_thumbstickX, joyStickX, 0);
				vr::VRDriverInput()->UpdateScalarComponent(m_thumbstickY, joyStickY, 0);
				vr::VRDriverInput()->UpdateScalarComponent(m_trackpadX, joyStickX, 0);
				vr::VRDriverInput()->UpdateScalarComponent(m_trackpadY, joyStickY, 0);
			}

			// Joystick button
			vr::VRDriverInput()->UpdateScalarComponent(m_trackpadForce, 0, 0);
			vr::VRDriverInput()->UpdateBooleanComponent(m_thumbstickClick, cd.buttons & SIXENSE_BUTTON_JOYSTICK, 0);
			if (IndexStickMode == 1) { // Touchpad mode
				vr::VRDriverInput()->UpdateBooleanComponent(m_thumbstickClick, false, 0);
				vr::VRDriverInput()->UpdateScalarComponent(m_trackpadForce, (cd.buttons & SIXENSE_BUTTON_JOYSTICK) ? 1.0f : 0, 0);
			} else if (IndexStickMode == 2) { // Touchpad mirror
				vr::VRDriverInput()->UpdateBooleanComponent(m_thumbstickClick, cd.buttons & SIXENSE_BUTTON_JOYSTICK, 0);
				vr::VRDriverInput()->UpdateScalarComponent(m_trackpadForce, (cd.buttons & SIXENSE_BUTTON_JOYSTICK) ? 1.0f : 0, 0);
			} else if (IndexStickMode == 3) { // Inverse dpad left and right on right controller
				if (sixenceControllerRole == HydraLeftRole)
					vr::VRDriverInput()->UpdateScalarComponent(m_trackpadForce, (cd.buttons & SIXENSE_BUTTON_JOYSTICK) ? 1.0f : 0, 0);
				else {
					if (joyStickX != 0 && joyStickY < 0.3f && joyStickY > -0.3f)
						vr::VRDriverInput()->UpdateScalarComponent(m_trackpadForce, (cd.buttons & SIXENSE_BUTTON_JOYSTICK) ? 0 : 1.0f, 0);
					else
						vr::VRDriverInput()->UpdateScalarComponent(m_trackpadForce, (cd.buttons & SIXENSE_BUTTON_JOYSTICK) ? 1.0f : 0, 0);
				}
			} else if (IndexStickMode == 4) { // Always pressed except dpad up & down on second controler
				if (sixenceControllerRole == HydraLeftRole)
					vr::VRDriverInput()->UpdateScalarComponent(m_trackpadForce, (cd.buttons & SIXENSE_BUTTON_JOYSTICK) ? 0 : 1.0f, 0);
				else {
					if (joyStickX != 0 && joyStickY < 0.3f && joyStickY > -0.3f)
						vr::VRDriverInput()->UpdateScalarComponent(m_trackpadForce, (cd.buttons & SIXENSE_BUTTON_JOYSTICK) ? 0 : 1.0f, 0);
					else
						vr::VRDriverInput()->UpdateScalarComponent(m_trackpadForce, (cd.buttons & SIXENSE_BUTTON_JOYSTICK) ? 1.0f : 0, 0);
				}
			}
			else if (IndexStickMode == 5) { // Fully Inverse touchpad press
				vr::VRDriverInput()->UpdateScalarComponent(m_trackpadForce, (cd.buttons & SIXENSE_BUTTON_JOYSTICK) ? 0 : 1.0f, 0);
			}

			// Trigger
			vr::VRDriverInput()->UpdateScalarComponent(m_triggerValue, cd.trigger, 0);
			vr::VRDriverInput()->UpdateBooleanComponent(m_triggerClick, cd.trigger > 0.9f, 0); //?
		
			// Grip / Hydra bumper. Verde_msk
			if (m_bGripToggle) { // Toggle mode
				if (sixenceControllerRole == HydraLeftRole) {
					if (pressed == true && bumperPressedLeft == false) {
						if (gripStateLeft == false) {
							gripStateLeft = true;
							vr::VRDriverInput()->UpdateScalarComponent(m_fingerMiddle, 1.0f, 0);
							vr::VRDriverInput()->UpdateScalarComponent(m_fingerRing, 1.0f, 0);
							vr::VRDriverInput()->UpdateScalarComponent(m_fingerPinky, 1.0f, 0);
							vr::VRDriverInput()->UpdateScalarComponent(m_gripValue, 1.0f, 0);
							vr::VRDriverInput()->UpdateScalarComponent(m_gripForce, 1.0f, 0);
						}
						else {
							gripStateLeft = false;
							vr::VRDriverInput()->UpdateScalarComponent(m_fingerMiddle, 0, 0);
							vr::VRDriverInput()->UpdateScalarComponent(m_fingerRing, 0, 0);
							vr::VRDriverInput()->UpdateScalarComponent(m_fingerPinky, 0, 0);
							vr::VRDriverInput()->UpdateScalarComponent(m_gripValue, 0, 0);
							vr::VRDriverInput()->UpdateScalarComponent(m_gripForce, 0, 0);
						}
						bumperPressedLeft = true;
					}
					if (pressed == false) {
						bumperPressedLeft = false;
					}
				}
				if (sixenceControllerRole == HydraRightRole) {
					if (pressed == true && bumperPressedRight == false) {
						if (gripStateRight == false) {
							gripStateRight = true;
							vr::VRDriverInput()->UpdateScalarComponent(m_fingerMiddle, 1.0f, 0);
							vr::VRDriverInput()->UpdateScalarComponent(m_fingerRing, 1.0f, 0);
							vr::VRDriverInput()->UpdateScalarComponent(m_fingerPinky, 1.0f, 0);
							vr::VRDriverInput()->UpdateScalarComponent(m_gripValue, 1.0f, 0);
							vr::VRDriverInput()->UpdateScalarComponent(m_gripForce, 1.0f, 0);
						}
						else {
							gripStateRight = false;
							vr::VRDriverInput()->UpdateScalarComponent(m_fingerMiddle, 0, 0);
							vr::VRDriverInput()->UpdateScalarComponent(m_fingerRing, 0, 0);
							vr::VRDriverInput()->UpdateScalarComponent(m_fingerPinky, 0, 0);
							vr::VRDriverInput()->UpdateScalarComponent(m_gripValue, 0, 0);
							vr::VRDriverInput()->UpdateScalarComponent(m_gripForce, 0, 0);
						}
						bumperPressedRight = true;
					}
					if (pressed == false) {
						bumperPressedRight = false;
					}
				}
			}

			else { // Hold mode
				if (cd.buttons & SIXENSE_BUTTON_BUMPER) {
					vr::VRDriverInput()->UpdateScalarComponent(m_fingerMiddle, 1.0f, 0);
					vr::VRDriverInput()->UpdateScalarComponent(m_fingerRing, 1.0f, 0);
					vr::VRDriverInput()->UpdateScalarComponent(m_fingerPinky, 1.0f, 0);
					vr::VRDriverInput()->UpdateScalarComponent(m_gripValue, 1.0f, 0);
					vr::VRDriverInput()->UpdateScalarComponent(m_gripForce, 1.0f, 0);
				}
				else {
					vr::VRDriverInput()->UpdateScalarComponent(m_fingerMiddle, 0, 0);
					vr::VRDriverInput()->UpdateScalarComponent(m_fingerRing, 0, 0);
					vr::VRDriverInput()->UpdateScalarComponent(m_fingerPinky, 0, 0);
					vr::VRDriverInput()->UpdateScalarComponent(m_gripValue, 0, 0);
					vr::VRDriverInput()->UpdateScalarComponent(m_gripForce, 0, 0);
				}
			}

			vr::VRDriverInput()->UpdateBooleanComponent(m_fingerIndex, cd.trigger > 0.1f ? 1.0f : 0, 0); //?

			// Custom button "Touchpad click"
			if (sixenceControllerRole == HydraLeftRole)
				m_bIndexCustomKeyPressed = cd.buttons & SIXENSE_BUTTON_4;
			if  (sixenceControllerRole == HydraRightRole && m_bIndexCustomKeyPressed) {

				if (EnabledCustomKey == false) {
					if (m_bCrouchEnable) { // If crouch is off, then we do nothing here
						vr::VRDriverInput()->UpdateScalarComponent(m_trackpadForce, 1.0f, 0);
						vr::VRDriverInput()->UpdateBooleanComponent(m_trackpadTouch, true, 0);
					}
				} else {
					keybd_event(m_nCustomPressKey, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 0); // Key down
					m_bIndexKeyboardKeyPressed = true;
				}

			} else if (m_bIndexKeyboardKeyPressed && EnabledCustomKey) {
				keybd_event(m_nCustomPressKey, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0); // Key up
				m_bIndexKeyboardKeyPressed = false;
			}

			// If crouch is disabled, then the touchpad click is emulated on controllers
			if (!m_bCrouchEnable) {
				if (!EnabledCustomKey && sixenceControllerRole == HydraLeftRole && cd.buttons & SIXENSE_BUTTON_4) {
					vr::VRDriverInput()->UpdateScalarComponent(m_trackpadForce, 1.0f, 0);
					vr::VRDriverInput()->UpdateBooleanComponent(m_trackpadTouch, true, 0);
				}
				if (sixenceControllerRole == HydraRightRole && cd.buttons & SIXENSE_BUTTON_3) {
					vr::VRDriverInput()->UpdateScalarComponent(m_trackpadForce, 1.0f, 0);
					vr::VRDriverInput()->UpdateBooleanComponent(m_trackpadTouch, true, 0);
				}
			}

			//vr::VRDriverInput()->UpdateSkeletonComponent(m_skeletonHandle, vr::VRSkeletalMotionRange_WithController, m_handBones, fingerTracking::NUM_BONES);
			//vr::VRDriverInput()->UpdateSkeletonComponent(m_skeletonHandle, vr::VRSkeletalMotionRange_WithoutController, m_handBones, fingerTracking::NUM_BONES);
		}
		else {
			// Oculus Touch (replaces Vive controller) Verde_msk
			// System button (left only? right works but reserved by steamvr?)
			vr::VRDriverInput()->UpdateBooleanComponent(m_systemTouch, cd.buttons & SIXENSE_BUTTON_START, 0);
			vr::VRDriverInput()->UpdateBooleanComponent(m_systemClick, cd.buttons & SIXENSE_BUTTON_START, 0);

			if (sixenceControllerRole == HydraLeftRole) {

				// X button
				vr::VRDriverInput()->UpdateBooleanComponent(m_xTouch, cd.buttons & SIXENSE_BUTTON_1, 0);
				vr::VRDriverInput()->UpdateBooleanComponent(m_xClick, cd.buttons & SIXENSE_BUTTON_1, 0);

				// Y button
				vr::VRDriverInput()->UpdateBooleanComponent(m_yTouch, cd.buttons & SIXENSE_BUTTON_3, 0);
				vr::VRDriverInput()->UpdateBooleanComponent(m_yClick, cd.buttons & SIXENSE_BUTTON_3, 0);
			}
			else { // Right controller

			 // A button
				vr::VRDriverInput()->UpdateBooleanComponent(m_aTouch, cd.buttons & SIXENSE_BUTTON_2, 0);
				vr::VRDriverInput()->UpdateBooleanComponent(m_aClick, cd.buttons & SIXENSE_BUTTON_2, 0);

				// B button
				vr::VRDriverInput()->UpdateBooleanComponent(m_bTouch, cd.buttons & SIXENSE_BUTTON_4, 0);
				vr::VRDriverInput()->UpdateBooleanComponent(m_bClick, cd.buttons & SIXENSE_BUTTON_4, 0);
			}

			// Joystick axis
			vr::VRDriverInput()->UpdateBooleanComponent(m_thumbstickTouch, false, 0);

			float joyStickX = 0, joyStickY = 0;
			if (fabsf(cd.joystick_x) > m_fJoystickDeadzone || fabsf(cd.joystick_y) > m_fJoystickDeadzone)
			{
				joyStickX = cd.joystick_x;
				joyStickY = cd.joystick_y;
				vr::VRDriverInput()->UpdateBooleanComponent(m_thumbstickTouch, true, 0);
			}

			if (IndexStickMode == 0 || IndexStickMode == 1 || IndexStickMode == 2 || IndexStickMode == 3 || IndexStickMode == 4) {
				vr::VRDriverInput()->UpdateScalarComponent(m_thumbstickX, joyStickX, 0);
				vr::VRDriverInput()->UpdateScalarComponent(m_thumbstickY, joyStickY, 0);
			}

			// Joystick button
			vr::VRDriverInput()->UpdateBooleanComponent(m_thumbstickClick, cd.buttons & SIXENSE_BUTTON_JOYSTICK, 0);

			// Trigger
			vr::VRDriverInput()->UpdateScalarComponent(m_triggerValue, cd.trigger, 0);
			vr::VRDriverInput()->UpdateBooleanComponent(m_triggerTouch, cd.trigger, 0); 

			// Grip / Hydra bumper. Verde_msk
			if (m_bGripToggle) { // Toggle mode
				if (sixenceControllerRole == HydraLeftRole) {
					if (pressed == true && bumperPressedLeft == false) {
						if (gripStateLeft == false) {
							gripStateLeft = true;
							vr::VRDriverInput()->UpdateScalarComponent(m_gripValue, 1.0f, 0);
							vr::VRDriverInput()->UpdateBooleanComponent(m_gripTouch, true, 0);
						}
						else {
							gripStateLeft = false;
							vr::VRDriverInput()->UpdateScalarComponent(m_gripValue, 0, 0);
							vr::VRDriverInput()->UpdateBooleanComponent(m_gripTouch, false, 0);
						}
						bumperPressedLeft = true;
					}
					if (pressed == false) {
						bumperPressedLeft = false;
					}
				}
				if (sixenceControllerRole == HydraRightRole) {
					if (pressed == true && bumperPressedRight == false) {
						if (gripStateRight == false) {
							gripStateRight = true;
							vr::VRDriverInput()->UpdateScalarComponent(m_gripValue, 1.0f, 0);
							vr::VRDriverInput()->UpdateBooleanComponent(m_gripTouch, true, 0);
						}
						else {
							gripStateRight = false;
							vr::VRDriverInput()->UpdateScalarComponent(m_gripValue, 0, 0);
							vr::VRDriverInput()->UpdateBooleanComponent(m_gripTouch, false, 0);
						}
						bumperPressedRight = true;
					}
					if (pressed == false) {
						bumperPressedRight = false;
					}
				}
			}

			else { // Hold mode
				if (cd.buttons & SIXENSE_BUTTON_BUMPER) {
					vr::VRDriverInput()->UpdateScalarComponent(m_gripValue, 1.0f, 0);
					vr::VRDriverInput()->UpdateBooleanComponent(m_gripTouch, true, 0);
				}
				else {
					vr::VRDriverInput()->UpdateScalarComponent(m_gripValue, 0, 0);
					vr::VRDriverInput()->UpdateBooleanComponent(m_gripTouch, false, 0);
				}
			}

			//vr::VRDriverInput()->UpdateBooleanComponent(m_fingerIndex, cd.trigger > 0.1f ? 1.0f : 0, 0); //?
			vr::VRDriverInput()->UpdateBooleanComponent(m_thumbrestTouch, false, 0); //

			// Custom button "Thumbrest Touch"
			if (sixenceControllerRole == HydraLeftRole)
				m_bIndexCustomKeyPressed = cd.buttons & SIXENSE_BUTTON_4;
			if (sixenceControllerRole == HydraLeftRole && m_bIndexCustomKeyPressed) {

				if (EnabledCustomKey == false) {
					if (m_bCrouchEnable) { // If crouch is off, then we do nothing here
						vr::VRDriverInput()->UpdateBooleanComponent(m_thumbrestTouch, true, 0); //
					}
				}
				else {
					keybd_event(m_nCustomPressKey, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 0); // Key down
					m_bIndexKeyboardKeyPressed = true;
				}

			}
			else if (m_bIndexKeyboardKeyPressed && EnabledCustomKey) {
				keybd_event(m_nCustomPressKey, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0); // Key up
				m_bIndexKeyboardKeyPressed = false;
			}

			// If crouch is disabled, then
			if (!m_bCrouchEnable) {
				if (!EnabledCustomKey && sixenceControllerRole == HydraLeftRole && cd.buttons & SIXENSE_BUTTON_4) {
					vr::VRDriverInput()->UpdateBooleanComponent(m_thumbrestTouch, true, 0);
				}
				if (sixenceControllerRole == HydraRightRole && cd.buttons & SIXENSE_BUTTON_3) {
					vr::VRDriverInput()->UpdateBooleanComponent(m_thumbrestTouch, true, 0);
				}
			}

			//vr::VRDriverInput()->UpdateSkeletonComponent(m_skeletonHandle, vr::VRSkeletalMotionRange_WithController, m_handBones, fingerTracking::NUM_BONES);
			//vr::VRDriverInput()->UpdateSkeletonComponent(m_skeletonHandle, vr::VRSkeletalMotionRange_WithoutController, m_handBones, fingerTracking::NUM_BONES);
		}
    }

    void UpdateTrackingState(sixenseControllerData & cd)
    {
        using namespace sixenseMath;

        // This is very hard to know with this driver, but CServerDriver_Hydra::ThreadFunc
        // tries to reduce latency as much as possible.  There is filtering in the Sixense SDK,
        // though, which causes additional unknown latency.  This time is used to know how much
        // extrapolation (via velocity and angular velocity) should be done when predicting poses.
        m_Pose.poseTimeOffset = -0.0166667f; // Verde_msk. Initially -0.016f. Not much of a difference?

        // The "driver" coordinate system is the one that vecPosition is in.  This is whatever
        // coordinates the driver naturally produces for position and orientation.  The "world"
        // coordinate system is the one that is presented to vrserver.  This should include
        // fixing any tilt to the world (caused by a tilted camera, for example) and can include
        // any other useful transformation for the driver (e.g. the driver is tracking from a
        // secondary camera, but uses this transform to move this object into the primary camera
        // coordinate system to be consistent with other objects).
        //
        // This transform is multiplied on the left of the predicted "driver" pose.  That becomes
        // the vr::TrackingUniverseRawAndUncalibrated origin, which is then further offset for
        // floor height and tracking space center by the chaperone system to produce both the
        // vr::TrackingUniverseSeated and vr::TrackingUniverseStanding spaces.
        //
        // In the hydra driver, we use it to unify our coordinate system with the HMD.
        m_Pose.qWorldFromDriverRotation.w = m_WorldFromDriverRotation[3];
        m_Pose.qWorldFromDriverRotation.x = m_WorldFromDriverRotation[0];
        m_Pose.qWorldFromDriverRotation.y = m_WorldFromDriverRotation[1];
        m_Pose.qWorldFromDriverRotation.z = m_WorldFromDriverRotation[2];
        m_Pose.vecWorldFromDriverTranslation[0] = m_WorldFromDriverTranslation[0];
        m_Pose.vecWorldFromDriverTranslation[1] = m_WorldFromDriverTranslation[1];
        m_Pose.vecWorldFromDriverTranslation[2] = m_WorldFromDriverTranslation[2];

        // The "head" coordinate system defines a natural point for the object.  While the "driver"
        // space may be chosen for mechanical, eletrical, or mathematical convenience (e.g. being
        // the location of the IMU), the "head" should be a point meaningful to the user.  For HMDs,
        // it's the point directly between the user's eyes.  The origin of this coordinate system
        // is the origin used for the rendermodel.
        //
        // This transform is multiplied on the right side of the "driver" pose.
        //
        // This transform was inadvertently left at identity for the GDC 2015 controllers, creating
        // a defacto standard "head" position for controllers at the location of the IMU for that
        // particular controller.  We will remedy that later by adding other, explicitly named and
        // chosen spaces.  For now, mimicking that point in this driver lets us run content authored
        // for the HTC Vive Developer Edition controller.  This was done by loading an existing
        // controller rendermodel along side the Hydra model and rotating the Hydra model to roughly
        // align the main features like the handle and trigger.
        m_Pose.qDriverFromHeadRotation.w = 0.945519f;
        m_Pose.qDriverFromHeadRotation.x = 0.325568f;
        m_Pose.qDriverFromHeadRotation.y = 0.0f;
        m_Pose.qDriverFromHeadRotation.z = 0.0f;
        m_Pose.vecDriverFromHeadTranslation[0] = 0.000f;
        m_Pose.vecDriverFromHeadTranslation[1] = 0.06413f;
        m_Pose.vecDriverFromHeadTranslation[2] = -0.08695f;

        // Set position
        Vector3 pos = Vector3(cd.pos) * k_fScaleSixenseToMeters;
        m_Pose.vecPosition[0] = pos[0];
		m_Pose.vecPosition[1] = pos[1] - PosZOffset; //Crouch;
        m_Pose.vecPosition[2] = pos[2];

        // Angular acceleration: the Unity steamVR plugin only provides angular velocity 
        // for the controller, so probably this is not too important.
        m_Pose.vecAngularAcceleration[0] = 0.0f;
        m_Pose.vecAngularAcceleration[1] = 0.0f;
        m_Pose.vecAngularAcceleration[2] = 0.0f;

        // Set rotational coordinates
        m_Pose.qRotation.w = cd.rot_quat[3];
        m_Pose.qRotation.x = cd.rot_quat[0];
        m_Pose.qRotation.y = cd.rot_quat[1];
        m_Pose.qRotation.z = cd.rot_quat[2];

        // Update Sixense Utils data
        m_Deriv.update(&cd);

        if (!m_bEnableIMUEmulation) {
            // The tradeoff here is that setting a valid velocity causes the controllers
            // to jitter, but the controllers feel much more "alive" and lighter.
            // The jitter while stationary is more annoying than the laggy feeling caused
            // by disabling velocity (which effectively disables prediction for rendering).
            // Even the Hydra (without IMU) could probably produce a better velocity here
            // with a different filter on top of the raw position.  Perhaps someone feels
            // like writing one??
			// P.S. This was done in the new IMU version. But does it help much is a question
			
			m_Pose.vecVelocity[0] = 0.0;
			m_Pose.vecVelocity[1] = 0.0;
			m_Pose.vecVelocity[2] = 0.0;

            // True acceleration is highly volatile, so it's not really reasonable to
            // extrapolate much from it anyway.  Passing it as 0 from any driver should
            // be fine.
            m_Pose.vecAcceleration[0] = 0.0;
            m_Pose.vecAcceleration[1] = 0.0;
            m_Pose.vecAcceleration[2] = 0.0;

            // Unmeasured.  XXX with no angular velocity, throwing might not work in some games
			m_Pose.vecAngularVelocity[0] = 0.0;
			m_Pose.vecAngularVelocity[1] = 0.0;
			m_Pose.vecAngularVelocity[2] = 0.0;

        }
        else { // IMU Emulation
			if (m_bAlternativeImuVersion == true) { // New version of IMU. Less jittery with multipliers? Verde_msk
				static Vector3 lastPositionLeft(0, 0, 0);
				static Vector3 lastPositionRight(0, 0, 0);
				static Vector3 smoothedVelocityLeft(0, 0, 0);
				static Vector3 smoothedVelocityRight(0, 0, 0);
				float smoothingFactor = 0.1f;
				//float throwMultiplier = 3.0f; // 

				static Vector3 lastVelocityLeft(0, 0, 0);
				static Vector3 lastVelocityRight(0, 0, 0);
				static Vector3 smoothedAccelerationLeft(0, 0, 0);
				static Vector3 smoothedAccelerationRight(0, 0, 0);

				Vector3 currentPosition = Vector3(cd.pos) * k_fScaleSixenseToMeters;
				Vector3 velocity;
				Vector3 smoothedVelocity;

				Vector3 acceleration;
				Vector3 smoothedAcceleration;

				if (sixenceControllerRole == HydraLeftRole) {
					for (int i = 0; i < 3; ++i) {
						velocity[i] = ((currentPosition[i] - lastPositionLeft[i]) / deltaTime) * m_fThrowMultiplier;
						smoothedVelocityLeft[i] = smoothingFactor * velocity[i] + (1.0f - smoothingFactor) * smoothedVelocityLeft[i];
						smoothedVelocity[i] = smoothedVelocityLeft[i];
						lastPositionLeft[i] = currentPosition[i];
						acceleration[i] = (velocity[i] - lastVelocityLeft[i]) / deltaTime * m_fThrowMultiplier;
						smoothedAccelerationLeft[i] = smoothingFactor * acceleration[i] + (1.0f - smoothingFactor) * smoothedAccelerationLeft[i];
						smoothedAcceleration[i] = smoothedAccelerationLeft[i];
						lastVelocityLeft[i] = velocity[i];
					}
				}
				else if (sixenceControllerRole == HydraRightRole) {
					for (int i = 0; i < 3; ++i) {
						velocity[i] = ((currentPosition[i] - lastPositionRight[i]) / deltaTime) * m_fThrowMultiplier;
						smoothedVelocityRight[i] = smoothingFactor * velocity[i] + (1.0f - smoothingFactor) * smoothedVelocityRight[i];
						smoothedVelocity[i] = smoothedVelocityRight[i];
						lastPositionRight[i] = currentPosition[i];
						acceleration[i] = (velocity[i] - lastVelocityRight[i]) / deltaTime * m_fThrowMultiplier;
						smoothedAccelerationRight[i] = smoothingFactor * acceleration[i] + (1.0f - smoothingFactor) * smoothedAccelerationRight[i];
						smoothedAcceleration[i] = smoothedAccelerationRight[i];
						lastVelocityRight[i] = velocity[i];
					}
				}

				for (int i = 0; i < 3; ++i) {
					m_Pose.vecVelocity[i] = smoothedVelocity[i];
					m_Pose.vecAcceleration[i] = smoothedAcceleration[i];
				}

				// Decided to keep the original angular velocity for now — it works well, no reason to switch to the newer versions (there are problems with them)
				int updatetime_ = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - m_ControllerLastUpdateTime).count();
				Eigen::Quaternionf rotation_ = Eigen::Quaternionf(cd.rot_quat[3], cd.rot_quat[0], cd.rot_quat[1], cd.rot_quat[2]);
				if (m_bHasUpdateHistory) {
					float expFactor_ = .1f; // smoothing factor
					// Calculate angular velocity
					if (m_bEnableAngularVelocity) {

						// the angular velocity's axis of rotation is the difference of the last two quats
						Eigen::Quaternionf diff_ = m_ControllerLastRotation.conjugate() * rotation_;
						Eigen::AngleAxisf angax_ = Eigen::AngleAxisf(diff_);
						Eigen::Vector3f angvel_ = angax_.axis();

						// get angular distance of current rotation from last rotation
						float angdist_ = m_ControllerLastRotation.angularDistance(rotation_);

						// the magnitude of the special angle/axis type vector is the speed of the rotation around the axis in rad/s
						angvel_ = angvel_ * (angdist_ * 1000 * 1000 / updatetime_);

						// add the calculated angular velocity with smoothing applied
						m_Pose.vecAngularVelocity[0] = expFactor_ * angvel_.x() + (1 - expFactor_) * m_LastAngularVelocity.x();
						m_Pose.vecAngularVelocity[1] = expFactor_ * angvel_.y() + (1 - expFactor_) * m_LastAngularVelocity.y();
						m_Pose.vecAngularVelocity[2] = expFactor_ * angvel_.z() + (1 - expFactor_) * m_LastAngularVelocity.z();
						//DriverLog("angvel: ad: %f, x: %f, y: %f, z: %f \n", m_ControllerLastRotation.angularDistance(rotation_), angvel_.x(), angvel_.y(), angvel_.z());

						// refresh history
						for (int i = 0; i < 3; i++)
						{
							m_LastAngularVelocity[i] = m_Pose.vecAngularVelocity[i];
						}
					}

				}
				else {
					m_Pose.vecAngularVelocity[0] = .0f;
					m_Pose.vecAngularVelocity[1] = .0f;
					m_Pose.vecAngularVelocity[2] = .0f;

					m_LastAngularVelocity[0] = .0f;
					m_LastAngularVelocity[1] = .0f;
					m_LastAngularVelocity[2] = .0f;

					m_bHasUpdateHistory = true;
				}

				// Refresh the history
				m_ControllerLastUpdateTime = std::chrono::steady_clock::now();
				m_ControllerLastRotation = rotation_;

				// ==== getEulerAngles ==== 180 problem (wrong angle)
				/*static sixenseMath::Quat lastRotation = sixenseMath::Quat(0, 0, 0, 1);
				sixenseMath::Quat currentRotation = sixenseMath::Quat(cd.rot_quat[0], cd.rot_quat[1], cd.rot_quat[2], cd.rot_quat[3]);

				sixenseMath::Quat deltaRotation = currentRotation * lastRotation.inverse();
				Vector3 angularVelocity = deltaRotation.getEulerAngles() * (M_PI / 180.0f) / deltaTime;

				m_Pose.vecAngularVelocity[0] = angularVelocity[0];
				m_Pose.vecAngularVelocity[1] = angularVelocity[1];
				m_Pose.vecAngularVelocity[2] = angularVelocity[2];*/

				// ==== AngleAxis Quaternion. Unstable at 180 ==== 
				/*static Eigen::Quaternionf lastRotLeft(1, 0, 0, 0); 
				static Eigen::Quaternionf lastRotRight(1, 0, 0, 0);
				static Eigen::Vector3f smoothedAngVelLeft(0, 0, 0);
				static Eigen::Vector3f smoothedAngVelRight(0, 0, 0);
				float angSmoothing = 0.1f;

				Eigen::Quaternionf rotation(cd.rot_quat[3], cd.rot_quat[0], cd.rot_quat[1], cd.rot_quat[2]);
				Eigen::Quaternionf delta;
				Eigen::Quaternionf& lastRotation = (sixenceControllerRole == HydraLeftRole) ? lastRotLeft : lastRotRight;

				delta = lastRotation.conjugate() * rotation;
				Eigen::AngleAxisf axisAngle(delta);
				Eigen::Vector3f angVel = axisAngle.axis() * (axisAngle.angle() / deltaTime);
				lastRotation = rotation;

				Eigen::Vector3f smoothedAngVel;

				if (sixenceControllerRole == HydraLeftRole) {
					smoothedAngVelLeft = angSmoothing * angVel + (1.0f - angSmoothing) * smoothedAngVelLeft;
					smoothedAngVel = smoothedAngVelLeft;
				}
				else if (sixenceControllerRole == HydraRightRole) {
					smoothedAngVelRight = angSmoothing * angVel + (1.0f - angSmoothing) * smoothedAngVelRight;
					smoothedAngVel = smoothedAngVelRight;
				}

				m_Pose.vecAngularVelocity[0] = smoothedAngVel.x();
				m_Pose.vecAngularVelocity[1] = smoothedAngVel.y();
				m_Pose.vecAngularVelocity[2] = smoothedAngVel.z();*/
			}

			else { // Original version of IMU. Multiplier causes more jitter (or not?)
				int updatetime_ = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - m_ControllerLastUpdateTime).count();

				// Get velocity from sixense_utils
				Vector3 vel = m_Deriv.getVelocity() * k_fScaleSixenseToMeters * m_fThrowMultiplier; // Multiplier added. Verde_msk

				// Get acceleration from sixense_utils
				Vector3 acc = m_Deriv.getAcceleration() * k_fScaleSixenseToMeters * m_fThrowMultiplier; // Multiplier added. Verde_msk

				Eigen::Quaternionf rotation_ = Eigen::Quaternionf(cd.rot_quat[3], cd.rot_quat[0], cd.rot_quat[1], cd.rot_quat[2]);

				if (m_bHasUpdateHistory) {

					float expFactor_ = .1f; // smoothing factor

					// add sixense_utils velocity with smoothing
					m_Pose.vecVelocity[0] = expFactor_ * vel[0] + (1 - expFactor_) * m_LastVelocity[0];
					m_Pose.vecVelocity[1] = expFactor_ * vel[1] + (1 - expFactor_) * m_LastVelocity[1];
					m_Pose.vecVelocity[2] = expFactor_ * vel[2] + (1 - expFactor_) * m_LastVelocity[2];
					//DriverLog("Sixense vel: %f, %f, %f \n", vel[0], vel[1], vel[2]);

					// add sixense_utils acceleration with smoothing
					m_Pose.vecAcceleration[0] = expFactor_ * acc[0] + (1 - expFactor_) * m_LastAcceleration[0];
					m_Pose.vecAcceleration[1] = expFactor_ * acc[1] + (1 - expFactor_) * m_LastAcceleration[1];
					m_Pose.vecAcceleration[2] = expFactor_ * acc[2] + (1 - expFactor_) * m_LastAcceleration[2];
					//DriverLog("Sixense acc: %f, %f, %f \n", acc[0], acc[1], acc[2]);

					// Calculate angular velocity
					if (m_bEnableAngularVelocity) {

						// the angular velocity's axis of rotation is the difference of the last two quats
						Eigen::Quaternionf diff_ = m_ControllerLastRotation.conjugate() * rotation_;
						Eigen::AngleAxisf angax_ = Eigen::AngleAxisf(diff_);
						Eigen::Vector3f angvel_ = angax_.axis();

						// get angular distance of current rotation from last rotation
						float angdist_ = m_ControllerLastRotation.angularDistance(rotation_);

						// the magnitude of the special angle/axis type vector is the speed of the rotation around the axis in rad/s
						angvel_ = angvel_ * (angdist_ * 1000 * 1000 / updatetime_);

						// add the calculated angular velocity with smoothing applied
						m_Pose.vecAngularVelocity[0] = expFactor_ * angvel_.x() + (1 - expFactor_) * m_LastAngularVelocity.x();
						m_Pose.vecAngularVelocity[1] = expFactor_ * angvel_.y() + (1 - expFactor_) * m_LastAngularVelocity.y();
						m_Pose.vecAngularVelocity[2] = expFactor_ * angvel_.z() + (1 - expFactor_) * m_LastAngularVelocity.z();
						//DriverLog("angvel: ad: %f, x: %f, y: %f, z: %f \n", m_ControllerLastRotation.angularDistance(rotation_), angvel_.x(), angvel_.y(), angvel_.z());

						// refresh history
						for (int i = 0; i < 3; i++)
						{
							m_LastAngularVelocity[i] = m_Pose.vecAngularVelocity[i];
						}
					}

				}
				else {
					m_Pose.vecAcceleration[0] = .0f;
					m_Pose.vecAcceleration[1] = .0f;
					m_Pose.vecAcceleration[2] = .0f;

					m_Pose.vecVelocity[0] = .0f;
					m_Pose.vecVelocity[1] = .0f;
					m_Pose.vecVelocity[2] = .0f;

					m_Pose.vecAngularVelocity[0] = .0f;
					m_Pose.vecAngularVelocity[1] = .0f;
					m_Pose.vecAngularVelocity[2] = .0f;

					m_LastAngularVelocity[0] = .0f;
					m_LastAngularVelocity[1] = .0f;
					m_LastAngularVelocity[2] = .0f;

					m_bHasUpdateHistory = true;
				}


				// Refresh the history
				m_ControllerLastUpdateTime = std::chrono::steady_clock::now();
				m_ControllerLastRotation = rotation_;
				for (int i = 0; i < 3; i++)
				{
					m_LastVelocity[i] = m_Pose.vecVelocity[i];
					m_LastAcceleration[i] = m_Pose.vecAcceleration[i];
				}
			}
        }

        // Don't show user any controllers until they have hemisphere tracking and
        // do the calibration gesture.  hydra_monitor should be prompting with an overlay
        if (m_eHemisphereTrackingState != k_eHemisphereTrackingEnabled)
            m_Pose.result = vr::TrackingResult_Uninitialized;
        else if (!m_bCalibrated)
            m_Pose.result = vr::TrackingResult_Calibrating_InProgress;
        else
            m_Pose.result = vr::TrackingResult_Running_OK;

        m_Pose.poseIsValid = m_bCalibrated;
        m_Pose.deviceIsConnected = true;

        // These should always be false from any modern driver.  These are for Oculus DK1-like
        // rotation-only tracking.  Support for that has likely rotted in vrserver.
        m_Pose.willDriftInYaw = false;
        m_Pose.shouldApplyHeadModel = false;

        // This call posts this pose to shared memory, where all clients will have access to it the next
        // moment they want to predict a pose.
        vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_unObjectId, m_Pose, sizeof(DriverPose_t));
    }

    void DelaySystemButtonForChording(sixenseControllerData & cd)
    {
        // Delay sending system button to vrserver while we see if it is being
        // chorded with the other system button to reset the coordinate system
        if (cd.buttons & SIXENSE_BUTTON_START)
        {
            switch (m_eSystemButtonState)
            {
            case k_eIdle:
                m_eSystemButtonState = k_eWaiting;
                m_SystemButtonDelay = std::chrono::steady_clock::now() + k_SystemButtonChordingDelay;
                cd.buttons &= ~SIXENSE_BUTTON_START;
                break;

            case k_eWaiting:
                if (std::chrono::steady_clock::now() >= m_SystemButtonDelay)
                {
                    m_eSystemButtonState = k_eSent;
                    // leave button state set, will reach vrserver
                }
                else
                {
                    cd.buttons &= ~SIXENSE_BUTTON_START;
                }
                break;

            case k_eSent:
                // still held down, nothing to do
                break;

            case k_ePulsed:
                // user re-pressed within 1 frame, just ignore lift
                m_eSystemButtonState = k_eSent;
                break;

            case k_eBlocked:
                // was consumed by chording gesture -- never send until released
                cd.buttons &= ~SIXENSE_BUTTON_START;
                break;
            }
        }
        else
        {
            switch (m_eSystemButtonState)
            {
            case k_eIdle:
            case k_eSent:
            case k_eBlocked:
                m_eSystemButtonState = k_eIdle;
                break;

            case k_eWaiting:
                // user pressed and released the button within the timeout, so
                // send a quick pulse to the application
                m_eSystemButtonState = k_ePulsed;
                m_SystemButtonDelay = std::chrono::steady_clock::now() + k_SystemButtonPulsingDuration;
                cd.buttons |= SIXENSE_BUTTON_START;
                break;

            case k_ePulsed:
                // stretch fake pulse so client sees it
                if (std::chrono::steady_clock::now() >= m_SystemButtonDelay)
                {
                    m_eSystemButtonState = k_eIdle;
                }
                cd.buttons |= SIXENSE_BUTTON_START;
                break;
            }
        }
    }

    // Initially block all button presses, stealing the first one to mean
    // that the controller is pointing at the base and we should tell the Sixense SDK
    bool WaitingForHemisphereTracking(sixenseControllerData & cd)
    {
        switch (m_eHemisphereTrackingState)
        {
        case k_eHemisphereTrackingDisabled:
            if (cd.buttons || cd.trigger > 0.8f)
            {
                // First button press
                m_eHemisphereTrackingState = k_eHemisphereTrackingButtonDown;
            }
            return true;

        case k_eHemisphereTrackingButtonDown:
            if (!cd.buttons && cd.trigger < 0.1f)
            {
                // Buttons released (so they won't leak into application), go!
                sixenseAutoEnableHemisphereTracking(m_nId);
                m_eHemisphereTrackingState = k_eHemisphereTrackingEnabled;
            }
            return true;

        case k_eHemisphereTrackingEnabled:
        default:
            return false;
        }
    }

};

const std::chrono::milliseconds CHydraControllerDriver::k_SystemButtonChordingDelay(150);
const std::chrono::milliseconds CHydraControllerDriver::k_SystemButtonPulsingDuration(100);
const float CHydraControllerDriver::k_fScaleSixenseToMeters = 0.001;  // sixense driver in mm


//-----------------------------------------------------------------------------
// Purpose: IServerTrackedDeviceProvider
//-----------------------------------------------------------------------------
void CServerDriver_Hydra::LaunchHydraMonitor()
{
    LaunchHydraMonitor(m_sDriverInstallDir.c_str());
}

// The hydra_monitor is a companion program which can display overlay prompts for us
// and tell us the pose of the HMD at the moment we want to calibrate.
void CServerDriver_Hydra::LaunchHydraMonitor(const char * pchDriverInstallDir)
{
    if (m_bLaunchedHydraMonitor)
        return;

    m_bLaunchedHydraMonitor = true;

    std::ostringstream ss;

    ss << pchDriverInstallDir << "\\bin\\";
#if defined( _WIN64 )
    ss << "win64";
#elif defined( _WIN32 )
    ss << "win32";
#else
#error Do not know how to launch hydra_monitor
#endif
    DriverLog("hydra_monitor path: %s\n", ss.str().c_str());

#if defined( _WIN32 )
    STARTUPINFOA sInfoProcess = { 0 };
    sInfoProcess.cb = sizeof(STARTUPINFOW);
    PROCESS_INFORMATION pInfoStartedProcess;
    BOOL okay = CreateProcessA((ss.str() + "\\hydra_monitor.exe").c_str(), NULL, NULL, NULL, FALSE, 0, NULL, ss.str().c_str(), &sInfoProcess, &pInfoStartedProcess);
    DriverLog("start hydra_monitor okay: %d %08x\n", okay, GetLastError());
#else
#error Do not know how to launch hydra_monitor
#endif
}


CServerDriver_Hydra::CServerDriver_Hydra():
    m_Thread(NULL), // initialize m_Thread
    m_bStopRequested(false),
    m_bLaunchedHydraMonitor(false)
{
}

CServerDriver_Hydra::~CServerDriver_Hydra()
{
    // 10/10/2015 benj:  vrserver is exiting without calling Cleanup() to balance Init()
    // causing std::thread to call std::terminate
    Cleanup();
}

EVRInitError CServerDriver_Hydra::Init(vr::IVRDriverContext *pDriverContext)
{
    VR_INIT_SERVER_DRIVER_CONTEXT(pDriverContext);
    InitDriverLog(vr::VRDriverLog());

    if (sixenseInit() != SIXENSE_SUCCESS)
        return vr::VRInitError_Driver_Failed;

	// base station
	vr::EVRSettingsError eError = vr::VRSettingsError_None;
	bShowBaseStation = vr::VRSettings()->GetBool(
		k_pch_Hydra_Section,
		k_pch_Hydra_ShowBaseStation_Bool,
		&eError
	);

	if (eError != vr::VRSettingsError_None) {
		bShowBaseStation = false; //
		DriverLog("Hydra: ShowBaseStation not found or invalid, defaulting to false\n");
	}
	else {
		DriverLog("Hydra: ShowBaseStation from config = %s\n", bShowBaseStation ? "true" : "false");
	}
	// base station

	// Turn the internal position and orientation Sixense filtering on or off. Verde_msk
	bool bSixenseFilterEnabled = vr::VRSettings()->GetBool(k_pch_Hydra_Section, k_pch_Hydra_SixenseFilterEnabled_Bool, false);
	if (bSixenseFilterEnabled) {
		sixenseSetFilterEnabled(1);
		DriverLog("Sixense filter enabled from settings\n");
	}
	else {
		sixenseSetFilterEnabled(0);
		DriverLog("Sixense filter disabled from settings\n");
	}
	
	// Dynamic Filter User Setting. Verde_msk
	m_fDynamicFilterPower = vr::VRSettings()->GetFloat(k_pch_Hydra_Section, k_pch_Hydra_DynamicFilterPower_Float);
	m_fMinFilteringVal = vr::VRSettings()->GetFloat(k_pch_Hydra_Section, k_pch_Hydra_MinFilteringVal_Float);
	m_fMaxFilteringVal = vr::VRSettings()->GetFloat(k_pch_Hydra_Section, k_pch_Hydra_MaxFilteringVal_Float);

	// Set the parameters that control the position and orientation filtering level. near_range, near_val, far_range, far_val. range broken? Verde_msk
	/*float filterNearRange = 5.0f;
	float filterNearVal = 0.8f;
	float filterFarRange = 600.0f;
	float filterFarVal = 0.98f;
	sixenseSetFilterParams(filterNearRange, filterNearVal, filterFarRange, filterFarVal);
	DriverLog("Sixense filter parameters set\n");*/

    // Getting driver install dir from resource path
    char buf[1024];
    VRResources()->GetResourceFullPath("{hydra}", "", buf, sizeof(buf));
    m_sDriverInstallDir = buf;
    m_sDriverInstallDir += "..";

    // Will not immediately detect controllers at this point.  Sixense driver must be initializing
    // in its own thread...  It's okay to dynamically detect devices later, but if controllers are
    // the only devices (e.g. requireHmd=false) we must have GetTrackedDeviceCount() != 0 before returning.
    for (int i = 0; i < 20; ++i)
    {
        ScanForNewControllers(false);
        if (GetTrackedDeviceCount())
            break;
        Sleep(100);
    }

    m_Thread = new std::thread(ThreadEntry, this);   // use new operator now

    return VRInitError_None;
}

void CServerDriver_Hydra::ThreadEntry(CServerDriver_Hydra * pDriver)
{
    pDriver->ThreadFunc();
}

void CServerDriver_Hydra::ThreadFunc()
{
    // We know the sixense SDK thread is running at "60 FPS", but we don't know when
    // those frames are.  To minimize latency, we sleep for slightly less than the
    // target rate, and detect when the frame has not advanced to wait a bit longer.
    auto longInterval = std::chrono::microseconds(16667); // Verde_msk. Initially milliseconds(16). Not much of a difference?
    auto retryInterval = std::chrono::milliseconds(2);
    auto scanInterval = std::chrono::seconds(1);
    auto pollDeadline = std::chrono::steady_clock::now();
    auto scanDeadline = std::chrono::steady_clock::now() + scanInterval;

#ifdef _WIN32
    // Request at least 2ms timing granularity for the life of this process
    timeBeginPeriod(2);
#endif

    while (!m_bStopRequested)
    {
        // Check for new controllers here because sixense API is modal
        // (e.g. sixenseSetActiveBase()) so it can't happen in parallel with pose updates
        if (pollDeadline > scanDeadline)
        {
            ScanForNewControllers(true);
            scanDeadline += scanInterval;
        }

        bool bAnyActivated = false;
        bool bAllUpdated = true;
        for (int base = 0; base < sixenseGetMaxBases(); ++base)
        {
            if (!sixenseIsBaseConnected(base))
                continue;

            sixenseAllControllerData acd;

            sixenseSetActiveBase(base);
            if (sixenseGetAllNewestData(&acd) != SIXENSE_SUCCESS)
                continue;
            for (int id = 0; id < sixenseGetMaxControllers(); ++id)
            {
                for (auto it = m_vecControllers.begin(); it != m_vecControllers.end(); ++it)
                {
                    CHydraControllerDriver *pHydra = *it;
                    if (pHydra->IsActivated() && pHydra->HasControllerId(base, id))
                    {
                        bAnyActivated = true;
                        // Returns true if this is new data (so we can sleep for long interval)
                        if (!pHydra->Update(acd.controllers[id]))
                        {
                            bAllUpdated = false;
                        }
                        break;
                    }
                }
            }
        }

        CheckForChordedSystemButtons();

        // If everyone just got new data, we can wait about 1/60s, else try again soon
        pollDeadline += !bAnyActivated ? scanInterval :
            bAllUpdated ? longInterval : retryInterval;
        std::this_thread::sleep_until(pollDeadline);
    }

#ifdef _WIN32
    timeEndPeriod(2);
#endif
}

void CServerDriver_Hydra::CheckForChordedSystemButtons()
{
    std::vector<CHydraControllerDriver *> vecHeldSystemButtons;

    for (auto it = m_vecControllers.begin(); it != m_vecControllers.end(); ++it)
    {
        CHydraControllerDriver *pHydra = *it;

        if (pHydra->IsHoldingSystemButton())
        {
            vecHeldSystemButtons.push_back(pHydra);
        }
    }
    // If two or more system buttons are pressed together, treat them as a chord
    // requesting a realignment of the coordinate system
    if (vecHeldSystemButtons.size() >= 2)
    {
        if (vecHeldSystemButtons.size() == 2)
        {
            CHydraControllerDriver::RealignCoordinates(vecHeldSystemButtons[0], vecHeldSystemButtons[1]);
        }

        for (auto it = vecHeldSystemButtons.begin(); it != vecHeldSystemButtons.end(); ++it)
        {
            (*it)->ConsumeSystemButtonPress();
        }
    }
}


uint32_t CServerDriver_Hydra::GetTrackedDeviceCount()
{
    scope_lock lock(m_Mutex);

    return m_vecControllers.size();
}

CHydraControllerDriver * CServerDriver_Hydra::FindTrackedDeviceDriver(const char * pchId)
{
    scope_lock lock(m_Mutex);

    for (auto it = m_vecControllers.begin(); it != m_vecControllers.end(); ++it)
    {
        if (0 == strcmp((*it)->GetSerialNumber().c_str(), pchId))
        {
            return *it;
        }
    }
    return nullptr;
}

void CServerDriver_Hydra::ScanForNewControllers(bool bNotifyServer)
{
    for (int base = 0; base < sixenseGetMaxBases(); ++base)
    {
        if (sixenseIsBaseConnected(base))
        {
            sixenseSetActiveBase(base);
            for (int i = 0; i < sixenseGetMaxControllers(); ++i)
            {
                if (sixenseIsControllerEnabled(i))
                {
                    char buf[256];
                    GenerateSerialNumber(buf, sizeof(buf), base, i);
                    scope_lock lock(m_Mutex);

                    CHydraControllerDriver * hydra = FindTrackedDeviceDriver(buf);
                    if (!hydra)
                    {
                        DriverLog("Enumerated device: %s\n", buf);
                        hydra = new CHydraControllerDriver(base, i);
						hydra->sixenceControllerRole = i; // initial role
                        m_vecControllers.push_back(hydra);
                    }

                    if (bNotifyServer && !hydra->IsActivated())
                    {
                        DriverLog("Activating device: %s\n", buf);
                        vr::VRServerDriverHost()->TrackedDeviceAdded(hydra->GetSerialNumber().c_str(), vr::TrackedDeviceClass_Controller, hydra);
                    }
                }
            }
        }
    }
	// base station
	// === Hydra Base Station dynamic add — after both controllers are added ===
	if (bShowBaseStation == true) {
		if (!g_bHydraTrackerAdded && m_vecControllers.size() >= 2)
		{
			sixenseAllControllerData acd;
			if (sixenseGetAllNewestData(&acd) == SIXENSE_SUCCESS)
			{
				sixenseMath::Vector3 pos0(
					acd.controllers[0].pos[0] * 0.001f,
					acd.controllers[0].pos[1] * 0.001f,
					acd.controllers[0].pos[2] * 0.001f
				);
				sixenseMath::Vector3 pos1(
					acd.controllers[1].pos[0] * 0.001f,
					acd.controllers[1].pos[1] * 0.001f,
					acd.controllers[1].pos[2] * 0.001f
				);

				sixenseMath::Vector3 avg = (pos0 + pos1) * 0.5f;
				//avg[1] -= 0.0f; // 0.15

				g_vecBaseEstimate.v[0] = avg[0];
				g_vecBaseEstimate.v[1] = avg[1];
				g_vecBaseEstimate.v[2] = avg[2];

				g_pHydraTracker = new CHydraTracker();
				g_hydraTrackerIndex = vr::VRServerDriverHost()->TrackedDeviceAdded("hydra/tracker", vr::TrackedDeviceClass_GenericTracker, g_pHydraTracker);

				if (g_hydraTrackerIndex != vr::k_unTrackedDeviceIndexInvalid)
				{
					g_bHydraTrackerAdded = true;
					DriverLog("Hydra: Base station added in ScanForNewControllers() at (%.3f, %.3f, %.3f)\n",
						avg[0], avg[1], avg[2]);
				}
				else
				{
					DriverLog("Hydra: Failed to add base station in ScanForNewControllers()\n");
				}
			}
		}
	}
	// base station
}

void CServerDriver_Hydra::Cleanup()
{
    CleanupDriverLog();

    if (m_Thread && m_Thread->joinable())  // test for NULL m_Thread
    {
        m_bStopRequested = true;
        m_Thread->join();
        m_Thread = NULL;  // force to NULL after thread join()
        sixenseExit();
    }

    for (auto it = m_vecControllers.begin(); it != m_vecControllers.end(); ++it)
    {
        delete *it;
        *it = NULL;
    }
}

// base station
bool CServerDriver_Hydra::IsAnyControllerCalibrated() {
	for (auto it = m_vecControllers.begin(); it != m_vecControllers.end(); ++it) {
		if ((*it)->IsCalibrated()) {
			return true;
		}
	}
	return false;
}
// base station

void CServerDriver_Hydra::RunFrame()
{
	// --- DYNAMIC FILTER (global, based on fastest controller) --- Verde_msk
	sixenseControllerData cdLeft, cdRight;
	sixenseGetNewestData(0, &cdLeft);
	sixenseGetNewestData(1, &cdRight);

	static sixenseMath::Vector3 prevPosLeft(0.0f, 0.0f, 0.0f);
	static sixenseMath::Vector3 prevPosRight(0.0f, 0.0f, 0.0f);

	// scale to meters
	sixenseMath::Vector3 posLeft(
		cdLeft.pos[0] * 0.001, // 0.001 - ScaleSixenseToMeters
		cdLeft.pos[1] * 0.001,
		cdLeft.pos[2] * 0.001
	);

	sixenseMath::Vector3 posRight(
		cdRight.pos[0] * 0.001,
		cdRight.pos[1] * 0.001,
		cdRight.pos[2] * 0.001
	);

	// speed calculation
	sixenseMath::Vector3 velLeft = (posLeft - prevPosLeft) / deltaTime;
	sixenseMath::Vector3 velRight = (posRight - prevPosRight) / deltaTime;

	// save position
	prevPosLeft = posLeft;
	prevPosRight = posRight;

	// speed module
	float speedLeft = velLeft.length();
	float speedRight = velRight.length();

	// choose the fastest one
	float speed = (std::max)(speedLeft, speedRight);
	float distance = (std::max)(posLeft.length(), posRight.length());
	float distanceFactor = (std::min)(1.0f, distance / MaxDist);
	float scaledMaxSpeed = closeMaxSpeed + (m_fDynamicFilterPower - closeMaxSpeed) * distanceFactor;
	float speedNorm = (std::min)(1.0f, speed / scaledMaxSpeed);

	// Logarithmic attenuation of the filter
	float dynamicFarVal = m_fMaxFilteringVal - logf(speedNorm * 9 + 1.0f) / logf(10.0f) * (m_fMaxFilteringVal - m_fMinFilteringVal);

	// Filter
	sixenseSetFilterParams(0.1f, 0.1f, 1.0f, dynamicFarVal);
	// --- DYNAMIC FILTER (global, based on fastest controller) --- Verde_msk

    for (auto it = m_vecControllers.begin(); it != m_vecControllers.end(); ++it)
    {
        (*it)->RunFrame();
    }

    vr::VREvent_t vrEvent;
	while (vr::VRServerDriverHost()->PollNextEvent(&vrEvent, sizeof(vrEvent)))
	{
		for (auto it = m_vecControllers.begin(); it != m_vecControllers.end(); ++it)
		{
			(*it)->ProcessEvent(vrEvent);
		}
	}

	// base station
	if (bShowBaseStation == true) {
		if (g_pHydraTracker && IsAnyControllerCalibrated()) {
			vr::DriverPose_t trackerPose = g_pHydraTracker->GetPose();
			vr::VRServerDriverHost()->TrackedDevicePoseUpdated(g_hydraTrackerIndex, trackerPose, sizeof(vr::DriverPose_t));
			//DriverLog("hydra: Force-updated pose for base station in RunFrame()");
		}
	}
	// position after calibration
	if (g_bHydraTrackerAdded && IsAnyControllerCalibrated()) {
		for (auto it = m_vecControllers.begin(); it != m_vecControllers.end(); ++it) {
			CHydraControllerDriver* pCtrl = *it;

			if (!pCtrl->IsCalibrated())
				continue;

			sixenseControllerData cd;
			sixenseGetNewestData(pCtrl->sixenceControllerRole, &cd);
			float* raw_pos = cd.pos;

			sixenseMath::Vector3 controllerWorldPos = pCtrl->m_WorldFromDriverTranslation;

			g_vecBaseEstimate.v[0] = controllerWorldPos[0] - raw_pos[0] * 0.001f;
			g_vecBaseEstimate.v[1] = controllerWorldPos[1] - raw_pos[1] * 0.001f;
			g_vecBaseEstimate.v[2] = controllerWorldPos[2] - raw_pos[2] * 0.001f;

			break; //
		}
	}
	// base station
}

