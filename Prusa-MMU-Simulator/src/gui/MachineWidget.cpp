#include "MachineWidget.hh"

#include <QPainter>
#include <QPaintEvent>
#include <QtMath>

MachineWidget::MachineWidget(QWidget *aParent)
  : QWidget(aParent),
    mStatus{}
{
  setMinimumHeight(310);
}

void MachineWidget::setStatus(const SimulatorStatus &aStatus)
{
  mStatus = aStatus;
  update();
}

void MachineWidget::paintEvent(QPaintEvent *aEvent)
{
  (void)aEvent;

  QPainter tPainter(this);
  tPainter.setRenderHint(QPainter::Antialiasing, true);

  const QRectF tCase(12.0, 12.0, width() - 24.0, height() - 24.0);
  tPainter.setPen(QPen(QColor(85, 85, 85), 3.0));
  tPainter.setBrush(QColor(235, 235, 235));
  tPainter.drawRoundedRect(tCase, 18.0, 18.0);

  tPainter.setPen(Qt::black);
  QFont tTitleFont = font();
  tTitleFont.setPointSize(15);
  tTitleFont.setBold(true);
  tPainter.setFont(tTitleFont);
  tPainter.drawText(QRectF(30.0, 24.0, width() - 60.0, 28.0),
                    Qt::AlignCenter,
                    "Prusa MMU Spooler Simulator");

  // Five front-panel LED positions.
  const qreal tLedY = 75.0;
  for(int tLedIndex = 0; tLedIndex < 5; ++tLedIndex)
  {
    const qreal tX = 90.0 + (static_cast<qreal>(tLedIndex) * 80.0);
    QColor tLedColor(60, 60, 60);
    if(mStatus.redLeds[static_cast<std::size_t>(tLedIndex)] &&
       mStatus.greenLeds[static_cast<std::size_t>(tLedIndex)])
      tLedColor = QColor(255, 185, 0);
    else if(mStatus.redLeds[static_cast<std::size_t>(tLedIndex)])
      tLedColor = QColor(220, 35, 35);
    else if(mStatus.greenLeds[static_cast<std::size_t>(tLedIndex)])
      tLedColor = QColor(45, 190, 70);

    tPainter.setPen(QPen(Qt::black, 1.5));
    tPainter.setBrush(tLedColor);
    tPainter.drawEllipse(QPointF(tX, tLedY), 11.0, 11.0);
  }

  // Animated shuttle track. Physical simulator coordinates are mapped between
  // the configured mechanical limits; firmware homing therefore visibly moves
  // the carriage toward the left-hand stop.
  const QRectF tTrack(55.0, 120.0, width() - 110.0, 36.0);
  tPainter.setPen(QPen(QColor(80, 80, 80), 2.0));
  tPainter.setBrush(QColor(210, 210, 210));
  tPainter.drawRoundedRect(tTrack, 8.0, 8.0);

  qreal tFraction = 0.0;
  const qreal tRange = static_cast<qreal>(mStatus.shuttleMaximum - mStatus.shuttleMinimum);
  if(tRange > 0.0)
  {
    tFraction = static_cast<qreal>(mStatus.motors[0].position - mStatus.shuttleMinimum) / tRange;
    tFraction = qBound(0.0, tFraction, 1.0);
  }

  const qreal tShuttleX = tTrack.left() + 12.0 +
    tFraction * (tTrack.width() - 24.0);
  tPainter.setBrush(QColor(40, 40, 40));
  tPainter.drawRoundedRect(QRectF(tShuttleX - 13.0, tTrack.top() - 7.0, 26.0, 50.0),
                           5.0, 5.0);

  drawMotor(tPainter, QPointF(width() * 0.25, 230.0), 42.0,
            mStatus.motors[0], "Shuttle");
  drawMotor(tPainter, QPointF(width() * 0.50, 230.0), 42.0,
            mStatus.motors[1], "Take-up");
  drawMotor(tPainter, QPointF(width() * 0.75, 230.0), 42.0,
            mStatus.motors[2], "Brake");

  tPainter.setPen(Qt::black);
  QFont tStatusFont = font();
  tStatusFont.setPointSize(10);
  tPainter.setFont(tStatusFont);
  tPainter.drawText(QRectF(30.0, height() - 42.0, width() - 60.0, 22.0),
                    Qt::AlignCenter,
                    QString("FINDA: %1    Shuttle: %2 steps")
                      .arg(mStatus.filamentPresent ? "filament present" : "no filament")
                      .arg(mStatus.motors[0].position));
}

void MachineWidget::drawMotor(QPainter &aPainter,
                              const QPointF &aCenter,
                              qreal aRadius,
                              const MotorStatus &aMotor,
                              const QString &aLabel) const
{
  const bool tFault = aMotor.stalled || !aMotor.driverPresent ||
                      !aMotor.driverIdentityValid || aMotor.underVoltage ||
                      aMotor.overTemperature;
  aPainter.setPen(QPen(tFault ? QColor(190, 30, 30) : QColor(45, 45, 45), 3.0));
  aPainter.setBrush(aMotor.enabled ? QColor(200, 200, 200) : QColor(225, 225, 225));
  aPainter.drawEllipse(aCenter, aRadius, aRadius);

  // Rotate a spoke from the observed step count. This intentionally represents
  // motion rather than absolute shaft angle; it gives immediate visual feedback
  // that the firmware is producing STEP pulses.
  const qreal tAngle = static_cast<qreal>(aMotor.stepCount % 200ULL) *
                       (2.0 * 3.14159265358979323846 / 200.0);
  const QPointF tEnd(aCenter.x() + qCos(tAngle) * (aRadius - 8.0),
                     aCenter.y() + qSin(tAngle) * (aRadius - 8.0));
  aPainter.drawLine(aCenter, tEnd);
  aPainter.drawEllipse(aCenter, 5.0, 5.0);

  aPainter.setPen(Qt::black);
  aPainter.drawText(QRectF(aCenter.x() - 65.0,
                           aCenter.y() + aRadius + 7.0,
                           130.0,
                           20.0),
                    Qt::AlignCenter,
                    QString("%1 %2")
                      .arg(aLabel)
                      .arg(aMotor.enabled ? "ON" : "OFF"));
}
