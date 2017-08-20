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

#include <list>
#include <thread>
#include <mutex>

typedef struct {
  int pitch;
  int velo;
  int channel;
} tnote;

// Arduino-assisted NeoPixel-based PianoTutor helper class
class Tutor {
      int tutorSerial;		// serial port file descriptor
      std::list<tnote> tutorEvents;
      bool checkSerial();
      void setTutorLight(int pitch, int velo, int channel);
      void clearTutorLight(int pitch);
      std::mutex mtx;

 public:
      Tutor();
      void addKey(int pitch, int velo, int channel);
      void clearKey(int pitch);
      void clearKeys();
      size_t size() { return tutorEvents.size(); }
};

#endif
