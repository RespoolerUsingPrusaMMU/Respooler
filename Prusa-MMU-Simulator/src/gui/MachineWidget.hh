#ifndef PRUSA_MMU_SIMULATOR_MACHINEWIDGET_HH
#define PRUSA_MMU_SIMULATOR_MACHINEWIDGET_HH

#include "SimulatorStatus.hh"

#include <QPainter>
#include <QPointF>
#include <QString>
#include <QWidget>

class MachineWidget : public QWidget
{
  Q_OBJECT

public:
  explicit MachineWidget(QWidget *aParent = nullptr);
  void setStatus(const SimulatorStatus &aStatus);

protected:
  void paintEvent(QPaintEvent *aEvent) override;

private:
  void drawMotor(QPainter &aPainter,
                 const QPointF &aCenter,
                 qreal aRadius,
                 const MotorStatus &aMotor,
                 const QString &aLabel) const;

  SimulatorStatus mStatus;
};

#endif
