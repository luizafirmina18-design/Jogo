// Cobra Arena 2P PRO - Menu + 2P sem bug + bot sem rodar
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <malloc.h>
#include <ppu-types.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#include <rsx/rsx.h>
#include <io/pad.h>
#define CB_SIZE (0x100000)
#define HOST_SIZE (32*1024*1024)
#define WORLD 3200.0f
#define MAXSEG 420
#define NSN 10
#define NPEL 500
#define SP 7.0f
#define PI 3.14159265f
#define STATE_MENU 0
#define STATE_PLAY 1
typedef struct { float x[MAXSEG], y[MAXSEG]; int len, alive, respawn, boost, grow, score, tick, think, wander; float ang, tgt; u32 col; } Snake;
typedef struct { float x, y; int size, active; u32 col; } Pellet;
static const u32 palette[8]={0x00FF4D4D,0x0055D6FF,0x0066FF66,0x00FFD23F,0x00C77DFF,0x00FF8FAB,0x0000E5C0,0x00FF9F43};
static gcmContextData *ctx; static u32 W,H,*fb[2],fboff[2],*back; static int cur=0,first_flip=1; static volatile int running=1;
static int state=STATE_MENU, menu_sel=0, dead_t=0, x_prev=0;
static float camx=WORLD/2,camy=WORLD/2;
static float p1_ax=0,p1_ay=0,p1_mag=0; static int p1_boost=0,p1_x=0;
static float p2_ax=0,p2_ay=0,p2_mag=0; static int p2_boost=0,p2_x=0;
static Snake sn[NSN]; static Pellet pel[NPEL]; static int pelrot=0;
static void sysutil_cb(u64 s,u64 p,void*u){(void)p;(void)u; if(s==SYSUTIL_EXIT_GAME) running=0;}
static void init_screen(void){ void *host=memalign(1024*1024,HOST_SIZE); rsxInit(&ctx,CB_SIZE,HOST_SIZE,host); videoState st; videoGetState(VIDEO_PRIMARY,0,&st); u32 resid=st.displayMode.resolution; if(videoGetResolutionAvailability(VIDEO_PRIMARY,VIDEO_RESOLUTION_720,VIDEO_ASPECT_AUTO,0)) resid=VIDEO_RESOLUTION_720; videoResolution res; videoGetResolution(resid,&res); W=res.width; H=res.height; videoConfiguration vc; memset(&vc,0,sizeof(vc)); vc.resolution=resid; vc.format=VIDEO_BUFFER_FORMAT_XRGB; vc.aspect=VIDEO_ASPECT_AUTO; vc.pitch=W*4; videoConfigure(VIDEO_PRIMARY,&vc,NULL,0); videoGetState(VIDEO_PRIMARY,0,&st); gcmSetFlipMode(GCM_FLIP_VSYNC); for(int i=0;i<2;i++){fb[i]=(u32*)rsxMemalign(64,W*H*4); gcmAddressToOffset(fb[i],&fboff[i]); gcmSetDisplayBuffer(i,fboff[i],W*4,W,H);} back=(u32*)memalign(64,W*H*4); memset(back,0,W*H*4); gcmResetFlipStatus(); }
static void wait_flip(void){ while(gcmGetFlipStatus()!=0) usleep(200); gcmResetFlipStatus(); }
static void present(void){ if(!first_flip) wait_flip(); else gcmResetFlipStatus(); memcpy(fb[cur],back,W*H*4); gcmSetFlip(ctx,cur); rsxFlushBuffer(ctx); gcmSetWaitFlip(ctx); cur^=1; first_flip=0; }
static void fill_rect(int x,int y,int w,int h,u32 c){ int x0=x<0?0:x,y0=y<0?0:y,x1=x+w,y1=y+h; if(x1>(int)W)x1=W; if(y1>(int)H)y1=H; for(int yy=y0;yy<y1;yy++){u32*p=back+yy*W; for(int xx=x0;xx<x1;xx++)p[xx]=c;} }
static void circle(int cx,int cy,int r,u32 c){ if(r<1)r=1; if(cx+r<0||cx-r>=(int)W||cy+r<0||cy-r>=(int)H)return; for(int dy=-r;dy<=r;dy++){int y=cy+dy; if(y<0||y>=(int)H)continue; int hw=(int)sqrtf((float)(r*r-dy*dy)); int x0=cx-hw,x1=cx+hw; if(x0<0)x0=0; if(x1>=(int)W)x1=W-1; u32*p=back+y*W; for(int x=x0;x<=x1;x++)p[x]=c;} }
typedef struct { char c; const char *p; } Glyph;
static const Glyph glyphs[]={ {'0',"###" "#.#" "#.#" "#.#" "###"},{'1',".#." "##." ".#." ".#." "###"},{'2',"###" "..#" "###" "#.." "###"},{'3',"###" "..#" "###" "..#" "###"},{'4',"#.#" "#.#" "###" "..#" "..#"},{'5',"###" "#.." "###" "..#" "###"},{'6',"###" "#.." "###" "#.#" "###"},{'7',"###" "..#" "..#" "..#" "..#"},{'8',"###" "#.#" "###" "#.#" "###"},{'9',"###" "#.#" "###" "..#" "###"},{'A',".#." "#.#" "###" "#.#" "#.#"},{'B',"##." "#.#" "##." "#.#" "##."},{'C',"###" "#.." "#.." "#.." "###"},{'E',"###" "#.." "###" "#.." "###"},{'G',"###" "#.." "#.#" "#.#" "###"},{'J',"..#" "..#" "..#" "#.#" "###"},{'M',"#.#" "###" "###" "#.#" "#.#"},{'N',"#.#" "###" "###" "#.#" "#.#"},{'O',"###" "#.#" "#.#" "#.#" "###"},{'P',"###" "#.#" "###" "#.." "#.."},{'R',"##." "#.#" "##." "#.#" "#.#"},{'S',"###" "#.." "###" "..#" "###"},{'T',"###" ".#." ".#." ".#." ".#."},{'U',"#.#" "#.#" "#.#" "#.#" "###"},{'V',"#.#" "#.#" "#.#" "#.#" ".#."},{'X',"#.#" "#.#" ".#." "#.#" "#.#"},{':',"..." ".#." "..." ".#." "..."},{' ',"..." "..." "..." "..." "..."} };
static const char *find_glyph(char c){ for(unsigned i=0;i<sizeof(glyphs)/sizeof(glyphs[0]);i++) if(glyphs[i].c==c) return glyphs[i].p; return NULL; }
static int text_width(const char *s,int sc){ return (int)strlen(s)*4*sc - sc; }
static void draw_text(const char *s,int x,int y,int sc,u32 c){ for(;*s;s++){ const char *g=find_glyph(*s); if(g) for(int r=0;r<5;r++) for(int k=0;k<3;k++) if(g[r*3+k]=='#') fill_rect(x+k*sc,y+r*sc,sc,sc,c); x+=4*sc; } }
static float frand(void){ return (float)rand()/(float)RAND_MAX; }
static float radius_of(const Snake *s){ int l=s->len>240?240:s->len; return 9.0f+l*0.035f; }
static void add_pellet(float x,float y,int size){ for(int k=0;k<NPEL;k++){ int i=(pelrot+k)%NPEL; if(!pel[i].active){ pel[i].x=x; pel[i].y=y; pel[i].size=size; pel[i].active=1; pel[i].col=palette[rand()%8]; pelrot=(i+1)%NPEL; return; } } }
static void spawn_snake(int i){ Snake *s=&sn[i]; float x=400+frand()*(WORLD-800), y=400+frand()*(WORLD-800); s->ang=frand()*2*PI; s->tgt=s->ang; s->len=10; for(int j=0;j<s->len;j++){s->x[j]=x-cosf(s->ang)*SP*j; s->y[j]=y-sinf(s->ang)*SP*j;} s->alive=1; s->respawn=0; s->boost=0; s->grow=0; s->tick=0; s->think=0; s->wander=0; s->col=palette[i%8]; }
static void kill_snake(int i){ Snake *s=&sn[i]; for(int j=0;j<s->len;j+=2) add_pellet(s->x[j]+(frand()-0.5f)*10,s->y[j]+(frand()-0.5f)*10,2); s->alive=0; s->respawn=240; }
static float angle_diff(float a,float b){ float d=b-a; while(d>PI)d-=2*PI; while(d<-PI)d+=2*PI; return d; }
static void bot_think(int i){
  Snake *s=&sn[i]; float hx=s->x[0], hy=s->y[0];
  s->think--; if(s->think>0) return; s->think=18;
  if(hx<350||hx>WORLD-350||hy<350||hy>WORLD-350){ float want=atan2f(WORLD/2-hy,WORLD/2-hx); float d=angle_diff(s->ang,want); if(d>0.6f) s->tgt=s->ang+0.6f; else if(d<-0.6f) s->tgt=s->ang-0.6f; else s->tgt=want; return; }
  float px=hx+cosf(s->ang)*70, py=hy+sinf(s->ang)*70; float rs=radius_of(s);
  for(int o=0;o<NSN;o++){ if(o==i||!sn[o].alive) continue; float lim=rs+radius_of(&sn[o])+20; lim*=lim; for(int j=0;j<sn[o].len;j+=3){ float dx=sn[o].x[j]-px, dy=sn[o].y[j]-py; if(dx*dx+dy*dy<lim){ s->tgt=s->ang+((rand()%2)?0.5f:-0.5f); s->wander=0; return; } } }
  float best=400*400; int bi=-1; for(int k=0;k<NPEL;k++){ if(!pel[k].active) continue; float dx=pel[k].x-hx, dy=pel[k].y-hy, d=dx*dx+dy*dy; if(d<best){best=d; bi=k;} }
  if(bi>=0 && best<300*300){ float want=atan2f(pel[bi].y-hy,pel[bi].x-hx); float d=angle_diff(s->ang,want); if(d>0.4f) s->tgt=s->ang+0.4f; else if(d<-0.4f) s->tgt=s->ang-0.4f; else s->tgt=want; return; }
  s->wander--; if(s->wander<=0){ s->wander=60+rand()%90; s->tgt=s->ang+(frand()-0.5f)*0.8f; }
}
static void update_snake(int i){
  Snake *s=&sn[i]; if(!s->alive){ if(i>1 && --s->respawn<=0) spawn_snake(i); return; } s->tick++;
  if(i==0){ if(p1_mag>0.35f) s->tgt=atan2f(p1_ay,p1_ax); s->boost=p1_boost; }
  else if(i==1){ if(p2_mag>0.35f) s->tgt=atan2f(p2_ay,p2_ax); s->boost=p2_boost; }
  else { bot_think(i); s->boost=(s->len>30 && (s->tick/90)%5==0)?1:0; }
  float d=angle_diff(s->ang,s->tgt); if(d>0.075f)d=0.075f; if(d<-0.075f)d=-0.075f; s->ang+=d;
  float sp=3.0f; if(s->boost && s->len>12){ sp=5.4f; if(s->tick%10==0){ s->len--; add_pellet(s->x[s->len],s->y[s->len],1); } }
  s->x[0]+=cosf(s->ang)*sp; s->y[0]+=sinf(s->ang)*sp;
  for(int j=1;j<s->len;j++){ float dx=s->x[j-1]-s->x[j], dy=s->y[j-1]-s->y[j]; float dd=sqrtf(dx*dx+dy*dy); if(dd>SP){ float f=(dd-SP)/dd; s->x[j]+=dx*f; s->y[j]+=dy*f; } }
  if(s->x[0]<0||s->x[0]>WORLD||s->y[0]<0||s->y[0]>WORLD){ kill_snake(i); return; }
  float rs=radius_of(s), er=rs+12;
  for(int k=0;k<NPEL;k++){ if(!pel[k].active) continue; float dx=pel[k].x-s->x[0], dy=pel[k].y-s->y[0]; if(dx*dx+dy*dy<er*er){ pel[k].active=0; s->grow+=pel[k].size; s->score+=pel[k].size*5; } }
  while(s->grow>=2 && s->len<MAXSEG){ s->x[s->len]=s->x[s->len-1]; s->y[s->len]=s->y[s->len-1]; s->len++; s->grow-=2; }
  if(s->len>=MAXSEG) s->grow=0;
  for(int o=0;o<NSN;o++){ if(o==i||!sn[o].alive) continue; float lim=(rs+radius_of(&sn[o]))*0.75f; lim*=lim; for(int j=0;j<sn[o].len;j++){ float dx=sn[o].x[j]-s->x[0], dy=sn[o].y[j]-s->y[0]; if(dx*dx+dy*dy<lim){ kill_snake(i); return; } } }
}
static void init_game(void){ memset(pel,0,sizeof(pel)); for(int k=0;k<NPEL;k++) add_pellet(frand()*WORLD,frand()*WORLD,1+(rand()%5==0)); for(int i=0;i<NSN;i++){ sn[i].score=0; spawn_snake(i);} dead_t=0; camx=sn[0].x[0]; camy=sn[0].y[0]; }
static void read_pad(void){
  padInfo info; padData d; p1_ax=p1_ay=p1_mag=0; p1_boost=p1_x=0; p2_ax=p2_ay=p2_mag=0; p2_boost=p2_x=0;
  ioPadGetInfo(&info);
  if(info.status[0]){ ioPadGetData(0,&d); float ax=((int)d.ANA_L_H-128)/128.0f, ay=((int)d.ANA_L_V-128)/128.0f; if(d.BTN_LEFT) ax=-1; if(d.BTN_RIGHT) ax=1; if(d.BTN_UP) ay=-1; if(d.BTN_DOWN) ay=1; float m=sqrtf(ax*ax+ay*ay); if(m<0.25f){ax=0; ay=0; m=0;} p1_ax=ax; p1_ay=ay; p1_mag=m; p1_boost=d.BTN_CROSS||d.BTN_R1||d.BTN_R2; p1_x=d.BTN_CROSS; }
  if(info.status[1]){ ioPadGetData(1,&d); float ax=((int)d.ANA_L_H-128)/128.0f, ay=((int)d.ANA_L_V-128)/128.0f; if(d.BTN_LEFT) ax=-1; if(d.BTN_RIGHT) ax=1; if(d.BTN_UP) ay=-1; if(d.BTN_DOWN) ay=1; float m=sqrtf(ax*ax+ay*ay); if(m<0.25f){ax=0; ay=0; m=0;} p2_ax=ax; p2_ay=ay; p2_mag=m; p2_boost=d.BTN_CROSS||d.BTN_R1||d.BTN_R2; p2_x=d.BTN_CROSS; }
}
static void update_game(void){
  for(int i=0;i<NSN;i++) update_snake(i);
  int count=0; for(int k=0;k<NPEL;k++) count+=pel[k].active;
  for(int n=0;n<2 && count<NPEL-40;n++,count++) add_pellet(frand()*WORLD,frand()*WORLD,1+(rand()%5==0));
  if(sn[0].alive){ camx=sn[0].x[0]; camy=sn[0].y[0]; } else if(sn[1].alive){ camx=sn[1].x[0]; camy=sn[1].y[0]; }
  else { dead_t++; if(dead_t>60 && (p1_x||p2_x) && !x_prev){ state=STATE_MENU; } }
  x_prev=(p1_x||p2_x);
}
static void render_menu(void){
  for(u32 i=0;i<W*H;i++) back[i]=0x000E1420;
  int ui=5; int cx=W/2, cy=H/2;
  draw_text("COBRA ARENA",cx-text_width("COBRA ARENA",ui*2)/2,cy-120,ui*2,0x0066FF66);
  draw_text("2P",cx-text_width("2P",ui*2)/2,cy-70,ui*2,0x00FFD23F);
  u32 c1 = menu_sel==0?0x00FFFFFF:0x00809BBF; u32 c2 = menu_sel==1?0x00FFFFFF:0x00809BBF;
  draw_text("JOGAR 2P",cx-text_width("JOGAR 2P",ui)/2,cy+20,ui,c1);
  draw_text("JOGAR 1P",cx-text_width("JOGAR 1P",ui)/2,cy+60,ui,c2);
  draw_text("X CONFIRMA",cx-text_width("X CONFIRMA",ui)/2,cy+130,ui,0x00809BBF);
  draw_text("P1 ANALOGICO + X TURBO",cx-text_width("P1 ANALOGICO + X TURBO",ui)/2,cy+170,ui,0x00809BBF);
  draw_text("P2 ANALOGICO + X TURBO",cx-text_width("P2 ANALOGICO + X TURBO",ui)/2,cy+200,ui,0x00809BBF);
}
static void render_game(void){
  float sc=(float)W/1280.0f; float hw=W/2.0f, hh=H/2.0f; u32 n=W*H; for(u32 i=0;i<n;i++) back[i]=0x000E1420;
  int sx0=(int)((0-camx)*sc+hw), sx1=(int)((WORLD-camx)*sc+hw); int sy0=(int)((0-camy)*sc+hh), sy1=(int)((WORLD-camy)*sc+hh); u32 red=0x00380E12;
  if(sx0>0) fill_rect(0,0,sx0,H,red); if(sx1<(int)W) fill_rect(sx1,0,W-sx1,H,red); if(sy0>0) fill_rect(0,0,W,sy0,red); if(sy1<(int)H) fill_rect(0,sy1,W,H-sy1,red);
  float gx0=floorf((camx-hw/sc)/100.0f)*100.0f; float gy0=floorf((camy-hh/sc)/100.0f)*100.0f;
  for(float gx=gx0;gx<camx+hw/sc;gx+=100.0f){ if(gx<0||gx>WORLD) continue; for(float gy=gy0;gy<camy+hh/sc;gy+=100.0f){ if(gy<0||gy>WORLD) continue; fill_rect((int)((gx-camx)*sc+hw),(int)((gy-camy)*sc+hh),4,4,0x00203047); } }
  for(int k=0;k<NPEL;k++){ if(!pel[k].active) continue; int px=(int)((pel[k].x-camx)*sc+hw), py=(int)((pel[k].y-camy)*sc+hh); circle(px,py,(int)((4+pel[k].size*2)*sc),pel[k].col); }
  for(int i=NSN-1;i>=0;i--){ Snake *s=&sn[i]; if(!s->alive) continue; int r=(int)(radius_of(s)*sc); u32 dark=(s->col>>1)&0x007F7F7F; for(int j=s->len-1;j>=0;j--){ int px=(int)((s->x[j]-camx)*sc+hw), py=(int)((s->y[j]-camy)*sc+hh); circle(px,py,r,(j%6<3)?s->col:dark); } int hx=(int)((s->x[0]-camx)*sc+hw), hy=(int)((s->y[0]-camy)*sc+hh); for(int e=-1;e<=1;e+=2){ int ex=hx+(int)(cosf(s->ang+e*0.7f)*r*0.6f); int ey=hy+(int)(sinf(s->ang+e*0.7f)*r*0.6f); circle(ex,ey,r*35/100+1,0x00FFFFFF); circle(ex+(int)(cosf(s->ang)*r*0.15f),ey+(int)(sinf(s->ang)*r*0.15f),r*18/100+1,0x00000000); } }
  char buf[32]; int ui=(int)(4*sc+0.5f); if(ui<3)ui=3;
  draw_text("P1",24,20,ui,0x00FFFFFF); sprintf(buf,"%d",sn[0].score); draw_text(buf,24,20+7*ui,ui+2,0x00FFD23F);
  draw_text("P2",24,20+16*ui,ui,0x00FFFFFF); sprintf(buf,"%d",sn[1].score); draw_text(buf,24,20+23*ui,ui+2,0x0055D6FF);
  draw_text("X TURBO",24,(int)H-8*ui,ui,0x00809BBF);
  if(!sn[0].alive && !sn[1].alive){ int bw=text_width("GAME OVER",ui*2)+80, bh=16*ui+40; fill_rect((int)W/2-bw/2,(int)H/2-bh/2,bw,bh,0x00000000); draw_text("GAME OVER", (int)W/2-text_width("GAME OVER",ui*2)/2,(int)H/2-bh/2+24,ui*2,0x00FF4D4D); if(dead_t>60) draw_text("PRESS X", (int)W/2-text_width("PRESS X",ui)/2,(int)H/2+ui*3,ui,0x00FFFFFF); }
}
int main(int argc,char**argv){ (void)argc;(void)argv; init_screen(); ioPadInit(7); sysUtilRegisterCallback(0,sysutil_cb,NULL); srand(20261109); init_game(); state=STATE_MENU;
  while(running){ sysUtilCheckCallback(); read_pad();
    if(state==STATE_MENU){
      static int menu_prev=0;
      if(p1_ay<-0.5f && !menu_prev) menu_sel=0; if(p1_ay>0.5f && !menu_prev) menu_sel=1;
      menu_prev = (p1_ay<-0.5f || p1_ay>0.5f);
      if(p1_x && !x_prev){ init_game(); if(menu_sel==1){ sn[1].alive=0; sn[1].respawn=999999; } state=STATE_PLAY; }
      x_prev=p1_x; render_menu();
    } else if(state==STATE_PLAY){ update_game(); render_game(); if(!sn[0].alive && !sn[1].alive && dead_t>90 && (p1_x||p2_x)) state=STATE_MENU; }
    present();
  } return 0; }