// Stargate

#include "app.h"
#define SPEED 2                 // Seconds per spin?

typedef struct ring_s ring_t;
struct ring_s
{                               // Definition of a ring
   uint16_t len;
   uint16_t start;
   uint16_t offset;
};

typedef struct stargate_s stargate_t;
struct stargate_s
{
   uint8_t dial[10];            // Dial sequence 1-39 terminated with 0
   uint8_t pos;                 // Symbol at the top
   uint8_t spins;               // The spinning rings
   const ring_t *spin;
   uint8_t chevs;               // The chevrons
   const ring_t *chev;
   uint8_t gates;               // The gate symbols
   const ring_t *gate;
   const ring_t *kawoosh;       // The final kawoosh ring
   uint8_t incoming:1;          // This is incoming
};

const ring_t spin210[] = { {117, 1, 0} };
const ring_t chev210[] = { {117, 1, 0}, {18, 157, 0}, {18, 175, 0}, {18, 193, 0} };
const ring_t gate210[] = { {39, 118, 0} };
const ring_t kawoosh210 = { 117, 1, 0 };

const ring_t spin372[] = { {117, 1, 58}, {117, 175, 58} };
const ring_t chev372[] = { {18, 157, 8}, {117, 175, 58}, {18, 292, 8}, {18, 310, 8}, {45, 328, 20} };
const ring_t gate372[] = { {39, 118, 19} };
const ring_t kawoosh372 = { 117, 1, 19 };

const ring_t spin507[] = { {117, 118, 58}, {117, 292, 58} };
const ring_t chev507[] = { {18, 274, 8}, {117, 292, 58}, {27, 409, 12}, {27, 436, 12}, {45, 463, 20} };
const ring_t gate507[] = { {39, 235, 19} };
const ring_t kawoosh507 = { 117, 1, 19 };

const char *
biggate (app_t * a)
{                               // Special large LED rings
   uint8_t max = a->fader / 2;  // Base brightness
   uint8_t kawooshlen = 117;    // Always 117?
   uint8_t *old = a->data,
      *new = old + kawooshlen;
   stargate_t *g = (void *) (new + kawooshlen);
   if (!a->cycle)
   {                            // Startup
      if (!a->colourset)
      {
         a->g = 255;            // Default glyph colour
         a->colourset = 1;
      }
      if (!a->limit)
         a->limit = 90 * cps;
      old = mallocspi (kawooshlen * 2 + sizeof (stargate_t));
      new = old + kawooshlen;
      g = (void *) (new + kawooshlen);
      memset (g, 0, sizeof (*g));
#define load(x)  if (a->len == x)						\
        {									\
           g->spin = spin##x;							\
           g->spins = sizeof (spin##x) / sizeof (*spin##x);			\
           g->chev = chev##x;							\
           g->chevs = sizeof (chev##x) / sizeof (*chev##x);			\
           g->gate = gate##x;							\
           g->gates = sizeof (gate##x) / sizeof (*gate##x);			\
	   g->kawoosh = &kawoosh##x;						\
	}
      load (210);
      load (372);
      load (507);
#undef	load
      if (!g->chevs)
         return "Bad gate";
      g->pos = 1;               // Home symbol
      if (a->data && ((char *) a->data)[0] == '*')
      {
         g->incoming = 1;
         a->stage = 13;
         free (a->data);
         a->data = NULL;
      }
      if (a->data)
      {                         // Dial sequence specified
         const char glyphs[] = "@ABCDEFGHIJKLMNOPQRSTUVWXYZ&-0123456789";
         for (int q = 0; q < 9 && ((char *) a->data)[q]; q++)
         {
            const char *z = strchr (glyphs, ((char *) a->data)[q]);
            if (!z)
               return "Bad dial string";
            g->dial[q] = 1 + (z - glyphs);
         }
         free (a->data);
      } else
      {                         // Random dial sequence
         int n = 6;
         if (esp_random () >= 0xF0000000)
            n = 8;
         else if (esp_random () >= 0xE0000000)
            n = 7;
         uint64_t f = 0x1FE0000;        // Avoid lower 8 glyphs
         for (int q = 0; q < n; q++)
         {
            int r = esp_random () % (38 - 8 - q) + 1,   // -8 for avoided glyphs
               p = 1;
            while (r || (f & (1LL << p)))
               if ((f & (1LL << p)) || r--)
                  p++;
            f |= (1LL << p);
            g->dial[q] = p;
         }
         g->dial[n] = 1;        // Final
      }
      a->data = old;
   }
   uint8_t q = 255;
   if (a->stop)
      q = 255 * a->stop / a->speed;     // Main fader for end
   void twinkle (void)
   {
      memcpy (old, new, kawooshlen);
      esp_fill_random (new, kawooshlen);
      for (int i = 0; i < kawooshlen; i++)
         new[i] = new[i] / 3 + 32;
   }
   void spinner (uint8_t o)
   {                            // Show 1 in 3 on spin rings
      for (int s = 0; s < g->spins; s++)
         for (int n = 0; n < g->spin[s].len; n++)
            setRGBl (a->start - 1 + g->spin[s].start + (n + o + g->spin[s].offset) % g->spin[s].len, 0, 0, n % 3 ? 0 : max, q);
   }
   uint8_t map (uint8_t c)
   {                            // Chev map
      if (c >= 8 || !g->dial[c + 1])
         c = 0;                 // Last chevron (top)
      else if (++c > 3)
         c += (g->dial[8] ? 0 : 1) + (g->dial[7] ? 0 : 1);      // Skip bottom ones as needed
      return c;
   }
   void lock (uint8_t c, uint8_t l)
   {
      c = map (c);
      for (int n = g->chevs - 2; n < g->chevs; n++)
      {
         uint8_t z = g->chev[n].len / 9;
         if (z & 1)
            setRGBl (a->start - 1 + g->chev[n].start + (g->chev[n].offset + c * z + z / 2) % g->chev[n].len, max, max / 2, 0, l);
      }
   }
   void chev (uint8_t c, int8_t f, int8_t t, uint8_t l)
   {
      if (f < 0)
         f = 0;
      if (t >= g->chevs - 1)
         t = g->chevs - 1;
      c = map (c);
      for (int n = f; n <= t; n++)
      {
         const ring_t *C = &g->chev[n];
         if (C->len == 117)
         {                      // Chevs part of full ring
            setRGBl (a->start - 1 + C->start + (c * 13 + 116 + C->offset) % C->len, max, max, 0, l);
            if (n == f || n == t)
               setRGBl (a->start - 1 + C->start + (c * 13 + C->offset) % C->len, max, max, 0, l);
            setRGBl (a->start - 1 + C->start + (c * 13 + 1 + C->offset) % C->len, max, max, 0, l);
         } else
         {                      // Chevs only
            uint8_t z = C->len / 9;
            uint16_t b = c * z;
            for (int q = 0; q < z; q += (n == f || n == t ? 1 : z - 1))
               setRGBl (a->start - 1 + C->start + (b + q + C->offset) % C->len, max, max, 0, l);
         }
      }
   }
   void gate (uint8_t c, uint8_t l)
   {
      if (!g->incoming && g->dial[c])
         for (int z = 0; z < g->gates; z++)
         {
            if (g->gate[z].len == 39)
               setRGBl (a->start - 1 + g->gate[z].start + (g->gate[z].offset + g->dial[c] - 1) % g->gate[z].len, a->r * max / 255,
                        a->g * max / 255, a->b * max / 255, l);
         }
   }
   void chevs (void)
   {
      for (int c = 0; c < a->stage / 10 - 1 && c < 9; c++)
         if (g->dial[c])
         {
            chev (c, g->chevs - 3, g->chevs - 1, q / 3);        // Top of chevron
            gate (c, q);
            lock (c, q);
         }
   }

   if (a->stage < 10)
      switch (a->stage)
      {
      case 0:                  // Fade up spins
         for (int s = 0; s < g->spins; s++)
            for (int n = 0; n < g->spin[s].len; n++)
               setRGBl (a->start - 1 + g->spin[s].start + n, 0, 0, max, a->step * q / 255);
         if ((a->step += 255 / a->speed) > 255)
         {
            a->step = 0;
            a->stage++;
         }
         break;
      case 1:                  // Fade down spins leaving 1/3 to dial
         for (int s = 0; s < g->spins; s++)
            for (int n = 0; n < g->spin[s].len; n++)
               setRGBl (a->start - 1 + g->spin[s].start + (n + g->spin[s].offset) % g->spin[s].len, 0, 0, max,
                        (n % 3 ? 255 - a->step : 255) * q / 255);
         if ((a->step += 255 / a->speed) > 255)
         {
            a->step = 0;
            a->stage = 10;
         }
         break;
   } else if (a->stage < 100)
      switch (a->stage % 10)
      {                         // 10 to 90 for chevrons 1 to 9
      case 0:                  // Spin spins to position
         {
            uint8_t target = g->dial[a->stage / 10 - 1];
            int8_t dir = -1;
            if ((g->pos < target && g->pos + 18 >= target) || (g->pos > target && target + 18 < g->pos))
               dir = 1;
            if (dir == -1)
               spinner (a->step);       // Yes direction is opposite as we are moving symbol to top
            else
               spinner (3 - a->step);
            chevs ();
            a->step++;
            if (a->step == 3)
            {
               g->pos += dir;
               if (!g->pos)
                  g->pos = 39;
               else if (g->pos == 40)
                  g->pos = 1;
               a->step = 0;
               if (target == g->pos)
                  a->stage++;
            }
            break;
         }
      case 1:                  // Engage top chevron
         spinner (0);
         chev (8, g->chevs * (255 - a->step) / 256, g->chevs - 1, q);
         chevs ();
         if ((a->step += 255 / a->speed) > 255)
         {
            a->step = 0;
            a->stage++;
         }
         break;
      case 2:                  // Disengage top chevron and glyph
         {
            spinner (0);
            chev (8, g->chevs * a->step / 256, g->chevs - 1, q);
            gate (a->stage / 10 - 1, a->step * q / 255);
            chevs ();
            if ((a->step += 255 / a->speed) > 255)
            {
               a->step = 0;
               a->stage++;
            }
         }
         break;
      case 3:                  // Light up selected chevron
         if (!g->incoming)
            spinner (0);
         chev (a->stage / 10 - 1, g->chevs - 3, g->chevs - 1, a->step * q / 255 / 3);
         lock (a->stage / 10 - 1, a->step * q / 255);
         gate (a->stage / 10 - 1, q);
         chevs ();
         if ((a->step += 255 / a->speed) > 255)
         {
            a->step = 0;
            a->stage += 7;
            if (g->incoming)
               a->stage += 3;
            if (!g->dial[a->stage / 10 - 1])
            {
               a->stage = 100;  // Last one
               a->step = a->speed;
               memset (new, 255, kawooshlen);
               twinkle ();
            }
         }
         break;
   } else
   {
      for (int i = 0; i < kawooshlen; i++)
      {
         uint8_t l = (int) (a->speed - a->step) * new[i] / a->speed + (int) a->step * old[i] / a->speed;
         setRGBl (a->start - 1 + g->kawoosh->start + i, l, l, l, q);
      }
      if (!--a->step)
      {                         // Next
         a->step = a->speed;
         twinkle ();
      }
      chevs ();
   }
   return NULL;
}

const char *
appstargate (app_t * a)
{
   if (a->len == 210 || a->len == 372 || a->len == 507)
      return biggate (a);
   if (!a->cycle)
   {
      free (a->data);           // Not used supplied
      a->data = mallocspi (a->len * 2 + 1);
   }
   uint8_t *old = a->data,
      *new = old + a->len,
      *posp = new + a->len,
      pos = *posp;
   uint8_t top;
   int8_t dir = 1;
   if (a->top < 0)
   {
      top = -a->top - a->start;
      dir = -1;
   } else
      top = a->top - a->start;
   if (!a->cycle)
   {                            // Sanity checks, etc
      if (!a->limit)
         a->limit = 60 * cps;
      a->step = a->speed;
      pos = esp_random ();
      if (!a->colourset)
         a->b = 63;             // Default ring blue
   }
   uint8_t q = 255;
   if (a->stop)
      q = 255 * a->stop / a->speed;
   void ring (uint8_t l)
   {
      for (unsigned int i = 0; i < a->len; i++)
         setl (a->start + i, a, i, a->len, (int) l * wheel[(256 * i / a->len + pos) & 255] / 255);
   }
   void chevron (uint8_t n, uint8_t l)
   {
      if (l > q)
         l = q;
      switch (n)
      {
      case 0:
         n = top;
         break;
      case 1:
         n = (top + a->len + dir * 4 * a->len / 39) % a->len;
         break;
      case 2:
         n = (top + a->len + dir * 8 * a->len / 39) % a->len;
         break;
      case 3:
         n = (top + a->len + dir * 12 * a->len / 39) % a->len;
         break;
      case 4:
         n = (top + a->len - dir * 12 * a->len / 39) % a->len;
         break;
      case 5:
         n = (top + a->len - dir * 8 * a->len / 39) % a->len;
         break;
      case 6:
         n = (top + a->len - dir * 4 * a->len / 39) % a->len;
         break;
      case 7:
         n = top;
         break;
      default:
         return;
      }
      setRGBl (a->start + n, 255, 127, 0, l);
   }
   void twinkle (void)
   {
      memcpy (old, new, a->len);
      esp_fill_random (new, a->len);
      for (int i = 0; i < a->len; i++)
         new[i] = new[i] / 4 + 32;
   }

   if (!a->stage)
   {                            // Fade up
      ring (255 * (a->speed - a->step) / a->speed);
      if (!--a->step)
      {
         a->stage = 10;
         a->step = a->speed + esp_random () % (a->speed * 2);
      }
   } else if (a->stage < 100)
   {                            // Dialling
      ring (255);
      for (int i = 1; i < a->stage / 10; i++)
         chevron (i, q);
      if (!(a->stage % 10))
      {                         // Dial
         if ((a->stage / 20) % 2)
            pos += 256 / a->speed / SPEED;
         else
            pos -= 256 / a->speed / SPEED;
         if (!--a->step)
         {
            a->stage++;
            a->step = a->speed;
         }
      } else if ((a->stage % 10) == 1)
      {                         // Lock top
         chevron (0, 255 * (a->speed - a->step) / a->speed);
         if (!--a->step)
         {
            a->stage++;
            a->step = a->speed;
         }
      } else if (a->stage == 72)
      {                         // Open gate
         chevron (0, q);
         uint8_t l = 255 * (a->speed - a->step) / a->speed;
         for (int i = 0; i < a->len; i++)
         {
            if (getR (a->start + i) < l)
               setR (a->start + i, l);
            if (getG (a->start + i) < l)
               setG (a->start + i, l);
            if (getB (a->start + i) < l)
               setB (a->start + i, l);
            if (getW (a->start + i) < l)
               setW (a->start + i, l);
         }
         if (!--a->step)
         {                      // Next chevron
            a->stage = 100;
            a->step = a->speed;
            memset (new, 255, a->len);
            twinkle ();
         }
      } else if ((a->stage % 10) == 2)
      {                         // Chevron
         chevron (0, 255 * a->step / a->speed);
         chevron (a->stage / 10, 255 * (a->speed - a->step) / a->speed);
         if (!--a->step)
         {                      // Next chevron
            a->stage += 8;
            a->step = a->speed + esp_random () % (a->speed * 2);
         }
      }
   } else
   {                            // Twinkling
      for (int i = 0; i < a->len; i++)
      {
         uint8_t l = (int) (a->speed - a->step) * new[i] / a->speed + (int) a->step * old[i] / a->speed;
         setRGBl (a->start + i, l, l, l, q);
      }
      if (!--a->step)
      {                         // Next
         a->step = a->speed;
         twinkle ();
      }
   }
   *posp = pos;
   return NULL;
}
