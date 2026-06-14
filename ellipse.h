#pragma once
#include <QColor>
#include <QGraphicsEllipseItem>

struct JsonHead
{
    QString format;          
    int version;             
    QString source_image;    
    QSize image_size;        
    QString generated_at;    
    QString profile;         
    bool sticker_mode;       
};

struct EllipseData {
    double x, y;   // 中心坐标
    double a, b;   // 两轴长度
    double angle;  // a轴相对x轴逆时针旋转角度（度）
    QColor color;  // RGBA颜色
};

class EllipseItem : public QGraphicsEllipseItem {
public:
    explicit EllipseItem(QGraphicsItem* parent = nullptr)
        : QGraphicsEllipseItem(parent), m_index(-1) {}
    void setIndex(int idx) { m_index = idx; }
    int index() const { return m_index; }
private:
    int m_index;
};