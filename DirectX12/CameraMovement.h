#pragma once

//Handles Camera Movement with Keyboard/Mouse Input and Controller Input
class CameraMovement 
{
	//Current Aspect Ratio for handling yaw looking
	float												mAspectRatio = 0;

	//What we need for getting time between calls
	float												mDeltaTime = 0;
	std::chrono::steady_clock::time_point				mLastUpdate;

	//Camera settings
	float												mCameraSpeed = 3.0f;
	float												mControllerSpeedModifier = 1.5f;

	//Matrix to apply movement to, then return 
	GW::MATH::GMATRIXF									mCameraMatrix = GW::MATH::GIdentityMatrixF;

	CameraMovement() {}

	static CameraMovement								instance;

private:

	void HandleVerticleMovement(GW::INPUT::GInput& ginput, GW::INPUT::GController& gcontroller)
	{
		float camY = 0, spaceKeyState = 0, lShiftState = 0, rightTriggerState = 0, leftTriggerState = 0;

		ginput.GetState(G_KEY_SPACE, spaceKeyState);
		ginput.GetState(G_KEY_LEFTSHIFT, lShiftState);
		gcontroller.GetState(0, G_RIGHT_TRIGGER_AXIS, rightTriggerState);
		gcontroller.GetState(0, G_LEFT_TRIGGER_AXIS, leftTriggerState);

		camY = spaceKeyState - lShiftState + rightTriggerState - leftTriggerState;
		
		GW::MATH::GVECTORF translation = { 0, 0, 0 };
		translation.y = camY * mCameraSpeed * mDeltaTime;

		GW::MATH::GMatrix::TranslateLocalF(mCameraMatrix, translation, mCameraMatrix);
	}

	void HandleStrafing(GW::INPUT::GInput& ginput, GW::INPUT::GController& gcontroller)
	{
		float wkeyState = 0, sKeyState = 0, dKeyState = 0, aKeyState = 0, leftStickYAxis = 0, leftStickXAxis = 0;

		ginput.GetState(G_KEY_W, wkeyState);
		ginput.GetState(G_KEY_S, sKeyState);
		ginput.GetState(G_KEY_D, dKeyState);
		ginput.GetState(G_KEY_A, aKeyState);
		gcontroller.GetState(0, G_LX_AXIS, leftStickYAxis);
		gcontroller.GetState(0, G_LY_AXIS, leftStickXAxis);

		float camZ = wkeyState - sKeyState + leftStickXAxis;
		float camX = dKeyState - aKeyState + leftStickYAxis;

		float perFrameSpeed = mCameraSpeed * mDeltaTime;
		if (leftStickXAxis != 0 || leftStickYAxis != 0)
			perFrameSpeed *= mControllerSpeedModifier;

		GW::MATH::GVECTORF translation = { 0, 0, 0 };
		translation.x = camX * perFrameSpeed;
		translation.z = camZ * perFrameSpeed;
		translation.y = 0;
		
		GW::MATH::GMatrix::TranslateLocalF(mCameraMatrix, translation, mCameraMatrix);
	}

	void HandleLooking(GW::INPUT::GInput& ginput, GW::INPUT::GController& gcontroller, GW::SYSTEM::GWindow& win)
	{
		const float PI = 3.141592f;
		float sens = 20.0f;
		const float Thumb_Speed = PI * mDeltaTime * sens;
		UINT width, height;
		win.GetClientWidth(width);
		win.GetClientHeight(height);

		float mouseY = 0, mouseX = 0, rightStickYAxis = 0, rightStickXAxis = 0;
		GW::GReturn result = ginput.GetMouseDelta(mouseX, mouseY);
		gcontroller.GetState(0, G_RY_AXIS, rightStickYAxis);
		gcontroller.GetState(0, G_RX_AXIS, rightStickXAxis);

		if (G_PASS(result) && result != GW::GReturn::REDUNDANT || rightStickYAxis != 0 || rightStickXAxis != 0)
		{
			// If the controller is being used for input, don't let the mouse interfere
			if (rightStickYAxis != 0 || rightStickXAxis != 0)
			{
				mouseY = 0;
				mouseX = 0;
			}

			float totalPitch = 65.0f * mouseY / height + rightStickYAxis * -Thumb_Speed;
			GW::MATH::GMATRIXF pitch;
			GW::MATH::GMatrix::RotationYawPitchRollF(0, G_DEGREE_TO_RADIAN_F(totalPitch), 0, pitch);
			GW::MATH::GMatrix::MultiplyMatrixF(pitch, mCameraMatrix, mCameraMatrix);
		
			float totalYaw = 65.0f * mAspectRatio * mouseX / width + rightStickXAxis * Thumb_Speed;
			GW::MATH::GMATRIXF yaw;

			GW::MATH::GMatrix::RotationYawPitchRollF(G_DEGREE_TO_RADIAN_F(totalYaw), 0, 0, yaw);

			GW::MATH::GVECTORF camPos = mCameraMatrix.row4;
			GW::MATH::GMatrix::MultiplyMatrixF(mCameraMatrix, yaw, mCameraMatrix);
			mCameraMatrix.row4 = camPos;
		}
	}

public:

	CameraMovement(const CameraMovement&) = delete;

	static CameraMovement& Get()
	{
		return instance;
	}

	GW::MATH::GMATRIXF GetCameraMatrixFromInput(GW::MATH::GMATRIXF oldCam, float aspectRatio, GW::SYSTEM::GWindow& win, GW::INPUT::GInput& ginput, GW::INPUT::GController& gcontroller)
	{
		mCameraMatrix = oldCam;
		mAspectRatio = aspectRatio;

		auto now = std::chrono::steady_clock::now();
		mDeltaTime = std::chrono::duration_cast<std::chrono::microseconds>(now - mLastUpdate).count() / 1000000.0f;
		mLastUpdate = now;

		HandleVerticleMovement(ginput, gcontroller);
		HandleStrafing(ginput, gcontroller);
		HandleLooking(ginput, gcontroller, win);

		return mCameraMatrix;
	}
};

CameraMovement CameraMovement::instance;