#pragma once

#include <QToolButton>
#include <QPainter>
#include <QPolygon>
#include <QBrush>
#include <QColor>
#include <QPaintEvent>
#include <QWidget>

// FlyOutViewButton.h
// Custom button with an upward arrow at the top-right corner
// of the button, used for fly-out views in a user interface.
// The button inherits from QToolButton and overrides the paintEvent
// to draw the custom arrow.

class FlyOutViewButton : public QToolButton
{
public:
	FlyOutViewButton(QWidget* parent = nullptr) : QToolButton(parent)
	{
		setStyleSheet(
			"QToolButton {"
			"    border: none;"
			"    background: transparent;"
			"    padding: 5px;"
			"    border-radius: 4px;"
			"}"
			"QToolButton:hover {"
			"    background-color: rgba(0, 120, 215, 50);"
			"    border: 1px solid #0078D7;"
			"}"
			"QToolButton:pressed {"
			"    background-color: rgba(0, 120, 215, 100);"
			"    border: 1px solid #005A9E;"
			"}"
			"QToolButton {"
			"    background: transparent;"
			"}"
			"QToolButton::menu-indicator {"
			"    image: none;"
			"    width: 0px;"
			"    height: 0px;"
			"}"
		);
	}

protected:
	void paintEvent(QPaintEvent* event) override
	{
		QToolButton::paintEvent(event);

		// Draw custom up arrow at the top of the button
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);

		int arrowSize = 6;
		QPoint arrowTip(width() - 12, 10); // Top-right area

		// Create upward triangle
		QPolygon triangle;
		triangle << arrowTip  // tip pointing up
			<< QPoint(arrowTip.x() - arrowSize, arrowTip.y() + arrowSize)  // bottom left
			<< QPoint(arrowTip.x() + arrowSize, arrowTip.y() + arrowSize); // bottom right

		painter.setBrush(QBrush(QColor(51, 51, 51)));
		painter.setPen(Qt::NoPen);
		painter.drawPolygon(triangle);
	}
};