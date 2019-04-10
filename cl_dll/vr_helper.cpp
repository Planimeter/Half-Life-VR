
#include <windows.h>
#include "Matrices.h"
#include "hud.h"
#include "cl_util.h"
#include "r_studioint.h"
#include "ref_params.h"
#include "vr_helper.h"
#include "vr_gl.h"
#include "vr_input.h"
#include <string>
#include <algorithm>

#ifndef DUCK_SIZE
#define DUCK_SIZE 36
#endif

#ifndef MAX_COMMAND_SIZE
#define MAX_COMMAND_SIZE 256
#endif

const Vector3 HL_TO_VR(1.44f / 10.f, 2.0f / 10.f, 1.44f / 10.f);
const Vector3 VR_TO_HL(1.f / HL_TO_VR.x, 1.f / HL_TO_VR.y, 1.f / HL_TO_VR.z);

// Set by message from server on load/restore/levelchange
float g_vrRestoreYaw_PrevYaw = 0.f;
float g_vrRestoreYaw_CurrentYaw = 0.f;
bool g_vrRestoreYaw_HasData = false;

// Set by message from server on spawn
float g_vrSpawnYaw = 0.f;
bool g_vrSpawnYaw_HasData = false;

extern engine_studio_api_t IEngineStudio;

VRHelper::VRHelper()
{
}

VRHelper::~VRHelper()
{
}

#define MAGIC_HL_TO_VR_FACTOR 3.75f
#define MAGIC_VR_TO_HL_FACTOR 10.f

void VRHelper::UpdateVRHLConversionVectors()
{
	float worldScale = CVAR_GET_FLOAT("vr_world_scale");
	float worldZStretch = CVAR_GET_FLOAT("vr_world_z_strech");

	if (worldScale < 0.1f)
	{
		worldScale = 0.1f;
	}
	else if (worldScale > 100.f)
	{
		worldScale = 100.f;
	}

	if (worldZStretch < 0.1f)
	{
		worldZStretch = 0.1f;
	}
	else if (worldZStretch > 100.f)
	{
		worldZStretch = 100.f;
	}

	hlToVR.x = worldScale * 1.f / MAGIC_HL_TO_VR_FACTOR;
	hlToVR.y = worldScale * 1.f / MAGIC_HL_TO_VR_FACTOR * worldZStretch;
	hlToVR.z = worldScale * 1.f / MAGIC_HL_TO_VR_FACTOR;

	vrToHL.x = MAGIC_VR_TO_HL_FACTOR * 1.f / hlToVR.x;
	vrToHL.y = MAGIC_VR_TO_HL_FACTOR * 1.f / hlToVR.y;
	vrToHL.z = MAGIC_VR_TO_HL_FACTOR * 1.f / hlToVR.z;
}

void VRHelper::UpdateWorldRotation()
{
	// Is rotating disabled?
	if (CVAR_GET_FLOAT("vr_playerturn_enabled") == 0.f)
	{
		m_lastYawUpdateTime = -1;
		m_prevYaw = 0.f;
		m_currentYaw = 0.f;
		m_currentYawOffsetDelta = Vector{};
		return;
	}

	if (m_lastYawUpdateTime == gHUD.m_flTime)
	{
		// already up-to-date
		return;
	}

	// New game
	if (m_lastYawUpdateTime == -1.f || m_lastYawUpdateTime > gHUD.m_flTime)
	{
		m_lastYawUpdateTime = gHUD.m_flTime;
		m_prevYaw = 0.f;
		m_currentYaw = 0.f;
		m_currentYawOffsetDelta = Vector{};
		return;
	}

	// Get angle from save/restore or from user input
	if (g_vrRestoreYaw_HasData)
	{
		m_prevYaw = g_vrRestoreYaw_PrevYaw;
		m_currentYaw = g_vrRestoreYaw_CurrentYaw;
		g_vrRestoreYaw_PrevYaw = 0.f;
		g_vrRestoreYaw_CurrentYaw = 0.f;
		g_vrRestoreYaw_HasData = false;

		// Normalize angles
		m_prevYaw = std::fmodf(m_prevYaw, 360.f);
		if (m_prevYaw < 0.f) m_prevYaw += 360.f;
		m_currentYaw = std::fmodf(m_currentYaw, 360.f);
		if (m_currentYaw < 0.f) m_currentYaw += 360.f;
	}
	else
	{
		if (g_vrSpawnYaw_HasData)
		{
			// Fix spawn yaw
			Vector currentViewAngles;
			GetViewAngles(vr::EVREye::Eye_Left, currentViewAngles);
			m_currentYaw += g_vrSpawnYaw - currentViewAngles.y;
			g_vrSpawnYaw = 0.f;
			g_vrSpawnYaw_HasData = false;
		}

		// Already up to date
		if (gHUD.m_flTime == m_lastYawUpdateTime)
		{
			return;
		}

		// Get time since last update
		float deltaTime = gHUD.m_flTime - m_lastYawUpdateTime;
		// Rotate
		m_prevYaw = m_currentYaw;
		if (g_vrInput.RotateLeft())
		{
			m_currentYaw += deltaTime * CVAR_GET_FLOAT("cl_yawspeed");
		}
		else if (g_vrInput.RotateRight())
		{
			m_currentYaw -= deltaTime * CVAR_GET_FLOAT("cl_yawspeed");
		}

		// Rotate with trains and platforms
		cl_entity_t *groundEntity = gHUD.GetGroundEntity();
		if (groundEntity)
		{
			if (groundEntity != m_groundEntity)
			{
				m_groundEntity = groundEntity;
				m_hasGroundEntityYaw = false;
			}
			if (CVAR_GET_FLOAT("vr_rotate_with_trains") != 0.f)
			{
				if (m_hasGroundEntityYaw)
				{
					m_currentYaw += groundEntity->angles.y - m_groundEntityYaw;
				}
			}
			m_groundEntityYaw = groundEntity->angles.y;
			m_hasGroundEntityYaw = true;
		}
		else
		{
			m_groundEntity = nullptr;
			m_groundEntityYaw = 0.f;
			m_hasGroundEntityYaw = false;
		}
		//g_vrInput.MoveWithGroundEntity(m_groundEntity);

		// Normalize angle
		m_currentYaw = std::fmodf(m_currentYaw, 360.f);
		if (m_currentYaw < 0.f) m_currentYaw += 360.f;

		// Remember time
		m_lastYawUpdateTime = gHUD.m_flTime;
	}
}

const Vector3 & VRHelper::GetVRToHL()
{
	return vrToHL;
}

const Vector3 & VRHelper::GetHLToVR()
{
	return hlToVR;
}

void VRHelper::Init()
{
	if (!AcceptsDisclaimer())
	{
		Exit();
	}
	else if (!IEngineStudio.IsHardware())
	{
		Exit(TEXT("Software mode not supported. Please start this game with OpenGL renderer. (Start Half-Life, open the Options menu, select Video, chose OpenGL as Renderer, save, quit Half-Life, then start Half-Life: VR)"));
	}
	else if (!InitAdditionalGLFunctions())
	{
		Exit(TEXT("Failed to load necessary OpenGL functions. Make sure you have a graphics card that can handle VR and up-to-date drivers, and make sure you are running the game in OpenGL mode."));
	}
	else if (!InitGLMatrixOverrideFunctions())
	{
		Exit(TEXT("Failed to load custom opengl32.dll. Make sure you launch this game through HLVRLauncher.exe, not the Steam menu. (Tipp: You can add a custom game shortcut to HLVRLauncher.exe in your Steam library.))"));
	}
	else
	{
		vr::EVRInitError vrInitError;
		vrSystem = vr::VR_Init(&vrInitError, vr::EVRApplicationType::VRApplication_Scene);
		vrCompositor = vr::VRCompositor();
		if (vrInitError != vr::EVRInitError::VRInitError_None || vrSystem == nullptr || vrCompositor == nullptr)
		{
			Exit(TEXT("Failed to initialize VR enviroment. Make sure your headset is properly connected and SteamVR is running."));
		}
		else
		{
			vrSystem->GetRecommendedRenderTargetSize(&vrRenderWidth, &vrRenderHeight);
			CreateGLTexture(&vrGLLeftEyeTexture, vrRenderWidth, vrRenderHeight);
			CreateGLTexture(&vrGLRightEyeTexture, vrRenderWidth, vrRenderHeight);
			int viewport[4];
			glGetIntegerv(GL_VIEWPORT, viewport);
			CreateGLTexture(&vrGLMenuTexture, viewport[2], viewport[3]);
			CreateGLTexture(&vrGLHUDTexture, viewport[2], viewport[3]);
			if (vrGLLeftEyeTexture == 0 || vrGLRightEyeTexture == 0 || vrGLMenuTexture == 0 || vrGLHUDTexture == 0)
			{
				Exit(TEXT("Failed to initialize OpenGL textures for VR enviroment. Make sure you have a graphics card that can handle VR and up-to-date drivers."));
			}
			else
			{
				CreateGLFrameBuffer(&vrGLLeftEyeFrameBuffer, vrGLLeftEyeTexture, vrRenderWidth, vrRenderHeight);
				CreateGLFrameBuffer(&vrGLRightEyeFrameBuffer, vrGLRightEyeTexture, vrRenderWidth, vrRenderHeight);
				if (vrGLLeftEyeFrameBuffer == 0 || vrGLRightEyeFrameBuffer == 0)
				{
					Exit(TEXT("Failed to initialize OpenGL framebuffers for VR enviroment. Make sure you have a graphics card that can handle VR and up-to-date drivers."));
				}
			}
		}
	}

	CVAR_CREATE("vr_movecontrols", "0", FCVAR_ARCHIVE);
	CVAR_CREATE("vr_world_scale", "1", FCVAR_ARCHIVE);
	CVAR_CREATE("vr_world_z_strech", "1", FCVAR_ARCHIVE);
	CVAR_CREATE("vr_lefthand_mode", "0", FCVAR_ARCHIVE);
	CVAR_CREATE("vr_playerturn_enabled", "0", FCVAR_ARCHIVE);
	CVAR_CREATE("vr_rotate_with_trains", "1", FCVAR_ARCHIVE);
	CVAR_CREATE("vr_debug_physics", "0", 0);
	CVAR_CREATE("vr_debug_controllers", "0", 0);

	g_vrInput.Init();

	UpdateVRHLConversionVectors();
}

bool VRHelper::AcceptsDisclaimer()
{
	return true;
}

void VRHelper::Exit(const char* lpErrorMessage)
{
	vrSystem = nullptr;
	vrCompositor = nullptr;
	if (lpErrorMessage != nullptr)
	{
		gEngfuncs.Con_DPrintf("Error starting Half-Life: VR: %s\n", lpErrorMessage);
		std::cerr << "Error starting Half-Life: VR: " << lpErrorMessage << std::endl << std::flush;
	}
	vr::VR_Shutdown();
	gEngfuncs.pfnClientCmd("quit");
	std::exit(lpErrorMessage != nullptr ? 1 : 0);
}

Matrix4 VRHelper::GetHMDMatrixProjectionEye(vr::EVREye eEye)
{
	float fNearZ = 0.01f;
	float fFarZ = 8192.f;
	return ConvertSteamVRMatrixToMatrix4(vrSystem->GetProjectionMatrix(eEye, fNearZ, fFarZ));
}

Matrix4 VRHelper::GetHMDMatrixPoseEye(vr::EVREye nEye)
{
	return ConvertSteamVRMatrixToMatrix4(vrSystem->GetEyeToHeadTransform(nEye)).invert();
}

Matrix4 VRHelper::ConvertSteamVRMatrixToMatrix4(const vr::HmdMatrix34_t &mat)
{
	return Matrix4(
		mat.m[0][0], mat.m[1][0], mat.m[2][0], 0.0f,
		mat.m[0][1], mat.m[1][1], mat.m[2][1], 0.0f,
		mat.m[0][2], mat.m[1][2], mat.m[2][2], 0.0f,
		mat.m[0][3], mat.m[1][3], mat.m[2][3], 0.1f
	);
}

Matrix4 VRHelper::ConvertSteamVRMatrixToMatrix4(const vr::HmdMatrix44_t &mat)
{
	return Matrix4(
		mat.m[0][0], mat.m[1][0], mat.m[2][0], mat.m[3][0],
		mat.m[0][1], mat.m[1][1], mat.m[2][1], mat.m[3][1],
		mat.m[0][2], mat.m[1][2], mat.m[2][2], mat.m[3][2],
		mat.m[0][3], mat.m[1][3], mat.m[2][3], mat.m[3][3]
	);
}

Vector VRHelper::GetHLViewAnglesFromVRMatrix(const Matrix4 &mat)
{
	Vector4 v1 = mat * Vector4(1, 0, 0, 0);
	Vector4 v2 = mat * Vector4(0, 1, 0, 0);
	Vector4 v3 = mat * Vector4(0, 0, 1, 0);
	v1.normalize();
	v2.normalize();
	v3.normalize();
	Vector angles;
	VectorAngles(Vector(-v1.z, -v2.z, -v3.z), angles);
	angles.x = 360.f - angles.x;	// viewangles pitch is inverted
	return angles;
}

Vector VRHelper::GetHLAnglesFromVRMatrix(const Matrix4 &mat)
{
	Vector4 forwardInVRSpace = mat * Vector4{ 0, 0, -1, 0 };
	Vector4 rightInVRSpace = mat * Vector4{ 1, 0, 0, 0 };
	Vector4 upInVRSpace = mat * Vector4{ 0, 1, 0, 0 };

	Vector forward{ forwardInVRSpace.x, -forwardInVRSpace.z, forwardInVRSpace.y };
	Vector right{ rightInVRSpace.x, -rightInVRSpace.z, rightInVRSpace.y };
	Vector up{ upInVRSpace.x, -upInVRSpace.z, upInVRSpace.y };

	forward.Normalize();
	right.Normalize();
	up.Normalize();

	Vector angles;
	GetAnglesFromVectors(forward, right, up, angles);
	angles.x = 360.f - angles.x;
	return angles;
}

Matrix4 VRHelper::GetModelViewMatrixFromAbsoluteTrackingMatrix(const Matrix4 &absoluteTrackingMatrix, const Vector translate)
{
	Matrix4 hlToVRScaleMatrix;
	hlToVRScaleMatrix[0] = GetHLToVR().x;
	hlToVRScaleMatrix[5] = GetHLToVR().y;
	hlToVRScaleMatrix[10] = GetHLToVR().z;

	Matrix4 hlToVRTranslateMatrix;
	hlToVRTranslateMatrix[12] = translate.x;
	hlToVRTranslateMatrix[13] = translate.y;
	hlToVRTranslateMatrix[14] = translate.z;

	Matrix4 switchYAndZTransitionMatrix;
	switchYAndZTransitionMatrix[5] = 0;
	switchYAndZTransitionMatrix[6] = -1;
	switchYAndZTransitionMatrix[9] = 1;
	switchYAndZTransitionMatrix[10] = 0;

	Matrix4 modelViewMatrix = absoluteTrackingMatrix * hlToVRScaleMatrix * switchYAndZTransitionMatrix * hlToVRTranslateMatrix;
	return modelViewMatrix.scale(10);
}

Vector VRHelper::GetOffsetInHLSpaceFromAbsoluteTrackingMatrix(const Matrix4 & absoluteTrackingMatrix)
{
	Vector4 originInVRSpace = absoluteTrackingMatrix * Vector4(0, 0, 0, 1);
	return Vector(originInVRSpace.x * GetVRToHL().x, -originInVRSpace.z * GetVRToHL().z, originInVRSpace.y * GetVRToHL().y);
}

Vector VRHelper::GetPositionInHLSpaceFromAbsoluteTrackingMatrix(const Matrix4 & absoluteTrackingMatrix)
{
	Vector originInRelativeHLSpace = GetOffsetInHLSpaceFromAbsoluteTrackingMatrix(absoluteTrackingMatrix);

	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	Vector clientGroundPosition = localPlayer->curstate.origin;
	clientGroundPosition.z += localPlayer->curstate.mins.z;

	return clientGroundPosition + originInRelativeHLSpace;
}

void VRHelper::PollEvents()
{
	UpdateVRHLConversionVectors();
	UpdateWorldRotation();
	vr::VREvent_t vrEvent;
	while (vrSystem != nullptr && vrSystem->PollNextEvent(&vrEvent, sizeof(vr::VREvent_t)))
	{
		switch (vrEvent.eventType)
		{
		case vr::EVREventType::VREvent_Quit:
		case vr::EVREventType::VREvent_ProcessQuit:
		case vr::EVREventType::VREvent_QuitAborted_UserPrompt:
		case vr::EVREventType::VREvent_QuitAcknowledged:
		case vr::EVREventType::VREvent_DriverRequestedQuit:
			Exit();
			return;
		case vr::EVREventType::VREvent_ButtonPress:
		case vr::EVREventType::VREvent_ButtonUnpress:
			if (g_vrInput.IsLegacyInput())
			{
				vr::ETrackedControllerRole controllerRole = vrSystem->GetControllerRoleForTrackedDeviceIndex(vrEvent.trackedDeviceIndex);
				if (controllerRole != vr::ETrackedControllerRole::TrackedControllerRole_Invalid)
				{
					vr::VRControllerState_t controllerState;
					vrSystem->GetControllerState(vrEvent.trackedDeviceIndex, &controllerState, sizeof(controllerState));
					g_vrInput.LegacyHandleButtonPress(vrEvent.data.controller.button, controllerState, controllerRole == vr::ETrackedControllerRole::TrackedControllerRole_LeftHand, vrEvent.eventType == vr::EVREventType::VREvent_ButtonPress);
				}
			}
			break;
		case vr::EVREventType::VREvent_ButtonTouch:
		case vr::EVREventType::VREvent_ButtonUntouch:
			if (g_vrInput.IsLegacyInput())
			{
				vr::ETrackedControllerRole controllerRole = vrSystem->GetControllerRoleForTrackedDeviceIndex(vrEvent.trackedDeviceIndex);
				if (controllerRole != vr::ETrackedControllerRole::TrackedControllerRole_Invalid)
				{
					vr::VRControllerState_t controllerState;
					vrSystem->GetControllerState(vrEvent.trackedDeviceIndex, &controllerState, sizeof(controllerState));
					g_vrInput.LegacyHandleButtonTouch(vrEvent.data.controller.button, controllerState, controllerRole == vr::ETrackedControllerRole::TrackedControllerRole_LeftHand, vrEvent.eventType == vr::EVREventType::VREvent_ButtonPress);
				}
			}
			break;
		default:
			break;
		}
	}
	if (!g_vrInput.IsLegacyInput())
	{
		g_vrInput.HandleInput();
	}
}

bool VRHelper::UpdatePositions(struct ref_params_s* pparams)
{
	UpdateVRHLConversionVectors();
	UpdateWorldRotation();

	if (vrSystem != nullptr && vrCompositor != nullptr)
	{
		vrCompositor->GetCurrentSceneFocusProcess();
		vrCompositor->SetTrackingSpace(isVRRoomScale ? vr::TrackingUniverseStanding : vr::TrackingUniverseSeated);
		vrCompositor->WaitGetPoses(positions.m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, nullptr, 0);

		if (positions.m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bDeviceIsConnected
			&& positions.m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid
			&& positions.m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].eTrackingResult == vr::TrackingResult_Running_OK)
		{
			//Matrix4 m_mat4SeatedPose = ConvertSteamVRMatrixToMatrix4(vrSystem->GetSeatedZeroPoseToStandingAbsoluteTrackingPose()).invert();
			//Matrix4 m_mat4StandingPose = ConvertSteamVRMatrixToMatrix4(vrSystem->GetRawZeroPoseToStandingAbsoluteTrackingPose()).invert();

			Matrix4 m_mat4HMDPose = GetAbsoluteHMDTransform().invert();

			positions.m_mat4LeftProjection = GetHMDMatrixProjectionEye(vr::Eye_Left);
			positions.m_mat4RightProjection = GetHMDMatrixProjectionEye(vr::Eye_Right);

			cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
			Vector clientGroundPosition = localPlayer->curstate.origin;
			clientGroundPosition.z += localPlayer->curstate.mins.z;
			positions.m_mat4LeftModelView = GetHMDMatrixPoseEye(vr::Eye_Left) * GetModelViewMatrixFromAbsoluteTrackingMatrix(m_mat4HMDPose, -clientGroundPosition);
			positions.m_mat4RightModelView = GetHMDMatrixPoseEye(vr::Eye_Right) * GetModelViewMatrixFromAbsoluteTrackingMatrix(m_mat4HMDPose, -clientGroundPosition);

			UpdateGunPosition(pparams);

			SendPositionUpdateToServer();

			return true;
		}
	}

	return false;
}

void VRHelper::PrepareVRScene(vr::EVREye eEye, struct ref_params_s* pparams)
{
	glBindFramebuffer(GL_FRAMEBUFFER, eEye == vr::EVREye::Eye_Left ? vrGLLeftEyeFrameBuffer : vrGLRightEyeFrameBuffer);

	glViewport(0, 0, vrRenderWidth, vrRenderHeight);

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glLoadMatrixf(eEye == vr::EVREye::Eye_Left ? positions.m_mat4LeftProjection.get() : positions.m_mat4RightProjection.get());

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glLoadMatrixf(eEye == vr::EVREye::Eye_Left ? positions.m_mat4LeftModelView.get() : positions.m_mat4RightModelView.get());

	glDisable(GL_CULL_FACE);
	glCullFace(GL_NONE);

	hlvrLockGLMatrices();
}

void VRHelper::FinishVRScene(struct ref_params_s* pparams)
{
	hlvrUnlockGLMatrices();

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glViewport(0, 0, pparams->viewport[2], pparams->viewport[3]);
}

void VRHelper::SubmitImage(vr::EVREye eEye, unsigned int texture)
{
	vr::Texture_t vrTexture;
	vrTexture.eType = vr::ETextureType::TextureType_OpenGL;
	vrTexture.eColorSpace = vr::EColorSpace::ColorSpace_Auto;
	vrTexture.handle = reinterpret_cast<void*>(texture);

	vrCompositor->Submit(eEye, &vrTexture);
}

void VRHelper::SubmitImages()
{
	SubmitImage(vr::EVREye::Eye_Left, vrGLLeftEyeTexture);
	SubmitImage(vr::EVREye::Eye_Right, vrGLRightEyeTexture);
	vrCompositor->PostPresentHandoff();
}

unsigned int VRHelper::GetVRTexture(vr::EVREye eEye)
{
	return eEye == vr::EVREye::Eye_Left ? vrGLLeftEyeTexture : vrGLRightEyeTexture;
}

void VRHelper::GetViewAngles(vr::EVREye eEye, float * angles)
{
	if (eEye == vr::EVREye::Eye_Left)
	{
		GetHLViewAnglesFromVRMatrix(positions.m_mat4LeftModelView).CopyToArray(angles);
	}
	else
	{
		GetHLViewAnglesFromVRMatrix(positions.m_mat4RightModelView).CopyToArray(angles);
	}
}

Matrix4 VRHelper::GetAbsoluteHMDTransform()
{
	auto vrTransform = positions.m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking;
	auto hlTransform = ConvertSteamVRMatrixToMatrix4(vrTransform);

	if (g_vrInput.IsDucking())
	{
		float duckHeightInVR = (DUCK_SIZE - 1.f) * GetHLToVR().y * hlTransform.get()[15];
		float hmdHeight = hlTransform.get()[13];
		const_cast<float*>(hlTransform.get())[13] = (std::min)(hmdHeight, duckHeightInVR);
		m_hmdDuckHeightDelta = hmdHeight - hlTransform.get()[13];
	}

	if (CVAR_GET_FLOAT("vr_playerturn_enabled") == 0.f || m_currentYaw == 0.f)
		return hlTransform;

	if (m_prevYaw != m_currentYaw)
	{
		Matrix4 hlPrevYawTransform = Matrix4{}.rotateY(m_prevYaw) * hlTransform;
		Matrix4 hlCurrentYawTransform = Matrix4{}.rotateY(m_currentYaw) * hlTransform;

		Vector3 previousOffset{ hlPrevYawTransform.get()[12], hlPrevYawTransform.get()[13], hlPrevYawTransform.get()[14] };
		Vector3 newOffset{ hlCurrentYawTransform.get()[12], hlCurrentYawTransform.get()[13], hlCurrentYawTransform.get()[14] };

		Vector3 yawOffsetDelta = newOffset - previousOffset;

		m_currentYawOffsetDelta = Vector{
			yawOffsetDelta.x * GetVRToHL().x,
			-yawOffsetDelta.z * GetVRToHL().z,
			yawOffsetDelta.y * GetVRToHL().y
		};
	}
	else
	{
		m_currentYawOffsetDelta = Vector{};
	}

	return Matrix4{}.rotateY(m_currentYaw) * hlTransform;
}

Matrix4 VRHelper::GetAbsoluteControllerTransform(vr::TrackedDeviceIndex_t controllerIndex)
{
	auto vrTransform = positions.m_rTrackedDevicePose[controllerIndex].mDeviceToAbsoluteTracking;
	auto hlTransform = ConvertSteamVRMatrixToMatrix4(vrTransform);

	if (g_vrInput.IsDucking())
	{
		float controllerHeight = hlTransform.get()[13];
		const_cast<float*>(hlTransform.get())[13] = controllerHeight - m_hmdDuckHeightDelta;
	}

	if (CVAR_GET_FLOAT("vr_playerturn_enabled") == 0.f || m_currentYaw == 0.f)
		return hlTransform;

	auto vrRawHMDTransform = positions.m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking;
	auto hlRawHMDTransform = ConvertSteamVRMatrixToMatrix4(vrRawHMDTransform);
	auto hlCurrentYawHMDTransform = GetAbsoluteHMDTransform();

	Vector3 rawHMDPos{ hlRawHMDTransform.get()[12], hlRawHMDTransform.get()[13], hlRawHMDTransform.get()[14] };
	Vector3 currentYawHMDPos{ hlCurrentYawHMDTransform.get()[12], hlCurrentYawHMDTransform.get()[13], hlCurrentYawHMDTransform.get()[14] };

	Vector4 controllerPos{ hlTransform.get()[12], hlTransform.get()[13], hlTransform.get()[14], 1.f };
	controllerPos.x -= rawHMDPos.x;
	controllerPos.z -= rawHMDPos.z;
	Vector4 rotatedControllerPos = Matrix4{}.rotateY(m_currentYaw) * controllerPos;
	rotatedControllerPos.x += currentYawHMDPos.x;
	rotatedControllerPos.y = controllerPos.y;
	rotatedControllerPos.z += currentYawHMDPos.z;

	Matrix4 hlRotatedTransform = Matrix4{}.rotateY(m_currentYaw) * hlTransform;

	return Matrix4{
		hlRotatedTransform.get()[0],
		hlRotatedTransform.get()[1],
		hlRotatedTransform.get()[2],
		hlRotatedTransform.get()[3],
		hlRotatedTransform.get()[4],
		hlRotatedTransform.get()[5],
		hlRotatedTransform.get()[6],
		hlRotatedTransform.get()[7],
		hlRotatedTransform.get()[8],
		hlRotatedTransform.get()[9],
		hlRotatedTransform.get()[10],
		hlRotatedTransform.get()[11],
		rotatedControllerPos.x,
		rotatedControllerPos.y,
		rotatedControllerPos.z,
		hlRotatedTransform.get()[15]
	};
}

void VRHelper::GetViewOrg(float * origin)
{
	GetPositionInHLSpaceFromAbsoluteTrackingMatrix(GetAbsoluteHMDTransform()).CopyToArray(origin);
}

void VRHelper::UpdateGunPosition(struct ref_params_s* pparams)
{
	cl_entity_t *viewent = gEngfuncs.GetViewModel();
	if (viewent != nullptr)
	{
		vr::TrackedDeviceIndex_t controllerIndex = vrSystem->GetTrackedDeviceIndexForControllerRole(vr::ETrackedControllerRole::TrackedControllerRole_RightHand);

		if (controllerIndex > 0 && controllerIndex != vr::k_unTrackedDeviceIndexInvalid && positions.m_rTrackedDevicePose[controllerIndex].bDeviceIsConnected && positions.m_rTrackedDevicePose[controllerIndex].bPoseIsValid)
		{
			Matrix4 controllerAbsoluteTrackingMatrix = GetAbsoluteControllerTransform(controllerIndex);

			m_rightControllerPosition = GetPositionInHLSpaceFromAbsoluteTrackingMatrix(controllerAbsoluteTrackingMatrix);

			VectorCopy(m_rightControllerPosition, viewent->origin);
			VectorCopy(m_rightControllerPosition, viewent->curstate.origin);
			VectorCopy(m_rightControllerPosition, viewent->latched.prevorigin);

			m_rightControllerAngles = GetHLAnglesFromVRMatrix(controllerAbsoluteTrackingMatrix);
			VectorCopy(m_rightControllerAngles, viewent->angles);
			VectorCopy(m_rightControllerAngles, viewent->curstate.angles);
			VectorCopy(m_rightControllerAngles, viewent->latched.prevangles);

			Vector velocityInVRSpace = Vector(positions.m_rTrackedDevicePose[controllerIndex].vVelocity.v);
			if (CVAR_GET_FLOAT("vr_playerturn_enabled") != 0.f)
			{
				Vector3 rotatedVelocity = Matrix4{}.rotateY(m_currentYaw) * Vector3(velocityInVRSpace.x, velocityInVRSpace.y, velocityInVRSpace.z);
				velocityInVRSpace = Vector(rotatedVelocity.x, rotatedVelocity.y, rotatedVelocity.z);
			}
			Vector velocityInHLSpace(velocityInVRSpace.x * GetVRToHL().x, -velocityInVRSpace.z * GetVRToHL().z, velocityInVRSpace.y * GetVRToHL().y);
			viewent->curstate.velocity = velocityInHLSpace;

			m_fRightControllerValid = true;
		}
		else
		{
			// Keep model visible and place it in front of player
			// studiomodelrenderer will discard it
			// that way we can always ensure rendering of left hand (see StudioModelRenderer::StudioDrawModel)
			// viewent->model = NULL;
			m_fRightControllerValid = false;
			cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
			VectorCopy(localPlayer->curstate.origin, viewent->origin);
			VectorCopy(localPlayer->curstate.origin, viewent->curstate.origin);
			VectorCopy(localPlayer->curstate.origin, viewent->latched.prevorigin);
		}

		float worldScale = CVAR_GET_FLOAT("vr_world_scale");
		if (worldScale < 0.1f)
		{
			worldScale = 0.1f;
		}
		else if (worldScale > 100.f)
		{
			worldScale = 100.f;
		}
		viewent->curstate.scale = 1.f / worldScale;
	}
}

void VRHelper::SendPositionUpdateToServer()
{
	cl_entity_t *localPlayer = gEngfuncs.GetLocalPlayer();
	cl_entity_t *viewent = gEngfuncs.GetViewModel();

	Matrix4 hdmAbsoluteTrackingMatrix = GetAbsoluteHMDTransform();
	Vector hmdOffset = GetOffsetInHLSpaceFromAbsoluteTrackingMatrix(hdmAbsoluteTrackingMatrix);
	/*
	auto lol =
		"ducking: " + std::to_string(g_vrInput.IsDucking()) + ", " +
		"hmdOffset.z (raw): " + std::to_string(hmdOffset.z) + ", " +
		"localPlayer->curstate.mins.z: " + std::to_string(localPlayer->curstate.mins.z) + ", " +
		"hmdOffset.z (gedingst): " + std::to_string(hmdOffset.z + localPlayer->curstate.mins.z) + "\n";
	gEngfuncs.pfnConsolePrint(lol.data());
	*/
	hmdOffset.z += localPlayer->curstate.mins.z;

	/*Vector hmdAngles = GetHLAnglesFromVRMatrix(hdmAbsoluteTrackingMatrix);*/

	vr::TrackedDeviceIndex_t leftControllerIndex = vrSystem->GetTrackedDeviceIndexForControllerRole(vr::ETrackedControllerRole::TrackedControllerRole_LeftHand);
	bool isLeftControllerValid = leftControllerIndex > 0 && leftControllerIndex != vr::k_unTrackedDeviceIndexInvalid && positions.m_rTrackedDevicePose[leftControllerIndex].bDeviceIsConnected && positions.m_rTrackedDevicePose[leftControllerIndex].bPoseIsValid;

	vr::TrackedDeviceIndex_t rightControllerIndex = vrSystem->GetTrackedDeviceIndexForControllerRole(vr::ETrackedControllerRole::TrackedControllerRole_RightHand);
	bool isRightControllerValid = rightControllerIndex > 0 && rightControllerIndex != vr::k_unTrackedDeviceIndexInvalid && positions.m_rTrackedDevicePose[rightControllerIndex].bDeviceIsConnected && positions.m_rTrackedDevicePose[rightControllerIndex].bPoseIsValid;

	Vector leftControllerOffset(0, 0, 0);
	Vector leftControllerAngles(0, 0, 0);
	Vector leftControllerVelocity(0, 0, 0);
	bool leftDragOn = false;
	if (isLeftControllerValid)
	{
		Matrix4 leftControllerAbsoluteTrackingMatrix = GetAbsoluteControllerTransform(leftControllerIndex);
		leftControllerOffset = GetOffsetInHLSpaceFromAbsoluteTrackingMatrix(leftControllerAbsoluteTrackingMatrix);
		leftControllerOffset.z += localPlayer->curstate.mins.z;
		leftControllerAngles = GetHLAnglesFromVRMatrix(leftControllerAbsoluteTrackingMatrix);

		Vector velocityInVRSpace = Vector(positions.m_rTrackedDevicePose[leftControllerIndex].vVelocity.v);
		if (CVAR_GET_FLOAT("vr_playerturn_enabled") != 0.f)
		{
			Vector3 rotatedVelocity = Matrix4{}.rotateY(m_currentYaw) * Vector3(velocityInVRSpace.x, velocityInVRSpace.y, velocityInVRSpace.z);
			velocityInVRSpace = Vector(rotatedVelocity.x, rotatedVelocity.y, rotatedVelocity.z);
		}
		leftControllerVelocity = Vector(velocityInVRSpace.x * GetVRToHL().x, -velocityInVRSpace.z * GetVRToHL().z, velocityInVRSpace.y * GetVRToHL().y);

		m_fLeftControllerValid = true;
		m_leftControllerPosition = GetPositionInHLSpaceFromAbsoluteTrackingMatrix(leftControllerAbsoluteTrackingMatrix);
		m_leftControllerAngles = leftControllerAngles;

		leftDragOn = g_vrInput.IsDragOn(leftControllerIndex);
	}
	else
	{
		m_fLeftControllerValid = false;
	}

	Vector weaponOrigin(0, 0, 0);
	Vector weaponOffset(0, 0, 0);
	Vector weaponAngles(0, 0, 0);
	Vector weaponVelocity(0, 0, 0);
	bool rightDragOn = false;
	if (isRightControllerValid && viewent)
	{
		weaponOrigin = viewent ? viewent->curstate.origin : Vector(0, 0, 0);
		weaponOffset = weaponOrigin - localPlayer->curstate.origin;
		weaponAngles = viewent ? viewent->curstate.angles : Vector(0, 0, 0);
		weaponVelocity = viewent ? viewent->curstate.velocity : Vector(0, 0, 0);
		rightDragOn = g_vrInput.IsDragOn(rightControllerIndex);
	}

	// void UpdateVRHeadsetPosition(const int timestamp, const Vector & offset, const Vector & angles);
	// void UpdateVRLeftControllerPosition(const int timestamp, const bool isValid, const Vector & offset, const Vector & angles, const Vector & velocity);
	// void UpdateVRRightControllerPosition(const int timestamp, const bool isValid, const Vector & offset, const Vector & angles, const Vector & velocity);

	char cmdHMD[MAX_COMMAND_SIZE] = { 0 };
	char cmdLeftController[MAX_COMMAND_SIZE] = { 0 };
	char cmdRightController[MAX_COMMAND_SIZE] = { 0 };
	sprintf_s(cmdHMD, "vrupd_hmd %i %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f",/* %.2f %.2f %.2f",*/
		m_vrUpdateTimestamp,
		hmdOffset.x, hmdOffset.y, hmdOffset.z,
		m_currentYawOffsetDelta.x, m_currentYawOffsetDelta.y, m_currentYawOffsetDelta.z,
		m_prevYaw, m_currentYaw	// for save/restore
		/*,
		hmdAngles.x, hmdAngles.y, hmdAngles.z*/
	);
	m_currentYawOffsetDelta = Vector{}; // reset after sending
	sprintf_s(cmdLeftController, "vrupd_lft %i %i %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %i",
		m_vrUpdateTimestamp,
		isLeftControllerValid ? 1 : 0,
		leftControllerOffset.x, leftControllerOffset.y, leftControllerOffset.z,
		leftControllerAngles.x, leftControllerAngles.y, leftControllerAngles.z,
		leftControllerVelocity.x, leftControllerVelocity.y, leftControllerVelocity.z,
		leftDragOn ? 1 : 0
	);
	sprintf_s(cmdRightController, "vrupd_rt %i %i %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %.2f %i",
		m_vrUpdateTimestamp,
		isRightControllerValid ? 1 : 0,
		weaponOffset.x, weaponOffset.y, weaponOffset.z,
		weaponAngles.x, weaponAngles.y, weaponAngles.z,
		weaponVelocity.x, weaponVelocity.y, weaponVelocity.z,
		rightDragOn ? 1 : 0
	);
	gEngfuncs.pfnClientCmd(cmdHMD);
	gEngfuncs.pfnClientCmd(cmdLeftController);
	gEngfuncs.pfnClientCmd(cmdRightController);
	m_vrUpdateTimestamp++;
}

void RenderLine(Vector v1, Vector v2, Vector color)
{
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_BLEND);
	glColor4f(color.x, color.y, color.z, 1.0f);
	glBegin(GL_LINES);
	glVertex3f(v1.x, v1.y, v1.z);
	glVertex3f(v2.x, v2.y, v2.z);
	glEnd();
}

void VRHelper::TestRenderControllerPosition(bool leftOrRight)
{
	vr::TrackedDeviceIndex_t controllerIndex = vrSystem->GetTrackedDeviceIndexForControllerRole(leftOrRight ? vr::ETrackedControllerRole::TrackedControllerRole_LeftHand : vr::ETrackedControllerRole::TrackedControllerRole_RightHand);

	if (controllerIndex > 0 && controllerIndex != vr::k_unTrackedDeviceIndexInvalid && positions.m_rTrackedDevicePose[controllerIndex].bDeviceIsConnected && positions.m_rTrackedDevicePose[controllerIndex].bPoseIsValid)
	{
		Matrix4 controllerAbsoluteTrackingMatrix = GetAbsoluteControllerTransform(controllerIndex);

		Vector originInHLSpace = GetPositionInHLSpaceFromAbsoluteTrackingMatrix(controllerAbsoluteTrackingMatrix);

		Vector4 forwardInVRSpace = controllerAbsoluteTrackingMatrix * Vector4(0, 0, -1, 0);
		Vector4 rightInVRSpace = controllerAbsoluteTrackingMatrix * Vector4(1, 0, 0, 0);
		Vector4 upInVRSpace = controllerAbsoluteTrackingMatrix * Vector4(0, 1, 0, 0);

		Vector forward(forwardInVRSpace.x * GetVRToHL().x, -forwardInVRSpace.z * GetVRToHL().z, forwardInVRSpace.y * GetVRToHL().y);
		Vector right(rightInVRSpace.x * GetVRToHL().x, -rightInVRSpace.z * GetVRToHL().z, rightInVRSpace.y * GetVRToHL().y);
		Vector up(upInVRSpace.x * GetVRToHL().x, -upInVRSpace.z * GetVRToHL().z, upInVRSpace.y * GetVRToHL().y);

		Vector velocityInVRSpace = Vector(positions.m_rTrackedDevicePose[controllerIndex].vVelocity.v);
		if (CVAR_GET_FLOAT("vr_playerturn_enabled") != 0.f)
		{
			Vector3 rotatedVelocity = Matrix4{}.rotateY(m_currentYaw) * Vector3(velocityInVRSpace.x, velocityInVRSpace.y, velocityInVRSpace.z);
			velocityInVRSpace = Vector(rotatedVelocity.x, rotatedVelocity.y, rotatedVelocity.z);
		}
		Vector velocity = Vector(velocityInVRSpace.x * GetVRToHL().x, -velocityInVRSpace.z * GetVRToHL().z, velocityInVRSpace.y * GetVRToHL().y);

		if (leftOrRight)
		{
			right = -right; // left
		}

		RenderLine(originInHLSpace, originInHLSpace + forward, Vector(1, 0, 0));
		RenderLine(originInHLSpace, originInHLSpace + right, Vector(0, 1, 0));
		RenderLine(originInHLSpace, originInHLSpace + up, Vector(0, 0, 1));
		RenderLine(originInHLSpace, originInHLSpace + velocity, Vector(1, 0, 1));
	}
}

bool VRHelper::IsRightControllerValid()
{
	return m_fRightControllerValid;
}

bool VRHelper::IsLeftControllerValid()
{
	return m_fLeftControllerValid;
}

const Vector & VRHelper::GetLeftHandPosition()
{
	return m_leftControllerPosition;
}

const Vector & VRHelper::GetLeftHandAngles()
{
	return m_leftControllerAngles;
}

const Vector & VRHelper::GetRightHandPosition()
{
	return m_rightControllerPosition;
}

const Vector & VRHelper::GetRightHandAngles()
{
	return m_rightControllerAngles;
}
