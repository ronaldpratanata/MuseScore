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
} tnote;

// Arduino-assisted NeoPixel-based PianoTutor helper class
class Tutor {
      int tutorSerial;		// serial port file descriptor
      tnote notes[256];		// addressed by pitch, .velo = -1 means unused
      std::mutex mtx;
      int num_curr_events;

      bool checkSerial();
      void setTutorLight(int pitch, int velo, int channel, int future);
      void clearTutorLight(int pitch);
      void flushNoLock();

 public:
      Tutor();
      void addKey(int pitch, int velo, int channel, int future = 0);
      void clearKey(int pitch);
      void clearKeys();
      size_t size() { return num_curr_events; }
      void flush();
};

#endif
