//code by bitluni give me a shout-out if you like it

//music credits
//"Also Sprach Zarathustra" Kevin MacLeod (incompetech.com)
//Licensed under Creative Commons: By Attribution 3.0 License
//http://creativecommons.org/licenses/by/3.0/

#include <soc/rtc.h>
#include "AudioSystem.h"
#include "AudioOutput.h"
#include "Graphics.h"
#include "Image.h"
#include "SimplePALOutput.h"
#include "GameControllers.h"
#include "Sprites.h"
#include "Font.h"

//lincude graphics and sounds
namespace font88
{
#include "gfx/font.h"
}
Font font(8, 8, font88::pixels);
#include "gfx/texture.h"
#include "sfx/music.h"

////////////////////////////
//controller pins
const int LATCH = 16;
const int CLOCK = 17;
const int CONTROLLER_DATA_PIN = 18;
GameControllers controllers;

////////////////////////////
//audio configuration
const int SAMPLING_RATE = 16000;
const int BUFFER_SIZE = 4000;
AudioSystem audioSystem(SAMPLING_RATE, BUFFER_SIZE);
AudioOutput audioOutput;

///////////////////////////
//Video configuration
//PAL MAX, half: 324x268 full: 648x536
//NTSC MAX, half: 324x224 full: 648x448
const int XRES = 320;
//const int YRES = 144;
const int YRES = 240;
Graphics graphics(XRES, YRES);
SimplePALOutput composite;

void compositeCore(void *data)
{
  while (true)
  {
    composite.sendFrame(&graphics.frame);
  }
}

int sintab[256];
int costab[256];

void setup()
{
  rtc_clk_cpu_freq_set(RTC_CPU_FREQ_240M);              //highest cpu frequency

  //initialize composite output and graphics
  composite.init();
  graphics.init();
  graphics.setFont(font);
  xTaskCreatePinnedToCore(compositeCore, "c", 1024, NULL, 1, NULL, 0);

  //initialize audio output
  audioOutput.init(audioSystem);

  //initialize controllers
  controllers.init(LATCH, CLOCK);
  controllers.setController(0, GameControllers::NES, CONTROLLER_DATA_PIN); //first controller

  //Play first sound in loop (music)
  music.play(audioSystem, 0);

  for(int i = 0; i < 256; i++)
  {
    sintab[i] = int(sin(i / 256.f * M_PI) * 256);
    costab[i] = int(cos(i / 256.f * M_PI) * 80 * 256);
  }
}

void loop()
{
  //fill audio buffer
  audioSystem.calcSamples();
  
/*  //read controllers
  controllers.poll();
  
  //play sounds on A or B buttons
  if(controllers.pressed(0, GameControllers::A))
    sounds.play(audioSystem, 1);
  if(controllers.pressed(0, GameControllers::B))
    sounds.play(audioSystem, 2);*/

  graphics.begin(0);
  //draw some sprites
  int t = millis();
  int rot = t / 100;
  int lightpos = min(t / 78, 256);
  int flare1 = min(t / 10, 256);
  int flare2 = min((t - 120 * 78) / 5, 256);
  for(int y = 0; y < 128; y++)
  {
    int vy = y - (lightpos >> 2) + 64;
    if(vy >= 0 && vy < 240)
      for(int x = 0; x < 128; x++)
      {
        int c = (((texture.get(1, x * 2, y * 2) & 15 ) * flare1) >> 8) | 0x80;
        graphics.backbuffer[vy][x + 32 + 64] = c;
      }
  }
  for(int y = 255; y >= 0; y--)
  {
    int light = lightpos - 256 + y;
    if(light > 256)
      light = 256;
    for(int x = 0; x < 256; x++)
    {
      int vx = (costab[x] >> 8) + 160;
      int vy = ((sintab[x] * costab[y]) >> 16) + 180;
      if(vy >= 240) continue;
      if(light <= 0)
        graphics.dotFast(vx, vy, graphics.rgb(0, 0, 0));
      else
      {
          int c = texture.get(0, 256 - x, (-rot - y) & 511);
          int r = (light * (c & 15)) >> 4;
          int g = (light * ((c >> 4) & 15)) >> 4;
          int b = (light * ((c >> 8) & 15)) >> 4;
          graphics.dotFast(vx, vy, graphics.rgb(r, g, b));
      }
    }
  }
  if(lightpos > 120)
  for(int y = 0; y < 256; y++)
  {
    int vy = y - (lightpos >> 2);
    if(vy >= 0 && vy < 240)
      for(int x = 0; x < 256; x++)
      {
        int c = (((texture.get(1, x, y) & 15 ) * flare2) >> 8);
        unsigned short oc = graphics.backbuffer[vy][x + 32];
        c += (oc & 15);
        if(c > 15) c = 15;
        graphics.backbuffer[vy][x + 32] = (oc & 0xf0) + c;
      }
  }  

  int ttext = (t - 30000)/100;
  if(ttext > 0)
  {
    char *text0 = "Greetings from bitluni's lab!";
    char text[32];
    int i = 0;
    for(; i < ttext && i < 32; i++)
    {
      text[i] = text0[i];
      if(!text[i]) break;
    }
    text[i] = 0;
    graphics.setCursor(160 - 4 * i, 16);
    graphics.setTextColor(graphics.rgb(255, 255, 255));
    graphics.print(text);
  }
  graphics.end();
}


