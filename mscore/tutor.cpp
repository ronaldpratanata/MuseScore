//=============================================================================
//  MuseScore
//  Linux Music Score Editor
//
//  Copyright (C) 2002-2011 Werner Schweer and others
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

#include "config.h"
#include "musescore.h"
#include "tutor.h"

#include <stdio.h>
#include <termios.h>

static const char *TUTOR_SERIAL_DEVICE="/dev/ttyACM0";
static const char *TUTOR_SERIAL_DEVICE2="/dev/ttyACM1";

static char cmd[1024];
static char *curr_cmd = cmd;

Tutor::Tutor() : tutorSerial(-1), num_curr_events(0) {
  // mark all notes as unused
  for (int i = 0; i < 256; i++) {
    notes[i].velo = -1;
  }
}

bool Tutor::checkSerial() {
  if (tutorSerial > 0)
    return true;
  if (tutorSerial < 0) {
    tutorSerial = open(TUTOR_SERIAL_DEVICE, O_WRONLY | O_NOCTTY);
    if (tutorSerial < 0) {
      tutorSerial = open(TUTOR_SERIAL_DEVICE2, O_WRONLY | O_NOCTTY);
    }
    if (tutorSerial > 0) {
      termios tio;
      if (tcgetattr(tutorSerial, &tio) < 0) {
	perror("tcgetattr() failed: ");
	return false;
      }
      tio.c_cflag &= ~(CSIZE | PARENB | CSTOPB | CRTSCTS);
      tio.c_cflag |= CS8 | CREAD | CLOCAL;
      tio.c_iflag &= ~(IXON | IXOFF | IXANY);
      tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
      tio.c_oflag = 0;
      if (cfsetispeed(&tio, B115200) < 0 || cfsetospeed(&tio, B115200) < 0) {
	perror("cfsetXspeed() failed: ");
	return false;
      }
      tcsetattr(tutorSerial, TCSANOW, &tio);
      if (tcsetattr(tutorSerial, TCSAFLUSH, &tio) < 0) {
	perror("tcsetattr() failed: ");
	return false;
      }
      return true;
    }
  }
  return false;
}

static void safe_write(int fd, char *data, int len) {
  data[len] = '\0';
  //printf("Writing to serial: <%s>\n", data);
  while (len > 0) {
    tcdrain(fd);
    int written = write(fd, data, (len < 17 ? len : 17));
    if (written < 0) {
      perror("write() failed!");
      return;
    }
    data += written;
    len -= written;
    usleep(10000);
  }
}

void Tutor::flushNoLock() {
  if (curr_cmd > cmd && checkSerial()) {
    safe_write(tutorSerial, cmd, curr_cmd - cmd);
    curr_cmd = cmd;
  }
}

int colors[][3] = {
  { 50, 0, 50},
  { 0, 50, 50}
};

void Tutor::setTutorLight(int pitch, int velo, int channel, int future) {
  if (pitch >= 24)
    pitch -= 24;
  if (checkSerial()) {
    int r = colors[channel % 2][0];
    int g = colors[channel % 2][1];
    int b = colors[channel % 2][2];
    if (future > 0) {
      r /= 10;
      g /= 10;
      b /= 10;
    }
    int cmdlen = snprintf(curr_cmd, sizeof(cmd) - 1 - (curr_cmd - cmd),
			  "k%03dr%03dg%03db%03d ", pitch*2, r, g, b);
    curr_cmd += cmdlen;
    //safe_write(tutorSerial, cmd, cmdlen);
  }
}

void Tutor::clearTutorLight(int pitch) {
  if (pitch >= 24)
    pitch -= 24;
  if (checkSerial()) {
    int cmdlen = snprintf(curr_cmd, sizeof(cmd) - 1 - (curr_cmd - cmd),
			  "k%03dr%03dg%03db%03d ", pitch*2, 0, 0, 0);
    curr_cmd += cmdlen;
    //safe_write(tutorSerial, cmd, cmdlen);
  }
}

void Tutor::addKey(int pitch, int velo, int channel, int future) {
  if (velo == 0) {
    clearKey(pitch);
    return;
  }
  pitch &= 255;
  std::lock_guard<std::mutex> lock(mtx);
  tnote & n = notes[pitch];
  if (velo == n.velo && channel == n.channel && future == n.future)
    return;
  if (n.velo != -1) {
    if (future == 0 && n.future > 0)
      ++num_curr_events;
    if (future > n.future || (future == n.future && velo < n.velo))
      return;
  } else {
    if (future == 0)
      ++num_curr_events;
  }
  n = (tnote) {velo, channel, future};
  setTutorLight(pitch, velo, channel, future);
}

void Tutor::clearKey(int pitch) {
  pitch &= 255;
  std::lock_guard<std::mutex> lock(mtx);
  tnote & n = notes[pitch];
  if (n.velo != -1) {
    clearTutorLight(pitch);
    if (n.future == 0)
      --num_curr_events;
    n.velo = -1;
  }
}

void Tutor::clearKeys() {
  std::lock_guard<std::mutex> lock(mtx);
  for (int i = 0; i < (int) (sizeof(notes) / sizeof(notes[0])); ++i) {
    if (notes[i].velo != -1) {
      notes[i].velo = -1;
      clearTutorLight(i);
    }
  }
  flushNoLock();
  num_curr_events = 0;
}

void Tutor::flush() {
  std::lock_guard<std::mutex> lock(mtx);
  flushNoLock();
}
