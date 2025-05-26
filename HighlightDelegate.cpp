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

    // 1. Draw the base item using the default style (preserves selection background)
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    QStyledItemDelegate::paint(painter, opt, index);

    // 2. Check if we need to highlight any search pattern
    QString text = index.data(Qt::DisplayRole).toString();
    if (m_pattern.isEmpty())
    {
        painter->restore();
        return;  // No search pattern: no custom highlighting
    }

    QString lowerText = text.toLower();
    QString lowerPattern = m_pattern.toLower();

    int matchStart = lowerText.indexOf(lowerPattern);
    if (matchStart < 0) {
        painter->restore();
        return;  // No match: nothing to highlight
    }

    // 3. Perform manual drawing over the text to highlight the match
    QRect textRect = option.rect;
    painter->setClipRect(textRect);

    QFontMetrics fm(option.font);
    QString before = text.left(matchStart);
    QString match = text.mid(matchStart, m_pattern.length());
    QString after = text.mid(matchStart + m_pattern.length());

    int x = textRect.left() + 4;  // Small left margin
    int y = textRect.top() + (textRect.height() + fm.ascent() - fm.descent()) / 2;

    // Measure width of each part
    int beforeWidth = fm.horizontalAdvance(before);
    int matchWidth = fm.horizontalAdvance(match);

    // Highlight background for matched text
    QRect matchRect(x + beforeWidth, textRect.top(), matchWidth, textRect.height());
    painter->fillRect(matchRect, QColor(255, 220, 220));  // light red

    // Set text pen color depending on selection state
    QColor penColor = option.state & QStyle::State_Selected
        ? option.palette.highlightedText().color()
        : option.palette.text().color();

    painter->setPen(penColor);

    // Draw the whole text
    painter->drawText(x, y, before + match + after);

    painter->restore();
}


