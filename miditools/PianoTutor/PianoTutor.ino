/* Piano Tutor - A piano tutoring/learning system based on MuseScore and LEDs.
 *
 * Copyright (C) 2017  Tommaso Cucinotta
 *
 * This progra, is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

// Which pin on the Arduino is connected to the NeoPixels?
// On a Trinket or Gemma we suggest changing this to 1
#define PIN            7

// How many NeoPixels are attached to the Arduino?
#define NUMPIXELS      144

// When we setup the NeoPixel library, we tell it how many pixels, and which pin to use to send signals.
// Note that for older NeoPixel strips you might need to change the third parameter--see the strandtest
// example for more information on possible values.
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

uint32_t col_white, col_black, col_off;

void setup() {
  col_white = pixels.Color(50, 50, 50);
  col_black = pixels.Color(10, 10, 50);
  col_off = pixels.Color(0, 0, 0);
  pixels.begin(); // This initializes the NeoPixel library.
  pixels.show();

  Serial.begin(115200);
  Serial.println("PianoTutor v0.2 is ready!");
}

char buf[9];  // hex-encoded key+rgb+sep, format: kkrrggbbs

int char2hex(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  else if (c >= 'a' && c <= 'f')
    return 10 + c - 'a';
  else if (c >= 'A' && c <= 'F')
    return 10 + c - 'A';
}

void loop() {
  int k, r, g, b;
  int avail = Serial.available();
  if (avail == 0)
    return;
  char ch = Serial.peek();
//  Serial.print("ch: ");
//  Serial.print(ch);
//  Serial.print(", avail: ");
//  Serial.println(avail);
  if (ch != 'k' && ch != 'K' && ch != 'h' && ch != 'H' && ch != 'c' && ch != 'C' && ch != 'F') {
//    if (ch != '\n') {
//      Serial.print("Ign: ");
//      Serial.println(ch);
//    }
    Serial.read(); // discard non-recognized command
    return;
  }
  if ((ch == 'k' || ch == 'K') && avail >= 16) {
    Serial.read(); // skip 'k'
    k = Serial.parseInt();
    Serial.read(); // skip 'r'
    r = Serial.parseInt();
    Serial.read(); // skip 'g'
    g = Serial.parseInt();
    Serial.read(); // skip 'b'
    b = Serial.parseInt();
    pixels.setPixelColor(k, r, g, b);
    if (ch == 'k') {
      pixels.show();
    }
  } else if ((ch == 'h' || ch == 'H') && avail >= 9) {
    Serial.read(); // skip 'h'
    Serial.readBytes(buf, 8);
    k = (char2hex(buf[0]) << 4) | char2hex(buf[1]);
    r = (char2hex(buf[2]) << 4) | char2hex(buf[3]);
    g = (char2hex(buf[4]) << 4) | char2hex(buf[5]);
    b = (char2hex(buf[6]) << 4) | char2hex(buf[7]);
//    Serial.print("cmd: k");
//    Serial.print(k);
//    Serial.print('r');
//    Serial.print(r);
//    Serial.print('g');
//    Serial.print(g);
//    Serial.print('b');
//    Serial.println(b);
    pixels.setPixelColor(k, r, g, b);
    if (ch == 'h') {
      pixels.show();
    }
  } else if (ch == 'F') {
    Serial.read(); // skip 'F';
    pixels.show();
  } else if (ch == 'c' || ch == 'C') {
    Serial.read(); // skip 'c';
    for (int k = 0; k < NUMPIXELS; ++k)
      pixels.setPixelColor(k, 0, 0, 0);
    if (ch == 'c')
      pixels.show();
  }
}

