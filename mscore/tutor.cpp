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

Tutor::Tutor() : tutorSerial(-1) {  }

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
  qDebug("Writing to serial: <%s>\n", data);
  while (len > 0) {
    int written = write(fd, data, len);
    if (written < 0) {
      perror("write() failed!");
      return;
    }
    data += written;
    len -= written;
  }
  usleep(10000);
  //fsync(fd);
  tcflush(fd, TCIOFLUSH);
}

int colors[][3] = {
  { 50, 0, 0},
  { 0, 50, 0}
};

void Tutor::setTutorLight(int pitch, int velo, int channel) {
  if (pitch >= 24)
    pitch -= 24;
  if (checkSerial()) {
    char cmd[18];
    int r = colors[channel % 2][0];
    int g = colors[channel % 2][1];
    int b = colors[channel % 2][2];
    int cmdlen = sprintf(cmd, "k%03dr%03dg%03db%03d\n", pitch*2, r, g, b);
    safe_write(tutorSerial, cmd, cmdlen);
  }
}

void Tutor::clearTutorLight(int pitch) {
  if (pitch >= 24)
    pitch -= 24;
  if (checkSerial()) {
    char cmd[18];
    int cmdlen = sprintf(cmd, "k%03dr%03dg%03db%03d\n", pitch*2, 0, 0, 0);
    safe_write(tutorSerial, cmd, cmdlen);
  }
}

void Tutor::addKey(int pitch, int velo, int channel) {
  if (velo == 0) {
    clearKey(pitch);
    return;
  }
  std::lock_guard<std::mutex> lock(mtx);
  tutorEvents.push_back((tnote) {pitch, velo, channel});
  setTutorLight(pitch, velo, channel);
}

void Tutor::clearKey(int pitch) {
  std::lock_guard<std::mutex> lock(mtx);
  for (auto it = tutorEvents.begin(); it != tutorEvents.end(); /* nothing */) {
    //qDebug("Comparing with: pitch=%d, vel=%d\n", it->pitch, it->velo);
    if (it->pitch == pitch) {
      //qDebug("Match!\n");
      clearTutorLight(pitch);
      it = tutorEvents.erase(it);
    } else
      ++it;
  }
}

void Tutor::clearKeys() {
  std::lock_guard<std::mutex> lock(mtx);
  for (auto it = tutorEvents.begin(); it != tutorEvents.end(); ++it)
    clearTutorLight(it->pitch);
  tutorEvents.clear();
}
