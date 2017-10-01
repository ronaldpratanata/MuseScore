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
#include <assert.h>

static const char *TUTOR_SERIAL_DEVICE="/dev/ttyACM0";
static const char *TUTOR_SERIAL_DEVICE2="/dev/ttyACM1";

int def_colors[2][3] = {
  { 16, 0, 16},
  { 0, 16, 16}
};

Tutor::Tutor() : tutorSerial(-1), num_curr_events(0),
		 c4light(71), coeff(-2.0),
		 needs_flush(false) {
  // mark all notes as unused
  for (int i = 0; i < 256; i++) {
    notes[i].velo = -1;
  }
  memcpy(colors, def_colors, sizeof(colors));
  last_flushed_ts = (struct timespec) { 0, 0 };
}

bool Tutor::checkSerial() {
  if (tutorSerial > 0)
    return true;
  if (tutorSerial < 0) {
    tutorSerial = open(TUTOR_SERIAL_DEVICE, O_RDWR | O_NOCTTY);
    if (tutorSerial < 0) {
      tutorSerial = open(TUTOR_SERIAL_DEVICE2, O_RDWR | O_NOCTTY);
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
      // // Waiting for "PianoTutor v1.0 is ready!" string
      // int to_read = 25;
      // while (to_read > 0) {
      // 	char ch;
      // 	int len = read(tutorSerial, &ch, 1);
      // 	if (len < 0) {
      // 	  perror("read() failed: ");
      // 	  return false;
      // 	}
      // 	to_read -= len;
      // }
      // //usleep(10000);
      return true;
    }
  }
  return false;
}

void Tutor::safe_write(char *data, int len, bool flush_op) {
  assert(!mtx.try_lock());

  // flush operation(s) update LEDs on the stripe, which is blocking
  // Arduino for a while -- without it pulling bytes out of its
  // hardware 1-byte buffer (!) -- let's wait for at least 2ms after a
  // flush before any further write()
  // note: spin-waiting with lock held in RT thread (!)
  struct timespec now;
  struct timespec &old = last_flushed_ts;
  do {
    clock_gettime(CLOCK_REALTIME, &now);
  } while ((old.tv_sec != 0 || old.tv_nsec != 0) &&
	   ((now.tv_sec - old.tv_sec) * 1000000 + (now.tv_nsec - old.tv_nsec) / 1000 < 10000));
  if (flush_op)
    last_flushed_ts = now;

  // useful to debug/printf() what's about to be written (beware of buffer overruns)
  data[len] = '\0';
  while (len > 0) {
    tcdrain(tutorSerial);
    int written = write(tutorSerial, data, len);
    printf("Written %d bytes (len=%d): %s\n", written, len, data);
    if (written < 0) {
      perror("write() failed!");
      close(tutorSerial);
      tutorSerial = -1;
      return;
    }
    data += written;
    len -= written;
  }
}

void Tutor::flushNoLock() {
  assert(!mtx.try_lock());
  if (checkSerial() && needs_flush) {
    char cmd[4];
    cmd[0]='F';
    cmd[1]='\n';
    printf("flushNoLock(): calling safe_write()\n");
    safe_write(cmd, 2, true);
    needs_flush = false;
  }
}

int Tutor::pitchToLight(int pitch) {
  int reflight, diffpitch;
  if (coeff < 0 && pitch >= 60) {
    reflight = c4light;
    diffpitch = pitch - 60;
  } else {
    reflight = c4light + 1;
    diffpitch = pitch - 59;
  }
  int led = round(diffpitch * coeff + reflight);
  if (led < 0)
    led = 0;
  else if (led > 255)
    led = 255;
  //printf("pitch %d -> light %d\n", pitch, led);
  return led;
}

void Tutor::setC4Pitch(int pitch) {
  clearKeys();
  c4light -= round((pitch - 60) * coeff);
}

void Tutor::setTutorLight(int pitch, int velo, int channel, int future) {
  assert(!mtx.try_lock());
  if (checkSerial()) {
    int r = colors[channel % 2][0];
    int g = colors[channel % 2][1];
    int b = colors[channel % 2][2];
    if (future > 0) {
      r /= 8;
      g /= 8;
      b /= 8;
    }
    char cmd[16];
    int cmdlen = snprintf(cmd, sizeof(cmd) - 1,
			  "H%02x%02x%02x%02x\n", pitchToLight(pitch), r, g, b);
    safe_write(cmd, cmdlen, false);
    needs_flush = true;
  }
}

void Tutor::clearTutorLight(int pitch) {
  assert(!mtx.try_lock());
  if (checkSerial()) {
    char cmd[16];
    int cmdlen = snprintf(cmd, sizeof(cmd) - 1,
			  "H%02x%02x%02x%02x\n", pitchToLight(pitch), 0, 0, 0);
    safe_write(cmd, cmdlen, false);
    needs_flush = true;
  }
}

void Tutor::addKey(int pitch, int velo, int channel, int future) {
  struct timespec prev = (struct timespec) {0, 0};
  if (velo == 0) {
    clearKey(pitch);
    return;
  }
  pitch &= 255;
  tnote & n = notes[pitch];
  std::lock_guard<std::mutex> lock(mtx);
  if (velo == n.velo && channel == n.channel && future == n.future)
    return;
  printf("addKey(): p=%d, v=%d, c=%d, f=%d\n", pitch, velo, channel, future);
  if (n.velo != -1) {
    if (future == 0 && n.future > 0) {
      ++num_curr_events;
      if (n.ts.tv_sec != 0 || n.ts.tv_nsec != 0)
	prev = n.ts;
    } else if (future > n.future || (future == n.future && velo < n.velo)) {
      return;
    }
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
      //printf("diff_us: %lu, now: %ld,%ld, prev: %ld,%ld\n", diff_us, now.tv_sec,now.tv_nsec, prev.tv_sec,prev.tv_nsec);
      clearTutorLight(pitch);
      --num_curr_events;
      n.velo = -1;
    }
    //printf("pitch %d, diff_us: %lu, size: %d\n", pitch, diff_us, num_curr_events);
  }
}

void Tutor::clearKeyNoLock(int pitch, bool mark) {
  printf("clearKey(): p=%d\n", pitch);
  assert(!mtx.try_lock());
  pitch &= 255;
  tnote & n = notes[pitch];
  if (n.velo != -1) {
    if (n.future == 0) {
      clearTutorLight(pitch);
      --num_curr_events;
      n.velo = -1;
    } else if (mark) {
      clock_gettime(CLOCK_MONOTONIC, &n.ts);
      //printf("Marking time for pitch %d: %d,%d\n", pitch, n.ts.tv_sec,n.ts.tv_nsec);
    }
  }
}

void Tutor::clearKey(int pitch, bool mark) {
  std::lock_guard<std::mutex> lock(mtx);
  clearKeyNoLock(pitch, mark);
}

void Tutor::clearKeys() {
  do {
    std::lock_guard<std::mutex> lock(mtx);
    if (checkSerial()) {
      char cmd[4];
      cmd[0]='c'; // 'c' also flushes
      cmd[1]='\n';
      safe_write(cmd, 2, true);
      needs_flush = false;
    }
    for (int i = 0; i < (int) (sizeof(notes) / sizeof(notes[0])); ++i)
      notes[i].velo = -1;
    num_curr_events = 0;
  } while (false);
  //usleep(10000);
}

void Tutor::flush() {
  do {
    std::lock_guard<std::mutex> lock(mtx);
    flushNoLock();
  } while (false);
}

void Tutor::setColor(int idx, int r, int g, int b) {
  colors[idx][0] = r;
  colors[idx][1] = g;
  colors[idx][2] = b;
}

int Tutor::keyPressed(int pitch, int velo) {
  pitch &= 255;
  tnote & n = notes[pitch];
  int rv = -1;
  do {
    std::lock_guard<std::mutex> lock(mtx);
    if (n.velo == -1) {
      rv = -1;
      break;
    }
    if (n.future == 0) {
      printf("Clearing event: pitch=%d\n", pitch);
      clearKeyNoLock(pitch);
      flushNoLock();
      rv = 0;
      break;
    } else if (n.future > 0 && num_curr_events == 0) {
      rv = n.future;
      printf("Clearing future event & skipping: pitch=%d\n", pitch);
      clearKeyNoLock(pitch, true);
      flushNoLock();
      break;
    }
  } while (false);
  //usleep(10000);
  return rv;
}

size_t Tutor::size() {
  std::lock_guard<std::mutex> lock(mtx);
  return num_curr_events;
}
