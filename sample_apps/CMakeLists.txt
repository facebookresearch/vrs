# Copyright (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

set(SAMPLE_PLAYBACK_APP_SRCS "SamplePlaybackApp.cpp")
add_executable(sample_playback_app ${SAMPLE_PLAYBACK_APP_SRCS})
target_link_libraries(sample_playback_app
    PUBLIC
        vrs
        vrs_logging
        vrs_os
)


set(SAMPLE_RECORDING_APP_SRCS "SampleRecordingApp.cpp")
add_executable(sample_recording_app ${SAMPLE_RECORDING_APP_SRCS})
target_link_libraries(sample_recording_app
    PUBLIC
        vrs
        vrs_logging
        vrs_os
)

#set(FOLDER_TO_VRS_SRCS "FolderToVRS.cpp")
#add_executable(FolderToVRS ${FOLDER_TO_VRS_SRCS})
#target_link_libraries(FolderToVRS
#    PUBLIC
#        vrs
#        vrs_logging
#        vrs_os
#)

#set(VERBATIM_JPG_PNG_TO_VRS_SRCS "VerbatimJpgPngToVRS.cpp")
#add_executable(VerbatimJpgPngToVRS ${VERBATIM_JPG_PNG_TO_VRS_SRCS})
#target_link_libraries(VerbatimJpgPngToVRS
#    PUBLIC
#        vrs
#        vrs_logging
#        vrs_os
#)
