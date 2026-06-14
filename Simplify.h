#pragma once

#include <QtWidgets/QWidget>
#include <QUndoStack>
#include <QVector>
#include <QCloseEvent>

#include "ui_Simplify.h"
#include "ellipse.h"
#include "Viewer.h"


class Simplify : public QWidget
{
    Q_OBJECT

public:
    Simplify(QWidget *parent = nullptr);

private:
    void closeEvent(QCloseEvent* event) override;

    void Reset();
    void ReFresh();
    bool OnOpen();
    bool OnSave();
    void filterSelect();
    void OnDelete();


    class DeleteCommand : public QUndoCommand {
    public:
        DeleteCommand(QVector<EllipseData>& vec, QList<int> indices, QUndoCommand* parent = nullptr);
        void redo() override;
        void undo() override;

    private:
        QVector<EllipseData>& dataVec;
        QList<int> selectedIndices;       // 执行删除时使用的索引（不变）
        QList<int> removedIndices;        // 保存的原始索引（升序）
        QList<EllipseData> removedItems;      // 对应的元素
    };

private:
    QVector<EllipseData> m_ellipses_ori;
    QVector<EllipseData> m_ellipses_view;
    QList<int> m_indices;
    QList<int> m_indicesFiltered;

    MyGraphicsView* m_viewer = nullptr;
    QUndoStack* m_undoStack;

    Ui::SimplifyDLG* ui;

    JsonHead fileHead;
};

