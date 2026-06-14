#include "Simplify.h"

#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue> 
#include <QShortcut>


Simplify::Simplify(QWidget *parent)
    : QWidget(parent)
{
    ui = new Ui::SimplifyDLG;
    ui->setupUi(this);

    m_viewer = new MyGraphicsView(m_ellipses_view, this);
    QVBoxLayout* layout = new QVBoxLayout(ui->m_pViewWgt);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_viewer);

    connect(ui->m_pOpenPB, &QPushButton::clicked, this, &Simplify::OnOpen);
    connect(ui->m_pSavePB, &QPushButton::clicked, this, &Simplify::OnSave);
    connect(m_viewer, &MyGraphicsView::ellipsesSelected, this, [=](QList<int> indices) {
        m_indices = indices;
        filterSelect();
        });

    connect(ui->m_pIDCB, &QCheckBox::toggled, this, [=](bool bcheck) {
        ui->m_pIDRangeWgt->setEnabled(bcheck);
        filterSelect();
        });
    connect(ui->m_pSizeCB, &QCheckBox::toggled, this, [=](bool bcheck) {
        ui->m_pSizeRangeWgt->setEnabled(bcheck);
        filterSelect();
        });
    connect(ui->m_pIDInRangeCB, &QCheckBox::toggled, this, &Simplify::filterSelect);
    connect(ui->m_pMinIDSB, &QSpinBox::valueChanged, this, &Simplify::filterSelect);
    connect(ui->m_pMaxIDSB, &QSpinBox::valueChanged, this, &Simplify::filterSelect);
    connect(ui->m_pSizeInRangeCB, &QCheckBox::toggled, this, &Simplify::filterSelect);
    connect(ui->m_pBothAxisCB, &QCheckBox::toggled, this, &Simplify::filterSelect);
    connect(ui->m_pMinSizeSB, &QDoubleSpinBox::valueChanged, this, &Simplify::filterSelect);
    connect(ui->m_pMaxSizeSB, &QDoubleSpinBox::valueChanged, this, &Simplify::filterSelect);
    connect(ui->m_pOutsideCB, &QCheckBox::toggled, this, &Simplify::filterSelect);

    connect(ui->m_pIDMinPB, &QPushButton::clicked, this, [=]() {ui->m_pMinIDSB->setValue(ui->m_pMinIDSB->minimum()); });
    connect(ui->m_pIDMaxPB, &QPushButton::clicked, this, [=]() {ui->m_pMaxIDSB->setValue(ui->m_pMaxIDSB->maximum()); });
    connect(ui->m_pSizeMinPB, &QPushButton::clicked, this, [=]() {ui->m_pMinSizeSB->setValue(ui->m_pMinSizeSB->minimum()); });
    connect(ui->m_pSizeMaxPB, &QPushButton::clicked, this, [=]() {ui->m_pMaxSizeSB->setValue(ui->m_pMaxSizeSB->maximum()); });

    m_undoStack = new QUndoStack(this);
    // 注册Delete Ctrl+Z 和 Ctrl+Shift+Z 快捷键
    QShortcut* deleteSc = new QShortcut(QKeySequence(Qt::Key_Delete), this);
    connect(deleteSc, &QShortcut::activated, this, &Simplify::OnDelete);
    QShortcut* undoSc = new QShortcut(QKeySequence::Undo, this);
    QShortcut* redoSc = new QShortcut(QKeySequence::Redo, this);
    connect(undoSc, &QShortcut::activated, m_undoStack, &QUndoStack::undo);
    connect(redoSc, &QShortcut::activated, m_undoStack, &QUndoStack::redo);
    connect(m_undoStack, &QUndoStack::indexChanged, this, &Simplify::ReFresh);
}

void Simplify::closeEvent(QCloseEvent* event)
{
    disconnect(m_undoStack, &QUndoStack::indexChanged, this, &Simplify::ReFresh);
    event->accept();
}

void Simplify::Reset()
{
    m_ellipses_ori.clear();
    m_ellipses_view.clear();
    m_indices.clear();
    m_indicesFiltered.clear();
    m_viewer->resetView(false, true);
    m_undoStack->clear();
    ui->m_pTotalCntLabel->setText("0");
    ui->m_pSelectCntLable->setText("0");
}

void Simplify::ReFresh()
{
    ui->m_pTotalCntLabel->setText(QString::number(m_ellipses_ori.size()));
    ui->m_pSelectCntLable->setText("0");
    m_indices.clear();
    m_indicesFiltered.clear();
    m_ellipses_view.clear();
    m_ellipses_view.resize(m_ellipses_ori.size());
    std::memcpy(m_ellipses_view.data(), m_ellipses_ori.data(), sizeof(EllipseData) * m_ellipses_ori.size());
    if (ui->m_pKeepPolygon->isChecked())
        m_viewer->resetView(false, false);
    else
        m_viewer->resetView(false, true);
}

bool Simplify::OnOpen()
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("select json file"), "", tr("json file (*.json)"));
    if (filePath.isEmpty())
        return false;
    Reset();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开文件:" << filePath;
        return false;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "JSON 解析错误:" << parseError.errorString();
        return false;
    }

    if (!doc.isObject()) {
        qWarning() << "JSON 根对象不是对象类型";
        return false;
    }

    QJsonObject rootObj = doc.object();
    // ========== 1. 读取头部字段 ==========
    // format (字符串)
    if (rootObj.contains("format") && rootObj["format"].isString())
        fileHead.format = rootObj["format"].toString();
    else
        qWarning() << "缺少 format 字段或类型错误";
    // version (整数)
    if (rootObj.contains("version") && rootObj["version"].isDouble())
        fileHead.version = rootObj["version"].toInt();
    else
        qWarning() << "缺少 version 字段或类型错误";
    // source_image (字符串)
    if (rootObj.contains("source_image") && rootObj["source_image"].isString())
        fileHead.source_image = rootObj["source_image"].toString();
    else
        qWarning() << "缺少 source_image 字段或类型错误";
    // image_size (数组 [width, height])
    if (rootObj.contains("image_size") && rootObj["image_size"].isArray()) {
        QJsonArray sizeArr = rootObj["image_size"].toArray();
        if (sizeArr.size() >= 2) {
            int w = sizeArr[0].toInt();
            int h = sizeArr[1].toInt();
            fileHead.image_size = QSize(w, h);
        }
        else {
            qWarning() << "image_size 数组长度不足2";
        }
    }
    else {
        qWarning() << "缺少 image_size 字段或类型错误";
    }
    // generated_at (字符串)
    if (rootObj.contains("generated_at") && rootObj["generated_at"].isString())
        fileHead.generated_at = rootObj["generated_at"].toString();
    else
        qWarning() << "缺少 generated_at 字段或类型错误";
    // profile (字符串)
    if (rootObj.contains("profile") && rootObj["profile"].isString())
        fileHead.profile = rootObj["profile"].toString();
    else
        qWarning() << "缺少 profile 字段或类型错误";
    // sticker_mode (布尔)
    if (rootObj.contains("sticker_mode") && rootObj["sticker_mode"].isBool())
        fileHead.sticker_mode = rootObj["sticker_mode"].toBool();
    else
        qWarning() << "缺少 sticker_mode 字段或类型错误";

    // ========== 2. 读取 shapes 数组 ==========
    if (!rootObj.contains("shapes") || !rootObj["shapes"].isArray()) {
        qWarning() << "缺少 'shapes' 数组";
        return false;
    }
    QJsonArray shapesArray = rootObj["shapes"].toArray();

    m_ellipses_ori.reserve(shapesArray.size()); // 预分配内存，提高效率
    double minAxis = std::numeric_limits<double>::max(), maxAxis = 0;
    for (const QJsonValue& shapeVal : shapesArray) {
        if (!shapeVal.isObject()) {
            qWarning() << "形状项不是对象，跳过";
            continue;
        }

        QJsonObject shapeObj = shapeVal.toObject();

        // 读取必需字段，并进行类型检查
        if (!shapeObj.contains("x") || !shapeObj.contains("y") ||
            !shapeObj.contains("rx") || !shapeObj.contains("ry") ||
            !shapeObj.contains("angle") || !shapeObj.contains("color")) {
            qWarning() << "形状对象缺少必要字段，跳过";
            continue;
        }

        double x = shapeObj["x"].toDouble();
        double y = shapeObj["y"].toDouble();
        double rx = shapeObj["rx"].toDouble();
        double ry = shapeObj["ry"].toDouble();
        double angle = shapeObj["angle"].toDouble();

        // 读取颜色数组 [R, G, B, A]
        QJsonValue colorVal = shapeObj["color"];
        if (!colorVal.isArray() || colorVal.toArray().size() < 4) {
            qWarning() << "颜色字段格式错误，需要 [R,G,B,A] 数组";
            continue;
        }
        QJsonArray colorArray = colorVal.toArray();
        int r = colorArray[0].toInt();
        int g = colorArray[1].toInt();
        int b = colorArray[2].toInt();
        int a = colorArray[3].toInt();

        EllipseData data;
        data.x = x;
        data.y = y;
        data.a = rx;
        data.b = ry;
        data.angle = angle;
        data.color = QColor(r, g, b, a);
        minAxis = qMin(minAxis, qMin(rx, ry));
        maxAxis = qMax(maxAxis, qMax(rx, ry));
        m_ellipses_ori.append(data);
    }

    ui->m_pTotalCntLabel->setText(QString::number(m_ellipses_ori.size()));
    ui->m_pMaxSizeSB->setMinimum(minAxis);
    ui->m_pMaxSizeSB->setMaximum(maxAxis);
    ui->m_pMaxSizeSB->setValue(maxAxis);
    ui->m_pMaxSizeSB->setToolTip(QString::number(maxAxis));
    ui->m_pMinSizeSB->setMinimum(minAxis);
    ui->m_pMinSizeSB->setMaximum(maxAxis);
    ui->m_pMinSizeSB->setValue(minAxis);
    ui->m_pMinSizeSB->setToolTip(QString::number(minAxis));
    ui->m_pMaxIDSB->setMaximum(m_ellipses_ori.size());
    ui->m_pMinIDSB->setMaximum(m_ellipses_ori.size());
    ui->m_pMaxIDSB->setValue(m_ellipses_ori.size());
    ui->m_pMinIDSB->setValue(0);
    m_ellipses_view.resize(m_ellipses_ori.size());
    std::memcpy(m_ellipses_view.data(), m_ellipses_ori.data(), sizeof(EllipseData) * m_ellipses_ori.size());
    m_viewer->resetView(true);
    return true;
}

bool Simplify::OnSave()
{
    QString filePath = QFileDialog::getSaveFileName(this, tr("select json file"), "", tr("json file (*.json)"));
    if (filePath.isEmpty())
        return false;

    QJsonObject rootObj;

    // 1. 写入头部字段
    rootObj["format"] = fileHead.format;
    rootObj["version"] = fileHead.version;
    rootObj["source_image"] = fileHead.source_image;
    QJsonArray sizeArr;
    sizeArr.append(fileHead.image_size.width());
    sizeArr.append(fileHead.image_size.height());
    rootObj["image_size"] = sizeArr;
    rootObj["shape_count"] = static_cast<int>(m_ellipses_ori.size()); // shape_count 使用当前形状的实际数量
    rootObj["generated_at"] = fileHead.generated_at;
    rootObj["profile"] = fileHead.profile;
    rootObj["sticker_mode"] = fileHead.sticker_mode;

    // 2. 写入 shapes 数组
    QJsonArray shapesArray;
    for (auto shape : m_ellipses_ori) {
        QJsonObject shapeObj;
        shapeObj["x"] = shape.x;
        shapeObj["y"] = shape.y;
        shapeObj["rx"] = shape.a;
        shapeObj["ry"] = shape.b;
        shapeObj["angle"] = shape.angle;

        QJsonArray colorArr;
        colorArr.append(shape.color.red());
        colorArr.append(shape.color.green());
        colorArr.append(shape.color.blue());
        colorArr.append(shape.color.alpha());
        shapeObj["color"] = colorArr;
        shapeObj["type"] = "rotated_ellipse";

        shapesArray.append(shapeObj);
    }
    rootObj["shapes"] = shapesArray;

    // 3. 写文件
    QJsonDocument doc(rootObj);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "无法打开文件用于写入:" << filePath;
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    return true;
}

void Simplify::filterSelect()
{
    if (m_indicesFiltered.isEmpty() && m_indices.isEmpty())
        return;

    bool bSelectionMode = m_viewer->getMode() == MyGraphicsView::Mode::Selection;

    QList<int> indices;
    indices.reserve(m_ellipses_ori.size());
    if (bSelectionMode && ui->m_pOutsideCB->isChecked() && !m_indices.isEmpty())
    {
        int j = 0;
        for (int i = 0; i < m_ellipses_ori.size(); ++i) {
            if (j < m_indices.size() && m_indices[j] == i)
                ++j;
            else
                indices.append(i);
        }
    }
    else
        indices = m_indices;

    QList<int>indicesFiltered;
    indicesFiltered.reserve(indices.size());
    if (bSelectionMode)
    {
        bool bFilterID = ui->m_pIDCB->isChecked();
        bool bIDInside = ui->m_pIDInRangeCB->isChecked();
        int minID = ui->m_pMinIDSB->value();
        int maxID = ui->m_pMaxIDSB->value();
        bool bFilterSize = ui->m_pSizeCB->isChecked();
        bool bSizeInside = ui->m_pSizeInRangeCB->isChecked();
        bool bBothAxis = ui->m_pBothAxisCB->isChecked();
        double minSize = ui->m_pMinSizeSB->value();
        double maxSize = ui->m_pMaxSizeSB->value();
        for (auto i : indices)
        {
            if (bFilterID)
                if (bIDInside ^ (i >= minID && i <= maxID))
                    continue;
            if (bFilterSize)
                if (bBothAxis ? (bSizeInside ^ (m_ellipses_view[i].a >= minSize && m_ellipses_view[i].a <= maxSize) || bSizeInside ^ (m_ellipses_view[i].b >= minSize && m_ellipses_view[i].b <= maxSize))
                    : (bSizeInside ^ (m_ellipses_view[i].a >= minSize && m_ellipses_view[i].a <= maxSize) && bSizeInside ^ (m_ellipses_view[i].b >= minSize && m_ellipses_view[i].b <= maxSize)))
                    continue;
            indicesFiltered.push_back(i);
        }
    }
    else
        indicesFiltered = indices;

    bool bSame = true;
    if (indicesFiltered.size() == m_indicesFiltered.size())
    {
        for (int i = 0; i < indicesFiltered.size(); i++)
            if (indicesFiltered[i] != m_indicesFiltered[i])
            {
                bSame = false;
                break;
            }
    }
    else
        bSame = false;
    if (bSame)
        return;

    m_indicesFiltered = indicesFiltered;
    m_ellipses_view.clear();
    m_ellipses_view.resize(m_ellipses_ori.size());
    std::memcpy(m_ellipses_view.data(), m_ellipses_ori.data(), sizeof(EllipseData) * m_ellipses_ori.size());
    for (auto i : m_indicesFiltered)
        m_ellipses_view[i].color = Qt::yellow;

    if(! bSelectionMode && !m_indicesFiltered.isEmpty())
        ui->m_pSelectCntLable->setText(QString::number(m_indicesFiltered[0]));
    else
        ui->m_pSelectCntLable->setText(QString::number(m_indicesFiltered.size()));
    m_viewer->resetView(false);

}

void Simplify::OnDelete() {
    auto* cmd = new DeleteCommand(m_ellipses_ori, m_indicesFiltered);
    m_undoStack->push(cmd);
}

Simplify::DeleteCommand::DeleteCommand(QVector<EllipseData>& vec, QList<int> indices, QUndoCommand* parent)
    : QUndoCommand(parent), dataVec(vec), selectedIndices(indices)
{
}

void Simplify::DeleteCommand::redo() {
    removedItems.clear();
    removedIndices.clear();

    // 从后往前删除，保存被删元素
    for (auto it = selectedIndices.rbegin(); it != selectedIndices.rend(); ++it) {
        int idx = *it;
        if (idx >= 0 && idx < dataVec.size()) {
            removedIndices.prepend(idx);     // 保持升序
            removedItems.prepend(dataVec[idx]);
            dataVec.removeAt(idx);
        }
    }
}

void Simplify::DeleteCommand::undo() {
    for (int i = 0; i < removedIndices.size(); ++i) {
        int idx = removedIndices[i];
        dataVec.insert(idx, removedItems[i]);
    }
}