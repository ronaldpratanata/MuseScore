//=============================================================================
//  MuseScore
//  Linux Music Score Editor
//  $Id: pianotutorpanel.h 4722 2011-09-01 12:26:07Z wschweer $
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

#ifndef __PIANOTUTORPANEL_H__
#define __PIANOTUTORPANEL_H__

#include "ui_pianotutorpanel.h"
#include "tutor.h"

namespace Ms {

class Score;

//---------------------------------------------------------
//   PianoTutorPanel
//---------------------------------------------------------

class PianoTutorPanel : public QDockWidget, private Ui::PianoTutorPanelBase {
      Q_OBJECT

      Tutor tutor_;

      virtual void closeEvent(QCloseEvent*);
      virtual void hideEvent (QHideEvent* event);
      virtual void showEvent(QShowEvent *);
      virtual void keyPressEvent(QKeyEvent*) override;

   private slots:
     void onParamsChanged();
     void onLeftHandColClicked();
     void onRightHandColClicked();

   protected:
      virtual void changeEvent(QEvent *event);
      void retranslate()  { retranslateUi(this); }

   signals:
      void closed(bool);

   public slots:

   public:
      PianoTutorPanel(QWidget* parent = 0);
      ~PianoTutorPanel();
      Tutor & tutor() { return tutor_; }
      bool tutorEnabled() { return tutorEnabledCB->isChecked(); }
      bool tutorWait() { return tutorWaitCB->isChecked(); }
      bool tutorLookAhead() { return tutorLookAheadCB->isChecked(); }
      };

} // namespace Ms
#endif
