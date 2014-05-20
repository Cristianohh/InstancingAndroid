/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

//BEGIN_INCLUDE(all)
#include <jni.h>
#include <errno.h>

#include <EGL/egl.h>
#include <GLES/gl.h>
#include <NDKHelper.h>


#include <android/sensor.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#include "CPUTScene.h"
#include "CPUTRenderStateBlockOGL.h"
#include "CPUT_OGL.h"
#include "CPUTGuiControllerOGL.h"
#include "CPUTWindowAndroid.h"
#include "CPUTSpriteOGL.h"
#include "CPUTRenderTargetOGL.h"
#include "CPUTTextureOGL.h"
#include "CPUTFont.h"


// define some controls
const CPUTControlID ID_MAIN_PANEL = 10;
const CPUTControlID ID_SECONDARY_PANEL = 20;
const CPUTControlID ID_FULLSCREEN_BUTTON = 100;
const CPUTControlID ID_NEXTMODEL_BUTTON = 101;
const CPUTControlID ID_TEST_CONTROL = 1000;
const CPUTControlID ID_IGNORE_CONTROL_ID = -1;

#define DEBUG_PRINT(s, ...) LOGI(s, ##__VA_ARGS__)
#define ENABLE_GUI
#define ROW_INCREASE 5000
#define ROW_DECREASE 5001
#define COLUMN_INCREASE 5002
#define COLUMN_DECREASE 5003
#define TOGGLE_INSTANCING 5004
#define INSTANCING_BUTTON_SIZE 100

#define GUI_LOCATION     "gui_assets/"
#define SYSTEM_LOCATION  "System/"
#define SCENE_FILE       "instanceV2.scene"
#define ASSET_LOCATION   "instanceV2/"
#define ASSET_SET_FILE   "instanceV2"
#define NUM_COLUMNS 16
#define NUM_ROWS 15

const UINT MAX_INSTANCES = NUM_COLUMNS * NUM_ROWS;

float currentTime;
float fpsTime = 0.0f;
float fps = 0.0f;
float cameraY = 1100.0f;
float lookY = cameraY / 4.0f;
float lookZ = lookY / 2.0f;
int currentRowCount = 0;
int currentColCount = 0;
int fpsCount = 0;
int width, height;
bool isInstanced;

CPUTAssetLibrary *pAssetLibrary;
CPUTMaterial *techMaterial;
CPUTMaterial *supportMaterial;
CPUTText *pTextMethod;
CPUTButton *pToggleButton;

static float4x4 world[MAX_INSTANCES];

bool drawCallback(CPUTModel* pModel, CPUTRenderParameters &renderParams, CPUTMesh* pMesh, CPUTMaterialEffect* pMaterial, CPUTMaterialEffect* , void* );

void SetEffectVals( UINT index )
{
	supportMaterial->SetCurrentEffect(index);
	techMaterial->SetCurrentEffect(index);
}

static float DegreesToRadians(float degree)
{
    return (degree * (kPi / 180.0f));
}

static string PrepareText(stringstream &ss)
{
	string text;
	std::wstring ws;
	ss >> text;
	return text;
}

class MySample : public CPUT_OGL
{
private:
    float                  mfElapsedTime;
    CPUTAssetSet          *mpAssetSet;
    CPUTCameraController  *mpCameraController;

    CPUTSprite            *mpDebugSprite;

    CPUTScene             *mpScene;

public:
    MySample() :
          mpAssetSet(NULL),
          mpCameraController(NULL),
        mpDebugSprite(NULL)
    {
    }

    virtual ~MySample()
    {
        // Note: these two are defined in the base.  We release them because we addref them.
        SAFE_RELEASE(mpCamera);

        SAFE_RELEASE(mpAssetSet);
        SAFE_DELETE( mpCameraController );
        SAFE_DELETE( mpDebugSprite);
        SAFE_DELETE( mpScene );
    }

    virtual CPUTEventHandledCode HandleKeyboardEvent(CPUTKey key, CPUTKeyState state)
    {
        if (mpCameraController)
        {
            return mpCameraController->HandleKeyboardEvent(key, state);
        }
        return CPUT_EVENT_UNHANDLED;
    }

    virtual CPUTEventHandledCode HandleMouseEvent(int x, int y, int wheel, CPUTMouseState state)
    {
        if (mpCameraController)
        {
            return mpCameraController->HandleMouseEvent(x, y, wheel, state);
        }
        return CPUT_EVENT_UNHANDLED;
    }

    virtual void Create();
    virtual void Render(double deltaSeconds);
    virtual void Update(double deltaSeconds);
    virtual void ResizeWindow(UINT width, UINT height);
	void HandleCallbackEvent( CPUTEventID Event, CPUTControlID ControlID, CPUTControl *pControl );
	void PositionInstancingControlButtons(int width, int height);
	void CreateInstancingControlButtons();
	CPUTEventHandledCode HandleInstancingControlButtons(CPUTEventID Event, CPUTControlID ControlID, CPUTControl *pControl);
    void UpdateMatrixBuffers();
};

void MySample::UpdateMatrixBuffers()
{
    // Update GUI
    int rows = currentRowCount;
    int columns = currentColCount;
    int index = 0;
    float angle_value = 360.0 / columns;
    float radius = (columns / k2Pi);
    for (int i = 0; i < rows; ++i)
    {
        int add_value = 0;
        for (int j = 0; j < columns; ++j)
        {
            if( (i % 2) == 0 )
            {
                add_value = angle_value + (angle_value / 2.0);
            }
            world[index] = float4x4RotationY( kPi / 2.0f) *
                           float4x4Translation(float3(0.0f, 0.0f + (100.0f * i), 100.0f * radius)) *
                           float4x4RotationY( DegreesToRadians(add_value + (angle_value * j)));
            ++index;
        }
    }

    // Now update buffer
    const int numInstances = columns * rows;

    // Support shader
    CPUTMaterialEffectOGL *pMatEffect = (CPUTMaterialEffectOGL*) supportMaterial->GetMaterialEffects()[0];
    DEBUG_PRINT("pMatEffect: %p", pMatEffect);
    GLuint shaderProgID = pMatEffect->GetShaderID();
    DEBUG_PRINT("shaderProgID: %d", shaderProgID);
    GLuint worldMatrixBufferLoc = glGetUniformLocation( shaderProgID, "instancedWorld" );
    DEBUG_PRINT("worldMatrixBufferLoc: %d", worldMatrixBufferLoc);
    GL_CHECK(glUseProgram(shaderProgID));
    GL_CHECK(glUniformMatrix4fv( worldMatrixBufferLoc, numInstances, GL_FALSE, (const GLfloat *) &world[0] ));

    // Tech unit shader
    pMatEffect = (CPUTMaterialEffectOGL*) techMaterial->GetMaterialEffects()[0];
    assert(pMatEffect);
    shaderProgID = pMatEffect->GetShaderID();
    worldMatrixBufferLoc = glGetUniformLocation( shaderProgID, "instancedWorld" );
    GL_CHECK(glUseProgram(shaderProgID));
    GL_CHECK(glUniformMatrix4fv( worldMatrixBufferLoc, numInstances, GL_FALSE, (const GLfloat *) &world[0] ));

    GL_CHECK(glUseProgram(0));
}

void MySample::Render(double deltaSeconds)
{
    CPUTRenderParameters renderParams;
    if(isInstanced)
        SetEffectVals( 0 );
    else
        SetEffectVals( 1 );


    renderParams.mpPerFrameConstants = mpPerFrameConstantBuffer;
    renderParams.mpPerModelConstants = mpPerModelConstantBuffer;
	renderParams.mpCamera = mpCamera;
    UpdatePerFrameConstantBuffer(renderParams, deltaSeconds);

    int width,height;
    mpWindow->GetClientDimensions(&width, &height);

    // Set default viewport
    glViewport( 0, 0, width, height );
    GL_CHECK(glClearColor ( 0.7f, 0.7f, 0.7f, 1.0f ));
    GL_CHECK(glClearDepthf(0.0f));
    GL_CHECK(glClear ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ));


    mpScene->Render( renderParams);

#ifdef ENABLE_GUI
	CPUTDrawGUI();
#endif
}

void MySample::Update(double deltaSeconds)
{
	static double time = 0.0f;
	float rows = 0.0f;
	float columns = 0.0f;
	float angle_value = 0.0f;
	
	// Update cam
	lookY = cameraY / 4.0f;
	lookZ = lookY / 3.0f;
	
	time += (deltaSeconds);

	/* DEMO SECTION */
	if (time > 360.0f)
		time = 0.0f;
	float4 temp = float4(1.0f, 1.0f, 1.0f, 1.0f) * (float4x4RotationY( kPi / 2.0f) * 
		float4x4Translation(float3(0.0f, 0.0f, 50.0f * (9 / k2Pi))) *
							   float4x4RotationY( DegreesToRadians((40.0f * ( 0.5f * time)))));
	
	mpCamera->SetPosition( temp.x, cameraY, temp.z );
	mpCamera->LookAt( 1.0f, lookY, lookZ );
	/* END DEMO SECTION */

    mpCameraController->Update((float)deltaSeconds);
	currentTime = (float)deltaSeconds;
	fpsTime += (float) deltaSeconds;
	fpsCount++;
	fps = fpsCount / fpsTime;
	if( fpsTime >= 1.0f )
	{
		fpsTime = 0.0f;
		fpsCount = 0;
	}

	// Update GUI
	rows = (float) currentRowCount; // for slider
	columns = (float) currentColCount; // for slider
	angle_value = 360.0 / columns;
	stringstream textStream;
	const int numInstances = (int)columns * (int) rows;
	textStream << "FPS:"<< fps;
	pTextMethod->SetText(PrepareText(textStream), 0.5f);
}

void MySample::Create()
{
	CPUTAssetLibrary *pAssetLibrary = CPUTAssetLibrary::GetAssetLibrary();
    cString ExecutableDirectory;
    CPUTFileSystem::GetExecutableDirectory(&ExecutableDirectory);
    ExecutableDirectory.append(_L(ASSET_LOCATION));
    pAssetLibrary->SetMediaDirectoryName(ExecutableDirectory);

#ifdef ENABLE_GUI
    CPUTGuiControllerOGL *pGUI = (CPUTGuiControllerOGL*)CPUTGetGuiController();
    std::string mediaDirectory = ws2s(pAssetLibrary->GetMediaDirectoryName());
    std::string fontDirectory("Font/");
    CPUTFont *pFont = CPUTFont::CreateFont(SYSTEM_LOCATION + fontDirectory, "arial_64.fnt");

    pGUI->SetFont(pFont);

    //
    // Create some controls
    //   
	CPUTDropdown *pDropdownMethod;
	
	pGUI->CreateText(_L("Test Font"), ID_IGNORE_CONTROL_ID, ID_MAIN_PANEL, &pTextMethod);
	pTextMethod->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	CreateInstancingControlButtons();
#endif

	CPUTFileSystem::GetExecutableDirectory(&ExecutableDirectory);
    ExecutableDirectory.append(_L(ASSET_LOCATION));
    pAssetLibrary->SetMediaDirectoryName(ExecutableDirectory);


    int width, height;
    mpWindow->GetClientDimensions(&width, &height);
    DEBUG_PRINT(_L("Resize Window"));

    ResizeWindow(width, height);

    mpScene = new CPUTScene();
    DEBUG_PRINT(_L("Load Scene"));

    // Load the scene
    if (CPUTFAILED(mpScene->LoadScene(SCENE_FILE)))
    {
        LOGI("Failed to Load Scene, try loading asset set individually");
        CPUTAssetSet *pAssetSet = NULL;
        pAssetSet = pAssetLibrary->GetAssetSet( ASSET_SET_FILE );
        mpScene->AddAssetSet(pAssetSet);
        pAssetSet->Release();
    }
    LOGI("Loaded the scene");

    // Get the camera. Get the first camera encountered in the scene or
    // if there are none, create a new one.
    unsigned int numAssets = mpScene->GetNumAssetSets();
    for (unsigned int i = 0; i < numAssets; ++i) {
        CPUTAssetSet *pAssetSet = mpScene->GetAssetSet(i);
        if (pAssetSet->GetCameraCount() > 0) {
            mpCamera = pAssetSet->GetFirstCamera();
            break;
        }
    }

    // Create the camera
    if (mpCamera == NULL)
    {
        mpCamera = new CPUTCamera();
        pAssetLibrary->AddCamera( _L(""), _L("SampleStart Camera"), _L(""), mpCamera );
		
		mpCamera->SetPosition( 0.0f, cameraY, 100.0f );

        // Set the projection matrix for all of the cameras to match our window.
        // TODO: this should really be a viewport matrix.  Otherwise, all cameras will have the same FOV and aspect ratio, etc instead of just viewport dimensions.
        mpCamera->SetAspectRatio(((float)width)/((float)height));
    }

    mpCamera->SetFov(DegToRad(90.0)); // TODO: Fix converter's FOV bug (Maya generates cameras for which fbx reports garbage for fov)
    mpCamera->SetFarPlaneDistance(100000.0f);
    mpCamera->Update();

    mpCameraController = new CPUTCameraControllerFPS();
    mpCameraController->SetCamera(mpCamera);
    mpCameraController->SetLookSpeed(0.004f);
    mpCameraController->SetMoveSpeed(150.0f);

	// retrieve function pointer
    glDrawElementsInstancedEXT = (PFNGLDRAWELEMENTSINSTANCEDEXTPROC) ( eglGetProcAddress( "glDrawElementsInstanced" ) );


    if( glDrawElementsInstancedEXT == NULL )
    {
        LOGE( "Failed to load extension pointer" );
    }
	
	currentRowCount = NUM_ROWS / 1.5;
	currentColCount = NUM_COLUMNS;
	
	LOGI("Getting handles for materials");

	// get handles to switch instancing / non instancing
	pAssetLibrary->PrintAssetLibrary();

	supportMaterial = pAssetLibrary->GetMaterialByName("instanceV2/Material/concreteMultiMaterial.mtl");
	techMaterial = pAssetLibrary->GetMaterialByName("instanceV2/Material/techALowMultiMataterial.mtl");

	isInstanced = true;
	
	CPUTModel *pModel = pAssetLibrary->GetModelByName( _L( ".instangeGrptechALow" ) );
	pModel->SetDrawModelCallBack((DrawModelCallBackFunc)drawCallback);
	pModel = pAssetLibrary->GetModelByName( _L( ".instangeGrpconcrete" ) );
	pModel->SetDrawModelCallBack((DrawModelCallBackFunc)drawCallback);

	UINT supportCount = supportMaterial->GetMaterialEffectCount();
	UINT techCount = techMaterial->GetMaterialEffectCount();

	glFrontFace(GL_CW);
    UpdateMatrixBuffers();
}


bool drawCallback(CPUTModel* pModel, CPUTRenderParameters &renderParams, CPUTMesh* pMesh, CPUTMaterialEffect* pMaterial, CPUTMaterialEffect* , void* )
{
	UINT mVertexCount = 0;
	UINT mIndexCount = 0;
	int index = 0;
	float rows = 0.0;
	float columns = 0.0;
	float instances = 0.0;
	float angle_value = 0.0;

	// Uniform locations
	GLint vpMatrixBufferLoc = -1;
	GLint worldMatrixBufferLoc = -1;
	
	float4x4 viewProj;
    float4x4 parents_world;
	
	rows = (float) currentRowCount;
	columns = (float) currentColCount;

	angle_value = 360.0 / columns;

	const int numInstances = (int)columns * (int) rows;
	pMaterial->SetRenderStates(renderParams);

	parents_world = pModel->GetParentsWorldMatrix();
    CPUTCamera *pCamera   = renderParams.mpCamera;
	
	// Set Uniform Data
	if(pCamera)
    {
		viewProj = *pCamera->GetViewMatrix() * *pCamera->GetProjectionMatrix();
    }
	
	// Drawing
	mVertexCount = ((CPUTMeshOGL*)pMesh)->GetVertexCount();
	mIndexCount = ((CPUTMeshOGL*)pMesh)->GetIndexCount();
	CPUTBufferOGL * mpIndexBuffer = ((CPUTMeshOGL*)pMesh)->GetIndexBuffer();

	if(mVertexCount == 0 && mIndexCount == 0)
        return false;

	((CPUTMeshOGL*)pMesh)->EnableVertexArray();
	
	
	CPUTMaterialEffectOGL *pMatEffect = (CPUTMaterialEffectOGL*) pMaterial;
	GLuint shaderProgID = pMatEffect->GetShaderID();
	
    vpMatrixBufferLoc = glGetUniformLocation( shaderProgID, "viewProjMatrix" );
	worldMatrixBufferLoc = glGetUniformLocation( shaderProgID, "worldMatrix" );

	if( isInstanced )
	{   

		glUniformMatrix4fv( vpMatrixBufferLoc, 1, GL_FALSE, (const GLfloat *) &viewProj );
        glUniformMatrix4fv( worldMatrixBufferLoc, numInstances, GL_FALSE, (const GLfloat *) &world[0] );

		if(mpIndexBuffer != NULL)
			GL_CHECK(glDrawElementsInstancedEXT( GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, NULL, numInstances ));
		else
			GL_CHECK(glDrawArraysInstanced(GL_TRIANGLES, 0, numInstances, mVertexCount));
	}
	else
	{
		index = 0;
		glUniformMatrix4fv( vpMatrixBufferLoc, 1, GL_FALSE, (const GLfloat *) &viewProj );

		for (int i = 0; i < rows; ++i)
		{
			for (int j = 0; j < columns; ++j)
			{
				glUniformMatrix4fv( worldMatrixBufferLoc, 1, GL_FALSE, (const GLfloat *) &world[index] );
				if(mpIndexBuffer != NULL)
					GL_CHECK(glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, NULL));
				else
					GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, mVertexCount));
				++index;
			}
		}
	}

	((CPUTMeshOGL*)pMesh)->DisableVertexArray();
    
    glUseProgram(0);

	return true;
}

// Handle resize events
//-----------------------------------------------------------------------------
void MySample::ResizeWindow(UINT width, UINT height)
{
    PositionInstancingControlButtons(width, height);
}

CPUTKey ConvertToCPUTKey(int aKey)
{
    if ((aKey >= AKEYCODE_0) && (aKey <= AKEYCODE_9))
        return (CPUTKey)(KEY_0 + aKey - AKEYCODE_0);

    if ((aKey >= AKEYCODE_A) && (aKey <= AKEYCODE_Z))
        return (CPUTKey)(KEY_A + aKey - AKEYCODE_A);

    switch (aKey)
    {
    case AKEYCODE_HOME:
        return KEY_HOME;
    case AKEYCODE_STAR:
        return KEY_STAR;
    case AKEYCODE_POUND:
        return KEY_HASH;
    case AKEYCODE_COMMA:
        return KEY_COMMA;
    case AKEYCODE_PERIOD:
        return KEY_PERIOD;
    case AKEYCODE_ALT_LEFT:
        return KEY_LEFT_ALT;
    case AKEYCODE_ALT_RIGHT:
        return KEY_RIGHT_ALT;
    case AKEYCODE_SHIFT_LEFT:
        return KEY_LEFT_SHIFT;
    case AKEYCODE_SHIFT_RIGHT:
        return KEY_RIGHT_SHIFT;
    case AKEYCODE_TAB:
        return KEY_TAB;
    case AKEYCODE_SPACE:
        return KEY_SPACE;
    case AKEYCODE_ENTER:
        return KEY_ENTER;
    case AKEYCODE_DEL:
        return KEY_DELETE;
    case AKEYCODE_MINUS:
        return KEY_MINUS;
    case AKEYCODE_LEFT_BRACKET:
        return KEY_OPENBRACKET;
    case AKEYCODE_RIGHT_BRACKET:
        return KEY_CLOSEBRACKET;
    case AKEYCODE_BACKSLASH:
        return KEY_BACKSLASH;
    case AKEYCODE_SEMICOLON:
        return KEY_SEMICOLON;
    case AKEYCODE_APOSTROPHE:
        return KEY_SINGLEQUOTE;
    case AKEYCODE_SLASH:
        return KEY_SLASH;
    case AKEYCODE_AT:
        return KEY_AT;
    case AKEYCODE_PLUS:
        return KEY_PLUS;
    case AKEYCODE_PAGE_UP:
        return KEY_PAGEUP;
    case AKEYCODE_PAGE_DOWN:
        return KEY_PAGEDOWN;
    default:
    case AKEYCODE_SOFT_LEFT:
    case AKEYCODE_SOFT_RIGHT:
    case AKEYCODE_BACK:
    case AKEYCODE_CALL:
    case AKEYCODE_ENDCALL:
    case AKEYCODE_DPAD_UP:
    case AKEYCODE_DPAD_DOWN:
    case AKEYCODE_DPAD_LEFT:
    case AKEYCODE_DPAD_RIGHT:
    case AKEYCODE_DPAD_CENTER:
    case AKEYCODE_VOLUME_UP:
    case AKEYCODE_VOLUME_DOWN:
    case AKEYCODE_POWER:
    case AKEYCODE_CAMERA:
    case AKEYCODE_CLEAR:
    case AKEYCODE_SYM:
    case AKEYCODE_EXPLORER:
    case AKEYCODE_ENVELOPE:
    case AKEYCODE_GRAVE:
    case AKEYCODE_EQUALS:
    case AKEYCODE_NUM:
    case AKEYCODE_HEADSETHOOK:
    case AKEYCODE_FOCUS:
    case AKEYCODE_MENU:
    case AKEYCODE_NOTIFICATION:
    case AKEYCODE_SEARCH:
    case AKEYCODE_MEDIA_PLAY_PAUSE:
    case AKEYCODE_MEDIA_STOP:
    case AKEYCODE_MEDIA_NEXT:
    case AKEYCODE_MEDIA_PREVIOUS:
    case AKEYCODE_MEDIA_REWIND:
    case AKEYCODE_MEDIA_FAST_FORWARD:
    case AKEYCODE_MUTE:
    case AKEYCODE_PICTSYMBOLS:
    case AKEYCODE_SWITCH_CHARSET:
    case AKEYCODE_BUTTON_L1:
    case AKEYCODE_BUTTON_R1:
    case AKEYCODE_BUTTON_L2:
    case AKEYCODE_BUTTON_R2:
    case AKEYCODE_BUTTON_THUMBL:
    case AKEYCODE_BUTTON_THUMBR:
    case AKEYCODE_BUTTON_START:
    case AKEYCODE_BUTTON_SELECT:
    case AKEYCODE_BUTTON_MODE:
    case AKEYCODE_UNKNOWN:
        return KEY_NONE;
    }

}

CPUTKeyState ConvertToCPUTKeyState(int aAction)
{
    switch (aAction)
    {
    case AKEY_EVENT_ACTION_UP:
        return CPUT_KEY_UP;
    case AKEY_EVENT_ACTION_DOWN:
    default:
        return CPUT_KEY_DOWN;
    }
}

int32_t CPUT_OGL::cput_handle_input(struct android_app* app, AInputEvent* event)
{    
	int n;
    MySample *pSample = (MySample *)app->userData;
	int lEventType = AInputEvent_getType(event);
    static float drag_center_x = 0.0f, drag_center_y = 0.0f;
    static float dist_squared = 0.0f;
    static bool isPanning = false;
    
    switch (lEventType) 
	{
        case AINPUT_EVENT_TYPE_MOTION:
			{
                ndk_helper::GESTURE_STATE doubleTapState = pSample->mDoubletapDetector.Detect(event);
                ndk_helper::GESTURE_STATE dragState      = pSample->mDragDetector.Detect(event);
                ndk_helper::GESTURE_STATE pinchState     = pSample->mPinchDetector.Detect(event);

                if( doubleTapState == ndk_helper::GESTURE_STATE_ACTION )
                {
                    LOGI("DOUBLE TAP RECEIVED");
                }
                else
                {
                    //Handle drag state
                    if( dragState & ndk_helper::GESTURE_STATE_START )
                    {
                        if (isPanning == false) {
                            //LOGI("GESTURE_STATE_START - drag");
                            ndk_helper::Vec2 v;
                            float x, y;
                            pSample->mDragDetector.GetPointer( v );

                            v.Value(x, y);
                            //LOGI("     TOUCH POINT: %f, %f", x, y);
                            pSample->CPUTHandleMouseEvent(x, y, 0.0f, CPUT_MOUSE_LEFT_DOWN);
                        }
                    }
                    else if( dragState & ndk_helper::GESTURE_STATE_MOVE )
                    {
                        if (isPanning == false) {
                            //LOGI("GESTURE_STATE_MOVE - drag");
                            ndk_helper::Vec2 v;
                            float x, y;
                            pSample->mDragDetector.GetPointer( v );
                            v.Value(x, y);
                            pSample->CPUTHandleMouseEvent(x, y, 0.0f, CPUT_MOUSE_LEFT_DOWN);
                        }
                    }
                    else if( dragState & ndk_helper::GESTURE_STATE_END )
                    {
                        pSample->CPUTHandleMouseEvent(0.0f, 0.0f, 0.0f, CPUT_MOUSE_NONE );
                        pSample->HandleKeyboardEvent(KEY_A, CPUT_KEY_UP);
                        pSample->HandleKeyboardEvent(KEY_D, CPUT_KEY_UP);
                        pSample->HandleKeyboardEvent(KEY_E, CPUT_KEY_UP);
                        pSample->HandleKeyboardEvent(KEY_W, CPUT_KEY_UP);
                        pSample->HandleKeyboardEvent(KEY_S, CPUT_KEY_UP);
                        pSample->HandleKeyboardEvent(KEY_Q, CPUT_KEY_UP);
                        isPanning = false;
                        //LOGI("GESTURE_STATE_END - drag");
                    }

                    //Handle pinch state
                    if( pinchState & ndk_helper::GESTURE_STATE_START )
                    {
                        if (isPanning == false) {
                            //LOGI("GESTURE_STATE_START - pinch");
                            //Start new pinch
                            ndk_helper::Vec2 v1;
                            ndk_helper::Vec2 v2;
                            float x1, y1, x2, y2;
                            pSample->mPinchDetector.GetPointers( v1, v2 );
                            v1.Value(x1, y1);
                            v2.Value(x2, y2);
                            drag_center_x = (x1 + x2) / 2.0f;
                            drag_center_y = (y1 + y2) / 2.0f;
                            dist_squared = ((x2 - x1) * (x2 - x1)) + ((y2 - y1) * (y2 - y1));
                        }
                   }
                    else if( pinchState & ndk_helper::GESTURE_STATE_MOVE )
                    {
                        isPanning = true;

                        CPUTKey key = (CPUTKey)0;
                        CPUTKeyState state = (CPUTKeyState)0;

                        //LOGI("GESTURE_STATE_MOVE - pinch");
                        
                        ndk_helper::Vec2 v1;
                        ndk_helper::Vec2 v2;
                        float x1, y1, x2, y2;
                        float new_center_x, new_center_y;
                        float new_dist_squared;
                        float delta_x, delta_y;
                        pSample->mPinchDetector.GetPointers( v1, v2 );
                        v1.Value(x1, y1);
                        v2.Value(x2, y2);
                            
                        new_center_x = (x1 + x2) / 2.0f;
                        new_center_y = (y1 + y2) / 2.0f;

                        new_dist_squared = ((x2 - x1) * (x2 - x1)) + ((y2 - y1) * (y2 - y1));

                        delta_x = drag_center_x - new_center_x;
                        delta_y = drag_center_y - new_center_y;

                        //
                        // For each direction of movement, the opposite direction is cancelled (KEY_UP)
                        //

                        // Handle pinch and zoom actions
                        if (abs(new_dist_squared - dist_squared) > 1000.0f) {
                            if (new_dist_squared < dist_squared) {
                                pSample->HandleKeyboardEvent(KEY_S, CPUT_KEY_UP);
                                pSample->HandleKeyboardEvent(KEY_W, CPUT_KEY_DOWN);
                            } else {
                                pSample->HandleKeyboardEvent(KEY_W, CPUT_KEY_UP);
                                pSample->HandleKeyboardEvent(KEY_S, CPUT_KEY_DOWN);
                            } 
                        } else {
                            pSample->HandleKeyboardEvent(KEY_W, CPUT_KEY_UP);
                            pSample->HandleKeyboardEvent(KEY_S, CPUT_KEY_UP);
                        }

                        // handle left and right drag
                        if (delta_x >= 2.0f) {
                            pSample->HandleKeyboardEvent(KEY_A, CPUT_KEY_DOWN);
                            pSample->HandleKeyboardEvent(KEY_D, CPUT_KEY_UP);
                        } else if (delta_x <= -2.0f) {
                            pSample->HandleKeyboardEvent(KEY_D, CPUT_KEY_DOWN);
                            pSample->HandleKeyboardEvent(KEY_A, CPUT_KEY_UP);
                        } else if (delta_x < 2.0 && delta_x > -2.0) {
                            pSample->HandleKeyboardEvent(KEY_A, CPUT_KEY_UP);
                            pSample->HandleKeyboardEvent(KEY_D, CPUT_KEY_UP);
                        }

                        // handle up and down drag
                        if (delta_y >= 2.0f) {
                            pSample->HandleKeyboardEvent(KEY_Q, CPUT_KEY_UP);
                            pSample->HandleKeyboardEvent(KEY_E, CPUT_KEY_DOWN);
                        } else if (delta_y <= -2.0f) {
                            pSample->HandleKeyboardEvent(KEY_E, CPUT_KEY_UP);
                            pSample->HandleKeyboardEvent(KEY_Q, CPUT_KEY_DOWN);
                        } else if (delta_y < 2.0 && delta_y > -2.0) {
                            pSample->HandleKeyboardEvent(KEY_E, CPUT_KEY_UP);
                            pSample->HandleKeyboardEvent(KEY_Q, CPUT_KEY_UP);
                        }

                        // current values become old values for next frame
                        dist_squared = new_dist_squared;
                        drag_center_x = new_center_x;
                        drag_center_y = new_center_y;
                    }
                }
                
			}
		case AINPUT_EVENT_TYPE_KEY:
			{
				int aKey = AKeyEvent_getKeyCode(event);
				CPUTKey cputKey = ConvertToCPUTKey(aKey);
				int aAction = AKeyEvent_getAction(event);
				CPUTKeyState cputKeyState = ConvertToCPUTKeyState(aAction);
				pSample->CPUTHandleKeyboardEvent(cputKey, cputKeyState);
				return 1;
			}
		default:
			return 0;
    }
    
    return 0;
}

static void cput_handle_cmd(struct android_app* app, int32_t cmd)
{
    MySample *pSample = (MySample *)app->userData;

	switch (cmd)
    {
    case APP_CMD_SAVE_STATE:
        LOGI("APP_CMD_SAVE_STATE");
        break;
    case APP_CMD_INIT_WINDOW:
        LOGI("APP_CMD_INIT_WINDOW");
        if (!pSample->HasWindow())
        {
            LOGI("Creating window");
            CPUTResult result;

            // window and device parameters
            CPUTWindowCreationParams params;
            params.samples = 1;

            // create the window and device context
            result = pSample->CPUTCreateWindowAndContext(_L("CPUTWindow OpenGLES"), params);
            if (result != CPUT_SUCCESS)
                LOGI("Unable to create window");
        }
        else
        {
            LOGI("Window already created");
        }
        break;
    case APP_CMD_TERM_WINDOW:
        LOGI("APP_CMD_TERM_WINDOW");
		exit(0);
        // Need clear window create and destroy calls
        // The window is being hidden or closed, clean it up.
        if (pSample->HasWindow())
        {
            pSample->DeviceShutdown();
        }
        break;
    case APP_CMD_GAINED_FOCUS:
        LOGI("APP_CMD_GAINED_FOCUS");
        break;
    case APP_CMD_LOST_FOCUS:
		exit(0);
        LOGI("APP_CMD_LOST_FOCUS");
        break;
	case APP_CMD_WINDOW_RESIZED:
        LOGI("APP_CMD_WINDOW_RESIZED");
        break;
    }
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(struct android_app* state)
{
    // Make sure glue isn't stripped.
    app_dummy();

    LOGI("\n\n********************************");
    LOGI("      STARTING SAMPLE");
    LOGI("********************************\n\n");

    // create an instance of my sample
    MySample *pSample = new MySample();
    if (!pSample)
    {
        LOGI("Failed to allocate MySample");
        return;
    }

     // Assign the sample back into the app state
    state->userData = pSample;
    state->onAppCmd = cput_handle_cmd;
    state->onInputEvent = CPUT_OGL::cput_handle_input;


    // We make the assumption we are running from the executable's dir in
    // the CPUT SampleStart directory or it won't be able to use the relative paths to find the default
    // resources    
    cString ResourceDirectory;

    CPUTFileSystem::GetExecutableDirectory(&ResourceDirectory);

    // Different executable and assets locations on different OS'es.
    // Consistency should be maintained in all OS'es and API's.
    ResourceDirectory.append(GUI_LOCATION);

	// Initialize the system and give it the base CPUT resource directory (location of GUI images/etc)
    // For now, we assume it's a relative directory from the executable directory.  Might make that resource
    // directory location an env variable/hardcoded later
    CPUTWindowAndroid::SetAppState(state);
	pSample->CPUTInitialize(ResourceDirectory);

    CPUTFileSystem::GetExecutableDirectory(&ResourceDirectory);

    // Different executable and assets locations on different OS'es.
    // Consistency should be maintained in all OS'es and API's.
    ResourceDirectory.append(SYSTEM_LOCATION);
    CPUTAssetLibrary *pAssetLibrary = CPUTAssetLibrary::GetAssetLibrary();
    pAssetLibrary->SetSystemDirectoryName(ResourceDirectory);

    // start the main message loop
    pSample->CPUTMessageLoop();

    // cleanup resources
    SAFE_DELETE(pSample);
    pSample = NULL;

    state->userData = NULL;
}
//END_INCLUDE(all)

// Handle any control callback events
//-----------------------------------------------------------------------------
void MySample::HandleCallbackEvent( CPUTEventID Event, CPUTControlID ControlID, CPUTControl *pControl )
{
	if(CPUT_EVENT_HANDLED == HandleInstancingControlButtons(Event, ControlID, pControl))
		return;
	switch(ControlID)
    {
    case ID_FULLSCREEN_BUTTON:
 //       CPUTToggleFullScreenMode();
        break;
    default:
        break;
    }
}

// Instancing Controls for Touch interface
void MySample::PositionInstancingControlButtons(int width, int height)
{
    int offset = INSTANCING_BUTTON_SIZE * 2;
    int top = height - 2 * offset;
    int left = 0;

    CPUTGuiControllerOGL *pGUI = (CPUTGuiControllerOGL*)CPUTGetGuiController();
    
    CPUTControl* pControl = NULL;
    pControl = pGUI->GetControl(ROW_INCREASE);
    pControl->SetPosition(left + 0 * offset, top + 0 * offset);

    pControl = pGUI->GetControl(ROW_DECREASE);
    pControl->SetPosition(left + 1 * offset, top + 0 * offset);

    pControl = pGUI->GetControl(COLUMN_INCREASE);
    pControl->SetPosition(left + 0 * offset, top + 1 * offset);
	
    pControl = pGUI->GetControl(COLUMN_DECREASE);
    pControl->SetPosition(left + 1 * offset, top + 1 * offset);
	
    pControl = pGUI->GetControl(TOGGLE_INSTANCING);
    pControl->SetPosition(left + 2 * offset, top + 0 * offset);
}

void MySample::CreateInstancingControlButtons()
{
    CPUTGuiControllerOGL *pGUI = (CPUTGuiControllerOGL*)CPUTGetGuiController();
    CPUTButton *pButton;

	if(isInstanced)
	{
		pGUI->CreateButton(_L("Enable Instancing     "), TOGGLE_INSTANCING, ID_MAIN_PANEL, &pButton);
		pButton->SetAutoArranged(true);
		pButton->SetDimensions(INSTANCING_BUTTON_SIZE, INSTANCING_BUTTON_SIZE);
	}
	else
	{
		pGUI->CreateButton(_L("Disable Instancing"), TOGGLE_INSTANCING, ID_MAIN_PANEL, &pButton);
		pButton->SetAutoArranged(true);
		pButton->SetDimensions(INSTANCING_BUTTON_SIZE, INSTANCING_BUTTON_SIZE);
	}

	pToggleButton = pButton;
    pGUI->CreateButton(_L("Add Row                 "), ROW_INCREASE, ID_MAIN_PANEL, &pButton);
    pButton->SetAutoArranged(true);
    pButton->SetDimensions(INSTANCING_BUTTON_SIZE, INSTANCING_BUTTON_SIZE);

    pGUI->CreateButton(_L("Remove Row          "), ROW_DECREASE, ID_MAIN_PANEL, &pButton);
    pButton->SetAutoArranged(true);
    pButton->SetDimensions(INSTANCING_BUTTON_SIZE, INSTANCING_BUTTON_SIZE);

    pGUI->CreateButton(_L("Add Column           "), COLUMN_INCREASE, ID_MAIN_PANEL, &pButton);
    pButton->SetAutoArranged(true);
    pButton->SetDimensions(INSTANCING_BUTTON_SIZE, INSTANCING_BUTTON_SIZE);
   
    pGUI->CreateButton(_L("Remove Column    "), COLUMN_DECREASE, ID_MAIN_PANEL, &pButton);
    pButton->SetAutoArranged(true);
    pButton->SetDimensions(INSTANCING_BUTTON_SIZE, INSTANCING_BUTTON_SIZE);
}

CPUTEventHandledCode MySample::HandleInstancingControlButtons(CPUTEventID Event, CPUTControlID ControlID, CPUTControl *pControl)
{

	CPUTKeyState state = (CPUTKeyState)0;
    if((ControlID == ROW_INCREASE) && (Event == CPUT_EVENT_DOWN))
	{
		if( currentRowCount < NUM_ROWS )
			++currentRowCount;
			if (cameraY < 1550.0f)
			{
				cameraY += 98.0f;
			}
        UpdateMatrixBuffers();
	}
    else if((ControlID == ROW_DECREASE) && (Event == CPUT_EVENT_DOWN))
	{
		if( currentRowCount > 1 )
			--currentRowCount;
			if (cameraY > 250.0f)
			{
				cameraY -= 98.0f;
			}
        UpdateMatrixBuffers();
	}
    else if((ControlID == COLUMN_INCREASE) && (Event == CPUT_EVENT_DOWN))
	{
		if( currentColCount < NUM_COLUMNS )
			++currentColCount;
        UpdateMatrixBuffers();
	}
    else if((ControlID == COLUMN_DECREASE) && (Event == CPUT_EVENT_DOWN))
	{
		if( currentColCount > 9 )
			--currentColCount;
        UpdateMatrixBuffers();
	}
	else if((ControlID == TOGGLE_INSTANCING) && (Event == CPUT_EVENT_DOWN))
	{
		if( isInstanced == true )
		{
			// change text on button
			isInstanced = false;
			pToggleButton->SetText("Enable Instancing");
		}
		else
		{
			isInstanced = true;
			pToggleButton->SetText("Disable Instancing");
		}
	}
    
	//if(Event == CPUT_EVENT_DOWN)
     //   state = CPUT_KEY_DOWN;

    //else if (Event == CPUT_KEY_UP || Event == CPUT_EVENT_OUT || Event == CPUT_EVENT_CLICK)
     //   state = CPUT_KEY_UP;
	return CPUT_EVENT_HANDLED;
}
