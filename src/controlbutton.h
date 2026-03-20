#ifndef CONTROLBUTTON_H
#define CONTROLBUTTON_H

#include <QPushButton>

class QEnterEvent;
class QEvent;
class QMouseEvent;
class QPoint;

class controlButton : public QPushButton {
  Q_OBJECT
public:
  controlButton(QWidget *parent = 0);

protected:
  bool event(QEvent *event) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;

private:
  void showImmediateToolTip(const QPoint &globalPos);
};

#endif // CONTROLBUTTON_H
