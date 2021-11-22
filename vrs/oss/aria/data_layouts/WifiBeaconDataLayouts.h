// Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

#pragma once

#include <cstdint>

#include <vrs/DataLayout.h>
#include <vrs/DataPieces.h>

// Note: The VRS stream type for Wi-Fi, beacon data is
// vrs::RecordableTypeId::WifiBeaconRecordableClass.

namespace aria {

struct WifiBeaconConfigurationLayout : public vrs::AutoDataLayout {
  static constexpr uint32_t kVersion = 1;

  vrs::DataPieceValue<std::uint32_t> streamId{"stream_id"};

  vrs::AutoDataLayoutEnd end;
};

struct WifiBeaconDataLayout : public vrs::AutoDataLayout {
  static constexpr uint32_t kVersion = 2;

  // Timestamp of the data sample in real time (UNIX epoch).
  vrs::DataPieceValue<std::int64_t> systemTimestampNs{"system_timestamp_ns"};

  // Timestamp of the data sample in board-clock domain.
  vrs::DataPieceValue<std::int64_t> boardTimestampNs{"board_timestamp_ns"};

  // Timestamp (in board clock) when scan request was issued.
  // This field is used to group samples in the same way as phones do it.
  vrs::DataPieceValue<std::int64_t> boardScanRequestStartTimestampNs{
      "board_request_start_timestamp_ns"};

  // Timestamp (in board clock) when scan request is completed.
  // This field is used to group samples and calculate single scan duration.
  vrs::DataPieceValue<std::int64_t> boardScanRequestCompleteTimestampNs{
      "board_request_complete_timestamp_ns"};

  // A string representing the Service Set Identifier (SSID) of the Wi-Fi beacon.
  vrs::DataPieceString ssid{"ssid"};

  // Basic Service Set Identifier (BSSID) or MAC address of the Wi-Fi beacon, a unique identifier
  // of the physical device.
  vrs::DataPieceString bssidMac{"bssid_mac"};

  // Received signal strength indication, in units of dBm.
  vrs::DataPieceValue<float> rssi{"rssi"};

  // Frequency of the signal, in units of MHz.
  vrs::DataPieceValue<float> freqMhz{"freq_mhz"};

  // A list of antenna signal strengths, in units of dBm. The index of an element corresponds to
  // the antenna ID, with its value being the signal strength.
  vrs::DataPieceVector<float> rssiPerAntenna{"rssi_per_antenna"};

  vrs::AutoDataLayoutEnd end;
};

} // namespace aria
