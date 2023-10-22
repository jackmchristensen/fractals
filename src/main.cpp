/*********************************/
/* CS 590CGS Lab framework        */
/* (C) Bedrich Benes 2020        */
/* bbenes ~ at ~ purdue.edu      */
/* Press +,- to change order	 */
/*       r to randomize          */
/*       s to change rotation    */
/*       c to render curve       */
/*       t to render tangents    */
/*       p to render points      */
/*       s to change rotation    */
/*********************************/

#include <GL/glew.h>
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <time.h>
#include <string>
#include <vector>			//Standard template library class
#include <GL/freeglut.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>


//in house created libraries
#include "math/vect3d.h"    //for vector manipulation
#include "trackball.h"

#pragma warning(disable : 4996)
#pragma comment(lib, "freeglut.lib")

using namespace std;

//some trackball variables -> used in mouse feedback
TrackBallC trackball;
bool mouseLeft, mouseMid, mouseRight;


GLuint points=0;  //number of points to display the object
int steps=64;     //# of subdivisions
bool needRedisplay=false;
GLfloat  sign=+1; //diretcion of rotation
const GLfloat defaultIncrement=0.7f; //speed of rotation
GLfloat  angleIncrement=defaultIncrement;
float order = 8.f;
GLuint shader = -1;

vector <Vect3d> v;   //all the points will be stored here
vector <float> c;	 // Color scalar, multiplier, something like that


//window size
GLint wWindow=1200;
GLint hWindow=800;

//this defines what will be rendered
//see Key() how is it controlled
bool tangentsFlag = false;
bool pointsFlag = true;
bool curveFlag = false;

//Create a NULL-terminated string by reading the provided file
static char* ReadShaderSource(const char* shaderFile)
{
	ifstream ifs(shaderFile, ios::in | ios::binary | ios::ate);
	if (ifs.is_open())
	{
		unsigned int filesize = static_cast<unsigned int>(ifs.tellg());
		ifs.seekg(0, ios::beg);
		char* bytes = new char[filesize + 1];
		memset(bytes, 0, filesize + 1);
		ifs.read(bytes, filesize);
		ifs.close();
		return bytes;
	}
	return NULL;
}

static unsigned int CompileShader(unsigned int type, const std::string& source)
{
	unsigned int id = glCreateShader(type);
	const char* src = source.c_str();
	glShaderSource(id, 1, &src, nullptr);
	glCompileShader(id);

	int result;
	glGetShaderiv(id, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE)
	{
		int length;
		glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
		char* message = (char*)alloca(length * sizeof(char));
		glGetShaderInfoLog(id, length, &length, message);
		std::cout << "Failed to compile " << (type == GL_VERTEX_SHADER ? "vertex" : "fragment") << " shader!" << std::endl;
		std::cout << message << std::endl;
		glDeleteShader(id);
		return 0;
	}

	return id;
}

static unsigned int CreateShader(const std::string& vertexShaderFile, const std::string& fragmentShaderFile) 
{
	std::string vertexShader = ReadShaderSource(vertexShaderFile.c_str());
	std::string fragmentShader = ReadShaderSource(fragmentShaderFile.c_str());
	GLuint program = glCreateProgram();
	unsigned int vs = CompileShader(GL_VERTEX_SHADER, vertexShader);
	unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, fragmentShader);

	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	glValidateProgram(program);

	glDeleteShader(vs);
	glDeleteShader(fs);

	return program;
}

/*********************************
Some OpenGL-related functions DO NOT TOUCH
**********************************/
//displays the text message in the GL window
void GLMessage(char *message)
{
	static int i;
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0.f, 100.f, 0.f, 100.f);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glColor3ub(0, 0, 255);
	glRasterPos2i(10, 10);
	for (i = 0; i<(int)strlen(message); i++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, message[i]);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
}

//called when a window is reshaped
void Reshape(int w, int h)
{
	glViewport(0, 0, w, h);
	glEnable(GL_DEPTH_TEST);
	//remember the settings for the camera
	wWindow = w;
	hWindow = h;
}

//Some simple rendering routines using old fixed-pipeline OpenGL
//draws line from a to b with color 
void DrawLine(Vect3d a, Vect3d b, Vect3d color) {

	glColor3fv(color);
	glBegin(GL_LINES);
		glVertex3fv(a);
		glVertex3fv(b);
	glEnd();
}

//draws point at a with color 
void DrawPoint(Vect3d a, Vect3d color) {

	glColor3fv(color);
	glPointSize(2);
	glBegin(GL_POINTS);
	 glVertex3fv(a);
	glEnd();
}

/**********************
LAB related MODIFY
***********************/

// Defines the points that make up the mandelbulb
void MandelbulbPoints(vector<Vect3d>* a, vector<float>* color, int n) {
	float step = 1.f/n;

	// Iterates through a cube of points (i, j, k)
	for (float i = -1.f; i <= 1.f; i += step) {
		for (float j = -1.f; j <= 1.f; j += step) {
			bool edge = false;	// Only renders the edge
			for (float k = -1.f; k <= 1.f; k += step) {
				a->push_back(Vect3d(i, j, k));
				color->push_back(i);
				color->push_back(j);
				color->push_back(k);
				//Vect3d zeta = Vect3d(0.f, 0.f, 0.f);
				//int iter = 0;
				//int maxIter = 20; // Controlls detail of the fractal

				//while (true) {
				//	// Convert zeta to spherical coordinates
				//	// Could be moved to its own function to keep things clean
				//	float xx = zeta.x() * zeta.x();
				//	float yy = zeta.y() * zeta.y();
				//	float zz = zeta.z() * zeta.z();
				//	float rad = sqrt(xx + yy + zz);
				//	float theta = atan2(sqrt(xx + yy), zeta.z());
				//	float phi = atan2(zeta.y(), zeta.x());

				//	// Mandelbulb algorithm
				//	Vect3d newVect = Vect3d(pow(rad, order) * sin(theta * order) * cos(phi * order),
				//							pow(rad, order) * sin(theta * order) * sin(phi * order),
				//							pow(rad, order) * cos(theta * order));

				//	zeta = Vect3d(i + newVect.x(), j + newVect.y(), k + newVect.z());
				//	iter++;
				//	
				//	// Checks if zeta converges
				//	if (rad > 2.f) {
				//		if (edge) edge = false;
				//		break;
				//	}
				//	// Only saves point (i, j, k) if zeta diverges
				//	if (iter > maxIter) {
				//		if (!edge) {
				//			edge = true;
				//			Vect3d mandelPoint = Vect3d(i, j, k);
				//			float col = mandelPoint.Length() / sqrt(2.f);
				//			a->push_back(mandelPoint);
				//			color->push_back(col);
				//		}
				//		break;
				//	}
				//}
			}
		}
	}
}

//Call THIS for a new curve. It clears the old one first
void InitArray(int n)
{
	v.clear();
	c.clear();
	MandelbulbPoints(&v, &c,n); 
}

//returns random number from <-1,1>
inline float random11() { 
	return 2.f*rand() / (float)RAND_MAX - 1.f;
}

//randomizes an existing curve by adding random number to each coordinate
void Randomize(vector <Vect3d> *a) {
	const float intensity = 0.01f;
	for (unsigned int i = 0; i < a->size(); i++) {
		Vect3d r(random11(), random11(), random11());
		a->at(i) = a->at(i) + intensity*r;
	}
}

//display coordinate system
void CoordSyst() {
	Vect3d a, b, c;
	Vect3d origin(0, 0, 0);
	Vect3d red(1, 0, 0), green(0, 1, 0), blue(0, 0, 1), almostBlack(0.1f, 0.1f, 0.1f), yellow(1, 1, 0);

	//draw the coordinate system 
	a.Set(1, 0, 0);
	b.Set(0, 1, 0);
	c.Set(Vect3d::Cross(a, b)); //use cross product to find the last vector
	glLineWidth(4);
	DrawLine(origin, a, red);
	DrawLine(origin, b, green);
	DrawLine(origin, c, blue);
	glLineWidth(1);

}

//this is the actual code for the lab
void Lab01() {
	//Vect3d a,b,c;
	Vect3d origin(0, 0, 0);
	Vect3d red(1, 0, 0), green(0, 1, 0), blue(0, 0, 1), almostBlack(0.1f, 0.1f, 0.1f), yellow(1, 1, 0);


	CoordSyst();
	//draw the curve
	if (curveFlag)
		for (unsigned int i = 0; i < v.size() - 1; i++) {
		DrawLine(v[i], v[i + 1], almostBlack);
	}

	//draw the points
	if (pointsFlag)
		for (unsigned int i = 0; i < v.size() - 1; i++) {
		DrawPoint(v[i], Vect3d(c[i], c[i], 1.f));
	}

//draw the tangents
	if (tangentsFlag)
	for (unsigned int i = 0; i < v.size() - 1; i++) {
		Vect3d tan;
		tan = v[i + 1] - v[i]; //too simple - could be better from the point after AND before
		tan.Normalize(); 
		tan *= 0.2;
		DrawLine(v[i], v[i]+tan, red);
	}
}

//the main rendering function
void RenderObjects()
{
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	//set camera
	glMatrixMode(GL_MODELVIEW);
	trackball.Set3DViewCamera();
	//call the student's code from here
	//Lab01();
}

//Add here if you want to control some global behavior
//see the pointFlag and how is it used
void Kbd(unsigned char a, int x, int y)//keyboard callback
{
	switch (a)
	{
	case 27: exit(0); break;
	case 't': tangentsFlag = !tangentsFlag; break;
	case 'p': pointsFlag = !pointsFlag; break;
	case 'c': curveFlag = !curveFlag; break;
	case 32: {
		if (angleIncrement == 0) angleIncrement = defaultIncrement;
		else angleIncrement = 0;
		break;
	}
	case 's': {sign = -sign; break; }
	case '-': {
		order -= 0.5f;
		if (order < 1.f) order = 1.f;
		InitArray(steps);
		break;
	}
	case '+': {
		order += 0.5f;
		InitArray(steps);
		break;
	}
	//case 'r': {
	//	Randomize(&v);
	//	break;
	//}
	}
	cout << "[points]=[" << steps << "]" << endl;
	glutPostRedisplay();
}


/*******************
OpenGL code. Do not touch.
******************/
void Idle(void)
{
  glClearColor(0.5f,0.5f,0.5f,1); //background color
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
  GLMessage("Final Project: 3D Fractals - CS 590CGS");
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(40,(GLfloat)wWindow/(GLfloat)hWindow,0.01,100); //set the camera
  glMatrixMode(GL_MODELVIEW); //set the scene
  glLoadIdentity();
  gluLookAt(0,10,10,0,0,0,0,1,0); //set where the camera is looking at and from. 
  static GLfloat angle=0;
  angle+=angleIncrement;
  if (angle>=360.f) angle=0.f;
  glRotatef(sign*angle,0,1,0);
  RenderObjects();
  glutSwapBuffers();  
}

void Display(void)
{
	/*glm::mat4 V = glm::lookAt(glm::vec3(0.f, 0.f, 1.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 1.f, 0.f));
	glm::mat4 P = glm::perspective(glm::pi<float>() / 2.0f, (float)wWindow / (float)hWindow, 0.1f, 100.0f);

	glUniformMatrix4fv(glGetUniformLocation(shader, "V"), 1, false, glm::value_ptr(V));
	glUniformMatrix4fv(glGetUniformLocation(shader, "P"), 1, false, glm::value_ptr(P));*/

	//glUseProgram(shader);
}

void Mouse(int button, int state, int x, int y) {
	if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
	{
		trackball.Set(true, x, y);
		mouseLeft = true;
	}
	if (button == GLUT_LEFT_BUTTON && state == GLUT_UP)
	{
		trackball.Set(false, x, y);
		mouseLeft = false;
	}
	if (button == GLUT_MIDDLE_BUTTON && state == GLUT_DOWN)
	{
		trackball.Set(true, x, y);
		mouseMid = true;
	}
	if (button == GLUT_MIDDLE_BUTTON && state == GLUT_UP)
	{
		trackball.Set(true, x, y);
		mouseMid = false;
	}
	if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN)
	{
		trackball.Set(true, x, y);
		mouseRight = true;
	}
	if (button == GLUT_RIGHT_BUTTON && state == GLUT_UP)
	{
		trackball.Set(true, x, y);
		mouseRight = false;
	}
}

void MouseMotion(int x, int y) {
	if (mouseLeft)  trackball.Rotate(x, y);
	if (mouseMid)   trackball.Translate(x, y);
	if (mouseRight) trackball.Zoom(x, y);
	glutPostRedisplay();
}

void init()
{
	glewInit();
	shader = CreateShader("shaders/vs.glsl", "shaders/fs.glsl");
	glUseProgram(shader);

	glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
}

int main(int argc, char **argv)
{ 
  glutInitDisplayString("stencil>=2 rgb double depth samples");
  glutInit(&argc, argv);
  glutInitWindowSize(wWindow,hWindow);
  glutInitWindowPosition(500,100);
  glutCreateWindow("Fractal");
  //GLenum err = glewInit();
  // if (GLEW_OK != err){
  // fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
  //}
  glutDisplayFunc(Display);
  glutIdleFunc(Idle);
  glutReshapeFunc(Reshape);
  glutKeyboardFunc(Kbd); //+ and -
  glutSpecialUpFunc(NULL); 
  glutSpecialFunc(NULL);
  glutMouseFunc(Mouse);
  glutMotionFunc(MouseMotion);
  InitArray(steps);
  init();

  /*unsigned int buffer = -1;
  glGenBuffers(1, &buffer);
  std::cout << glIsBuffer(buffer) << std::endl;
  glBindBuffer(GL_ARRAY_BUFFER, buffer);
  glBufferData(GL_ARRAY_BUFFER, 3 * c.size() * sizeof(float), 0, GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, 0, 0, 0);*/

  glutMainLoop();
  return 0;        
}
