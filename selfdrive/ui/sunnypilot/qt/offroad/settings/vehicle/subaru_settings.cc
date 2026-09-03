/**
 * Copyright (c) 2021-, Haibin Wen, sunnypilot, and a number of other contributors.
 *
 * This file is part of sunnypilot and is licensed under the MIT License.
 * See the LICENSE.md file in the root directory for more details.
 */

#include "selfdrive/ui/sunnypilot/qt/offroad/settings/vehicle/subaru_settings.h"

#include <QJsonDocument>
#include <QJsonObject>

SubaruSettings::SubaruSettings(QWidget *parent) : BrandSettingsInterface(parent) {
  experimentalAutoParkingBrakeToggle = new ParamControl(
    "SubaruExperimentalAutoParkingBrake",
    tr("Experimental Auto Parking Brake"),
    EXPERIMENTAL_AUTO_PARKING_BRAKE_DESC,
    ""
  );
  experimentalAutoParkingBrakeToggle->setConfirmation(true, false);
  experimentalAutoParkingBrakeToggle->showDescription();
  list->addItem(experimentalAutoParkingBrakeToggle);
}

void SubaruSettings::updateSettings() {
  bool supported = false;

  // A manual vehicle selection takes precedence over the last automatically fingerprinted car.
  // CarPlatformBundle stores the enum name with underscores, while CarParams stores its value.
  QString platform_bundle = QString::fromStdString(params.get("CarPlatformBundle"));
  if (!platform_bundle.isEmpty()) {
    QJsonDocument json = QJsonDocument::fromJson(platform_bundle.toUtf8());
    if (json.isObject()) {
      supported = json.object().value("platform").toString() == "SUBARU_ASCENT_2023";
    }
  } else {
    auto cp_bytes = params.get("CarParamsPersistent");
    if (!cp_bytes.empty()) {
      AlignedBuffer aligned_buf;
      capnp::FlatArrayMessageReader cmsg(aligned_buf.align(cp_bytes.data(), cp_bytes.size()));
      cereal::CarParams::Reader CP = cmsg.getRoot<cereal::CarParams>();
      supported = CP.getCarFingerprint() == "SUBARU ASCENT 2023";
    }
  }

  experimentalAutoParkingBrakeToggle->setEnabled(offroad && supported);
  if (!supported) {
    experimentalAutoParkingBrakeToggle->setDescription(
      tr("This unverified test is restricted to the 2023-24 Subaru Ascent platform."));
  } else if (!offroad) {
    experimentalAutoParkingBrakeToggle->setDescription(
      tr("Park the vehicle before changing this setting.<br><br>") + EXPERIMENTAL_AUTO_PARKING_BRAKE_DESC);
  } else {
    experimentalAutoParkingBrakeToggle->setDescription(EXPERIMENTAL_AUTO_PARKING_BRAKE_DESC);
  }
  experimentalAutoParkingBrakeToggle->showDescription();
}
