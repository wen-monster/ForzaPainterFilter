#pragma once
#include "ellipse.h"
#include <QGraphicsView>
#include <QVector>
#include <QList>
#include <QSet>
#include <QPointF>
#include <QPolygonF>

class MyGraphicsView : public QGraphicsView {
    Q_OBJECT
public:
    enum Mode { Normal, Selection };
    explicit MyGraphicsView(QVector<EllipseData>& ellipses, QWidget* parent = nullptr);
    void resetView(bool bFitWindow = false, bool bRemovePolygons = false);
    Mode getMode();

signals:
    void ellipsesSelected(QList<int> indices);          // 框选完成一个多边形后，返回内部椭圆索引合集

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void drawForeground(QPainter* painter, const QRectF& rect) override;

private:
    void addPoint(QPointF scenePos);
    void removeLastPoint();
    void closeCurrentPolygon();
    void discardCurrentPolygon();
    void getInsideIdx();

    QGraphicsScene* m_scene;
    QVector<EllipseData>& m_ellipses;
    Mode m_mode = Normal;

    // 框选状态
    QVector<QPointF> m_currentPoints;          // 当前多边形锚点（场景坐标）
    QList<QPolygonF> m_completedPolygons;      // 已完成的多边形

    // 中键平移
    bool m_middlePressed = false;
    QPoint m_lastMiddlePos;
};