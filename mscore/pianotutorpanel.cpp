//=============================================================================
//  MuseScore
//  Linux Music Score Editor
//  $Id: pianotutorpanel.cpp 4775 2011-09-12 14:25:31Z wschweer $
//
//  Copyright (C) 2002-2016 Werner Schweer and others
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

#include "pianotutorpanel.h"
#include "libmscore/sig.h"
#include "libmscore/score.h"
#include "libmscore/repeatlist.h"
#include "seq.h"
#include "musescore.h"
#include "libmscore/measure.h"
#include <math.h>
#include <sstream>
#include <iomanip>
#include <qcolordialog.h>

namespace Ms {

static char h[] = "0123456789ABCDEF";
static std::string col2hex(int *col) {
  char c[] = "000000";
  c[0] = h[col[0] >> 4];
  c[1] = h[col[0] & 0x0f];
  c[2] = h[col[1] >> 4];
  c[3] = h[col[1] & 0x0f];
  c[4] = h[col[2] >> 4];
  c[5] = h[col[2] & 0x0f];
  return std::string(c);
}

//---------------------------------------------------------
//   PianoTutorPanel
//---------------------------------------------------------

PianoTutorPanel::PianoTutorPanel(QWidget* parent)
    : QDockWidget("PianoTutorPanel", parent)
      {
      setupUi(this);
      setWindowFlags(Qt::Tool);
      setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);
      setAllowedAreas(Qt::DockWidgetAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea));
      MuseScore::restoreGeometry(this);

      showConfig();

      connect(lightsPerMeter, SIGNAL(editTextChanged(const QString &)), this, SLOT(onParamsChanged()));
      connect(midCLight, SIGNAL(textChanged(const QString &)), this, SLOT(onParamsChanged()));
      connect(backwardLayout, SIGNAL(stateChanged(int)), this, SLOT(onParamsChanged()));
      connect(leftHandColor, SIGNAL(clicked()), this, SLOT(onLeftHandColClicked()));
      connect(rightHandColor, SIGNAL(clicked()), this, SLOT(onRightHandColClicked()));
      connect(tutorWizard, SIGNAL(clicked()), this, SLOT(onWizardClicked()));

      wizard_ = 0;
      }

PianoTutorPanel::~PianoTutorPanel()
      {
      // if widget is visible, store geometry and pos into settings
      // if widget is not visible/closed, pos is not reliable (and anyway
      // has been stored into settings when the widget has been hidden)
      if (isVisible()) {
            MuseScore::saveGeometry(this);
            }
      }

//---------------------------------------------------------
//   closeEvent
//
//    Called when the PianoTutorPanel is closed with its own button
//    but not when it is hidden with the main menu command
//---------------------------------------------------------

void PianoTutorPanel::closeEvent(QCloseEvent* ev)
      {
      emit closed(false);
      QWidget::closeEvent(ev);
      }

//---------------------------------------------------------
//   hideEvent
//
//    Called both when the PianoTutorPanel is closed with its own button and
//    when it is hidden via the main menu command
//
//    Stores widget geometry and position into settings.
//---------------------------------------------------------

void PianoTutorPanel::hideEvent(QHideEvent* ev)
      {
      MuseScore::saveGeometry(this);
      QWidget::hideEvent(ev);
      }

void PianoTutorPanel::showConfig() {
  lightsPerMeter->blockSignals(true);
  midCLight->blockSignals(true);
  backwardLayout->blockSignals(true);

  lightsPerMeter->setCurrentText(QString::number(72 * fabs(tutor_.getCoeff())));
  midCLight->setText(QString::number(tutor_.getC4Light()));
  backwardLayout->setChecked(tutor_.getCoeff() < 0);

  lightsPerMeter->blockSignals(false);
  midCLight->blockSignals(false);
  backwardLayout->blockSignals(false);

  leftHandColor->setStyleSheet(QString::fromStdString(std::string("background-color:#") + col2hex(tutor_.getColor(1))));
  rightHandColor->setStyleSheet(QString::fromStdString(std::string("background-color:#") + col2hex(tutor_.getColor(0))));
}

//---------------------------------------------------------
//   showEvent
//---------------------------------------------------------

void PianoTutorPanel::showEvent(QShowEvent* e)
      {
      QWidget::showEvent(e);
      activateWindow();
      setFocus();
      }

void PianoTutorPanel::keyPressEvent(QKeyEvent* ev) {
      if (ev->key() == Qt::Key_Escape && ev->modifiers() == Qt::NoModifier) {
            close();
            return;
            }
      QWidget::keyPressEvent(ev);
      }

//---------------------------------------------------------
//   changeEvent
//---------------------------------------------------------

void PianoTutorPanel::changeEvent(QEvent *event)
      {
      QWidget::changeEvent(event);
      if (event->type() == QEvent::LanguageChange)
            retranslate();
      }

void PianoTutorPanel::onParamsChanged()
      {
	double lpm = lightsPerMeter->currentText().toDouble();
	tutor_.setC4Light(midCLight->text().toInt());
	tutor_.setCoeff(lpm / 72.0 * (backwardLayout->isChecked() ? -1 : 1));
      }

void PianoTutorPanel::onLeftHandColClicked()
{
  int *c = tutor_.getColor(1);
  QColor col_init(c[0], c[1], c[2]);
  QColor col = QColorDialog::getColor(col_init, this);
  if (col.isValid()) {
    tutor_.setColor(1, col.red(), col.green(), col.blue());
    leftHandColor->setStyleSheet(QString::fromStdString(std::string("background-color:#") + col2hex(tutor_.getColor(1))));
  }
}

void PianoTutorPanel::onRightHandColClicked()
{
  int *c = tutor_.getColor(0);
  QColor col_init(c[0], c[1], c[2]);
  QColor col = QColorDialog::getColor(col_init, this);
  if (col.isValid()) {
    tutor_.setColor(0, col.red(), col.green(), col.blue());
    rightHandColor->setStyleSheet(QString::fromStdString(std::string("background-color:#") + col2hex(tutor_.getColor(0))));
  }
}

void PianoTutorPanel::onWizardClicked()
{
  wizard_ = new QMessageBox(this);
  wizard_->setText("Please, press the key on your keyboard closest to the lighted LED");
  wizard_->setStandardButtons(QMessageBox::Cancel);
  wizard_->setWindowModality(Qt::ApplicationModal);
  tutor_.addKey(60, 127, 0);
  tutor_.flush();
  wizard_->exec();
}

void PianoTutorPanel::midiNoteReceived(int ch, int pitch, int velo) {
  if (wizard_) {
    // pitch corresponds to midCLight
    tutor_.setC4Pitch(pitch);
    wizard_->close();
    wizard_ = 0;
    showConfig();
  }
}

}
