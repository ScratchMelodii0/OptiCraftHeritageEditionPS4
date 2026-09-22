// Ps4GlesPipelineTests.cpp — pixel tests for the PS4 fixed-function emulation.
//
// Builds src/platform/RenderAPI_GLES_PS4.cpp and src/ps4/render/* against a
// host EGL + GLES2 (Mesa; run headless with EGL_PLATFORM=surfaceless) and
// checks rendered pixels for each piece of GL 1.x behaviour the game relies
// on: vertex/current colour, matrix stacks, texture MODULATE + alpha test,
// linear fog, two-light colour-material lighting, GL_COMPILE display lists,
// the unit-1 lightmap with its texture matrix, and quad runs longer than the
// 16-bit index buffer.
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include "platform/RenderAPI.h"
#include "ps4/render/Ps4GlesPipeline.h"
#include <cstdio>
#include <cstring>
#include <cmath>
static int W=64,H=64, fails=0;
static void px(int x,int y,unsigned char o[4]){ glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,o); }
static void expect(const char* name,int x,int y,int r,int g,int b,int tol=3){
  unsigned char o[4]; px(x,y,o);
  bool ok = std::abs(o[0]-r)<=tol && std::abs(o[1]-g)<=tol && std::abs(o[2]-b)<=tol;
  printf("%-34s (%3d,%3d,%3d) want (%3d,%3d,%3d) %s\n",name,o[0],o[1],o[2],r,g,b,ok?"OK":"FAIL"); if(!ok) fails++; }
struct V{ float x,y,z,u,v; unsigned char c[4]; signed char n[4]; short lu,lv; };
static RenderInterleavedMesh mesh(const V* v,int n){ RenderInterleavedMesh m; m.data=v; m.stride=sizeof(V); m.count=n; m.primitive=RenderPrimitive::Quads;
 m.hasTexture=true; m.texCoordOffset=12; m.hasColor=true; m.colorOffset=20; m.hasNormals=true; m.normalOffset=24; return m; }
static void quad(V* q,float x0,float y0,float x1,float y1,float z,unsigned char r,unsigned char g,unsigned char b,unsigned char a=255){
 float xs[4]={x0,x0,x1,x1}, ys[4]={y0,y1,y1,y0}, us[4]={0,0,1,1}, vs[4]={0,1,1,0};
 for(int i=0;i<4;i++){ q[i]={xs[i],ys[i],z,us[i],vs[i],{r,g,b,a},{0,0,127,0},0,0}; } }
int main(){
 EGLDisplay d=eglGetDisplay(EGL_DEFAULT_DISPLAY); eglInitialize(d,0,0); eglBindAPI(EGL_OPENGL_ES_API);
 EGLint ca[]={EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_DEPTH_SIZE,24,EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_NONE};
 EGLConfig c; EGLint n; if(!eglChooseConfig(d,ca,&c,1,&n)||n<1){printf("no egl config\n");return 2;}
 EGLint pa[]={EGL_WIDTH,W,EGL_HEIGHT,H,EGL_NONE}; EGLSurface s=eglCreatePbufferSurface(d,c,pa);
 EGLint xa[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE}; EGLContext x=eglCreateContext(d,c,EGL_NO_CONTEXT,xa); eglMakeCurrent(d,s,s,x);
 printf("GL: %s\n",glGetString(GL_RENDERER));
 if(!Ps4Gles::initialize()){printf("init failed\n");return 1;}
 renderViewport(0,0,W,H);
 renderMatrixMode(RenderMatrixMode::Projection); renderLoadIdentity(); renderOrtho(0,W,0,H,-100,100);
 renderMatrixMode(RenderMatrixMode::ModelView); renderLoadIdentity();
 renderClearColor(0,0,0,1); renderClear(RenderClearMask::Color|RenderClearMask::Depth);
 V q[8];
 // 1. plain vertex-colour quads (two quads in one draw)
 quad(q,0,0,32,32,0,255,0,0); quad(q+4,32,32,64,64,0,0,0,255);
 renderDrawInterleaved(mesh(q,8));
 expect("quad colour (red)",10,10,255,0,0); expect("second quad (blue)",50,50,0,0,255); expect("uncovered stays black",50,10,0,0,0);
 // 2. matrix: translate + current colour without colour array
 renderClear(RenderClearMask::Color); renderPushMatrix(); renderTranslate(32,0,0);
 { RenderInterleavedMesh m=mesh(q,4); m.hasColor=false; renderColor4f(0,1,0,1); renderDrawInterleaved(m); }
 renderPopMatrix(); expect("translate + glColor (green)",40,10,0,255,0); expect("not drawn at origin",10,10,0,0,0);
 // 3. texture modulate + alpha test
 int tex; renderGenerateTextures(1,&tex); renderBindTexture(tex);
 unsigned char t[4*4]={255,255,255,255, 255,255,255,0, 255,255,255,255, 255,255,255,0}; // 2x2: alpha 255/0
 renderTextureImageRgba(0,2,2,t); renderTextureParameters(false,false,true);
 renderEnable(RenderCapability::Texture2D); renderEnable(RenderCapability::AlphaTest); renderAlphaFunc(RenderCompare::Greater,0.1f);
 renderClear(RenderClearMask::Color); quad(q,0,0,64,64,0,255,128,0); renderDrawInterleaved(mesh(q,4));
 expect("texel alpha=255 passes (modulated)",10,10,255,128,0); expect("texel alpha=0 discarded",50,10,0,0,0);
 renderDisable(RenderCapability::AlphaTest); renderDisable(RenderCapability::Texture2D);
 // 4. linear fog: eye z = -50 with start 0 end 100 -> 50% fog colour
 renderClear(RenderClearMask::Color); renderEnable(RenderCapability::Fog); renderFogi(RenderFogParameter::Mode,RenderFogMode::Linear);
 renderFogf(RenderFogParameter::Start,0); renderFogf(RenderFogParameter::End,100); float fc[4]={0,0,1,1}; renderFogColor(fc);
 renderMatrixMode(RenderMatrixMode::Projection); renderLoadIdentity(); renderOrtho(0,W,0,H,0,200); renderMatrixMode(RenderMatrixMode::ModelView);
 quad(q,0,0,64,64,-50,255,0,0); renderDrawInterleaved(mesh(q,4));
 expect("linear fog 50%",10,10,128,0,127);
 renderDisable(RenderCapability::Fog);
 renderMatrixMode(RenderMatrixMode::Projection); renderLoadIdentity(); renderOrtho(0,W,0,H,-100,100); renderMatrixMode(RenderMatrixMode::ModelView);
 // 5. lighting: normal +z, light dir +z, diffuse 0.6, ambient model 0.4 -> 1.0 * colour
 renderClear(RenderClearMask::Color); renderEnable(RenderCapability::Lighting); renderEnable(RenderCapability::Light0); renderEnable(RenderCapability::ColorMaterial);
 float amb[4]={0.4f,0.4f,0.4f,1}, dif[4]={0.6f,0.6f,0.6f,1}, pos[4]={0,0,1,0}, zero[4]={0,0,0,1};
 renderLightModelAmbient(amb); renderLightfv(0,RenderLightParameter::Diffuse,dif); renderLightfv(0,RenderLightParameter::Ambient,zero); renderLightfv(0,RenderLightParameter::Position,pos);
 quad(q,0,0,32,64,0,200,200,200); renderDrawInterleaved(mesh(q,4));
 pos[2]=-1; renderLightfv(0,RenderLightParameter::Position,pos); // light from behind: ambient only
 quad(q,32,0,64,64,0,200,200,200); renderDrawInterleaved(mesh(q,4));
 expect("lit front (ambient+diffuse)",10,10,200,200,200); expect("lit back (ambient only)",50,10,80,80,80);
 renderDisable(RenderCapability::Lighting);
 // 6. display list: record translate+colour+quad, replay twice
 int list=renderGenerateDisplayLists(1);
 renderBeginDisplayList(list); renderTranslate(16,0,0); quad(q,0,0,16,16,0,255,255,0); renderDrawInterleaved(mesh(q,4)); renderEndDisplayList();
 renderClear(RenderClearMask::Color); expect("list compile draws nothing",20,8,0,0,0);
 renderLoadIdentity(); renderCallDisplayList(list); renderCallDisplayList(list);
 expect("list replay #1 at x=16",20,8,255,255,0); expect("list replay #2 at x=32",36,8,255,255,0); expect("nothing at x=0",8,8,0,0,0);
 renderDeleteDisplayLists(list,1);
 // 7. lightmap on unit 1 with texture matrix scale 1/256 + 8
 renderLoadIdentity(); renderClear(RenderClearMask::Color);
 int lm; renderGenerateTextures(1,&lm); renderSetActiveTextureUnit(0x84C1); renderBindTexture(lm);
 unsigned char lmt[16*16*4]; for(int i=0;i<256;i++){ int u=i%16; lmt[i*4]=lmt[i*4+1]=lmt[i*4+2]=(unsigned char)(u*17); lmt[i*4+3]=255; }
 renderTextureImageRgba(0,16,16,lmt); renderTextureParameters(false,false,true);
 renderMatrixMode(RenderMatrixMode::Texture); renderLoadIdentity(); renderScale(1/256.f,1/256.f,1/256.f); renderTranslate(8,8,8); renderMatrixMode(RenderMatrixMode::ModelView);
 renderEnable(RenderCapability::Texture2D); renderSetActiveTextureUnit(0x84C0);
 quad(q,0,0,64,64,0,255,255,255); for(int i=0;i<4;i++){ q[i].lu=240; q[i].lv=0; }
 { RenderInterleavedMesh m=mesh(q,4); m.hasBrightness=true; m.brightnessOffset=28; renderDrawInterleaved(m); }
 expect("lightmap full block light",10,10,255,255,255);
 for(int i=0;i<4;i++){ q[i].lu=0; }
 renderClear(RenderClearMask::Color);
 { RenderInterleavedMesh m=mesh(q,4); m.hasBrightness=true; m.brightnessOffset=28; renderDrawInterleaved(m); }
 expect("lightmap dark",10,10,0,0,0);
 // 8. large quad run > 65536 vertices (chunked index path)
 renderSetActiveTextureUnit(0x84C1); renderDisable(RenderCapability::Texture2D); renderSetActiveTextureUnit(0x84C0);
 renderClear(RenderClearMask::Color);
 static V big[70000]; for(int i=0;i<17499;i++) quad(big+i*4,0,0,1,1,0,0,0,0); quad(big+17499*4,40,40,64,64,0,255,0,255);
 renderDrawInterleaved(mesh(big,70000)); expect("quad beyond 65536 vertices",50,50,255,0,255);
 printf("%s (%d failures)\n", fails? "FAILED":"ALL PASSED", fails); return fails?1:0;
}
