#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define tracef(x) printf(#x " -> %f\n", (double)(x));
#define tracei(x) printf(#x " -> %i\n", (int)(x));
#define trace2f(x,y) printf(#x ", " #y " -> %f, %f\n", (double)(x), (double)(y));

#define patchWidth 1024
#define patchHeight patchWidth

int windowWidth, windowHeight;

static GLdouble zoomFactor = 1.0;
static GLint height;

#define maxIter 400

void
fractal(double x, double y, GLubyte *r, GLubyte *g, GLubyte *b)
{
  double cr = x, ci = y;
  double zr = 0, zi = 0;
  int iter = 0;
  while (1) {
    if (iter > maxIter)
      break;
    double z2r = zr*zr - zi*zi;
    double z2i = 2*zr*zi;
    if (z2r + z2i > 4)
      break;
    zr = z2r + cr;
    zi = z2i + ci;
    iter++;
  }
  *r = *g = *b = 0;
  if (iter >= maxIter)
    *r = *g = *b = 0;
  if (iter < 100)
    *r = iter*255/100;
  else if (iter < 200)
    *g = (iter-100)*255/100;
  else if (iter < 300)
    *b = (iter-200)*255/100;
  else if (iter < 400)
    *r = *b = (iter-300)*255/100;
  else if (iter < 500)
    *g = *b = (iter-400)*255/100;
  else
    *r = *g = *b = (iter-500)*255/500;
}

struct patch {
  struct patch *next;
  double xstart, ystart, xend, yend;
  GLubyte (*data)[patchHeight][patchWidth][3];
  GLuint textureId;
};


struct patch*
makeFractalPatch(double xstart, double ystart, double xend, double yend)
{
  int i, j;
  struct patch *p = (struct patch*)malloc(sizeof(struct patch));
  GLubyte (*data)[patchHeight][patchWidth][3] =
    (GLubyte (*)[patchHeight][patchWidth][3])malloc(patchWidth*patchHeight*3);
  p->next = NULL;
  p->data = data;
  p->xstart = xstart;
  p->ystart = ystart;
  p->xend = xend;
  p->yend = yend;
  double xlen = xend-xstart;
  double ylen = yend-ystart;
  double xstep = xlen / patchWidth;
  double ystep = ylen / patchHeight;
    
  for (i = 0; i < patchHeight; i++) {
    double y = ystart + ystep * i;
    for (j = 0; j < patchWidth; j++) {
      double x = xstart + xstep * j;
      
      GLubyte r, g, b;
      fractal(x, y, &r, &g, &b);
      // if (i < 2 || j < 2) { r = g = b = 99; }
      (*data)[i][j][0] = r;
      (*data)[i][j][1] = g;
      (*data)[i][j][2] = b;
    }
  }

  glGenTextures(1, &p->textureId);
  glBindTexture(GL_TEXTURE_2D, p->textureId);
  glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, patchWidth, patchHeight, 0,
               GL_RGB, GL_UNSIGNED_BYTE, data);
  // glTexImage2DMultisample
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                  GL_LINEAR
                  // GL_NEAREST
                  );
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR
                  // GL_NEAREST
                  );
  return p;
}



double
  current_xcenter = 0,
  current_width = 4,
  current_ycenter = 0,
  current_height = 2.25;

// map x/y to screen x/y
double mapx(double x) {
  return
    (x-current_xcenter)
    / (current_width/2)
    * (windowWidth/2)
    + (windowWidth/2);
}
double mapy(double y) {
  return
    (y-current_ycenter)
    / (current_height/2)
    * (windowHeight/2)
    + (windowHeight/2);
}


struct patch *all_patches = NULL;

void
computePatches() {
  double logw = log(current_width)/log(2.0);
  double logh = log(current_height)/log(2.0);
  double flogw = floor(logw)-4;
  double flogh = floor(logh)-4;
  double step = pow(2, flogw);
  double sy = floor((current_ycenter - current_height/2) / step) * step;
  double ey = ceil((current_ycenter + current_height/2) / step) * step;
  double sx = floor((current_xcenter - current_width/2) / step) * step;
  double ex = ceil((current_xcenter + current_width/2) / step) * step;
  for (double y = sy; y < ey; y += step)
    {
      for (double x = sx; x < ex; x += step)
        {
          trace2f(x, y);
          struct patch* p = makeFractalPatch(x, y, x+step, y+step);
          p->next = all_patches;
          all_patches = p;
        }
    }
}


void init(void)
{    
  computePatches();
    
  glEnable(GL_TEXTURE_2D);
  glClearColor (0.0, 0.0, 0.0, 0.0);
  glShadeModel(GL_FLAT);
}

void display(void)
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
 
  // glColor4f(0.0, 0.0, 0.0, 0.0);
  
  struct patch* p = all_patches;
  while (p) {
    glBindTexture(GL_TEXTURE_2D, p->textureId);
    glBegin(GL_QUADS);

    double xs = mapx(p->xstart);
    double xe = mapx(p->xend);
    double ys = mapy(p->ystart);
    double ye = mapy(p->yend);

    glTexCoord2f(0.0, 0.0); glVertex3f(xs, ys, 0.0f);
    glTexCoord2f(0.0, 1.0); glVertex3f(xs, ye, 0.0f);
    glTexCoord2f(1.0, 1.0); glVertex3f(xe, ye, 0.0f);
    glTexCoord2f(1.0, 0.0); glVertex3f(xe, ys, 0.0f);
    glEnd();
    p = p->next;
  }
  glutSwapBuffers();
}

void reshape(int w, int h)
{
  windowWidth = w;
  windowHeight = h;
  glViewport(0, 0, (GLsizei) w, (GLsizei) h);
  height = (GLint) h;
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0.0, (GLdouble) w, 0.0, (GLdouble) h);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

int zoomin = 0;
int zoomout = 0;
int mousex = -1, mousey = -1;

void
mouseEvent(int button, int state, int x, int y) {
  /* if (button == GLUT_LEFT_BUTTON) */
  /*   leftPressed = state; */
}
void
mouseMove(int x, int y) {
  mousex = x;
  mousey = y;
}

void
step() {
  if (mousex < 0)
    return;
  
  double zoom = !zoomout ? 0.998 : 1.002;
  double x = (double)mousex/(windowWidth/2) - 1;
  double y = - ((double)mousey/(windowHeight/2) - 1);
  current_xcenter += current_width/2 * x * 0.01;
  current_ycenter += current_height/2 * y * 0.01;
  if (zoomin || zoomout) {
    current_width *= zoom;
    current_height *= zoom;
  }
  glutPostRedisplay();
}


void keyboard(unsigned char key, int x, int y)
{
   switch (key) {
   case 'a':
   case 'A':
     zoomin = 1;
     zoomout = 0;
     break;
   case 'e':
   case 'E':
     zoomin = 0;
     zoomout = 1;
     break;
   case 'o':
   case 'O':
     zoomin = 0;
     zoomout = 0;
     break;
   case 27:
     exit(0);
     break;
   default:
     tracei(key);
     break;
   }
}

int main(int argc, char** argv)
{
  glutInit(&argc, argv);
  
   glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
   glutInitWindowSize(250, 250);
   glutInitWindowPosition(100, 100);
   glutCreateWindow(argv[0]);

   windowWidth = glutGet(GLUT_WINDOW_WIDTH);
   windowHeight = glutGet(GLUT_WINDOW_HEIGHT);
   printf("w: %i h: %i\n", windowWidth, windowHeight);

   init();
   glutDisplayFunc(display);
   glutReshapeFunc(reshape);
   glutKeyboardFunc(keyboard);
   glutMouseFunc(mouseEvent);
   glutMotionFunc(mouseMove);
   glutPassiveMotionFunc(mouseMove);
   glutIdleFunc(step);
   glutMainLoop();
   return 0; 
}
