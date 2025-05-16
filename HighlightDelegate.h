// HighlightDelegate.h
#pragma once
#include <QStyledItemDelegate>
#include <QRegularExpression>

class HighlightDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit HighlightDelegate(QObject* parent = nullptr);
    void setPattern(const QString& pattern);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override;

private:
    QString m_pattern;
};
