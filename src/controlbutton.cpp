#include "controlbutton.h"

#include <QCursor>
#include <QMouseEvent>
#include <QToolTip>
#include <QWindow>

controlButton::controlButton(QWidget *parent) : QPushButton(parent) {
  setMouseTracking(true);
}

bool controlButton::event(QEvent *event) {
  switch (event->type()) {
  case QEvent::ToolTip:
	// We show hints immediately on hover; skip delayed default tooltip event.
	return true;
  case QEvent::Hide:
  case QEvent::Leave:
  case QEvent::MouseButtonPress:
  case QEvent::EnabledChange:
	QToolTip::hideText();
	break;
  default:
	break;
  }

  return QPushButton::event(event);
}

void controlButton::enterEvent(QEnterEvent *event) {
  QPushButton::enterEvent(event);
  showImmediateToolTip(QCursor::pos());
}

void controlButton::leaveEvent(QEvent *event) {
  QToolTip::hideText();
  QPushButton::leaveEvent(event);
}

void controlButton::mouseMoveEvent(QMouseEvent *event) {
  if (event->buttons() == Qt::NoButton) {
	showImmediateToolTip(mapToGlobal(event->position().toPoint()));
  }
  QPushButton::mouseMoveEvent(event);
}

void controlButton::showImmediateToolTip(const QPoint &globalPos) {
  if (!isEnabled() || toolTip().isEmpty() || !isVisible()) {
	return;
  }

  QWidget *topLevel = window();
  if (!topLevel || !topLevel->windowHandle()) {
	return;
  }

  // Pass the widget and its rect so the tooltip popup gets a valid parent.
  QToolTip::showText(globalPos, toolTip(), this, rect());
}
