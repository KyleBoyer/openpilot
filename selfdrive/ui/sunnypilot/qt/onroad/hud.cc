/**
 * Copyright (c) 2021-, Haibin Wen, sunnypilot, and a number of other contributors.
 *
 * This file is part of sunnypilot and is licensed under the MIT License.
 * See the LICENSE.md file in the root directory for more details.
 */

#include "selfdrive/ui/sunnypilot/qt/onroad/hud.h"

#include <cmath>

#include "selfdrive/ui/qt/onroad/buttons.h"
#include "selfdrive/ui/qt/util.h"


HudRendererSP::HudRendererSP() {}

void HudRendererSP::updateState(const UIState &s) {
  HudRenderer::updateState(s);

  const SubMaster &sm = *(s.sm);
  const bool cs_alive = sm.alive("controlsState");
  const auto cs = sm["controlsState"].getControlsState();
  const auto car_state = sm["carState"].getCarState();
  const auto car_control = sm["carControl"].getCarControl();
  const auto radar_state = sm["radarState"].getRadarState();
  const auto is_gps_location_external = sm.rcv_frame("gpsLocationExternal") > 1;
  const auto gpsLocation = is_gps_location_external ? sm["gpsLocationExternal"].getGpsLocationExternal() : sm["gpsLocation"].getGpsLocation();
  const auto ltp = sm["liveTorqueParameters"].getLiveTorqueParameters();
  const auto car_params = sm["carParams"].getCarParams();

  parking_brake_indicators_visible = car_params.getCarFingerprint() == "SUBARU ASCENT 2023" &&
                                     sm["carControlSP"].getCarControlSP().getSubaruExperimentalAutoParkingBrake();
  const auto car_state_sp = sm["carStateSP"].getCarStateSP();
  parking_brake_engaged = car_state_sp.getSubaruParkingBrakeReported();
  parking_brake_requesting = car_state_sp.getSubaruExperimentalParkingBrakeRequesting();
  driver_on_right = sm["driverMonitoringState"].getDriverMonitoringState().getIsRHD();

  static int reverse_delay = 0;
  bool reverse_allowed = false;
  if (int(car_state.getGearShifter()) != 4) {
    reverse_delay = 0;
    reverse_allowed = false;
  } else {
    reverse_delay += 50;
    if (reverse_delay >= 1000) {
      reverse_allowed = true;
    }
  }

  reversing = reverse_allowed;
  is_metric = s.scene.is_metric;

  // Handle older routes where vEgoCluster is not set
  v_ego_cluster_seen = v_ego_cluster_seen || car_state.getVEgoCluster() != 0.0;
  float v_ego = v_ego_cluster_seen ? car_state.getVEgoCluster() : car_state.getVEgo();
  speed = cs_alive ? std::max<float>(0.0, v_ego) : 0.0;
  speed *= is_metric ? MS_TO_KPH : MS_TO_MPH;

  latActive = car_control.getLatActive();
  steerOverride = car_state.getSteeringPressed();

  devUiInfo = s.scene.dev_ui_info;

  speedUnit = is_metric ? tr("km/h") : tr("mph");
  lead_d_rel = radar_state.getLeadOne().getDRel();
  lead_v_rel = radar_state.getLeadOne().getVRel();
  lead_status = radar_state.getLeadOne().getStatus();
  steerControlType = car_params.getSteerControlType();
  actuators = car_control.getActuators();
  torqueLateral = steerControlType == cereal::CarParams::SteerControlType::TORQUE;
  angleSteers = car_state.getSteeringAngleDeg();
  desiredCurvature = cs.getDesiredCurvature();
  curvature = cs.getCurvature();
  roll = sm["liveParameters"].getLiveParameters().getRoll();
  memoryUsagePercent = sm["deviceState"].getDeviceState().getMemoryUsagePercent();
  gpsAccuracy = is_gps_location_external ? gpsLocation.getHorizontalAccuracy() : 1.0;  // External reports accuracy, internal does not.
  altitude = gpsLocation.getAltitude();
  vEgo = car_state.getVEgo();
  aEgo = car_state.getAEgo();
  steeringTorqueEps = car_state.getSteeringTorqueEps();
  bearingAccuracyDeg = gpsLocation.getBearingAccuracyDeg();
  bearingDeg = gpsLocation.getBearingDeg();
  torquedUseParams = ltp.getUseParams();
  latAccelFactorFiltered = ltp.getLatAccelFactorFiltered();
  frictionCoefficientFiltered = ltp.getFrictionCoefficientFiltered();
  liveValid = ltp.getLiveValid();

  // liveMapDataSP — speed limit and road name
  if (sm.alive("liveMapDataSP") && sm.valid("liveMapDataSP")) {
    const auto map_data = sm["liveMapDataSP"].getLiveMapDataSP();
    speed_limit_valid = map_data.getSpeedLimitValid();
    speed_limit = map_data.getSpeedLimit() * (is_metric ? MS_TO_KPH : MS_TO_MPH);
    road_name = QString(map_data.getRoadName().cStr());
  } else {
    speed_limit_valid = false;
    speed_limit = 0.0f;
    road_name.clear();
  }

  // Standstill timer
  bool is_standstill = car_state.getVEgo() < 0.3f;
  if (is_standstill && !at_standstill) {
    standstill_timer.start();
  }
  at_standstill = is_standstill;

  // Green light indicator — model-driven: longitudinalPlan.shouldStop clears while at standstill
  bool model_stop = sm["longitudinalPlan"].getLongitudinalPlan().getShouldStop();
  if (is_standstill && model_stop) {
    was_model_stopped = true;
  }
  green_light_go = is_standstill && was_model_stopped && !model_stop;
  if (!is_standstill) {
    was_model_stopped = false;
    green_light_go = false;
  }
}

void HudRendererSP::draw(QPainter &p, const QRect &surface_rect) {
  HudRenderer::draw(p, surface_rect);
  if (!reversing) {
    // Bottom Dev UI
    if (devUiInfo == 2) {
      QRect rect_bottom(surface_rect.left(), surface_rect.bottom() - 60, surface_rect.width(), 61);
      p.setPen(Qt::NoPen);
      p.setBrush(QColor(0, 0, 0, 100));
      p.drawRect(rect_bottom);
      drawBottomDevUI(p, rect_bottom.left(), rect_bottom.center().y());
    }

    // Right Dev UI
    if (devUiInfo != 0) {
      QRect rect_right(surface_rect.right() - (UI_BORDER_SIZE * 2), UI_BORDER_SIZE * 1.5, 184, 170);
      drawRightDevUI(p, surface_rect.right() - 184 - UI_BORDER_SIZE * 2, UI_BORDER_SIZE * 2 + rect_right.height());
    }

    if (speed_limit_valid) {
      drawSpeedLimit(p, surface_rect);
    }
    if (!road_name.isEmpty()) {
      drawRoadName(p, surface_rect);
    }
    if (at_standstill) {
      drawStandstillTimer(p, surface_rect);
    }
    if (green_light_go) {
      drawGreenLight(p, surface_rect);
    }
    if (parking_brake_indicators_visible) {
      drawExperimentalParkingBrake(p, surface_rect);
    }
  }
}

void HudRendererSP::drawText(QPainter &p, int x, int y, const QString &text, QColor color) {
  QRect real_rect = p.fontMetrics().boundingRect(text);
  real_rect.moveCenter({x, y - real_rect.height() / 2});
  p.setPen(color);
  p.drawText(real_rect.x(), real_rect.bottom(), text);
}

int HudRendererSP::drawRightDevUIElement(QPainter &p, int x, int y, const QString &value, const QString &label, const QString &units, QColor &color) {

  p.setFont(InterFont(28, QFont::Bold));
  x += 92;
  y += 80;
  drawText(p, x, y, label);

  p.setFont(InterFont(30 * 2, QFont::Bold));
  y += 65;
  drawText(p, x, y, value, color);

  p.setFont(InterFont(28, QFont::Bold));

  if (units.length() > 0) {
    p.save();
    x += 120;
    y -= 25;
    p.translate(x, y);
    p.rotate(-90);
    drawText(p, 0, 0, units);
    p.restore();
  }

  return 130;
}

void HudRendererSP::drawRightDevUI(QPainter &p, int x, int y) {
  int rh = 5;
  int ry = y;

  UiElement dRelElement = DeveloperUi::getDRel(lead_status, lead_d_rel);
  rh += drawRightDevUIElement(p, x, ry, dRelElement.value, dRelElement.label, dRelElement.units, dRelElement.color);
  ry = y + rh;

  UiElement vRelElement = DeveloperUi::getVRel(lead_status, lead_v_rel, is_metric, speedUnit);
  rh += drawRightDevUIElement(p, x, ry, vRelElement.value, vRelElement.label, vRelElement.units, vRelElement.color);
  ry = y + rh;

  UiElement steeringAngleDegElement = DeveloperUi::getSteeringAngleDeg(angleSteers, latActive, steerOverride);
  rh += drawRightDevUIElement(p, x, ry, steeringAngleDegElement.value, steeringAngleDegElement.label, steeringAngleDegElement.units, steeringAngleDegElement.color);
  ry = y + rh;

  UiElement actuatorsOutputLateralElement = DeveloperUi::getActuatorsOutputLateral(steerControlType, actuators, desiredCurvature, vEgo, roll, latActive, steerOverride);
  rh += drawRightDevUIElement(p, x, ry, actuatorsOutputLateralElement.value, actuatorsOutputLateralElement.label, actuatorsOutputLateralElement.units, actuatorsOutputLateralElement.color);
  ry = y + rh;

  UiElement actualLateralAccelElement = DeveloperUi::getActualLateralAccel(curvature, vEgo, roll, latActive, steerOverride);
  rh += drawRightDevUIElement(p, x, ry, actualLateralAccelElement.value, actualLateralAccelElement.label, actualLateralAccelElement.units, actualLateralAccelElement.color);
}

int HudRendererSP::drawBottomDevUIElement(QPainter &p, int x, int y, const QString &value, const QString &label, const QString &units, QColor &color) {
  p.setFont(InterFont(38, QFont::Bold));
  QFontMetrics fm(p.font());
  QRect init_rect = fm.boundingRect(label + " ");
  QRect real_rect = fm.boundingRect(init_rect, 0, label + " ");
  real_rect.moveCenter({x, y});

  QRect init_rect2 = fm.boundingRect(value);
  QRect real_rect2 = fm.boundingRect(init_rect2, 0, value);
  real_rect2.moveTop(real_rect.top());
  real_rect2.moveLeft(real_rect.right() + 10);

  QRect init_rect3 = fm.boundingRect(units);
  QRect real_rect3 = fm.boundingRect(init_rect3, 0, units);
  real_rect3.moveTop(real_rect.top());
  real_rect3.moveLeft(real_rect2.right() + 10);

  p.setPen(Qt::white);
  p.drawText(real_rect, Qt::AlignLeft | Qt::AlignVCenter, label);

  p.setPen(color);
  p.drawText(real_rect2, Qt::AlignRight | Qt::AlignVCenter, value);
  p.drawText(real_rect3, Qt::AlignLeft | Qt::AlignVCenter, units);
  return 430;
}

void HudRendererSP::drawBottomDevUI(QPainter &p, int x, int y) {
  int rw = 90;

  UiElement aEgoElement = DeveloperUi::getAEgo(aEgo);
  rw += drawBottomDevUIElement(p, rw, y, aEgoElement.value, aEgoElement.label, aEgoElement.units, aEgoElement.color);

  UiElement vEgoLeadElement = DeveloperUi::getVEgoLead(lead_status, lead_v_rel, vEgo, is_metric, speedUnit);
  rw += drawBottomDevUIElement(p, rw, y, vEgoLeadElement.value, vEgoLeadElement.label, vEgoLeadElement.units, vEgoLeadElement.color);

  if (torqueLateral && torquedUseParams) {
    UiElement frictionCoefficientFilteredElement = DeveloperUi::getFrictionCoefficientFiltered(frictionCoefficientFiltered, liveValid);
    rw += drawBottomDevUIElement(p, rw, y, frictionCoefficientFilteredElement.value, frictionCoefficientFilteredElement.label, frictionCoefficientFilteredElement.units, frictionCoefficientFilteredElement.color);

    UiElement latAccelFactorFilteredElement = DeveloperUi::getLatAccelFactorFiltered(latAccelFactorFiltered, liveValid);
    rw += drawBottomDevUIElement(p, rw, y, latAccelFactorFilteredElement.value, latAccelFactorFilteredElement.label, latAccelFactorFilteredElement.units, latAccelFactorFilteredElement.color);
  } else {
    UiElement steeringTorqueEpsElement = DeveloperUi::getSteeringTorqueEps(steeringTorqueEps);
    rw += drawBottomDevUIElement(p, rw, y, steeringTorqueEpsElement.value, steeringTorqueEpsElement.label, steeringTorqueEpsElement.units, steeringTorqueEpsElement.color);

    UiElement bearingDegElement = DeveloperUi::getBearingDeg(bearingAccuracyDeg, bearingDeg);
    rw += drawBottomDevUIElement(p, rw, y, bearingDegElement.value, bearingDegElement.label, bearingDegElement.units, bearingDegElement.color);
  }

  UiElement altitudeElement = DeveloperUi::getAltitude(gpsAccuracy, altitude);
  rw += drawBottomDevUIElement(p, rw, y, altitudeElement.value, altitudeElement.label, altitudeElement.units, altitudeElement.color);
}

void HudRendererSP::drawSpeedLimit(QPainter &p, const QRect &surface_rect) {
  // International circular speed limit sign, positioned below the set-speed box (top-left)
  const int cx = 160, cy = 310, r = 50;

  p.save();
  p.setPen(QPen(QColor(220, 0, 0), 10));
  p.setBrush(QColor(255, 255, 255, 230));
  p.drawEllipse(QPoint(cx, cy), r, r);

  p.setFont(InterFont(46, QFont::Bold));
  p.setPen(QColor(0, 0, 0));
  p.drawText(QRect(cx - r, cy - r, r * 2, r * 2), Qt::AlignCenter,
             QString::number(std::nearbyint(speed_limit)));
  p.restore();
}

void HudRendererSP::drawRoadName(QPainter &p, const QRect &surface_rect) {
  p.save();
  p.setFont(InterFont(44, QFont::DemiBold));
  QFontMetrics fm(p.font());
  QRect text_rect = fm.boundingRect(road_name);

  const int padding_h = 20, padding_v = 10;
  const int cx = surface_rect.center().x();
  const int cy = surface_rect.bottom() - 65;

  QRect bg_rect(
    cx - text_rect.width() / 2 - padding_h,
    cy - text_rect.height() / 2 - padding_v,
    text_rect.width() + padding_h * 2,
    text_rect.height() + padding_v * 2
  );

  p.setPen(Qt::NoPen);
  p.setBrush(QColor(0, 0, 0, 150));
  p.drawRoundedRect(bg_rect, 12, 12);

  p.setPen(Qt::white);
  p.drawText(bg_rect, Qt::AlignCenter, road_name);
  p.restore();
}

void HudRendererSP::drawStandstillTimer(QPainter &p, const QRect &surface_rect) {
  qint64 elapsed_ms = standstill_timer.isValid() ? standstill_timer.elapsed() : 0;
  int total_s = elapsed_ms / 1000;
  QString timer_str = QString("%1:%2")
    .arg(total_s / 60)
    .arg(total_s % 60, 2, 10, QChar('0'));

  p.save();
  p.setFont(InterFont(60, QFont::Bold));
  drawText(p, surface_rect.center().x(), 360, timer_str, QColor(0xff, 0xff, 0xff, 230));
  p.restore();
}

void HudRendererSP::drawGreenLight(QPainter &p, const QRect &surface_rect) {
  // Green circle with upward arrow, centered horizontally just below the current speed
  const int cx = surface_rect.center().x();
  const int cy = 410;
  const int r = 44;

  p.save();

  // Outer dark ring for contrast
  p.setPen(QPen(QColor(0, 0, 0, 180), 6));
  p.setBrush(QColor(0, 200, 80, 230));
  p.drawEllipse(QPoint(cx, cy), r, r);

  // Upward arrow
  p.setPen(Qt::NoPen);
  p.setBrush(Qt::white);
  const int aw = 14, ah = 26, tip = cy - r / 2;
  QPolygon arrow;
  arrow << QPoint(cx, tip - 12)           // tip
        << QPoint(cx + aw, tip + ah - 12) // lower-right
        << QPoint(cx - aw, tip + ah - 12); // lower-left
  p.drawPolygon(arrow);
  // Arrow stem
  p.drawRect(cx - aw / 2, tip + ah - 12, aw, 14);

  p.restore();
}

void HudRendererSP::drawExperimentalParkingBrake(QPainter &p, const QRect &surface_rect) {
  constexpr int radius = 42;
  constexpr int gap = 18;
  const int face_offset = UI_BORDER_SIZE + btn_size / 2;
  int y = surface_rect.bottom() - face_offset;
  y -= devUiInfo > 1 ? 50 : 0;

  const int first_x = UI_BORDER_SIZE + btn_size + gap + radius;
  const int second_x = first_x + radius * 2 + gap;
  const int status_x = driver_on_right ? surface_rect.right() - first_x : first_x;
  const int request_x = driver_on_right ? surface_rect.right() - second_x : second_x;

  auto draw_indicator = [&](int x, const QString &symbol, const QString &label, bool active, QColor active_color) {
    const QColor inactive_color(170, 170, 170, 180);
    QColor color = active ? active_color : inactive_color;
    QColor background(0, 0, 0, active ? 180 : 110);

    p.setPen(QPen(color, 6));
    p.setBrush(background);
    p.drawEllipse(QPoint(x, y), radius, radius);

    p.setFont(InterFont(symbol == "TX" ? 30 : 46, QFont::Bold));
    p.setPen(active ? Qt::white : QColor(210, 210, 210, 190));
    p.drawText(QRect(x - radius, y - radius, radius * 2, radius * 2), Qt::AlignCenter, symbol);

    p.setFont(InterFont(20, QFont::DemiBold));
    p.setPen(color);
    p.drawText(QRect(x - 60, y + radius + 4, 120, 28), Qt::AlignHCenter | Qt::AlignTop, label);
  };

  p.save();
  draw_indicator(status_x, "P", tr("EPB"), parking_brake_engaged, QColor(0, 220, 90, 245));

  QColor request_color(255, 166, 0);
  if (parking_brake_requesting) {
    const double pulse = 0.65 + 0.35 * ((std::sin(millis_since_boot() / 90.0) + 1.0) / 2.0);
    request_color.setAlphaF(pulse);
  } else {
    request_color.setAlpha(180);
  }
  draw_indicator(request_x, "TX", tr("REQUEST"), parking_brake_requesting, request_color);
  p.restore();
}
