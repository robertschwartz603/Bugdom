//
// input.h
//

#ifndef __INPUT_H
#define __INPUT_H


#define NUM_MOUSE_SENSITIVITY_LEVELS		5
#define DEFAULT_MOUSE_SENSITIVITY_LEVEL		(NUM_MOUSE_SENSITIVITY_LEVELS/2)


			/* ASCII */
			
#define	CHAR_RETURN			0x0d	/* ASCII code for Return key */
#define CHAR_UP				0x1e
#define CHAR_DOWN			0x1f
#define	CHAR_LEFT			0x1c
#define	CHAR_RIGHT			0x1d
#define	CHAR_DELETE			0x08
#define CHAR_FORWARD_DELETE	0x7f


	/* KEYBOARD EQUATE */

enum
{
	kKey_Pause,
	kKey_ToggleMusic,
	kKey_RaiseVolume,
	kKey_LowerVolume,
	kKey_ToggleFullscreen,

	kKey_SwivelCameraLeft,
	kKey_SwivelCameraRight,
	kKey_ZoomIn,
	kKey_ZoomOut,

	kKey_MorphPlayer,
	kKey_BuddyAttack,
	kKey_Jump,
	kKey_KickBoost,

	kKey_AutoWalk,
	kKey_Forward,
	kKey_Backward,
	kKey_Left,
	kKey_Right,

	kKey_UI_Confirm,
	kKey_UI_Skip,
	kKey_UI_Cancel,

	kKey_MAX
};



//============================================================================================


void UpdateInput(void);
Boolean GetNewKeyState(unsigned short key);
Boolean GetKeyState_SDL(unsigned short sdlScanCode);
Boolean GetKeyState(unsigned short key);
Boolean GetNewKeyState_SDL(unsigned short sdlScanCode);
Boolean GetSkipScreenInput(void);
Boolean AreAnyNewKeysPressed(void);
void ResetInputState(void);
void UpdateKeyMap(void);

Boolean FlushMouseButtonPress(void);
void EatMouseEvents(void);
void GetMouseDelta(float *dx, float *dy);

void CaptureMouse(Boolean doCapture);

SDL_GameController* TryOpenController(bool showMessageOnFailure);
void OnJoystickRemoved(SDL_JoystickID which);


#endif



