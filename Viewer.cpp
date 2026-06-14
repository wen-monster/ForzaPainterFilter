#include "Viewer.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QScrollBar>
#include <algorithm>

MyGraphicsView::MyGraphicsView(QVector<EllipseData>& ellipses, QWidget* parent)
    : m_ellipses(ellipses)
    , QGraphicsView(parent)
{
    m_scene = new QGraphicsScene(this);
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::NoDrag);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::ArrowCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void MyGraphicsView::resetView(bool bFitWindow, bool bRemovePolygons) {
    int nLastShapes = m_scene->items().size();
    m_scene->clear();
    for (int i = 0; i < m_ellipses.size(); ++i) {
        const EllipseData& ed = m_ellipses[i];
        auto* item = new EllipseItem();
        item->setRect(-ed.a, -ed.b, 2 * ed.a, 2 * ed.b);
        item->setPos(ed.x, ed.y);
        item->setRotation(ed.angle);
        item->setTransformOriginPoint(0, 0);
        item->setBrush(ed.color);
        item->setPen(Qt::NoPen);   // 无边框
        item->setZValue(i);        // 按索引确定层级
        item->setIndex(i);
        m_scene->addItem(item);
    }

    m_scene->setSceneRect(m_scene->itemsBoundingRect().adjusted(-20, -20, 20, 20));

    if(bFitWindow)
        fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);

    if (bRemovePolygons) {
        m_currentPoints.clear();
        m_completedPolygons.clear();
        viewport()->update();
    }
    if(nLastShapes != m_scene->items().size())
        getInsideIdx();
}

MyGraphicsView::Mode MyGraphicsView::getMode()
{
    return m_mode;
}

// ---------- 鼠标事件 ----------
void MyGraphicsView::mousePressEvent(QMouseEvent* event) {
    // 中键平移优先
    if (event->button() == Qt::MiddleButton) {
        m_middlePressed = true;
        m_lastMiddlePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (m_mode == Normal) {
        if (event->button() == Qt::LeftButton) {
            QPointF scenePos = mapToScene(event->pos());
            // 获取最顶层椭圆（Z值降序）
            QList<QGraphicsItem*> items = m_scene->items(scenePos, Qt::IntersectsItemShape, Qt::DescendingOrder);
            QList<int> result;
            for (QGraphicsItem* it : items) {
                //
                if (auto* ei = dynamic_cast<EllipseItem*>(it)) {
                    result << ei->index();
                    break;
                }
            }
            emit ellipsesSelected(result);
            event->accept();
            return;
        }
    }
    else if (m_mode == Selection) {
        if (event->button() == Qt::LeftButton) {
            addPoint(mapToScene(event->pos()));   // 左键单击/双击第一次按下均添加锚点
            event->accept();
            return;
        }
        else if (event->button() == Qt::RightButton) {
            removeLastPoint();                   // 右键单击删除最后一个锚点（双击时也会触发，不影响最终丢弃）
            event->accept();
            return;
        }
    }
    QGraphicsView::mousePressEvent(event);
}

void MyGraphicsView::mouseDoubleClickEvent(QMouseEvent* event) {
    if (m_mode == Selection) {
        if (event->button() == Qt::LeftButton) {
            // 双击时第二次按下已添加一个锚点，此处直接封闭多边形
            closeCurrentPolygon();
        }
        else if (event->button() == Qt::RightButton) {
            discardCurrentPolygon();
        }
        event->accept();
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void MyGraphicsView::mouseMoveEvent(QMouseEvent* event) {
    if (m_middlePressed) {
        QPoint delta = event->pos() - m_lastMiddlePos;
        m_lastMiddlePos = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void MyGraphicsView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        m_middlePressed = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void MyGraphicsView::wheelEvent(QWheelEvent* event) {
    const double factor = 1.15;
    if (event->angleDelta().y() > 0)
        scale(factor, factor);
    else
        scale(1.0 / factor, 1.0 / factor);
    event->accept();
}

// ---------- 键盘事件 ----------
void MyGraphicsView::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_X && !event->isAutoRepeat()) {
        if (m_mode == Normal) {
            m_mode = Selection;
            setCursor(Qt::CrossCursor);
            m_currentPoints.clear();
            m_completedPolygons.clear();
            emit ellipsesSelected(QList<int>());
            viewport()->update();
        }
        else if (m_mode == Selection) {
            m_mode = Normal;
            setCursor(Qt::ArrowCursor);
            m_currentPoints.clear();
            m_completedPolygons.clear();
            emit ellipsesSelected(QList<int>());
            viewport()->update();
        }
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

// ---------- 锚点操作 ----------
void MyGraphicsView::addPoint(QPointF scenePos) {
    m_currentPoints.append(scenePos);
    viewport()->update();
}

void MyGraphicsView::removeLastPoint() {
    if (!m_currentPoints.isEmpty()) {
        m_currentPoints.removeLast();
        viewport()->update();
    }
}

void MyGraphicsView::closeCurrentPolygon() {
    if (m_currentPoints.size() >= 3) {
        QPolygonF poly(m_currentPoints);
        m_completedPolygons.append(poly);
        m_currentPoints.clear();

        getInsideIdx();
    }
    else {
        // 点数不足，直接丢弃
        m_currentPoints.clear();
    }
    viewport()->update();
}

void MyGraphicsView::discardCurrentPolygon() {
    m_currentPoints.clear();
    viewport()->update();
}

// 计算所有中心位于任一已完成多边形内的椭圆索引
void MyGraphicsView::getInsideIdx()
{
    QSet<int> idxSet;
    for (const QPolygonF& p : m_completedPolygons) {
        for (int i = 0; i < m_ellipses.size(); ++i) {
            QPointF center(m_ellipses[i].x, m_ellipses[i].y);
            if (p.containsPoint(center, Qt::OddEvenFill))
                idxSet.insert(i);
        }
    }
    QList<int> result = idxSet.values();
    std::sort(result.begin(), result.end());
    emit ellipsesSelected(result);
}

// ---------- 前景绘制（框选多边形） ----------
void MyGraphicsView::drawForeground(QPainter* painter, const QRectF& /*rect*/) {
    if (m_mode != Selection) return;
    painter->save();

    // 已完成的多边形
    painter->setPen(QPen(Qt::blue, 2));
    painter->setBrush(QColor(0, 0, 0, 0));
    for (const QPolygonF& poly : m_completedPolygons)
        painter->drawPolygon(poly);

    // 当前绘制中的多边形
    if (m_currentPoints.size() == 1) {
        painter->setBrush(Qt::green);
        painter->drawEllipse(m_currentPoints.first(), 4, 4);
    }
    else if (m_currentPoints.size() >= 2) {
        painter->setPen(QPen(Qt::green, 2));
        painter->setBrush(Qt::NoBrush);
        painter->drawPolyline(m_currentPoints.data(), m_currentPoints.size());
        painter->setBrush(Qt::green);
        for (const QPointF& pt : m_currentPoints)
            painter->drawEllipse(pt, 4, 4);
    }

    painter->restore();
}