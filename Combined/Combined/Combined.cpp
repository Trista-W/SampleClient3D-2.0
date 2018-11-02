/* 
Copyright ?2012 NaturalPoint Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */


// NatNetSample.cpp : Defines the entry point for the application.
//
#ifdef WIN32
#  define _CRT_SECURE_NO_WARNINGS
#  define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#endif

#include <cstring> // For memset.
#include <windows.h>
#include <winsock.h>
#include "resource.h"
#include <algorithm>


// Include GLEW
#include <GL/glew.h>


#include <GL/gl.h>
#include <GL/glu.h>

//NatNet SDK
#include "NatNetTypes.h"
#include "../include/NatNetTypes.h"
#include "../include/NatNetCAPI.h"
#include "../include/NatNetClient.h"
#include "natutils.h"

#include "GLPrint.h"
#include "RigidBodyCollection.h"
#include "MarkerPositionCollection.h"
#include "OpenGLDrawingFunctions.h"

#include <map>
#include <string>

#include <math.h>

#include <stdio.h>
#include <stdlib.h>



// Include GLFW
//#include <GLFW/glfw3.h>
//GLFWwindow* window;

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

#include <common/shader.hpp>
#include <common/shader.cpp>
#include <common/texture.hpp>
#include <common/texture.cpp>


#define ID_RENDERTIMER 101

#define MATH_PI 3.14159265F

// globals
// Class for printing bitmap fonts in OpenGL
GLPrint glPrinter;

// Creating a window


HINSTANCE hInst;

// OpenGL rendering context.
// HGLRC is a type of OpenGL context
HGLRC openGLRenderContext = nullptr;

// Our NatNet client object.
NatNetClient natnetClient;

// Objects for saving off marker and rigid body data streamed
// from NatNet.
MarkerPositionCollection markerPositions;
RigidBodyCollection rigidBodies;

std::map<int, std::string> mapIDToName;

// Ready to render?
bool render = true;

// Show rigidbody info
bool showText = true;

// Used for converting NatNet data to the proper units.
float unitConversion = 1.0f;

// World Up Axis (default to Y)
int upAxis = 1; // 

// NatNet server IP address.
int IPAddress[4] = { 127, 0, 0, 1 };

// Timecode string 
char szTimecode[128] = "";

// Initial Eye position and rotation
float g_fEyeX = 0, g_fEyeY = 1, g_fEyeZ = 5;
float g_fRotY = 0;
float g_fRotX = 0;


// functions
// Win32
BOOL InitInstance(HINSTANCE, int);
// HWND-A handle to the window; UINT-The message; WPARAM-Additional message information; LPARAM-Same as WPARAM;
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK NatNetDlgProc(HWND, UINT, WPARAM, LPARAM);
// OpenGL
void RenderOGLScene();
void Update(HWND hWnd);
// NatNet
void NATNET_CALLCONV DataHandler(sFrameOfMocapData* data, void* pUserData);    // receives data from the server
void NATNET_CALLCONV MessageHandler(Verbosity msgType, const char* msg);      // receives NatNet error messages
bool InitNatNet(LPSTR szIPAddress, LPSTR szServerIPAddress, ConnectionType connType);
bool ParseRigidBodyDescription(sDataDescriptions* pDataDefs);

//****************************************************************************
//
// Windows Functions 
//

// Register our window.
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);  // Sizeof, used when actual size must be known
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wcex.lpfnWndProc = (WNDPROC)WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = NULL;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCE(IDC_NATNETSAMPLE);
    wcex.lpszClassName = "NATNETSAMPLE";
    wcex.hIconSm = NULL;

    return RegisterClassEx(&wcex);
}

// WinMain
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
	
	MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
        return false;

    MSG msg;
    while (true)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
        {
            if (!GetMessage(&msg, NULL, 0, 0))
                break;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            if (render)
                Update(msg.hwnd);
        }
    }

    return (int)msg.wParam;
}

// Initialize new instances of our application
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    HWND hWnd = CreateWindow("NATNETSAMPLE", "Optitrack Tracking", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);
	// createwindow(lpClassName, lpWindowName, dwstyle[overlapped window/ pop-up window/ child window], x, y, nWidth, nHeight, hWndParent, hM)

    if (!hWnd)
        return false;

    // Define pixel format; create an OpenGl rendering context
    PIXELFORMATDESCRIPTOR pfd;
    int nPixelFormat;
    memset(&pfd, NULL, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 16;
    pfd.iLayerType = PFD_MAIN_PLANE;

    // Set pixel format. Needed for drawing OpenGL bitmap fonts.
    HDC hDC = GetDC(hWnd);
    nPixelFormat = ChoosePixelFormat(hDC, &pfd);// Convert the PIXELFORMATDESCRIPTOR into a pixel format number
												// that represents the closest match it can find in the list of supported pixel formats
    SetPixelFormat(hDC, nPixelFormat, &pfd);    // Set the pixel number into the DC, this function takes the DC, the pixel format number, and a PFD struct pointer

    // Create and set the current OpenGL rendering context.
    openGLRenderContext = wglCreateContext(hDC);   // This function takes the DC as a parameter and returns a handle to the the OpenGL context 
												   // (of type HGLRC, for handle to GL Rendering Context).
    wglMakeCurrent(hDC, openGLRenderContext);      // Before using OpenGL, the created context must be current.

    // Set some OpenGL options.
    glClearColor(0.400f, 0.400f, 0.400f, 1.0f);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);   // Enable the depth buffer for depth test

    // Set the device context for our OpenGL printer object.
    glPrinter.SetDeviceContext(hDC);

    wglMakeCurrent(0, 0);
    ReleaseDC(hWnd, hDC);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Make a good guess as to the IP address of our NatNet server.
    //in_addr MyAddress[10];
    //int nAddresses = NATUtils::GetLocalIPAddresses((unsigned long *)&MyAddress, 10);
    //if (nAddresses > 0)
    //{
    //    IPAddress[0] = MyAddress[0].S_un.S_un_b.s_b1;
    //    IPAddress[1] = MyAddress[0].S_un.S_un_b.s_b2;
    //    IPAddress[2] = MyAddress[0].S_un.S_un_b.s_b3;
    //    IPAddress[3] = MyAddress[0].S_un.S_un_b.s_b4;
    //}

    // schedule to render on UI thread every 30 milliseconds�� UI thread is the main thread in the application
    UINT renderTimer = SetTimer(hWnd, ID_RENDERTIMER, 30, NULL);
	// (Handle to main window, timer identifier, time interval, no timer callback)
    return true;
}


// Windows message processing function.
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    PAINTSTRUCT ps;
    HDC hdc;

    switch (message)    // The message parameter contains the message sent
    {
    case WM_COMMAND:    // Handle menu selections, etc.
        wmId = LOWORD(wParam);
        wmEvent = HIWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_CONNECT:
            DialogBox(hInst, (LPCTSTR)IDD_NATNET, hWnd, (DLGPROC)NatNetDlgProc);
			// (A module which contains the dialog box template;
            // The dialog box template;
            // A handle to the window that owns the dialog box;
            // A pointer to the dialog box procedure)
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
			// Calls the default window procedure to provide default processing for any window messages that an application does not process.
			// This function ensures that every message is processed.
        }
        break;

    case WM_TIMER:      // To process messages the WM_TIMER messages generated by the timer ID_RENDERTIMER
        if (wParam == ID_RENDERTIMER)
            Update(hWnd);
        break;

    case WM_KEYDOWN:   // KEYUP ALSO��
    {
        bool bShift = (GetKeyState(VK_SHIFT) & 0x80) != 0;   // nVirtKey-	Determines how to process the keystroke
        bool bCtrl = (GetKeyState(VK_CONTROL) & 0x80) != 0;
        switch (wParam)
        {
        case VK_UP:    // Process the UP ARROW key
            if (bCtrl)
                g_fRotX += 1;
            else if (bShift)
                g_fEyeY += 0.03f;
            else
                g_fEyeZ -= 0.03f;
            break;
        case VK_DOWN:
            if (bCtrl)
                g_fRotX -= 1;
            else if (bShift)
                g_fEyeY -= 0.03f;
            else
                g_fEyeZ += 0.03f;
            break;
        case VK_LEFT:
            if (bCtrl)
                g_fRotY += 1;
            else
                g_fEyeX -= 0.03f;
            break;
        case VK_RIGHT:
            if (bCtrl)
                g_fRotY -= 1;
            else
                g_fEyeX += 0.03f;
            break;
        case 'T':
        case 't':
            showText = !showText;
            break;
        }
        InvalidateRect(hWnd, NULL, TRUE);
    }
        break;

    case WM_PAINT:   // An application makes a request to paint a portion of an application's window
        hdc = BeginPaint(hWnd, &ps);  // Retrieve the display device context
        Update(hWnd);
        EndPaint(hWnd, &ps);
        break;

    case WM_SIZE:   // Set display resolution or resizing requested
    {
        int cx = LOWORD(lParam), cy = HIWORD(lParam);
        if (cx != 0 && cy != 0 && hWnd != nullptr)
        {
            GLfloat fFovy = 40.0f; // Field-of-view
            GLfloat fZNear = 1.0f;  // Near clipping plane
            GLfloat fZFar = 10000.0f;  // Far clipping plane

            HDC hDC = GetDC(hWnd);
            wglMakeCurrent(hDC, openGLRenderContext);

            // Calculate OpenGL viewport aspect
            RECT rv;
            GetClientRect(hWnd, &rv);  // Retrieve the coordinates of a window's client area, which specify the upper-left and lower-right corners. 
			                           // The upper-left i (0,0)
            GLfloat fAspect = (GLfloat)(rv.right - rv.left) / (GLfloat)(rv.bottom - rv.top);   // Aspect ratio = width / height

            // Define OpenGL viewport
            glMatrixMode(GL_PROJECTION);  // Transform eye space coordinates into clip coordinates
            glLoadIdentity();
            gluPerspective(fFovy, fAspect, fZNear, fZFar);   // Sets up a perspective projection matrix
			// (The field of view angle, in degrees, in the y-direction; 
            // Determines the field of view in the x-direction. The aspect ratio is the ratio of x (width) to y (height);
            // The distance from the viewer to the near clipping plane (always positive);
            // The distance from the viewer to the far clipping plane (always positive))
            glViewport(rv.left, rv.top, rv.right - rv.left, rv.bottom - rv.top);
            glMatrixMode(GL_MODELVIEW);
			// Viewing transformation, OpenGL moves the scene with the inverse of the camera transformation//
			// Transform object space coordinates into eye space coordinates,
			// Think of the projection matrix as describing the attributes of your camera, 
			// such as field of view, focal length, fish eye lens, etc. 
			// Think of the ModelView matrix as where you stand with the camera and the direction you point it.//

            Update(hWnd);

            wglMakeCurrent(0, 0);
            ReleaseDC(hWnd, hDC);
        }
    }
        break;

    case WM_DESTROY:
    {
        HDC hDC = GetDC(hWnd);
        wglMakeCurrent(hDC, openGLRenderContext);
        natnetClient.Disconnect();
        wglMakeCurrent(0, 0);
        wglDeleteContext(openGLRenderContext);
        ReleaseDC(hWnd, hDC);
        PostQuitMessage(0);
    }
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

// Update OGL window
void Update(HWND hwnd)
{
    HDC hDC = GetDC(hwnd);
	// FOR DISPLAY: To carry out drawing that must occur instantly rather than when a WM_PAINT message is processing
	// Such drawing is usually in response to an action by the user, such as making a selection or drawing with the mouse,
	// In such cases, the user should receive instant feedback and must not be forced to stop selecting or drawing in order for the application to display the result
    if (hDC)
    {
        wglMakeCurrent(hDC, openGLRenderContext);
        RenderOGLScene();
        SwapBuffers(hDC);  // Switch the front and back buffer
        wglMakeCurrent(0, 0);
    }
    ReleaseDC(hwnd, hDC);
}

void ConvertRHSPosZupToYUp(float& x, float& y, float& z)
{
    /*
    [RHS, Y-Up]     [RHS, Z-Up]

                          Y
     Y                 Z /
     |__ X             |/__ X
     /
    Z

    Xyup  =  Xzup
    Yyup  =  Zzup
    Zyup  =  -Yzup
    */
    float yOriginal = y;
    y = z;
    z = -yOriginal;
}

void ConvertRHSRotZUpToYUp(float& qx, float& qy, float& qz, float& qw)
{
    // -90 deg rotation about +X
    float qRx, qRy, qRz, qRw;
    float angle = -90.0f * MATH_PI / 180.0f;
    qRx = sin(angle / 2.0f);
    qRy = 0.0f;
    qRz = 0.0f;
    qRw = cos(angle / 2.0f);

    // rotate quat using quat multiply
    float qxNew, qyNew, qzNew, qwNew;
    qxNew = qw*qRx + qx*qRw + qy*qRz - qz*qRy;
    qyNew = qw*qRy - qx*qRz + qy*qRw + qz*qRx;
    qzNew = qw*qRz + qx*qRy - qy*qRx + qz*qRw;
    qwNew = qw*qRw - qx*qRx - qy*qRy - qz*qRz;

    qx = qxNew;
    qy = qyNew;
    qz = qzNew;
    qw = qwNew;
}

// Render OpenGL scene
void RenderOGLScene()
{
    //GLfloat m[9];
    GLfloat v[3];
    float fRadius = 5.0f;

	glewExperimental = true; // Needed for core profile
    if (glewInit() != GLEW_OK) {
	fprintf(stderr, "Failed to initialize GLEW\n");
	//return -1;
    }
    // Setup OpenGL viewport
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear buffers
    glLoadIdentity(); // Load identity matrix
    GLfloat glfLightPos[] = { -4.0f, 4.0f, 4.0f, 0.0f };
	// Specify the position of the light in homogeneous object coordinates
	// The forth parameter w is 0.0, which means the light is treated as an directional source
    GLfloat glfLightAmb[] = { .3f, .3f, .3f, 1.0f };  // Specify the ambient RGBA intensity of the light
    glLightfv(GL_LIGHT0, GL_AMBIENT, glfLightAmb);  // The light position is transformed by the modelview matrix and stored in eye coordinates
    glLightfv(GL_LIGHT1, GL_POSITION, glfLightPos);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	// Causes a material color to track the current color
	// Both the front and back material parameters and ambient and diffuse material parameters track the current color

    glPushMatrix();   // Save the current matrix stack


    // Draw timecode
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);   // Set the current color(red, green, blue, alpha-opacity)
    glPushMatrix();
    glTranslatef(2400.f, -1750.f, -5000.0f);   // Multiplies the current matrix by a translation matrix(x, y, z)
    glPrinter.Print(0.0f, 0.0f, szTimecode);
    glPopMatrix();

    // Position and rotate the camera
    glTranslatef(g_fEyeX * -1000, g_fEyeY * -1000, g_fEyeZ * -1000);
    glRotatef(g_fRotY, 0, 1, 0);   // Multiplies the current matrix by a rotation matrix
    glRotatef(g_fRotX, 1, 0, 0);
    

    // Draw reference axis triad
    // x
    glLineWidth(3.0f);
    glBegin(GL_LINES);
    glColor3f(.8f, 0.0f, 0.0f);
    glVertex3f(0, 0, 0);
    glVertex3f(300, 0, 0);
    // y
    glColor3f(0.0f, .8f, 0.0f);
    glVertex3f(0, 0, 0);
    glVertex3f(0, 300, 0);
    // z
    glColor3f(0.0f, 0.0f, .8f);
    glVertex3f(0, 0, 0);
    glVertex3f(0, 0, 300);
    glEnd();

    //// Draw grid
    //glLineWidth(1.0f);
    //OpenGLDrawingFunctions::DrawGrid();
	/////////////////////////////////////////////////////////////////////////



	//// Enable depth test
	//glEnable(GL_DEPTH_TEST);
	//// Accept fragment if it is closer to the camera than the former one
	//glDepthFunc(GL_LESS);

	//GLuint VertexArrayID;
	//glGenVertexArrays(1, &VertexArrayID);
	//glBindVertexArray(VertexArrayID);

	//// Create and compile our GLSL program from the shaders
	//GLuint programID = LoadShaders("TransformVertexShader.vertexshader", "TextureFragmentShader.fragmentshader");

	//// Get a handle for our "MVP" uniform
	//GLuint MatrixID = glGetUniformLocation(programID, "MVP");

	//// Projection matrix : 45?Field of View, 4:3 ratio, display range : 0.1 unit <-> 100 units
	//glm::mat4 Projection = glm::perspective(glm::radians(45.0f), 1.0f / 1.0f, 0.1f, 100.0f);
	//// Camera matrix
	//glm::mat4 View = glm::lookAt(
	//	glm::vec3(0, 3, 5), // Camera is at (0,3,5), in World Space
	//	glm::vec3(0, 0, 0), // and looks at the origin
	//	glm::vec3(0, 1, 0)  // Head is up (set to 0,-1,0 to look upside-down)
	//);
	//// Model matrix : an identity matrix (model will be at the origin)
	//glm::mat4 Model = glm::mat4(1.0f);
	//// Our ModelViewProjection : multiplication of our 3 matrices
	//glm::mat4 MVP = Projection * View * Model; // Remember, matrix multiplication is the other way around

	//// Load the texture using any two methods
	//GLuint Texture = loadBMP_custom("Texture.bmp");
	////GLuint Texture = loadDDS("uvtemplate.DDS");

	//// Get a handle for our "myTextureSampler" uniform
	//GLuint TextureID = glGetUniformLocation(programID, "myTextureSampler");

	//// Our vertices. Tree consecutive floats give a 3D vertex; Three consecutive vertices give a triangle.
	//// A cube has 6 faces with 2 triangles each, so this makes 6*2=12 triangles, and 12*3 vertices
	//static const GLfloat g_vertex_buffer_data[] = {

	//	 1.0f,-1.0f, 1.0f,
	//	-1.0f,-1.0f,-1.0f,
	//	 1.0f,-1.0f,-1.0f,

	//	 1.0f,-1.0f, 1.0f,
	//	-1.0f,-1.0f, 1.0f,
	//	-1.0f,-1.0f,-1.0f,


	//};

	//// Two UV coordinatesfor each vertex. They were created with Blender.
	//static const GLfloat g_uv_buffer_data[] = {


	//	0.3f, 0.3f,
	//	0.0f, 0.0f,
	//	0.3f, 0.0f,
	//	0.3f, 0.3f,
	//	0.0f, 0.3f,
	//	0.0f, 0.0f


	//};

	//GLuint vertexbuffer;
	//glGenBuffers(1, &vertexbuffer);
	//glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	//glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);

	//GLuint uvbuffer;
	//glGenBuffers(1, &uvbuffer);
	//glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
	//glBufferData(GL_ARRAY_BUFFER, sizeof(g_uv_buffer_data), g_uv_buffer_data, GL_STATIC_DRAW);

	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	//	// Clear the screen
	//	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//	// Use our shader
	//	glUseProgram(programID);

	//	// Send our transformation to the currently bound shader, 
	//	// in the "MVP" uniform
	//	glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);

	//	// Bind our texture in Texture Unit 0
	//	glActiveTexture(GL_TEXTURE0);
	//	glBindTexture(GL_TEXTURE_2D, Texture);
	//	// Set our "myTextureSampler" sampler to use Texture Unit 0
	//	glUniform1i(TextureID, 0);

	//	// 1rst attribute buffer : vertices
	//	glEnableVertexAttribArray(0);
	//	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	//	glVertexAttribPointer(
	//		0,                  // attribute. No particular reason for 0, but must match the layout in the shader.
	//		3,                  // size
	//		GL_FLOAT,           // type
	//		GL_FALSE,           // normalized?
	//		0,                  // stride
	//		(void*)0            // array buffer offset
	//	);

	//	// 2nd attribute buffer : UVs
	//	glEnableVertexAttribArray(1);
	//	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
	//	glVertexAttribPointer(
	//		1,                                // attribute. No particular reason for 1, but must match the layout in the shader.
	//		2,                                // size : U+V => 2
	//		GL_FLOAT,                         // type
	//		GL_FALSE,                         // normalized?
	//		0,                                // stride
	//		(void*)0                          // array buffer offset
	//	);

	//	// Draw the triangle !
	//	glDrawArrays(GL_TRIANGLES, 0, 4 * 3); // 12*3 indices starting at 0 -> 12 triangles


	//	glDisableVertexAttribArray(0);
	//	glDisableVertexAttribArray(1);




	//// Cleanup VBO and shader
	//glDeleteBuffers(1, &vertexbuffer);
	//glDeleteBuffers(1, &uvbuffer);
	//glDeleteProgram(programID);
	//glDeleteTextures(1, &Texture);
	//glDeleteVertexArrays(1, &VertexArrayID);



	////////////////////////////////////////////////////////////////////////

    // Draw rigid bodies
    float textX = -3200.0f;
    float textY = 2700.0f;
    GLfloat x, y, z;
    Quat q;
    EulerAngles ea;
    int order;

    for (size_t i = 0; i < rigidBodies.Count(); i++)
    {
        // RigidBody position
        std::tie(x, y, z) = rigidBodies.GetCoordinates(i);
        // convert to millimeters
        x *= unitConversion;
        y *= unitConversion;
        z *= unitConversion;

        // RigidBody orientation
        GLfloat qx, qy, qz, qw;
        std::tie(qx, qy, qz, qw) = rigidBodies.GetQuaternion(i);
        q.x = qx;
        q.y = qy;
        q.z = qz;
        q.w = qw;

        // If Motive is streaming Z-up, convert to this renderer's Y-up coordinate system
        if (upAxis==2)
        {
            // convert position
            ConvertRHSPosZupToYUp(x, y, z);
            // convert orientation
            ConvertRHSRotZUpToYUp(q.x, q.y, q.z, q.w);
        }

        // Convert Motive quaternion output to euler angles
        // Motive coordinate conventions : X(Pitch), Y(Yaw), Z(Roll), Relative, RHS
        order = EulOrdXYZr;
        ea = Eul_FromQuat(q, order);

        ea.x = NATUtils::RadiansToDegrees(ea.x);
        ea.y = NATUtils::RadiansToDegrees(ea.y);
        ea.z = NATUtils::RadiansToDegrees(ea.z);

        // Draw RigidBody as cube
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glPushMatrix();

        glTranslatef(x, y, z);

        // source is Y-Up (default)
        glRotatef(ea.x, 1.0f, 0.0f, 0.0f);
        glRotatef(ea.y, 0.0f, 1.0f, 0.0f);
        glRotatef(ea.z, 0.0f, 0.0f, 1.0f);

        /*
        // alternate Z-up conversion - convert only euler rotation interpretation
        //  Yyup  =  Zzup
        //  Zyup  =  -Yzup
        glRotatef(ea.x, 1.0f, 0.0f, 0.0f);
        glRotatef(ea.y, 0.0f, 0.0f, 1.0f);
        glRotatef(ea.z, 0.0f, -1.0f, 0.0f);
        */

        OpenGLDrawingFunctions::DrawCube(100.0f);
        glPopMatrix();
        glPopAttrib();

        if (showText)
        {
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            std::string rigidBodyName = mapIDToName.at(rigidBodies.ID(i));
            glPrinter.Print(textX, textY, "%s (Pitch: %3.1f, Yaw: %3.1f, Roll: %3.1f)", rigidBodyName.c_str(), ea.x, ea.y, ea.z);
            textY -= 100.0f;
        }

    }

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	for (size_t i = 0; i < markerPositions.LabeledMarkerPositionCount(); i++)
	{
		const sMarker& markerData = markerPositions.GetLabeledMarker(i);

		// Set color dependent on marker params for labeled/unlabeled
		if ((markerData.params & 0x10) != 0) 
			glColor4f(0.8f, 0.4f, 0.0f, 0.8f);
		else
			glColor4f(0.8f, 0.8f, 0.8f, 0.8f);

		v[0] = markerData.x * unitConversion;
		v[1] = markerData.y * unitConversion;
		v[2] = markerData.z * unitConversion;
		fRadius = markerData.size * unitConversion;

		// If Motive is streaming Z-up, convert to this renderer's Y-up coordinate system
		if (upAxis == 2)
		{
			ConvertRHSPosZupToYUp(v[0], v[1], v[2]);
		}

		glPushMatrix();
		glTranslatef(v[0], v[1], v[2]);
		OpenGLDrawingFunctions::DrawSphere(1, fRadius);
		glPopMatrix();

	}
	glPopAttrib();

    // Done rendering a frame. The NatNet callback function DataHandler
    // will set render to true when it receives another frame of data.
    render = false;

}

// Callback for the connect-to-NatNet dialog. Gets the server and local IP 
// addresses and attempts to initialize the NatNet client.
LRESULT CALLBACK NatNetDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    char szBuf[512];
    switch (message)
    {
    case WM_INITDIALOG:
        SetDlgItemText(hDlg, IDC_EDIT1, _itoa(IPAddress[0], szBuf, 10));
        SetDlgItemText(hDlg, IDC_EDIT2, _itoa(IPAddress[1], szBuf, 10));
        SetDlgItemText(hDlg, IDC_EDIT3, _itoa(IPAddress[2], szBuf, 10));
        SetDlgItemText(hDlg, IDC_EDIT4, _itoa(IPAddress[3], szBuf, 10));
        SetDlgItemText(hDlg, IDC_EDIT5, _itoa(IPAddress[0], szBuf, 10));
        SetDlgItemText(hDlg, IDC_EDIT6, _itoa(IPAddress[1], szBuf, 10));
        SetDlgItemText(hDlg, IDC_EDIT7, _itoa(IPAddress[2], szBuf, 10));
        SetDlgItemText(hDlg, IDC_EDIT8, _itoa(IPAddress[3], szBuf, 10));
        SendDlgItemMessage( hDlg, IDC_COMBO_CONNTYPE, CB_ADDSTRING, 0, (LPARAM)TEXT( "Multicast" ) );
        SendDlgItemMessage( hDlg, IDC_COMBO_CONNTYPE, CB_ADDSTRING, 0, (LPARAM)TEXT( "Unicast" ) );
        SendDlgItemMessage( hDlg, IDC_COMBO_CONNTYPE, CB_SETCURSEL, 0, 0 );
        return true;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_CONNECT:
        {
            char szMyIPAddress[30], szServerIPAddress[30];
            char ip1[5], ip2[5], ip3[5], ip4[5];
            GetDlgItemText(hDlg, IDC_EDIT1, ip1, 4);
            GetDlgItemText(hDlg, IDC_EDIT2, ip2, 4);
            GetDlgItemText(hDlg, IDC_EDIT3, ip3, 4);
            GetDlgItemText(hDlg, IDC_EDIT4, ip4, 4);
            sprintf_s(szMyIPAddress, 30, "%s.%s.%s.%s", ip1, ip2, ip3, ip4);

            GetDlgItemText(hDlg, IDC_EDIT5, ip1, 4);
            GetDlgItemText(hDlg, IDC_EDIT6, ip2, 4);
            GetDlgItemText(hDlg, IDC_EDIT7, ip3, 4);
            GetDlgItemText(hDlg, IDC_EDIT8, ip4, 4);
            sprintf_s(szServerIPAddress, 30, "%s.%s.%s.%s", ip1, ip2, ip3, ip4);

            const ConnectionType connType = (ConnectionType)SendDlgItemMessage( hDlg, IDC_COMBO_CONNTYPE, CB_GETCURSEL, 0, 0 );

            // Try and initialize the NatNet client.
            if (InitNatNet( szMyIPAddress, szServerIPAddress, connType ) == false)
            {
                natnetClient.Disconnect();
                MessageBox(hDlg, "Failed to connect", "", MB_OK);
            }
        }
        case IDOK:
        case IDCANCEL:
            EndDialog(hDlg, LOWORD(wParam));
            return true;
        }
    }
    return false;
}

// Initialize the NatNet client with client and server IP addresses.
bool InitNatNet( LPSTR szIPAddress, LPSTR szServerIPAddress, ConnectionType connType )
{
    unsigned char ver[4];
    NatNet_GetVersion(ver);

    // Set callback handlers
    // Callback for NatNet messages.
    NatNet_SetLogCallback( MessageHandler );
    // this function will receive data from the server
    natnetClient.SetFrameReceivedCallback(DataHandler);

    sNatNetClientConnectParams connectParams;
    connectParams.connectionType = connType;
    connectParams.localAddress = szIPAddress;
    connectParams.serverAddress = szServerIPAddress;
    int retCode = natnetClient.Connect( connectParams );
    if (retCode != ErrorCode_OK)
    {
        //Unable to connect to server.
        return false;
    }
    else
    {
        // Print server info
        sServerDescription ServerDescription;
        memset(&ServerDescription, 0, sizeof(ServerDescription));
        natnetClient.GetServerDescription(&ServerDescription);
        if (!ServerDescription.HostPresent)
        {
            //Unable to connect to server. Host not present
            return false;
        }
    }

    // Retrieve RigidBody description from server
    sDataDescriptions* pDataDefs = NULL;
    retCode = natnetClient.GetDataDescriptionList(&pDataDefs);
    if (retCode != ErrorCode_OK || ParseRigidBodyDescription(pDataDefs) == false)
    {
        //Unable to retrieve RigidBody description
        //return false;
    }
    NatNet_FreeDescriptions( pDataDefs );
    pDataDefs = NULL;

    // example of NatNet general message passing. Set units to millimeters
    // and get the multiplicative conversion factor in the response.
    void* response;
    int nBytes;
    retCode = natnetClient.SendMessageAndWait("UnitsToMillimeters", &response, &nBytes);
    if (retCode == ErrorCode_OK)
    {
        unitConversion = *(float*)response;
    }

    retCode = natnetClient.SendMessageAndWait("UpAxis", &response, &nBytes);
    if (retCode == ErrorCode_OK)
    {
        upAxis = *(long*)response;
    }

    return true;
}

bool ParseRigidBodyDescription(sDataDescriptions* pDataDefs)
{
    mapIDToName.clear();

    if (pDataDefs == NULL || pDataDefs->nDataDescriptions <= 0)
        return false;

    // preserve a "RigidBody ID to Rigid Body Name" mapping, which we can lookup during data streaming
    int iSkel = 0;
    for (int i = 0, j = 0; i < pDataDefs->nDataDescriptions; i++)
    {
        if (pDataDefs->arrDataDescriptions[i].type == Descriptor_RigidBody)
        {
            sRigidBodyDescription *pRB = pDataDefs->arrDataDescriptions[i].Data.RigidBodyDescription;
            mapIDToName[pRB->ID] = std::string(pRB->szName);
        }
        else if (pDataDefs->arrDataDescriptions[i].type == Descriptor_Skeleton)
        {
            sSkeletonDescription *pSK = pDataDefs->arrDataDescriptions[i].Data.SkeletonDescription;
            for (int i = 0; i < pSK->nRigidBodies; i++)
            {
                // Note: Within FrameOfMocapData, skeleton rigid body ids are of the form:
                //   parent skeleton ID   : high word (upper 16 bits of int)
                //   rigid body id        : low word  (lower 16 bits of int)
                // 
                // However within DataDescriptions they are not, so apply that here for correct lookup during streaming
                int id = pSK->RigidBodies[i].ID | (pSK->skeletonID << 16);
                mapIDToName[id] = std::string(pSK->RigidBodies[i].szName);
            }
            iSkel++;
        }
        else
            continue;
    }

    return true;
}

// [Optional] Handler for NatNet messages. 
void NATNET_CALLCONV MessageHandler(Verbosity msgType, const char* msg)
{
    //	printf("\n[SampleClient] Message received: %s\n", msg);
}

// NatNet data callback function. Stores rigid body and marker data in the file level 
// variables markerPositions, and rigidBodies and sets the file level variable render
// to true. This signals that we have a frame ready to render.
void DataHandler(sFrameOfMocapData* data, void* pUserData)
{
    int mcount = minimum(MarkerPositionCollection::MAX_MARKER_COUNT, data->MocapData->nMarkers);
    markerPositions.SetMarkerPositions(data->MocapData->Markers, mcount);

    // Marker Data
    markerPositions.SetLabledMarkers(data->LabeledMarkers, data->nLabeledMarkers);

	// nOtherMarkers is deprecated
    // mcount = min(MarkerPositionCollection::MAX_MARKER_COUNT, data->nOtherMarkers);
    // markerPositions.AppendMarkerPositions(data->OtherMarkers, mcount);

    // rigid bodies
    int rbcount = minimum(RigidBodyCollection::MAX_RIGIDBODY_COUNT, data->nRigidBodies);
    rigidBodies.SetRigidBodyData(data->RigidBodies, rbcount);

    // skeleton segment (bones) as collection of rigid bodies
    for (int s = 0; s < data->nSkeletons; s++)
    {
        rigidBodies.AppendRigidBodyData(data->Skeletons[s].RigidBodyData, data->Skeletons[s].nRigidBodies);
    }

    // timecode
    NatNetClient* pClient = (NatNetClient*)pUserData;
    int hour, minute, second, frame, subframe;
    NatNet_DecodeTimecode( data->Timecode, data->TimecodeSubframe, &hour, &minute, &second, &frame, &subframe );
    // decode timecode into friendly string
    NatNet_TimecodeStringify( data->Timecode, data->TimecodeSubframe, szTimecode, 128 );

    render = true;
}
