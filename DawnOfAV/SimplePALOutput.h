#pragma once
#include "driver/i2s.h"

const int lineSamples = 854;
const int memSamples = 856;
const int syncSamples = 64;
const int burstSamples = 38;

const int burstStart = 70;
const int frameStart = 160; //must be even to simplify buffer word swap
const int imageSamples = 640;

const int syncLevel = 0;
const int blankLevel = 23;
const int burstAmp = 12;
const int maxLevel = 54;
const float burstPerSample = (2 * M_PI) / (13333333 / 4433618.75);
const float colorFactor = (M_PI * 2) / 16;
const float burstPhase = M_PI / 4 * 3;

class SimplePALOutput
{
  public:    
  unsigned short longSync[memSamples];
  unsigned short shortSync[memSamples];
  unsigned short line[2][memSamples];
  unsigned short blank[memSamples];
  short SIN[imageSamples];
  short COS[imageSamples];

  short YLUT[16];
  short UVLUT[16];

  static const i2s_port_t I2S_PORT = (i2s_port_t)I2S_NUM_0;
    
  SimplePALOutput()
  {
    for(int i = 0; i < syncSamples; i++)
    {
      shortSync[i ^ 1] = syncLevel << 8;
      longSync[(lineSamples - syncSamples + i) ^ 1] = blankLevel  << 8;
      line[0][i ^ 1] = syncLevel  << 8;
      line[1][i ^ 1] = syncLevel  << 8;
      blank[i ^ 1] = syncLevel  << 8;
    }
    for(int i = 0; i < lineSamples - syncSamples; i++)
    {
      shortSync[(i + syncSamples) ^ 1] = blankLevel  << 8;
      longSync[(lineSamples - syncSamples + i) ^ 1] = syncLevel  << 8;
      line[0][(i + syncSamples) ^ 1] = blankLevel  << 8;
      line[1][(i + syncSamples) ^ 1] = blankLevel  << 8;
      blank[(i + syncSamples) ^ 1] = blankLevel  << 8;
    }
    for(int i = 0; i < burstSamples; i++)
    {
      int p = burstStart + i;
      unsigned short b0 = ((short)(blankLevel + sin(i * burstPerSample + burstPhase) * burstAmp)) << 8;
      unsigned short b1 = ((short)(blankLevel + sin(i * burstPerSample - burstPhase) * burstAmp)) << 8;
      line[0][p ^ 1] = b0;
      line[1][p ^ 1] = b1;
      blank[p ^ 1] = b0;
    }

    
    /*float r = 1;
    float g = 0;
    float b = 0;
    float y = 0.299 * r + 0.587 * g + 0.114 * b;
    //float u = 0.493 * (b - y);
    //float v = 0.877 * (r - y);
    float u =  -0.147407 * r - 0.289391 * g + 0.436798 * b; //(-0.436798, 0.436798)
    float v = 0.614777 * r - 0.514799 * g - 0.099978 * b;   //(-0.614777, 0.614777)
    for(int i = 0; i < imageSamples; i++)
    {
      int p = frameStart + i;
      int c = p - burstStart;
      unsigned short b0 = ((short)(blankLevel + (y + u * sin(c * burstPerSample) + v * cos(c * burstPerSample)) * 50 )) << 8;
      unsigned short b1 = ((short)(blankLevel + (y + u * sin(c * burstPerSample) - v * cos(c * burstPerSample)) * 50 )) << 8;
      //unsigned short b0 = ((short)(blankLevel + sin(p * burstPerSample + (i/40) * colorFactor) * 20) + 20) << 8;
      line[0][p ^ 1] = b0;
      line[1][p ^ 1] = b1;
    }*/
    
    for(int i = 0; i < imageSamples; i++)
    {
      int p = frameStart + i;
      int c = p - burstStart;
      SIN[i] = round(0.436798 * sin(c * burstPerSample) * 256);
      COS[i] = round(0.614777 * cos(c * burstPerSample) * 256);     
    }

    for(int i = 0; i < 16; i++)
    {
      YLUT[i] = (blankLevel << 8) + round(i / 15. * 256 * maxLevel);
      UVLUT[i] = round((i - 8) / 7. * maxLevel);
    }
  }

  void init()
  {
    i2s_config_t i2s_config = {
       .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
       .sample_rate = 1000000,  //not really used
       .bits_per_sample = (i2s_bits_per_sample_t)I2S_BITS_PER_SAMPLE_16BIT, 
       .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
       .communication_format = I2S_COMM_FORMAT_I2S_MSB,
       .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
       .dma_buf_count = 10,
       .dma_buf_len = lineSamples   //a buffer per line
    };
    
    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);    //start i2s driver
    i2s_set_pin(I2S_PORT, NULL);                           //use internal DAC
    i2s_set_sample_rates(I2S_PORT, 1000000);               //dummy sample rate, since the function fails at high values
  
    //this is the hack that enables the highest sampling rate possible ~13MHz, have fun
    SET_PERI_REG_BITS(I2S_CLKM_CONF_REG(0), I2S_CLKM_DIV_A_V, 1, I2S_CLKM_DIV_A_S);
    SET_PERI_REG_BITS(I2S_CLKM_CONF_REG(0), I2S_CLKM_DIV_B_V, 1, I2S_CLKM_DIV_B_S);
    SET_PERI_REG_BITS(I2S_CLKM_CONF_REG(0), I2S_CLKM_DIV_NUM_V, 2, I2S_CLKM_DIV_NUM_S); 
    SET_PERI_REG_BITS(I2S_SAMPLE_RATE_CONF_REG(0), I2S_TX_BCK_DIV_NUM_V, 2, I2S_TX_BCK_DIV_NUM_S);

    //untie DACs
    SET_PERI_REG_BITS(I2S_CONF_CHAN_REG(0), I2S_TX_CHAN_MOD_V, 3, I2S_TX_CHAN_MOD_S);
    SET_PERI_REG_BITS(I2S_FIFO_CONF_REG(0), I2S_TX_FIFO_MOD_V, 1, I2S_TX_FIFO_MOD_S);
  }

  void sendLine(unsigned short *l)
  {
    i2s_write_bytes(I2S_PORT, (char*)l, lineSamples * sizeof(unsigned short), portMAX_DELAY);
  }

  void sendFrame(char ***frame)
  {
    int l = 0;
    //long sync
    for(int i = 0; i < 3; i++)
      sendLine(longSync);
    //short sync
    for(int i = 0; i < 4; i++)
      sendLine(shortSync);
    //blank lines
    for(int i = 0; i < 37; i++)
      sendLine(blank);

    //image
    for(int i = 0; i < 240; i += 2)
    {
      char *pixels0 = (*frame)[i];
      char *pixels1 = (*frame)[i + 1];
      int j = frameStart;
      for(int x = 0; x < imageSamples; x += 2)
      {
        unsigned short p0 = *(pixels0++);
        unsigned short p1 = *(pixels1++);
        short y0 = YLUT[p0 & 15];
        short y1 = YLUT[p1 & 15];
        unsigned char p04 = p0 >> 4;
        unsigned char p14 = p1 >> 4;
        short u0 = (SIN[x] * UVLUT[p04]);
        short u1 = (SIN[x + 1] * UVLUT[p04]);
        short v0 = (COS[x] * UVLUT[p14]);
        short v1 = (COS[x + 1] * UVLUT[p14]);
        //word order is swapped for I2S packing (j + 1) comes first then j
        line[0][j] = y0 + u1 + v1;
        line[1][j] = y1 + u1 - v1;
        line[0][j + 1] = y0 + u0 + v0;
        line[1][j + 1] = y1 + u0 - v0;
        j += 2;
      }
      sendLine(line[0]);
      sendLine(line[1]);
    }
    
    for(int i = 0; i < 25; i++)
      sendLine(blank);
    for(int i = 0; i < 3; i++)
      sendLine(shortSync);
  }
};


