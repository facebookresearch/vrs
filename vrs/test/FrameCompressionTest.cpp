// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>

#include <vector>

#define DEFAULT_LOG_CHANNEL "FrameCompressionTest"
#include <logging/Log.h>

#include <vrs/os/Utils.h>

#include <vrs/Compressor.h>
#include <vrs/Decompressor.h>
#include <vrs/DiskFile.h>
#include <vrs/ErrorCode.h>

using namespace vrs;
using namespace std;

namespace {

const char* kBigJson =
    "{\"file_name\":\"/Users/gberenger/ovrsource/Software/CoreTech/test_data/VRS_Files/ar_camera."
    "vrs\",\"file_size_short\":\"1.74 MB\",\"file_size\":1826436,\"tags\":{\"device_type\":\"Mobi"
    "le\",\"device_version\":\"iPhone10,6\"},\"number_of_devices\":8,\"number_of_records\":250,\""
    "start_time\":88112.77534275,\"end_time\":88113.375026666,\"devices\":[{\"recordable_name\":"
    "\"Facebook AR Camera\",\"recordable_id\":10000,\"instance_id\":1,\"tags\":{\"type\":\"servic"
    "e_pixel_buffer\"},\"vrs_tag\":{\"DL:Configuration:1:0\":\"{\\\"data_layout\\\":[{\\\"name\\"
    "\":\\\"image_width\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":0},{\\\""
    "name\\\":\\\"image_height\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":4"
    "},{\\\"name\\\":\\\"image_pixel_format\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\"
    "\"offset\\\":8},{\\\"name\\\":\\\"image_stride\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>"
    "\\\",\\\"offset\\\":12},{\\\"name\\\":\\\"rotation\\\",\\\"type\\\":\\\"DataPieceValue<int32"
    "_t>\\\",\\\"offset\\\":16},{\\\"name\\\":\\\"field_of_view_radians\\\",\\\"type\\\":\\\"Data"
    "PieceValue<float>\\\",\\\"offset\\\":20},{\\\"name\\\":\\\"flip_vertically\\\",\\\"type\\\":"
    "\\\"DataPieceValue<Bool>\\\",\\\"offset\\\":24},{\\\"name\\\":\\\"camera_sensor_rotation\\\""
    ",\\\"type\\\":\\\"DataPieceValue<int32_t>\\\",\\\"offset\\\":25}]}\",\"DL:Configuration:1:1"
    "\":\"{\\\"data_layout\\\":[{\\\"name\\\":\\\"present\\\",\\\"type\\\":\\\"DataPieceValue<Boo"
    "l>\\\",\\\"offset\\\":0,\\\"default\\\":false},{\\\"name\\\":\\\"camera_distortion_1\\\",\\"
    "\"type\\\":\\\"DataPieceValue<double>\\\",\\\"offset\\\":1},{\\\"name\\\":\\\"camera_distort"
    "ion_2\\\",\\\"type\\\":\\\"DataPieceValue<double>\\\",\\\"offset\\\":9},{\\\"name\\\":\\\"ca"
    "mera_focal_length\\\",\\\"type\\\":\\\"DataPieceValue<double>\\\",\\\"offset\\\":17},{\\\"na"
    "me\\\":\\\"camera_principal_point_x\\\",\\\"type\\\":\\\"DataPieceValue<double>\\\",\\\"offs"
    "et\\\":25},{\\\"name\\\":\\\"camera_principal_point_y\\\",\\\"type\\\":\\\"DataPieceValue<do"
    "uble>\\\",\\\"offset\\\":33},{\\\"name\\\":\\\"imu_from_landscape_camera_x\\\",\\\"type\\\":"
    "\\\"DataPieceValue<double>\\\",\\\"offset\\\":41},{\\\"name\\\":\\\"imu_from_landscape_camer"
    "a_y\\\",\\\"type\\\":\\\"DataPieceValue<double>\\\",\\\"offset\\\":49},{\\\"name\\\":\\\"imu"
    "_from_landscape_camera_z\\\",\\\"type\\\":\\\"DataPieceValue<double>\\\",\\\"offset\\\":57},"
    "{\\\"name\\\":\\\"attitude_time_delay\\\",\\\"type\\\":\\\"DataPieceValue<double>\\\",\\\"of"
    "fset\\\":65},{\\\"name\\\":\\\"skip_attitude_input\\\",\\\"type\\\":\\\"DataPieceValue<Bool>"
    "\\\",\\\"offset\\\":73},{\\\"name\\\":\\\"vision_only_slam\\\",\\\"type\\\":\\\"DataPieceVal"
    "ue<Bool>\\\",\\\"offset\\\":74},{\\\"name\\\":\\\"is_slam_capable\\\",\\\"type\\\":\\\"DataP"
    "ieceValue<Bool>\\\",\\\"offset\\\":75},{\\\"name\\\":\\\"is_exposure_control_enabled\\\",\\"
    "\"type\\\":\\\"DataPieceValue<Bool>\\\",\\\"offset\\\":76},{\\\"name\\\":\\\"is_calibrated_d"
    "evice_config\\\",\\\"type\\\":\\\"DataPieceValue<Bool>\\\",\\\"offset\\\":77},{\\\"name\\\":"
    "\\\"slam_configuration_params\\\",\\\"type\\\":\\\"DataPieceString\\\",\\\"index\\\":0}]}\","
    "\"DL:Data:1:0\":\"{\\\"data_layout\\\":[{\\\"name\\\":\\\"presentation_time\\\",\\\"type\\\""
    ":\\\"DataPieceValue<double>\\\",\\\"offset\\\":0},{\\\"name\\\":\\\"pixel_buffer_format\\\","
    "\\\"type\\\":\\\"DataPieceValue<uint8_t>\\\",\\\"offset\\\":8},{\\\"name\\\":\\\"buffer_size"
    "s\\\",\\\"type\\\":\\\"DataPieceValue<Point3Di>\\\",\\\"offset\\\":9},{\\\"name\\\":\\\"buff"
    "er_strides\\\",\\\"type\\\":\\\"DataPieceValue<Point3Di>\\\",\\\"offset\\\":21}]}\",\"RF:Con"
    "figuration:1\":\"data_layout/size=29+data_layout\",\"RF:Data:1\":\"data_layout/size=33+image"
    "/raw\",\"VRS_Original_Recordable_Name\":\"Facebook AR Camera\"},\"configuration\":{\"number_"
    "of_records\":1,\"start_time\":88112.986265708,\"end_time\":88112.986265708},\"state\":{\"num"
    "ber_of_records\":1,\"start_time\":88112.77534275,\"end_time\":88112.77534275},\"data\":{\"nu"
    "mber_of_records\":3,\"start_time\":88112.9862685,\"end_time\":88113.360107041}},{\"recordabl"
    "e_name\":\"Facebook AR Camera\",\"recordable_id\":10000,\"instance_id\":2,\"tags\":{\"type\""
    ":\"configuration\"},\"vrs_tag\":{\"DL:Configuration:1:0\":\"{\\\"data_layout\\\":[{\\\"name"
    "\\\":\\\"effectId\\\",\\\"type\\\":\\\"DataPieceString\\\",\\\"index\\\":0},{\\\"name\\\":\\"
    "\"effectInstanceId\\\",\\\"type\\\":\\\"DataPieceString\\\",\\\"index\\\":1},{\\\"name\\\":"
    "\\\"device_type\\\",\\\"type\\\":\\\"DataPieceString\\\",\\\"index\\\":2}]}\",\"RF:Configura"
    "tion:1\":\"data_layout\",\"VRS_Original_Recordable_Name\":\"Facebook AR Camera\"},\"configur"
    "ation\":{\"number_of_records\":1,\"start_time\":88112.775353666,\"end_time\":88112.775353666"
    "},\"state\":{\"number_of_records\":1,\"start_time\":88112.775361125,\"end_time\":88112.77536"
    "1125},\"data\":{\"number_of_records\":0}},{\"recordable_name\":\"Facebook AR Camera\",\"reco"
    "rdable_id\":10000,\"instance_id\":3,\"tags\":{\"type\":\"camera_info\"},\"vrs_tag\":{\"D"
    "L:Data:1:0\":\"{\\\"data_layout\\\":[{\\\"name\\\":\\\"camera_info_present\\\",\\\"type\\\":"
    "\\\"DataPieceValue<int32_t>\\\",\\\"offset\\\":0,\\\"default\\\":0},{\\\"name\\\":\\\"width"
    "\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":4},{\\\"name\\\":\\\"heigh"
    "t\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":8},{\\\"name\\\":\\\"devi"
    "ce_rotation\\\",\\\"type\\\":\\\"DataPieceValue<int16_t>\\\",\\\"offset\\\":12},{\\\"name\\"
    "\":\\\"is_capturing_photo\\\",\\\"type\\\":\\\"DataPieceValue<Bool>\\\",\\\"offset\\\":14},{"
    "\\\"name\\\":\\\"is_recording_video\\\",\\\"type\\\":\\\"DataPieceValue<Bool>\\\",\\\"offset"
    "\\\":15},{\\\"name\\\":\\\"screen_scale\\\",\\\"type\\\":\\\"DataPieceValue<float>\\\",\\\"o"
    "ffset\\\":16},{\\\"name\\\":\\\"preview_size_width\\\",\\\"type\\\":\\\"DataPieceValue<uint3"
    "2_t>\\\",\\\"offset\\\":20},{\\\"name\\\":\\\"preview_size_height\\\",\\\"type\\\":\\\"DataP"
    "ieceValue<uint32_t>\\\",\\\"offset\\\":24},{\\\"name\\\":\\\"effect_safe_area_insets_top\\\""
    ",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":28},{\\\"name\\\":\\\"effect_s"
    "afe_area_insets_left\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":32},{"
    "\\\"name\\\":\\\"effect_safe_area_insets_bottom\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t"
    ">\\\",\\\"offset\\\":36},{\\\"name\\\":\\\"effect_safe_area_insets_right\\\",\\\"type\\\":\\"
    "\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":40},{\\\"name\\\":\\\"capture_device_position"
    "\\\",\\\"type\\\":\\\"DataPieceString\\\",\\\"index\\\":0}]}\",\"RF:Data:1\":\"data_layout\""
    ",\"VRS_Original_Recordable_Name\":\"Facebook AR Camera\"},\"configuration\":{\"number_of_rec"
    "ords\":1,\"start_time\":88112.775361708,\"end_time\":88112.775361708},\"state\":{\"number_of"
    "_records\":1,\"start_time\":88112.775361958,\"end_time\":88112.775361958},\"data\":{\"number"
    "_of_records\":1,\"start_time\":88112.983675416,\"end_time\":88112.983675416}},{\"recordable_"
    "name\":\"Facebook AR Gyroscope\",\"recordable_id\":10001,\"instance_id\":1,\"tags\":{},\"vrs"
    "_tag\":{\"DL:Configuration:1:0\":\"{\\\"data_layout\\\":[{\\\"name\\\":\\\"stream_index\\\","
    "\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":0},{\\\"name\\\":\\\"device_id"
    "\\\",\\\"type\\\":\\\"DataPieceValue<uint64_t>\\\",\\\"offset\\\":4},{\\\"name\\\":\\\"nomin"
    "al_rate\\\",\\\"type\\\":\\\"DataPieceValue<double>\\\",\\\"offset\\\":12},{\\\"name\\\":\\"
    "\"has_accelerometer\\\",\\\"type\\\":\\\"DataPieceValue<uint8_t>\\\",\\\"offset\\\":20},{\\"
    "\"name\\\":\\\"has_gyroscope\\\",\\\"type\\\":\\\"DataPieceValue<uint8_t>\\\",\\\"offset\\\""
    ":21},{\\\"name\\\":\\\"has_magnetometer\\\",\\\"type\\\":\\\"DataPieceValue<uint8_t>\\\",\\"
    "\"offset\\\":22},{\\\"name\\\":\\\"device_type\\\",\\\"type\\\":\\\"DataPieceString\\\",\\\""
    "index\\\":0},{\\\"name\\\":\\\"device_serial\\\",\\\"type\\\":\\\"DataPieceString\\\",\\\"in"
    "dex\\\":1},{\\\"name\\\":\\\"sensor_model\\\",\\\"type\\\":\\\"DataPieceString\\\",\\\"index"
    "\\\":2},{\\\"name\\\":\\\"factory_calibration\\\",\\\"type\\\":\\\"DataPieceString\\\",\\\"i"
    "ndex\\\":3},{\\\"name\\\":\\\"online_calibration\\\",\\\"type\\\":\\\"DataPieceString\\\",\\"
    "\"index\\\":4},{\\\"name\\\":\\\"description\\\",\\\"type\\\":\\\"DataPieceString\\\",\\\"in"
    "dex\\\":5}]}\",\"DL:Data:1:0\":\"{\\\"data_layout\\\":[{\\\"name\\\":\\\"group_size\\\",\\\""
    "type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":0},{\\\"name\\\":\\\"group_index\\"
    "\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":4},{\\\"name\\\":\\\"acceler"
    "ometer_valid\\\",\\\"type\\\":\\\"DataPieceValue<uint8_t>\\\",\\\"offset\\\":8},{\\\"name\\"
    "\":\\\"gyroscope_valid\\\",\\\"type\\\":\\\"DataPieceValue<uint8_t>\\\",\\\"offset\\\":9},{"
    "\\\"name\\\":\\\"magnetometer_valid\\\",\\\"type\\\":\\\"DataPieceValue<uint8_t>\\\",\\\"off"
    "set\\\":10},{\\\"name\\\":\\\"haptics_active\\\",\\\"type\\\":\\\"DataPieceValue<uint8_t>\\"
    "\",\\\"offset\\\":11},{\\\"name\\\":\\\"flags\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>"
    "\\\",\\\"offset\\\":12},{\\\"name\\\":\\\"temperature_deg_c\\\",\\\"type\\\":\\\"DataPieceVa"
    "lue<double>\\\",\\\"offset\\\":16},{\\\"name\\\":\\\"capture_timestamp\\\",\\\"type\\\":\\\""
    "DataPieceValue<double>\\\",\\\"offset\\\":24},{\\\"name\\\":\\\"arrival_timestamp\\\",\\\"ty"
    "pe\\\":\\\"DataPieceValue<double>\\\",\\\"offset\\\":32},{\\\"name\\\":\\\"processing_start_"
    "timestamp\\\",\\\"type\\\":\\\"DataPieceValue<double>\\\",\\\"offset\\\":40},{\\\"name\\\":"
    "\\\"accelerometer\\\",\\\"type\\\":\\\"DataPieceArray<float>\\\",\\\"offset\\\":48,\\\"size"
    "\\\":3},{\\\"name\\\":\\\"gyroscope\\\",\\\"type\\\":\\\"DataPieceArray<float>\\\",\\\"offse"
    "t\\\":60,\\\"size\\\":3},{\\\"name\\\":\\\"magnetometer\\\",\\\"type\\\":\\\"DataPieceArray<"
    "float>\\\",\\\"offset\\\":72,\\\"size\\\":3}]}\",\"RF:Configuration:1\":\"data_layout\",\"RF"
    ":Data:1\":\"data_layout/size=84\",\"VRS_Original_Recordable_Name\":\"Facebook AR Gyroscope\""
    "},\"configuration\":{\"number_of_records\":1,\"start_time\":88112.775428666,\"end_time\":881"
    "12.775428666},\"state\":{\"number_of_records\":1,\"start_time\":88112.775436625,\"end_time\""
    ":88112.775436625},\"data\":{\"number_of_records\":60,\"start_time\":88112.782654333,\"end_ti"
    "me\":88113.375006166}},{\"recordable_name\":\"Facebook AR Magnetometer\",\"recordable_id\":1"
    "0002,\"instance_id\":1,\"tags\":{},\"vrs_tag\":{\"DL:Configuration:1:0\":\"{\\\"data_layout"
    "\\\":[{\\\"name\\\":\\\"stream_index\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"o"
    "ffset\\\":0},{\\\"name\\\":\\\"device_id\\\",\\\"type\\\":\\\"DataPieceValue<uint64_t>\\\","
    "\\\"offset\\\":4},{\\\"name\\\":\\\"nominal_rate\\\",\\\"type\\\":\\\"DataPieceValue<double>"
    "\\\",\\\"offset\\\":12},{\\\"name\\\":\\\"has_accelerometer\\\",\\\"type\\\":\\\"DataPieceVa"
    "lue<uint8_t>\\\",\\\"offset\\\":20},{\\\"name\\\":\\\"has_gyroscope\\\",\\\"type\\\":\\\"Dat"
    "aPieceValue<uint8_t>\\\",\\\"offset\\\":21},{\\\"name\\\":\\\"has_magnetometer\\\",\\\"type"
    "\\\":\\\"DataPieceValue<uint8_t>\\\",\\\"offset\\\":22},{\\\"name\\\":\\\"device_type\\\",\\"
    "\"type\\\":\\\"DataPieceString\\\",\\\"index\\\":0},{\\\"name\\\":\\\"device_serial\\\",\\\""
    "type\\\":\\\"DataPieceString\\\",\\\"index\\\":1},{\\\"name\\\":\\\"sensor_model\\\",\\\"typ"
    "e\\\":\\\"DataPieceString\\\",\\\"index\\\":2},{\\\"name\\\":\\\"factory_calibration\\\",\\"
    "\"type\\\":\\\"DataPieceString\\\",\\\"index\\\":3},{\\\"name\\\":\\\"online_calibration\\\""
    ",\\\"type\\\":\\\"DataPieceString\\\",\\\"index\\\":4},{\\\"name\\\":\\\"description\\\",\\"
    "\"type\\\":\\\"DataPieceString\\\",\\\"index\\\":5}]}\",\"DL:Data:1:0\":\"{\\\"data_layout\\"
    "\":[{\\\"name\\\":\\\"group_size\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offse"
    "t\\\":0},{\\\"name\\\":\\\"group_index\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\"
    "\"offset\\\":4},{\\\"name\\\":\\\"accelerometer_valid\\\",\\\"type\\\":\\\"DataPieceValue<ui"
    "nt8_t>\\\",\\\"offset\\\":8},{\\\"name\\\":\\\"gyroscope_valid\\\",\\\"type\\\":\\\"DataPiec"
    "eValue<uint8_t>\\\",\\\"offset\\\":9},{\\\"name\\\":\\\"magnetometer_valid\\\",\\\"type\\\":"
    "\\\"DataPieceValue<uint8_t>\\\",\\\"offset\\\":10},{\\\"name\\\":\\\"haptics_active\\\",\\\""
    "type\\\":\\\"DataPieceValue<uint8_t>\\\",\\\"offset\\\":11},{\\\"name\\\":\\\"flags\\\",\\\""
    "type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":12},{\\\"name\\\":\\\"temperature_d"
    "eg_c\\\",\\\"type\\\":\\\"DataPieceValue<double>\\\",\\\"offset\\\":16},{\\\"name\\\":\\\"ca"
    "pture_timestamp\\\",\\\"type\\\":\\\"DataPieceValue<double>\\\",\\\"offset\\\":24},{\\\"name"
    "\\\":\\\"arrival_timestamp\\\",\\\"type\\\":\\\"DataPieceValue<double>\\\",\\\"offset\\\":32"
    "},{\\\"name\\\":\\\"processing_start_timestamp\\\",\\\"type\\\":\\\"DataPieceValue<double>\\"
    "\",\\\"offset\\\":40},{\\\"name\\\":\\\"accelerometer\\\",\\\"type\\\":\\\"DataPieceArray<fl"
    "oat>\\\",\\\"offset\\\":48,\\\"size\\\":3},{\\\"name\\\":\\\"gyroscope\\\",\\\"type\\\":\\\""
    "DataPieceArray<float>\\\",\\\"offset\\\":60,\\\"size\\\":3},{\\\"name\\\":\\\"magnetometer\\"
    "\",\\\"type\\\":\\\"DataPieceArray<float>\\\",\\\"offset\\\":72,\\\"size\\\":3}]}\",\"RF:Con"
    "figuration:1\":\"data_layout\",\"RF:Data:1\":\"data_layout/size=84\",\"VRS_Original_Recordab"
    "le_Name\":\"Facebook AR Magnetometer\"},\"configuration\":{\"number_of_records\":1,\"start_t"
    "ime\":88112.775438083,\"end_time\":88112.775438083},\"state\":{\"number_of_records\":1,\"sta"
    "rt_time\":88112.775446625,\"end_time\":88112.775446625},\"data\":{\"number_of_records\":59,"
    "\"start_time\":88112.780556833,\"end_time\":88113.37104425}},{\"recordable_name\":\"Facebook"
    " AR Accelerometer\",\"recordable_id\":10003,\"instance_id\":1,\"tags\":{},\"vrs_tag\":{\"DL:"
    "Configuration:1:0\":\"{\\\"data_layout\\\":[{\\\"name\\\":\\\"stream_index\\\",\\\"type\\\":"
    "\\\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":0},{\\\"name\\\":\\\"device_id\\\",\\\"type"
    "\\\":\\\"DataPieceValue<uint64_t>\\\",\\\"offset\\\":4},{\\\"name\\\":\\\"nominal_rate\\\","
    "\\\"type\\\":\\\"DataPieceValue<double>\\\",\\\"offset\\\":12},{\\\"name\\\":\\\"has_acceler"
    "ometer\\\",\\\"type\\\":\\\"DataPieceValue<uint8_t>\\\",\\\"offset\\\":20},{\\\"name\\\":\\"
    "\"has_gyroscope\\\",\\\"type\\\":\\\"DataPieceValue<uint8_t>\\\",\\\"offset\\\":21},{\\\"nam"
    "e\\\":\\\"has_magnetometer\\\",\\\"type\\\":\\\"DataPieceValue<uint8_t>\\\",\\\"offset\\\":2"
    "2},{\\\"name\\\":\\\"device_type\\\",\\\"type\\\":\\\"DataPieceString\\\",\\\"index\\\":0},{"
    "\\\"name\\\":\\\"device_serial\\\",\\\"type\\\":\\\"DataPieceString\\\",\\\"index\\\":1},{\\"
    "\"name\\\":\\\"sensor_model\\\",\\\"type\\\":\\\"DataPieceString\\\",\\\"index\\\":2},{\\\"n"
    "ame\\\":\\\"factory_calibration\\\",\\\"type\\\":\\\"DataPieceString\\\",\\\"index\\\":3},{"
    "\\\"name\\\":\\\"online_calibration\\\",\\\"type\\\":\\\"DataPieceString\\\",\\\"index\\\":4"
    "},{\\\"name\\\":\\\"description\\\",\\\"type\\\":\\\"DataPieceString\\\",\\\"index\\\":5}]}"
    "\",\"DL:Data:1:0\":\"{\\\"data_layout\\\":[{\\\"name\\\":\\\"group_size\\\",\\\"type\\\":\\"
    "\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":0},{\\\"name\\\":\\\"group_index\\\",\\\"type"
    "\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":4},{\\\"name\\\":\\\"accelerometer_vali"
    "d\\\",\\\"type\\\":\\\"DataPieceValue<uint8_t>\\\",\\\"offset\\\":8},{\\\"name\\\":\\\"gyros"
    "cope_valid\\\",\\\"type\\\":\\\"DataPieceValue<uint8_t>\\\",\\\"offset\\\":9},{\\\"name\\\":"
    "\\\"magnetometer_valid\\\",\\\"type\\\":\\\"DataPieceValue<uint8_t>\\\",\\\"offset\\\":10},{"
    "\\\"name\\\":\\\"haptics_active\\\",\\\"type\\\":\\\"DataPieceValue<uint8_t>\\\",\\\"offset"
    "\\\":11},{\\\"name\\\":\\\"flags\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offse"
    "t\\\":12},{\\\"name\\\":\\\"temperature_deg_c\\\",\\\"type\\\":\\\"DataPieceValue<double>\\"
    "\",\\\"offset\\\":16},{\\\"name\\\":\\\"capture_timestamp\\\",\\\"type\\\":\\\"DataPieceValu"
    "e<double>\\\",\\\"offset\\\":24},{\\\"name\\\":\\\"arrival_timestamp\\\",\\\"type\\\":\\\"Da"
    "taPieceValue<double>\\\",\\\"offset\\\":32},{\\\"name\\\":\\\"processing_start_timestamp\\\""
    ",\\\"type\\\":\\\"DataPieceValue<double>\\\",\\\"offset\\\":40},{\\\"name\\\":\\\"accelerome"
    "ter\\\",\\\"type\\\":\\\"DataPieceArray<float>\\\",\\\"offset\\\":48,\\\"size\\\":3},{\\\"na"
    "me\\\":\\\"gyroscope\\\",\\\"type\\\":\\\"DataPieceArray<float>\\\",\\\"offset\\\":60,\\\"si"
    "ze\\\":3},{\\\"name\\\":\\\"magnetometer\\\",\\\"type\\\":\\\"DataPieceArray<float>\\\",\\\""
    "offset\\\":72,\\\"size\\\":3}]}\",\"RF:Configuration:1\":\"data_layout\",\"RF:Data:1\":\"dat"
    "a_layout/size=84\",\"VRS_Original_Recordable_Name\":\"Facebook AR Accelerometer\"},\"configu"
    "ration\":{\"number_of_records\":1,\"start_time\":88112.775416333,\"end_time\":88112.77541633"
    "3},\"state\":{\"number_of_records\":1,\"start_time\":88112.775426958,\"end_time\":88112.7754"
    "26958},\"data\":{\"number_of_records\":60,\"start_time\":88112.781456875,\"end_time\":88113."
    "373714291}},{\"recordable_name\":\"Facebook AR Calibrated Device Motion Data\",\"recordable_"
    "id\":10004,\"instance_id\":1,\"tags\":{\"type\":\"motion_result\"},\"vrs_tag\":{\"DL:Data:1:"
    "0\":\"{\\\"data_layout\\\":[{\\\"name\\\":\\\"motion_present\\\",\\\"type\\\":\\\"DataPieceV"
    "alue<int32_t>\\\",\\\"offset\\\":0,\\\"default\\\":0},{\\\"name\\\":\\\"device_rotation_quat"
    "ernion\\\",\\\"type\\\":\\\"DataPieceValue<Point4Df>\\\",\\\"offset\\\":4},{\\\"name\\\":\\"
    "\"gravity\\\",\\\"type\\\":\\\"DataPieceValue<Point3Df>\\\",\\\"offset\\\":20},{\\\"name\\\""
    ":\\\"acceleration\\\",\\\"type\\\":\\\"DataPieceValue<Point3Df>\\\",\\\"offset\\\":32},{\\\""
    "name\\\":\\\"rotation\\\",\\\"type\\\":\\\"DataPieceValue<Point3Df>\\\",\\\"offset\\\":44},{"
    "\\\"name\\\":\\\"motion_timestamp\\\",\\\"type\\\":\\\"DataPieceValue<double>\\\",\\\"offset"
    "\\\":56},{\\\"name\\\":\\\"uncorrected_motion_timestamp\\\",\\\"type\\\":\\\"DataPieceValue<"
    "double>\\\",\\\"offset\\\":64,\\\"default\\\":0.0}]}\",\"RF:Data:1\":\"data_layout/size=72\""
    ",\"VRS_Original_Recordable_Name\":\"Facebook AR Calibrated Device Motion Data\"},\"configura"
    "tion\":{\"number_of_records\":1,\"start_time\":88112.775364583,\"end_time\":88112.775364583}"
    ",\"state\":{\"number_of_records\":1,\"start_time\":88112.775365458,\"end_time\":88112.775365"
    "458},\"data\":{\"number_of_records\":48,\"start_time\":88112.903192583,\"end_time\":88113.37"
    "5026666}},{\"recordable_name\":\"Facebook AR Snapshot\",\"recordable_id\":10005,\"instance_i"
    "d\":1,\"tags\":{\"type\":\"effect_data_snapshot\"},\"vrs_tag\":{\"DL:Data:1:0\":\"{\\\"data_"
    "layout\\\":[{\\\"name\\\":\\\"camera_info_present\\\",\\\"type\\\":\\\"DataPieceValue<int32_"
    "t>\\\",\\\"offset\\\":0,\\\"default\\\":0},{\\\"name\\\":\\\"width\\\",\\\"type\\\":\\\"Data"
    "PieceValue<uint32_t>\\\",\\\"offset\\\":4},{\\\"name\\\":\\\"height\\\",\\\"type\\\":\\\"Dat"
    "aPieceValue<uint32_t>\\\",\\\"offset\\\":8},{\\\"name\\\":\\\"device_rotation\\\",\\\"type\\"
    "\":\\\"DataPieceValue<int16_t>\\\",\\\"offset\\\":12},{\\\"name\\\":\\\"is_capturing_photo\\"
    "\",\\\"type\\\":\\\"DataPieceValue<Bool>\\\",\\\"offset\\\":14},{\\\"name\\\":\\\"is_recordi"
    "ng_video\\\",\\\"type\\\":\\\"DataPieceValue<Bool>\\\",\\\"offset\\\":15},{\\\"name\\\":\\\""
    "screen_scale\\\",\\\"type\\\":\\\"DataPieceValue<float>\\\",\\\"offset\\\":16},{\\\"name\\\""
    ":\\\"preview_size_width\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":20}"
    ",{\\\"name\\\":\\\"preview_size_height\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\"
    "\"offset\\\":24},{\\\"name\\\":\\\"effect_safe_area_insets_top\\\",\\\"type\\\":\\\"DataPiec"
    "eValue<uint32_t>\\\",\\\"offset\\\":28},{\\\"name\\\":\\\"effect_safe_area_insets_left\\\","
    "\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":32},{\\\"name\\\":\\\"effect_sa"
    "fe_area_insets_bottom\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>\\\",\\\"offset\\\":36},{"
    "\\\"name\\\":\\\"effect_safe_area_insets_right\\\",\\\"type\\\":\\\"DataPieceValue<uint32_t>"
    "\\\",\\\"offset\\\":40},{\\\"name\\\":\\\"face_tracking_present\\\",\\\"type\\\":\\\"DataPie"
    "ceValue<int32_t>\\\",\\\"offset\\\":44,\\\"default\\\":0},{\\\"name\\\":\\\"faces_count\\\","
    "\\\"type\\\":\\\"DataPieceValue<int32_t>\\\",\\\"offset\\\":48},{\\\"name\\\":\\\"raw_head_m"
    "atrix\\\",\\\"type\\\":\\\"DataPieceValue<Matrix4Df>\\\",\\\"offset\\\":52},{\\\"name\\\":\\"
    "\"rotation_degrees\\\",\\\"type\\\":\\\"DataPieceValue<int32_t>\\\",\\\"offset\\\":116},{\\"
    "\"name\\\":\\\"brightness\\\",\\\"type\\\":\\\"DataPieceValue<float>\\\",\\\"offset\\\":120}"
    ",{\\\"name\\\":\\\"timestamp\\\",\\\"type\\\":\\\"DataPieceValue<uint64_t>\\\",\\\"offset\\"
    "\":124},{\\\"name\\\":\\\"id\\\",\\\"type\\\":\\\"DataPieceValue<int32_t>\\\",\\\"offset\\\""
    ":132},{\\\"name\\\":\\\"confidenceScore\\\",\\\"type\\\":\\\"DataPieceValue<float>\\\",\\\"o"
    "ffset\\\":136},{\\\"name\\\":\\\"hand_tracking_present\\\",\\\"type\\\":\\\"DataPieceValue<i"
    "nt32_t>\\\",\\\"offset\\\":140,\\\"default\\\":0},{\\\"name\\\":\\\"depth_present\\\",\\\"ty"
    "pe\\\":\\\"DataPieceValue<int32_t>\\\",\\\"offset\\\":144,\\\"default\\\":0},{\\\"name\\\":"
    "\\\"map_width\\\",\\\"type\\\":\\\"DataPieceValue<int32_t>\\\",\\\"offset\\\":148},{\\\"name"
    "\\\":\\\"map_height\\\",\\\"type\\\":\\\"DataPieceValue<int32_t>\\\",\\\"offset\\\":152},{\\"
    "\"name\\\":\\\"segmentation_present\\\",\\\"type\\\":\\\"DataPieceValue<int32_t>\\\",\\\"off"
    "set\\\":156,\\\"default\\\":0},{\\\"name\\\":\\\"foreground_percent\\\",\\\"type\\\":\\\"Dat"
    "aPieceValue<float>\\\",\\\"offset\\\":160},{\\\"name\\\":\\\"mask_width\\\",\\\"type\\\":\\"
    "\"DataPieceValue<int32_t>\\\",\\\"offset\\\":164},{\\\"name\\\":\\\"mask_height\\\",\\\"type"
    "\\\":\\\"DataPieceValue<int32_t>\\\",\\\"offset\\\":168},{\\\"name\\\":\\\"motion_present\\"
    "\",\\\"type\\\":\\\"DataPieceValue<int32_t>\\\",\\\"offset\\\":172,\\\"default\\\":0},{\\\"n"
    "ame\\\":\\\"device_rotation_quaternion\\\",\\\"type\\\":\\\"DataPieceValue<Point4Df>\\\",\\"
    "\"offset\\\":176},{\\\"name\\\":\\\"gravity\\\",\\\"type\\\":\\\"DataPieceValue<Point3Df>\\"
    "\",\\\"offset\\\":192},{\\\"name\\\":\\\"acceleration\\\",\\\"type\\\":\\\"DataPieceValue<Po"
    "int3Df>\\\",\\\"offset\\\":204},{\\\"name\\\":\\\"rotation\\\",\\\"type\\\":\\\"DataPieceVal"
    "ue<Point3Df>\\\",\\\"offset\\\":216},{\\\"name\\\":\\\"motion_timestamp\\\",\\\"type\\\":\\"
    "\"DataPieceValue<double>\\\",\\\"offset\\\":228},{\\\"name\\\":\\\"uncorrected_motion_timest"
    "amp\\\",\\\"type\\\":\\\"DataPieceValue<double>\\\",\\\"offset\\\":236,\\\"default\\\":0.0},"
    "{\\\"name\\\":\\\"world_tracking_present\\\",\\\"type\\\":\\\"DataPieceValue<int32_t>\\\",\\"
    "\"offset\\\":244,\\\"default\\\":0},{\\\"name\\\":\\\"device_fov\\\",\\\"type\\\":\\\"DataPi"
    "eceValue<Point2Df>\\\",\\\"offset\\\":248},{\\\"name\\\":\\\"should_hide_model\\\",\\\"type"
    "\\\":\\\"DataPieceValue<Bool>\\\",\\\"offset\\\":256},{\\\"name\\\":\\\"tracked_object_trans"
    "form\\\",\\\"type\\\":\\\"DataPieceValue<Matrix4Df>\\\",\\\"offset\\\":257},{\\\"name\\\":\\"
    "\"view_matrix\\\",\\\"type\\\":\\\"DataPieceValue<Matrix4Df>\\\",\\\"offset\\\":321},{\\\"na"
    "me\\\":\\\"detected_plane_normal\\\",\\\"type\\\":\\\"DataPieceValue<Point3Df>\\\",\\\"offse"
    "t\\\":385},{\\\"name\\\":\\\"detected_plane_offset\\\",\\\"type\\\":\\\"DataPieceValue<float"
    ">\\\",\\\"offset\\\":397},{\\\"name\\\":\\\"optical_flow_present\\\",\\\"type\\\":\\\"DataPi"
    "eceValue<int32_t>\\\",\\\"offset\\\":401},{\\\"name\\\":\\\"audio_buffer_present\\\",\\\"typ"
    "e\\\":\\\"DataPieceValue<int32_t>\\\",\\\"offset\\\":405,\\\"default\\\":0},{\\\"name\\\":\\"
    "\"volume_level\\\",\\\"type\\\":\\\"DataPieceValue<float>\\\",\\\"offset\\\":409},{\\\"name"
    "\\\":\\\"touch_gestures_present\\\",\\\"type\\\":\\\"DataPieceValue<int32_t>\\\",\\\"offset"
    "\\\":413,\\\"default\\\":0},{\\\"name\\\":\\\"speed_present\\\",\\\"type\\\":\\\"DataPieceVa"
    "lue<int32_t>\\\",\\\"offset\\\":417,\\\"default\\\":0},{\\\"name\\\":\\\"speed_kph\\\",\\\"t"
    "ype\\\":\\\"DataPieceValue<float>\\\",\\\"offset\\\":421},{\\\"name\\\":\\\"volume_present\\"
    "\",\\\"type\\\":\\\"DataPieceValue<int32_t>\\\",\\\"offset\\\":425,\\\"default\\\":0},{\\\"n"
    "ame\\\":\\\"device_volume_level\\\",\\\"type\\\":\\\"DataPieceValue<int32_t>\\\",\\\"offset"
    "\\\":429},{\\\"name\\\":\\\"device_muted\\\",\\\"type\\\":\\\"DataPieceValue<Bool>\\\",\\\"o"
    "ffset\\\":433},{\\\"name\\\":\\\"object_tracking_present\\\",\\\"type\\\":\\\"DataPieceValue"
    "<int32_t>\\\",\\\"offset\\\":434},{\\\"name\\\":\\\"x_ray_present\\\",\\\"type\\\":\\\"DataP"
    "ieceValue<int32_t>\\\",\\\"offset\\\":438,\\\"default\\\":0},{\\\"name\\\":\\\"x_ray_confide"
    "nce\\\",\\\"type\\\":\\\"DataPieceValue<float>\\\",\\\"offset\\\":442},{\\\"name\\\":\\\"tag"
    "et_tracking_present\\\",\\\"type\\\":\\\"DataPieceValue<int32_t>\\\",\\\"offset\\\":446,\\\""
    "default\\\":0},{\\\"name\\\":\\\"taget_tracking_device_fov\\\",\\\"type\\\":\\\"DataPieceVal"
    "ue<Point2Df>\\\",\\\"offset\\\":450},{\\\"name\\\":\\\"target_global_transform\\\",\\\"type"
    "\\\":\\\"DataPieceValue<Matrix4Df>\\\",\\\"offset\\\":458},{\\\"name\\\":\\\"camera_global_t"
    "ransform\\\",\\\"type\\\":\\\"DataPieceValue<Matrix4Df>\\\",\\\"offset\\\":522},{\\\"name\\"
    "\":\\\"frame_brightness_present\\\",\\\"type\\\":\\\"DataPieceValue<int32_t>\\\",\\\"offset"
    "\\\":586,\\\"default\\\":0},{\\\"name\\\":\\\"brightness\\\",\\\"type\\\":\\\"DataPieceValue"
    "<float>\\\",\\\"offset\\\":590},{\\\"name\\\":\\\"capture_device_position\\\",\\\"type\\\":"
    "\\\"DataPieceString\\\",\\\"index\\\":0},{\\\"name\\\":\\\"raw_landmarks_2d\\\",\\\"type\\\""
    ":\\\"DataPieceVector<Point2Df>\\\",\\\"index\\\":1},{\\\"name\\\":\\\"raw_landmarks_3d\\\","
    "\\\"type\\\":\\\"DataPieceVector<Point3Df>\\\",\\\"index\\\":2},{\\\"name\\\":\\\"expression"
    "_offsets\\\",\\\"type\\\":\\\"DataPieceVector<Point3Df>\\\",\\\"index\\\":3},{\\\"name\\\":"
    "\\\"deformation_coefficients\\\",\\\"type\\\":\\\"DataPieceVector<float>\\\",\\\"index\\\":4"
    "},{\\\"name\\\":\\\"mesh_type\\\",\\\"type\\\":\\\"DataPieceString\\\",\\\"index\\\":5},{\\"
    "\"name\\\":\\\"blend_shape_weights\\\",\\\"type\\\":\\\"DataPieceVector<float>\\\",\\\"index"
    "\\\":6},{\\\"name\\\":\\\"center\\\",\\\"type\\\":\\\"DataPieceVector<Point2Df>\\\",\\\"inde"
    "x\\\":7},{\\\"name\\\":\\\"center_3d\\\",\\\"type\\\":\\\"DataPieceVector<Point3Df>\\\",\\\""
    "index\\\":8},{\\\"name\\\":\\\"hand_id\\\",\\\"type\\\":\\\"DataPieceVector<int32_t>\\\",\\"
    "\"index\\\":9},{\\\"name\\\":\\\"hand_index\\\",\\\"type\\\":\\\"DataPieceVector<int32_t>\\"
    "\",\\\"index\\\":10},{\\\"name\\\":\\\"map\\\",\\\"type\\\":\\\"DataPieceVector<uint8_t>\\\""
    ",\\\"index\\\":11},{\\\"name\\\":\\\"mask\\\",\\\"type\\\":\\\"DataPieceVector<uint8_t>\\\","
    "\\\"index\\\":12},{\\\"name\\\":\\\"tracking_confidence\\\",\\\"type\\\":\\\"DataPieceString"
    "\\\",\\\"index\\\":13},{\\\"name\\\":\\\"point_cloud\\\",\\\"type\\\":\\\"DataPieceVector<Po"
    "int3Df>\\\",\\\"index\\\":14},{\\\"name\\\":\\\"coord_x\\\",\\\"type\\\":\\\"DataPieceVector"
    "<float>\\\",\\\"index\\\":15},{\\\"name\\\":\\\"coord_y\\\",\\\"type\\\":\\\"DataPieceVector"
    "<float>\\\",\\\"index\\\":16},{\\\"name\\\":\\\"delta_x\\\",\\\"type\\\":\\\"DataPieceVector"
    "<float>\\\",\\\"index\\\":17},{\\\"name\\\":\\\"delta_y\\\",\\\"type\\\":\\\"DataPieceVector"
    "<float>\\\",\\\"index\\\":18},{\\\"name\\\":\\\"touch_gesture_id\\\",\\\"type\\\":\\\"DataPi"
    "eceVector<int32_t>\\\",\\\"index\\\":19},{\\\"name\\\":\\\"touch_gesture_location\\\",\\\"ty"
    "pe\\\":\\\"DataPieceVector<Point2Df>\\\",\\\"index\\\":20},{\\\"name\\\":\\\"touch_gesture_s"
    "tate\\\",\\\"type\\\":\\\"DataPieceVector<string>\\\",\\\"index\\\":21},{\\\"name\\\":\\\"to"
    "uch_gesture_type\\\",\\\"type\\\":\\\"DataPieceVector<string>\\\",\\\"index\\\":22},{\\\"nam"
    "e\\\":\\\"touch_gesture_extra_float\\\",\\\"type\\\":\\\"DataPieceVector<float>\\\",\\\"inde"
    "x\\\":23},{\\\"name\\\":\\\"touch_gesture_extra_location\\\",\\\"type\\\":\\\"DataPieceVecto"
    "r<Point2Df>\\\",\\\"index\\\":24},{\\\"name\\\":\\\"object_tracking_score\\\",\\\"type\\\":"
    "\\\"DataPieceVector<float>\\\",\\\"index\\\":25},{\\\"name\\\":\\\"object_tracking_label\\\""
    ",\\\"type\\\":\\\"DataPieceVector<string>\\\",\\\"index\\\":26},{\\\"name\\\":\\\"object_tra"
    "cking_x0\\\",\\\"type\\\":\\\"DataPieceVector<float>\\\",\\\"index\\\":27},{\\\"name\\\":\\"
    "\"object_tracking_y0\\\",\\\"type\\\":\\\"DataPieceVector<float>\\\",\\\"index\\\":28},{\\\""
    "name\\\":\\\"object_tracking_x1\\\",\\\"type\\\":\\\"DataPieceVector<float>\\\",\\\"index\\"
    "\":29},{\\\"name\\\":\\\"object_tracking_y1\\\",\\\"type\\\":\\\"DataPieceVector<float>\\\","
    "\\\"index\\\":30},{\\\"name\\\":\\\"object_tracking_unique_id\\\",\\\"type\\\":\\\"DataPiece"
    "Vector<string>\\\",\\\"index\\\":31},{\\\"name\\\":\\\"x_ray_category\\\",\\\"type\\\":\\\"D"
    "ataPieceString\\\",\\\"index\\\":32}]}\",\"RF:Data:1\":\"data_layout\",\"VRS_Original_Record"
    "able_Name\":\"Facebook AR Snapshot\"},\"configuration\":{\"number_of_records\":1,\"start_tim"
    "e\":88112.775348583,\"end_time\":88112.775348583},\"state\":{\"number_of_records\":1,\"start"
    "_time\":88112.775351791,\"end_time\":88112.775351791},\"data\":{\"number_of_records\":3,\"st"
    "art_time\":88113.292305458,\"end_time\":88113.36506}}]}";

struct FrameCompressionTest : testing::Test {};

template <class T>
void randomInit(vector<T>& buffer, uint32_t root) {
  for (size_t k = 0; k < buffer.size(); k++) {
    buffer[k] = static_cast<T>((k * root) ^ root);
  }
}

#define EXPECT_ZERO_OR_RETURN(operation__)                                \
  {                                                                       \
    int result = operation__;                                             \
    if (result != 0) {                                                    \
      XR_LOGW("{} failed: {}", #operation__, errorCodeToMessage(result)); \
      return result;                                                      \
    }                                                                     \
  }

int makeAndWriteFrame(
    WriteFileHandler& file,
    Compressor& compressor,
    vector<uint8_t>& input,
    size_t frameSize,
    size_t maxChunkSize,
    uint32_t root,
    CompressionPreset preset,
    size_t& totalSize) {
  input.resize(frameSize);
  randomInit(input, root);
  uint32_t writtenSize;
  EXPECT_ZERO_OR_RETURN(compressor.startFrame(input.size(), preset, writtenSize));
  size_t addedSize = 0;
  size_t remainingSize = frameSize;
  while (remainingSize > 0) {
    size_t chunkSize = std::min<size_t>(maxChunkSize, remainingSize);
    EXPECT_ZERO_OR_RETURN(
        compressor.addFrameData(file, input.data() + addedSize, chunkSize, writtenSize));
    addedSize += chunkSize;
    remainingSize -= chunkSize;
  }
  EXPECT_ZERO_OR_RETURN(compressor.endFrame(file, writtenSize));
  totalSize += writtenSize;
  return 0;
}

int readFrame(
    FileHandler& file,
    Decompressor& decompressor,
    vector<uint8_t>& output,
    size_t& maxReadSize) {
  size_t frameSize = 0;
  EXPECT_ZERO_OR_RETURN(decompressor.initFrame(file, frameSize, maxReadSize));
  output.resize(frameSize);
  EXPECT_ZERO_OR_RETURN(decompressor.readFrame(file, output.data(), frameSize, maxReadSize));
  return 0;
}

} // namespace

TEST_F(FrameCompressionTest, compressedFrames) {
  const std::string testPath = os::getTempFolder() + "compressedFrames.vrs";
  DiskFile file;

  Compressor compressor;
  ASSERT_EQ(file.create(testPath), 0);

  size_t totalSize = 0;
  vector<uint8_t> frame1, frame2, frame3, frame4;
  ASSERT_EQ(
      makeAndWriteFrame(
          file, compressor, frame1, 94562, 1024, 1687, CompressionPreset::ZstdFast, totalSize),
      0);
  ASSERT_EQ(
      makeAndWriteFrame(
          file, compressor, frame2, 1, 1024, 681, CompressionPreset::ZstdTight, totalSize),
      0);
  ASSERT_EQ(
      makeAndWriteFrame(
          file, compressor, frame3, 32, 7, 4654, CompressionPreset::ZstdLight, totalSize),
      0);
  ASSERT_EQ(
      makeAndWriteFrame(
          file, compressor, frame4, 2357, 4096, 465564, CompressionPreset::ZstdMedium, totalSize),
      0);
  vector<uint8_t> noise(42);
  randomInit(noise, 2369);
  EXPECT_EQ(file.write(noise.data(), noise.size()), 0);

  EXPECT_EQ(file.close(), 0);

  Decompressor decompressor;
  ASSERT_EQ(file.open(testPath), 0);
  size_t maxReadSize = static_cast<size_t>(os::getFileSize(testPath)) - noise.size();
  EXPECT_EQ(maxReadSize, totalSize);
  vector<uint8_t> readFrame1, readFrame2, readFrame3, readFrame4;
  ASSERT_EQ(readFrame(file, decompressor, readFrame1, maxReadSize), 0);
  ASSERT_EQ(readFrame(file, decompressor, readFrame2, maxReadSize), 0);
  ASSERT_EQ(readFrame(file, decompressor, readFrame3, maxReadSize), 0);
  ASSERT_EQ(readFrame(file, decompressor, readFrame4, maxReadSize), 0);
  EXPECT_EQ(frame1, readFrame1);
  EXPECT_EQ(frame2, readFrame2);
  EXPECT_EQ(frame3, readFrame3);
  EXPECT_EQ(frame4, readFrame4);
  EXPECT_EQ(maxReadSize, 0);

  os::remove(testPath);
}

TEST_F(FrameCompressionTest, truncatedFrame) {
  const std::string testPath = os::getTempFolder() + "truncatedFrame.vrs";
  DiskFile file;

  Compressor compressor;
  ASSERT_EQ(file.create(testPath), 0);

  size_t totalSize = 0;
  vector<uint8_t> frame;
  ASSERT_EQ(
      makeAndWriteFrame(
          file, compressor, frame, 500, 1500, 1687, CompressionPreset::ZstdLight, totalSize),
      0);

  EXPECT_EQ(file.close(), 0);

  Decompressor decompressor;
  ASSERT_EQ(file.open(testPath), 0);
  // prevent reading the last byte, which is still in the file
  size_t maxReadSize = static_cast<size_t>(os::getFileSize(testPath) - 1);
  EXPECT_EQ(maxReadSize, totalSize - 1);
  vector<uint8_t> readFrame1;
  ASSERT_EQ(readFrame(file, decompressor, readFrame1, maxReadSize), NOT_ENOUGH_DATA);

  os::remove(testPath);
}

TEST_F(FrameCompressionTest, truncatedFrames) {
  const std::string testPath = os::getTempFolder() + "truncatedFrames.vrs";
  DiskFile file;

  vector<size_t> truncationSizes = {1, 5, 25, 100};

  for (size_t truncateSize : truncationSizes) {
    Compressor compressor;
    ASSERT_EQ(file.create(testPath), 0);

    size_t totalSize = 0;
    vector<uint8_t> frame1, frame2;
    ASSERT_EQ(
        makeAndWriteFrame(
            file, compressor, frame1, 2500, 2500, 1687, CompressionPreset::ZstdFast, totalSize),
        0);
    // truncate last byte of the frame
    file.setPos(file.getPos() - static_cast<int64_t>(truncateSize));
    file.truncate();
    ASSERT_EQ(
        makeAndWriteFrame(
            file, compressor, frame2, 2357, 4096, 465564, CompressionPreset::ZstdMedium, totalSize),
        0);

    EXPECT_EQ(file.close(), 0);

    Decompressor decompressor;
    ASSERT_EQ(file.open(testPath), 0);
    size_t maxReadSize = static_cast<size_t>(os::getFileSize(testPath));
    EXPECT_EQ(maxReadSize, totalSize - truncateSize);
    vector<uint8_t> readFrame1, readFrame2;
    EXPECT_NE(readFrame(file, decompressor, readFrame1, maxReadSize), 0);
  }
  os::remove(testPath);
}

TEST_F(FrameCompressionTest, stringReadWrite) {
  const std::string testPath = os::getTempFolder() + "string.vrs";

  string writtenString;
  string readString;

  ASSERT_EQ(DiskFile::writeToFile(testPath, writtenString), 0);
  readString = "hello";
  EXPECT_EQ(DiskFile::readFromFile(testPath, readString), 0);
  EXPECT_EQ(writtenString, readString);

  writtenString = "some short string";
  ASSERT_EQ(DiskFile::writeToFile(testPath, writtenString), 0);
  readString = "hello";
  EXPECT_EQ(DiskFile::readFromFile(testPath, readString), 0);
  EXPECT_EQ(writtenString, readString);

  writtenString = kBigJson;
  ASSERT_EQ(DiskFile::writeToFile(testPath, writtenString), 0);
  readString = "hello";
  EXPECT_EQ(DiskFile::readFromFile(testPath, readString), 0);
  EXPECT_EQ(writtenString, readString);

  size_t l = writtenString.length() / 2;
  ASSERT_EQ(DiskFile::writeToFile(testPath, kBigJson, l), 0);
  vector<char> readBuffer(l);
  EXPECT_EQ(DiskFile::readFromFile(testPath, &readBuffer.front(), l), 0);
  EXPECT_EQ(readBuffer.size(), l);
  EXPECT_EQ(memcmp(readBuffer.data(), kBigJson, l), 0);

  os::remove(testPath);
}
