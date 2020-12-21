#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <atomic_ops.h>
#include <pthread.h>
#include <unistd.h>

#define tracef(x) printf(#x " -> %f\n", (double)(x));
#define tracei(x) printf(#x " -> %i\n", (int)(x));
#define trace2f(x,y) printf(#x ", " #y " -> %f, %f\n", (double)(x), (double)(y));

#define patchWidth 256
#define patchHeight patchWidth

int windowWidth, windowHeight;

static GLint height;

#define maxIter 2000

void
fractal(double x, double y, GLubyte *r, GLubyte *g, GLubyte *b)
{
  double cr = x, ci = y;
  double zr = 0, zi = 0;
  int iter = 0;
  *r = *g = *b = 0;
  while (1) {
    if (iter > maxIter)
      return;
    double zrsq = zr*zr;
    double zisq = zi*zi;
    double z2r = zrsq - zisq;
    double z2i = 2*zr*zi;
    if (zrsq + zisq > 4)
      break;
    zr = z2r + cr;
    zi = z2i + ci;
    iter++;
  }
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
  volatile long unsigned int computedBy;
  int finished;
  int textureCreated;
  double xstart, ystart, xend, yend;
  GLubyte (*data)[patchHeight][patchWidth][3];
  GLuint textureId;
};


struct patch*
makeFractalPatch(double xstart, double ystart, double xend, double yend)
{
  int i, j;
  struct patch *p = (struct patch*)malloc(sizeof(struct patch));
  p->next = NULL;
  p->data = NULL;
  p->xstart = xstart;
  p->ystart = ystart;
  p->xend = xend;
  p->yend = yend;
  p->computedBy = 0;
  p->finished = 0;
  p->textureCreated = 0;
  return p;
}

void
fillFractalPatch(struct patch* p) {
  double xstart = p->xstart;
  double xend = p->xend;
  double ystart = p->ystart;
  double yend = p->yend;

  GLubyte (*data)[patchHeight][patchWidth][3] =
    (GLubyte (*)[patchHeight][patchWidth][3])malloc(patchWidth*patchHeight*3);
  p->data = data;

  double xlen = xend-xstart;
  double ylen = yend-ystart;
  double xstep = xlen / patchWidth;
  double ystep = ylen / patchHeight;
    
  for (int i = 0; i < patchHeight; i++) {
    double y = ystart + ystep * i;
    for (int j = 0; j < patchWidth; j++) {
      double x = xstart + xstep * j;
      
      GLubyte r, g, b;
      fractal(x, y, &r, &g, &b);
      // if (i < 2 || j < 2) { r = g = b = 99; }
      (*data)[i][j][0] = r;
      (*data)[i][j][1] = g;
      (*data)[i][j][2] = b;
    }
  }
}



double
  // current_xcenter = 0, current_width = 4, current_ycenter = 0, current_height = 2.25,
  current_xcenter = -1.038822, current_width = 0.124967, current_ycenter = -0.334381, current_height = 0.070294,

  current_xspeed = 0,
  current_yspeed = 0;


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



#define NUM_BUCKETS 7777
struct patch *all_patches[NUM_BUCKETS];

unsigned int
computeHash(double x, double y, double step) {
  assert(sizeof(double) == sizeof(long));
  long hash =
    1779823742398473297L**(long*)&x
    + 8348384834833111327l**(long*)&y
    + 8387474346364372377L**(long*)&step;
  return (unsigned long)hash % (unsigned int)NUM_BUCKETS;
}

struct patch*
findPatch(double x, double y, double step) {
  unsigned int hash = computeHash(x, y, step);
  struct patch *p = all_patches[hash];
  while (p) {
    if (p->xstart == x && p->ystart == y && p->xend == x+step)
      return p;
    p = p->next;
  }
  return NULL;
}

void
insertPatch(double x, double y, double step, struct patch *pat) {
  int hash = computeHash(x, y, step);
  struct patch **p = all_patches + hash;
  while (1) {
    if (*p == NULL) {
      *p = pat;
      break;
    }
    p = &(*p)->next;
  }
}


#define walkCurrentPatches(rel)                                         \
  double logw = log(current_width)/log(2.0);                            \
  double logh = log(current_height)/log(2.0);                           \
  double flogw = floor(logw)-4-rel;                                     \
  double step = pow(2, flogw);                                          \
  double sy = floor((current_ycenter - current_height/2) / step) * step; \
  double ey = ceil((current_ycenter + current_height/2) / step) * step; \
  double sx = floor((current_xcenter - current_width/2) / step) * step; \
  double ex = ceil((current_xcenter + current_width/2) / step) * step;  \
  for (double y = sy; y < ey; y += step)                                \
    for (double x = sx; x < ex; x += step)


void
computePatches(int maxr) {
  /* printf("computing patches at center %f,%f extent %f,%f\n", */
  /*        current_xcenter, current_ycenter, current_width, current_height); */
  for (int r = 0; r <= 0; r++) {
    walkCurrentPatches(r) {
      struct patch *p = findPatch(x, y, step);
      if (p == NULL) {
        p = makeFractalPatch(x, y, x+step, y+step);
        insertPatch(x, y, step, p);
      }
    }
  }
}

int num_threads = 3;
pthread_t threads[100];


void*
worker(void* arg) {
  unsigned long threadId = (long)arg;

  while (1) {
    for (int i = 0; i < NUM_BUCKETS; i++) {
      struct patch *p = all_patches[i];
      while (p) {
        if (!p->computedBy && !p->finished) {
          int take = AO_compare_and_swap_full(&p->computedBy, 0, threadId); 
          if (take) {
            /* printf("worker %li computes %li\n", threadId, (long)p); */
            fillFractalPatch(p);
            /* printf("worker %li finishes %li\n", threadId, (long)p); */
            p->finished = 1;
          }
        }
        p = p->next;
      }
    }
    sleep(1); // TODO
  }
  return NULL;
}

void init(void)
{    
  for (int i = 0; i < NUM_BUCKETS; i++) all_patches[i] = NULL;
  computePatches(0);

  for (int i = 0; i < num_threads; i++)
    pthread_create(threads+i, NULL, worker, (void*)(long)(1000+i));
    
  glEnable(GL_TEXTURE_2D);
  glClearColor (0.0, 0.0, 0.0, 0.0);
  glShadeModel(GL_FLAT);
}

void display(void)
{
  glClear(GL_COLOR_BUFFER_BIT);
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
 
  // glColor4f(0.0, 0.0, 0.0, 0.0);
  
  int really_drawn = 0;
  walkCurrentPatches(0) {
    struct patch *p = findPatch(x, y, step);
    while (p != NULL) {
      if (p->textureCreated) {
        really_drawn ++;
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
      }
      p = p->next;
    }
  }
  /* printf("really drawn %i\n", really_drawn); */
  glutSwapBuffers();
}

void reshape(int w, int h)
{
  windowWidth = w;
  windowHeight = h;
  glViewport(0, 0, (GLsizei) w, (GLsizei) h);
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

double clamped(double x) {
  double lim = 0.05;
  if (x < -lim) return x+lim;
  if (x > lim) return x-lim;
  return 0;
}

void
step() {
  // trigger compute more patches
  computePatches(1);

  // finish texture of computed patches
  {
    int num_drawn = 0;
    walkCurrentPatches(0) {
      struct patch *p = findPatch(x, y, step);
      while (p != NULL) {
        if (p->finished && !p->textureCreated)
          {
            /* printf("creating texture %li\n", (long)p); */
            glGenTextures(1, &p->textureId);
            glBindTexture(GL_TEXTURE_2D, p->textureId);
            glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, patchWidth, patchHeight, 0,
                         GL_RGB, GL_UNSIGNED_BYTE, p->data);
            // glTexImage2DMultisample
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                            GL_LINEAR
                            // GL_NEAREST
                            );
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                            GL_LINEAR
                            // GL_NEAREST
                            );
            p->textureCreated = 1;
            glutPostRedisplay();
          }
        p = p->next;
      }
    }
  }

  if (mousex < 0)
    return;
  
  double zoom = 1.0;
  if (zoomin > 0)
    zoom -= 0.0005*zoomin;
  else if (zoomout > 0)
    zoom += 0.0005*zoomout;
  double x = (double)mousex/(windowWidth/2) - 1;
  double y = - ((double)mousey/(windowHeight/2) - 1);
  x = clamped(x);
  y = clamped(y);
  current_xspeed += current_width/2 * x * 0.01;
  current_yspeed += current_height/2 * y * 0.01;
  double f = 0.95;
  current_xspeed *= f;
  current_yspeed *= f;
  current_xcenter += current_xspeed * 0.01;
  current_ycenter += current_yspeed * 0.01;
  if (zoom != 1.0) {
    current_width *= zoom;
    current_height *= zoom;
  }
  glutPostRedisplay();
}


void keyboard(unsigned char key, int x, int y)
{
   switch (key) {
   case 'e':
     zoomin += 1;
     zoomout = 0;
     break;
   case 'a':
     zoomin = 0;
     zoomout += 1;
     break;
   case 'o':
   case 'O':
     zoomin = 0;
     zoomout = 0;
     break;
   case 'c':
     computePatches(0);
     glutPostRedisplay();
     break;
   case 'r':
     glutPostRedisplay();
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
