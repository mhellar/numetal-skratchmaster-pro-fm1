/* host: render the CyberSkratch bank with each SAM voice, print the sizes (8-bit at 22050 Hz, silence trimmed) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SamData.h"
#include "sam.h"
#include "reciter.h"
SamData *samdata;
static SamData sd;
static unsigned char buf[300000];
static int n;
static void cb(void *u, unsigned char b) { (void)u; if (n < (int)sizeof buf) buf[n++] = b; }
static const char *W[] = {"AUTOMATON","FEEDBACK","ENTROPY","SIGNAL","NOISE","HIGHER STATE","HOMEOSTASIS","INFORMATION",
 "MESSAGE","LOOP","OSCILLATE","ZERO ONE","MEMORY","PATTERN","SYNAPSE","PARADOX","ECHO","NEXUS","KALEIDOSCOPE","WONDER",
 "ALWAYS MERRY AND BRIGHT","STAND STILL LIKE THE HUMMINGBIRD","INNER SPACE","CRYSTALLINE","SIGHBER SKRATCH","SIGHBER","SKRATCH"};
static const unsigned char V[6][4] = {{92,60,190,190},{72,64,128,128},{72,64,110,160},{100,64,150,200},{82,72,110,105},{82,32,145,145}};
int main(void) {
  samdata = &sd;
  for (int v = 0; v < 6; v++) {
    int tot = 0;
    for (int w = 0; w < 27; w++) {
      char in[256]; strcpy(in, W[w]); strcat(in, "[");
      n = 0; memset(&sd, 0, sizeof sd);
      EnableSingmode(0); SetSpeed(V[v][0]); SetPitch(V[v][1]); SetThroat(V[v][2]); SetMouth(V[v][3]);
      if (!TextToPhonemes(in)) { printf("fail %s\n", W[w]); continue; }
      SetInput(in); SAMMain(cb, 0);
      int a = 0, b = n; while (a < b && abs(buf[a]-128) < 4) a++; while (b > a && abs(buf[b-1]-128) < 4) b--;
      tot += b - a;
      if (v == 0) printf("%-34s %6d\n", W[w], b - a);
    }
    printf("voice %d total %d bytes\n", v, tot);
  }
}
