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
#include <time.h>

static const char *TUTOR_SERIAL_DEVICE="/dev/ttyACM0";
static const char *TUTOR_SERIAL_DEVICE2="/dev/ttyACM1";

static char cmd[1024];
static char *curr_cmd = cmd;

int def_colors[2][3] = {
  { 16, 0, 16},
  { 0, 16, 16}
};

Tutor::Tutor() : tutorSerial(-1), num_curr_events(0), c4light(71), coeff(-2.0) {
  // mark all notes as unused
  for (int i = 0; i < 256; i++) {
    notes[i].velo = -1;
  }
  memcpy(colors, def_colors, sizeof(colors));
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

void Tutor::safe_write(char *data, int len) {
  data[len] = '\0';
  while (len > 0) {
    tcdrain(tutorSerial);
    int written = write(tutorSerial, data, (len < 17 ? len : 17));
    if (written < 0) {
      perror("write() failed!");
      close(tutorSerial);
      tutorSerial = -1;
      return;
    }
    data += written;
    len -= written;
    usleep(10000);
  }
}

void Tutor::flushNoLock() {
  if (curr_cmd > cmd && checkSerial()) {
    safe_write(cmd, curr_cmd - cmd);
    curr_cmd = cmd;
  }
}

int Tutor::pitchToLight(int pitch) {
  int led = round((pitch - 60) * coeff + c4light);
  if (led < 0)
    led = 0;
  else if (led > 255)
    led = 255;
  printf("pitch %d -> light %d\n", pitch, led);
  return led;
}

void Tutor::setC4Pitch(int pitch) {
  clearKeys();
  c4light -= round((pitch - 60) * coeff);
}

void Tutor::setTutorLight(int pitch, int velo, int channel, int future) {
  if (checkSerial()) {
    int r = colors[channel % 2][0];
    int g = colors[channel % 2][1];
    int b = colors[channel % 2][2];
    if (future > 0) {
      r /= 8;
      g /= 8;
      b /= 8;
    }
    int cmdlen = snprintf(curr_cmd, sizeof(cmd) - 1 - (curr_cmd - cmd),
			  "k%03dr%03dg%03db%03d ", pitchToLight(pitch), r, g, b);
    curr_cmd += cmdlen;
    //safe_write(cmd, cmdlen);
  }
}

void Tutor::clearTutorLight(int pitch) {
  if (checkSerial()) {
    int cmdlen = snprintf(curr_cmd, sizeof(cmd) - 1 - (curr_cmd - cmd),
			  "k%03dr%03dg%03db%03d ", pitchToLight(pitch), 0, 0, 0);
    curr_cmd += cmdlen;
    //safe_write(cmd, cmdlen);
  }
}

void Tutor::addKey(int pitch, int velo, int channel, int future) {
  struct timespec prev = {0, 0};
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
    if (future == 0 && n.future > 0) {
      ++num_curr_events;
      if (n.ts.tv_sec != 0 || n.ts.tv_nsec != 0)
	prev = n.ts;
    }
    if (future > n.future || (future == n.future && velo < n.velo))
      return;
  } else {
    if (future == 0)
      ++num_curr_events;
  }
  n = (tnote) {velo, channel, future, {0, 0}};
  setTutorLight(pitch, velo, channel, future);

  // Future keys pressed less than 100ms ago are automatically cleared
  // Purposedly light up then turn off LED (unless too fast to be visible)
  if (prev.tv_sec != 0 || prev.tv_nsec != 0) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    unsigned long diff_us =
      (now.tv_sec - prev.tv_sec) * 1000000 + (now.tv_nsec - prev.tv_nsec) / 1000;
    if (diff_us < 100000) {
      clearTutorLight(pitch);
      --num_curr_events;
      n.velo = -1;
    }
    //printf("pitch %d, diff_us: %lu, size: %d\n", pitch, diff_us, num_curr_events);
  }
}

void Tutor::clearKey(int pitch) {
  pitch &= 255;
  std::lock_guard<std::mutex> lock(mtx);
  tnote & n = notes[pitch];
  if (n.velo != -1) {
    if (n.future == 0) {
      clearTutorLight(pitch);
      --num_curr_events;
      n.velo = -1;
    } else {
      clock_gettime(CLOCK_MONOTONIC, &n.ts);
      //printf("Marking time for pitch %d\n", pitch);
    }
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

void Tutor::setColor(int idx, int r, int g, int b) {
  colors[idx][0] = r;
  colors[idx][1] = g;
  colors[idx][2] = b;
}
