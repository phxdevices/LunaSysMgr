/* @@@LICENSE
*
*      Copyright (c) 2010-2012 Hewlett-Packard Development Company, L.P.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* LICENSE@@@ */




#include "Common.h"

#include <QTouchEvent>
#include <QLineF>
#include <QtCore>
#include <glib.h>
#include "Time.h"
#include "SingleClickGesture.h"
#include "SingleClickGestureRecognizer.h"
#include "Logging.h"

bool SingleClickGestureRecognizer::g_logSingleClick = false;

/* Phoenix
    These should be packages in another class, issue I'm struggling with is hw to bypass the double click for a triple click, 
    or have a double click event wait to see if a triple click event occurs
    */
static int s_clickCount = 0;
    
QGesture* SingleClickGestureRecognizer::create (QObject* target)
{
    return new SingleClickGesture;
}

QGestureRecognizer::Result SingleClickGestureRecognizer::recognize (QGesture* gesture, QObject* watched, QEvent* event)
{
    QGestureRecognizer::Result result = QGestureRecognizer::Ignore;

    SingleClickGesture* singleClickGesture = static_cast<SingleClickGesture*>(gesture);
    
    static int s_tapRadius = -1;
    if (G_UNLIKELY(s_tapRadius < 0))
		s_tapRadius = Settings::LunaSettings()->tapRadius;

    static uint32_t s_lastDblClickTime = 0;

    if (watched == singleClickGesture && event->type() == QEvent::Timer) {
        singleClickGesture->stopSingleClickTimer();
        singleClickGesture->stopTripleClickTimer();
    
    	if (singleClickGesture->state() != Qt::GestureCanceled
    		&& singleClickGesture->state() != Qt::GestureFinished) {
    	    if (!singleClickGesture->m_mouseDown) {
    		  result = QGestureRecognizer::FinishGesture | QGestureRecognizer::ConsumeEventHint;
    	    } else {
        		result |= QGestureRecognizer::ConsumeEventHint;
        		singleClickGesture->m_triggerSingleClickOnRelease = true;
        		singleClickGesture->m_triggerTripleClickOnRelease = true;
    	    }
            // if finger down at the same location for longer than the timer, 
            // the single tap has to be triggered after the release
    	}
    	return result;
    }

    switch (event->type()) {
	case QEvent::TouchBegin:
	case QEvent::TouchUpdate:
	case QEvent::TouchEnd:
	    {
		if (singleClickGesture->state() == Qt::GestureStarted
			|| singleClickGesture->state() == Qt::GestureUpdated)
		{
		    QTouchEvent* touchEvent = static_cast<const QTouchEvent *>(event);
		    // this starts the gesture, so dont check existing state
		    if (touchEvent->touchPoints().size() > 1) {
                singleClickGesture->m_triggerSingleClickOnRelease = false;
                singleClickGesture->stopSingleClickTimer();
                singleClickGesture->m_triggerTripleClickOnRelease = false;
                singleClickGesture->stopTripleClickTimer();
    			result = QGestureRecognizer::CancelGesture;
		    }
		}
		break;
	    }

	case QEvent::MouseButtonPress:
	    {
    		QMouseEvent* mouseEvent = static_cast<const QMouseEvent *>(event);
    		singleClickGesture->stopSingleClickTimer();
    		singleClickGesture->m_penDownPos = mouseEvent->posF();
    		singleClickGesture->setHotSpot (mouseEvent->globalPos());
    		singleClickGesture->m_mouseDown = true;
    		singleClickGesture->m_triggerSingleClickOnRelease = false;
    		singleClickGesture->m_triggerTripleClickOnRelease = false;
    		singleClickGesture->m_modifiers = mouseEvent->modifiers();
    		result = QGestureRecognizer::TriggerGesture;
            singleClickGesture->startSingleClickTimer();
            singleClickGesture->startTripleClickTimer();
    		
    		break;
	    }

	case QEvent::MouseMove:
	    if (singleClickGesture->state() == Qt::GestureStarted
		    || singleClickGesture->state() == Qt::GestureUpdated)
	    {
    		QMouseEvent* mouseEvent = static_cast<const QMouseEvent *>(event);
    		int moveDistance = (int) qAbs(QLineF (mouseEvent->posF(), singleClickGesture->m_penDownPos).length());
    		if (moveDistance > s_tapRadius)
    		{
    		    // move is outside tap radius or multiple fingers are down, so this is not a tap
                singleClickGesture->stopSingleClickTimer();
                singleClickGesture->m_triggerSingleClickOnRelease = false;
                singleClickGesture->stopTripleClickTimer();
                singleClickGesture->m_triggerTripleClickOnRelease = false;
    		    result = QGestureRecognizer::CancelGesture;
                s_clickCount = 0;
    		}
	    }
	    break;

	case QEvent::MouseButtonRelease:
	    // Phoenix - detect triple click using counter
        s_clickCount++;
	    if (singleClickGesture->state() == Qt::GestureStarted
		    || singleClickGesture->state() == Qt::GestureUpdated) {
		    singleClickGesture->m_mouseDown = false;
            if (singleClickGesture->m_triggerSingleClickOnRelease)  {
                result = QGestureRecognizer::FinishGesture;
                singleClickGesture->m_triggerSingleClickOnRelease = false;
            }
            if (singleClickGesture->m_triggerTripleClickOnRelease)  {
                result = QGestureRecognizer::FinishGesture;
                singleClickGesture->m_triggerTripleClickOnRelease = false;
            }
	    }

        if(G_UNLIKELY(g_logSingleClick)){
            g_message("SYSMGR PERF: Single Click Occurred time:%d", Time::curTimeMs());
        }

	    break;

	case QEvent::MouseButtonDblClick:
	    if (singleClickGesture->state() == Qt::GestureStarted
		    || singleClickGesture->state() == Qt::GestureUpdated)
	    {
            singleClickGesture->stopSingleClickTimer();
            singleClickGesture->m_triggerSingleClickOnRelease = false;
            result = QGestureRecognizer::CancelGesture;
            g_debug("Phoenix SingleClickGestureRecognizer double click %d", s_clickCount);

            // TODO Phoenix - How to differentiate double from triple tap?
	    }
	    break;
    default:
    //TODO Need to figure out how to set a custom QEvent on the timer
    // Two issues I need to solve: (1) How to differentiate between double & triple tap
    // (2) How to clear triple tap if delay between taps too long
        // Phoenix - Detect triple tap
        // A Bit kludgey, but basically trap clicks greater than 2, and check if it's 3
        // then treat it like a triple click
        // A better way to do this would be add a MouseButtonTripleClick event to QEvent
        if (s_clickCount == 3 && (singleClickGesture->state() == Qt::GestureStarted
            || singleClickGesture->state() == Qt::GestureUpdated))
        {
            qDebug() << "Phoenix SingleClickGestureRecognizer triple click in test block " << s_clickCount << " release " << singleClickGesture->m_triggerTripleClickOnRelease;
            singleClickGesture->stopTripleClickTimer();
            singleClickGesture->m_triggerTripleClickOnRelease = false;
            result = QGestureRecognizer::CancelGesture;

            s_clickCount = 0;
        }
        break;
    }
    if (s_clickCount > 3) {
        s_clickCount = 0;
    }
    return result;
}
    
void SingleClickGestureRecognizer::reset (QGesture* state)
{
    SingleClickGesture *gesture = static_cast<SingleClickGesture *>(state);
    gesture->stopSingleClickTimer();
    gesture->stopTripleClickTimer();
	gesture->m_penDownPos = QPointF();
    gesture->m_timerId = 0;
    gesture->m_tripleTimerId = 0;
	gesture->m_mouseDown = false;
    gesture->m_triggerSingleClickOnRelease = false;
    gesture->m_triggerTripleClickOnRelease = false;
	gesture->m_modifiers = 0;
	QGestureRecognizer::reset(state);
}
