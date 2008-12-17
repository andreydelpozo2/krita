/*
 * Copyright (c) 2008 Cyrille Berger <cberger@cberger.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <kis_ruler_assistant_tool.h>

#include <qpainter.h>

#include <kis_debug.h>
#include <klocale.h>

#include <KoViewConverter.h>
#include <KoPointerEvent.h>

#include <canvas/kis_canvas2.h>
#include <kis_cursor.h>
#include <kis_image.h>
#include <kis_view2.h>

#include "RulerAssistant.h"
#include "RulerDecoration.h"
#include "Ruler.h"

KisRulerAssistantTool::KisRulerAssistantTool(KoCanvasBase * canvas)
    : KisTool(canvas, KisCursor::arrowCursor()), m_canvas( dynamic_cast<KisCanvas2*>(canvas) )
{
    Q_ASSERT(m_canvas);
    setObjectName("tool_rulerassistanttool");
    m_mode = MODE_NOTHING;
    m_widget = 0;
}

KisRulerAssistantTool::~KisRulerAssistantTool()
{
}

QPointF adjustPointF( const QPointF& _pt, const QRectF& _rc)
{
    return QPointF( qBound( _rc.left(), _pt.x(), _rc.right() ), qBound( _rc.top(), _pt.y(), _rc.bottom() ) );
}

void KisRulerAssistantTool::activate(bool )
{
    // Add code here to initialize your tool when it got activated
    KisTool::activate();
    
    RulerAssistant* ra = dynamic_cast<RulerAssistant*>( KisPaintingAssistant::currentAssistant() );
    
    if( !ra )
    {
      ra = new RulerAssistant();
      KisPaintingAssistant::setCurrentAssistant( ra );
    }
    QRectF imageArea = QRectF( pixelToView( QPoint(0,0) ),
                               m_canvas->image()->pixelToDocument( QPoint( m_canvas->image()->width(), m_canvas->image()->height()) ) );
    
    dbgPlugins << imageArea << ra->ruler()->point1() << ra->ruler()->point2();
    
    ra->ruler()->setPoint1( adjustPointF( ra->ruler()->point1(), imageArea ) );
    ra->ruler()->setPoint2( adjustPointF( ra->ruler()->point2(), imageArea ) );
    
    m_rulerDecoration = dynamic_cast<RulerDecoration*>(m_canvas->decoration("ruler"));
    Q_ASSERT(m_rulerDecoration);
    m_rulerDecoration->setRuler( ra->ruler() );
    m_rulerDecoration->setVisible( true);
}

void KisRulerAssistantTool::deactivate()
{
    // Add code here to initialize your tool when it got deactivated
    KisTool::deactivate();
}


inline double norm2(const QPointF& p)
{
    return sqrt(p.x() * p.x() + p.y() * p.y() );
}

void KisRulerAssistantTool::mousePressEvent(KoPointerEvent *event)
{
  if( norm2(event->point - m_rulerDecoration->ruler()->point1() ) < 10)
  {
      m_mode = MODE_POINT1DRAGING;
      m_canvas->updateCanvas(); // TODO update only the relevant part of the canvas
  } else if( norm2(event->point - m_rulerDecoration->ruler()->point2() ) < 10 )
  {
      m_mode = MODE_POINT2DRAGING;
      m_canvas->updateCanvas(); // TODO update only the relevant part of the canvas
  } else {
      event->ignore();
  }
}


void KisRulerAssistantTool::mouseMoveEvent(KoPointerEvent *event)
{
    if( m_mode == MODE_POINT1DRAGING)
    {
        m_rulerDecoration->ruler()->setPoint1( event->point);
        m_canvas->updateCanvas(); // TODO update only the relevant part of the canvas
    } else if( m_mode == MODE_POINT2DRAGING)
    {
        m_rulerDecoration->ruler()->setPoint2( event->point);
        m_canvas->updateCanvas(); // TODO update only the relevant part of the canvas
    } else {
        event->ignore();
    }
}

void KisRulerAssistantTool::mouseReleaseEvent(KoPointerEvent *event)
{
    Q_UNUSED(event);
    m_mode = MODE_NOTHING;
    m_canvas->updateCanvas(); // TODO update only the relevant part of the canvas
}

void KisRulerAssistantTool::paint(QPainter& _gc, const KoViewConverter &_converter)
{
    if( m_mode == MODE_POINT1DRAGING)
    {
      _gc.setBrush( QColor(0,0,0,125) );
    } else {
      _gc.setBrush( QColor(0,0,0,0) );
    }
    _gc.drawEllipse( QRectF( _converter.documentToView( m_rulerDecoration->ruler()->point1() ) - QPointF(5,5), QSizeF(10,10)));
    if( m_mode == MODE_POINT2DRAGING)
    {
      _gc.setBrush( QColor(0,0,0,125) );
    } else {
      _gc.setBrush( QColor(0,0,0,0) );
    }
    _gc.drawEllipse( QRectF( _converter.documentToView( m_rulerDecoration->ruler()->point2() ) - QPointF(5,5), QSizeF(10,10)));
}

QWidget* KisRulerAssistantTool::createOptionWidget()
{
    m_widget = 0;
    return m_widget;
}

QWidget* KisRulerAssistantTool::optionWidget()
{
    return m_widget;
}

#include "kis_ruler_assistant_tool.moc"
