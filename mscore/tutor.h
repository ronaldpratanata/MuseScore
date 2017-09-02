//=============================================================================
//  MusE Score
//  Linux Music Score Editor
//
//  Copyright (C) 2002-2009 Werner Schweer and others
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//=============================================================================

#ifndef __TUTOR_H__
#define __TUTOR_H__

#include <thread>
#include <mutex>

typedef struct {
  int velo;
  int channel;
  int future;
  struct timespec ts;	// last press abs time
} tnote;

// Arduino-assisted NeoPixel-based PianoTutor helper class
class Tutor {
      int tutorSerial;		// serial port file descriptor
      tnote notes[256];		// addressed by pitch, .velo = -1 means unused
      std::mutex mtx;
      int num_curr_events;

      int c4light;
      double coeff;

      int colors[2][3];

      bool checkSerial();
      void setTutorLight(int pitch, int velo, int channel, int future);
      void clearTutorLight(int pitch);
      void flushNoLock();
      void safe_write(char *data, int len);
      int pitchToLight(int pitch);

 public:
      Tutor();
      void setC4Light(int num) { c4light = num; }
      void setC4Pitch(int pitch);
      int getC4Light() const { return c4light; }
      void setCoeff(double c) { coeff = c; }
      double getCoeff() const { return coeff; }
      int* getColor(int idx) {  return colors[idx];  }
      void setColor(int idx, int r, int g, int b);

      void addKey(int pitch, int velo, int channel, int future = 0);
      void clearKey(int pitch);
      // Return 0 if invalid
      const tnote *getKey(int pitch) { return notes[pitch].velo != -1 ? &notes[pitch] : NULL; }
      void clearKeys();
      size_t size() { return num_curr_events; }
      void flush();
};

#endif
