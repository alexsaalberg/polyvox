#include "BlockVolume.h"
#include "BlockVolumeIterator.h"
#include "Utility.h"

#include <windows.h>   // Standard Header For Most Programs
#include <gl/gl.h>     // The GL Header File
#include <gl/glut.h>   // The GL Utility Toolkit (Glut) Header

//Some namespaces we need
using namespace boost;
using namespace PolyVox;
using namespace std;

//Global variables are easier for demonstration purposes, especially
//as I'm not sure if I can pass variables to the GLUT functions.
const uint16_t g_uVolumeSideLength = 128;
BlockVolume<uint8_t> g_volData(logBase2(g_uVolumeSideLength)); //Creates a volume 128x128x128

void createSphereInVolume(void)
{
	//Create an iterator to access data in the volume
	BlockVolumeIterator<uint8_t> volIter(g_volData); 

	//A region corresponding to the whole volume
	const Region& regWholeVolume = g_volData.getEnclosingRegion(); 

	//This iterator will iterate over the whole volume
	volIter.setValidRegion(regWholeVolume); 

	//Start at the begining
	volIter.setPosition(static_cast<Vector3DInt16>(regWholeVolume.getLowerCorner()));
	do
	{
		//Get our current position
		const uint16_t uX = volIter.getPosX();
		const uint16_t uY = volIter.getPosY();
		const uint16_t uZ = volIter.getPosZ();

		//The centre of the volume
		const uint16_t uVolCenterX = g_uVolumeSideLength / 2;
	}while (volIter.isValidForRegion()); //In our case this covers the whole volume
}

void init ( GLvoid )     // Create Some Everyday Functions
{

  glShadeModel(GL_SMOOTH);							// Enable Smooth Shading
	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);				// Black Background
	glClearDepth(1.0f);									// Depth Buffer Setup
	glEnable(GL_DEPTH_TEST);							// Enables Depth Testing
	glDepthFunc(GL_LEQUAL);								// The Type Of Depth Testing To Do
   glEnable ( GL_COLOR_MATERIAL );
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
}

void display ( void )   // Create The Display Function
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	// Clear Screen And Depth Buffer
	glLoadIdentity();									// Reset The Current Modelview Matrix
	glTranslatef(0.0f,0.0f,-200.0f);					// Move Right 3 Units
	glBegin(GL_QUADS);									// Draw A Quad
		glVertex3f(0.0f, 0.0f, 0.0f);					// Top Left
		glVertex3f( 128.0f, 0.0f, 0.0f);					// Top Right
		glVertex3f( 128.0f, 128.0f, 0.0f);					// Bottom Right
		glVertex3f(0.0f, 128.0f, 0.0f);					// Bottom Left
	glEnd();


  glutSwapBuffers ( );
  // Swap The Buffers To Not Be Left With A Clear Screen
}

void reshape ( int w, int h )   // Create The Reshape Function (the viewport)
{
  glViewport     ( 0, 0, w, h );
  glMatrixMode   ( GL_PROJECTION );  // Select The Projection Matrix
  glLoadIdentity ( );                // Reset The Projection Matrix
  if ( h==0 )  // Calculate The Aspect Ratio Of The Window
     gluPerspective ( 80, ( float ) w, 1.0, 5000.0 );
  else
     gluPerspective ( 80, ( float ) w / ( float ) h, 1.0, 5000.0 );
  glMatrixMode   ( GL_MODELVIEW );  // Select The Model View Matrix
  glLoadIdentity ( );    // Reset The Model View Matrix
}

void keyboard ( unsigned char key, int x, int y )  // Create Keyboard Function
{
  switch ( key ) {
    case 27:        // When Escape Is Pressed...
      exit ( 0 );   // Exit The Program
      break;        // Ready For Next Case
    default:        // Now Wrap It Up
      break;
  }
}

void arrow_keys ( int a_keys, int x, int y )  // Create Special Function (required for arrow keys)
{
  switch ( a_keys ) {
    case GLUT_KEY_UP:     // When Up Arrow Is Pressed...
      glutFullScreen ( ); // Go Into Full Screen Mode
      break;
    case GLUT_KEY_DOWN:               // When Down Arrow Is Pressed...
      glutReshapeWindow ( 500, 500 ); // Go Into A 500 By 500 Window
      break;
    default:
      break;
  }
}

void main ( int argc, char** argv )   // Create Main Function For Bringing It All Together
{
  glutInit            ( &argc, argv ); // Erm Just Write It =)
  init ();
  glutInitDisplayMode ( GLUT_RGB | GLUT_DOUBLE ); // Display Mode
  glutInitWindowSize  ( 500, 500 ); // If glutFullScreen wasn't called this is the window size
  glutCreateWindow    ( "NeHe's OpenGL Framework" ); // Window Title (argv[0] for current directory as title)
  glutFullScreen      ( );          // Put Into Full Screen
  glutDisplayFunc     ( display );  // Matching Earlier Functions To Their Counterparts
  glutReshapeFunc     ( reshape );
  glutKeyboardFunc    ( keyboard );
  glutSpecialFunc     ( arrow_keys );
  glutMainLoop        ( );          // Initialize The Main Loop
}
