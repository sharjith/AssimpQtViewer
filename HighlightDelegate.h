// HighlightDelegate.h
#pragma once
#include <QRegularExpression>
#include <QStyledItemDelegate>

class HighlightDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit HighlightDelegate(QObject *parent = nullptr);

    void setPattern(const QString &pattern);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

private:
    QString m_pattern;
};
