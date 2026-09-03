/**
 * Copyright (c) 2021-, Haibin Wen, sunnypilot, and a number of other contributors.
 *
 * This file is part of sunnypilot and is licensed under the MIT License.
 * See the LICENSE.md file in the root directory for more details.
 */

#pragma once

#include "selfdrive/ui/sunnypilot/qt/offroad/settings/vehicle/brand_settings_interface.h"

#include "selfdrive/ui/qt/util.h"
#include "selfdrive/ui/sunnypilot/ui.h"
#include "selfdrive/ui/sunnypilot/qt/offroad/settings/settings.h"
#include "selfdrive/ui/sunnypilot/qt/widgets/controls.h"

class SubaruSettings : public BrandSettingsInterface {
  Q_OBJECT

public:
  explicit SubaruSettings(QWidget *parent = nullptr);
  void updateSettings() override;

private:
  ParamControl *experimentalAutoParkingBrakeToggle;

  const QString EXPERIMENTAL_AUTO_PARKING_BRAKE_DESC = tr(
    "2023-24 Ascent only. When the vehicle is stopped with the brake pedal pressed, this sends an unverified EyeSight CAN request after shifting from Drive or Reverse into Park. "
    "The Cruise_EPB signal has not been confirmed to command the parking brake. It may do nothing or cause unexpected vehicle behavior. Verify that the parking brake actually engaged before releasing the brake pedal."
  );
};
