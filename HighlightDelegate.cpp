// HighlightDelegate.cpp
#include "HighlightDelegate.h"
#include <QPainter>
#include <QTextLayout>

HighlightDelegate::HighlightDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {
}

void HighlightDelegate::setPattern(const QString& pattern) {
    m_pattern = pattern;
}

void HighlightDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
    const QModelIndex& index) const {
    painter->save();

    QString text = index.data(Qt::DisplayRole).toString();
    QString lowerText = text.toLower();
    QString lowerPattern = m_pattern.toLower();

    QRect rect = option.rect;
    painter->setClipRect(rect);

    QFont font = option.font;
    painter->setFont(font);
    painter->setPen(option.palette.text().color());

    int matchStart = lowerText.indexOf(lowerPattern);
    if (matchStart < 0 || lowerPattern.isEmpty()) {
        // No match: draw whole text normally
        painter->drawText(rect, Qt::AlignVCenter | Qt::AlignLeft, text);
    }
    else {
        QString before = text.left(matchStart);
        QString match = text.mid(matchStart, m_pattern.length());
        QString after = text.mid(matchStart + m_pattern.length());

        int x = rect.left();
        int y = rect.top() + (rect.height() + option.fontMetrics.ascent() - option.fontMetrics.descent()) / 2;

        // Draw 'before' part
        painter->drawText(x, y, before);
        x += option.fontMetrics.horizontalAdvance(before);

        // Draw background for matched text
        int matchWidth = option.fontMetrics.horizontalAdvance(match);
        QRect matchRect(x, rect.top(), matchWidth, rect.height());
        painter->fillRect(matchRect, QColor(255, 220, 220));  // light red background

        // Draw matched text
        painter->setPen(Qt::black);  // darker text for contrast
        painter->drawText(x, y, match);
        x += matchWidth;

        // Draw 'after' part
        painter->setPen(option.palette.text().color());
        painter->drawText(x, y, after);
    }

    painter->restore();
}

