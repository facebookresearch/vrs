// Facebook Technologies, LLC Proprietary and Confidential.

#pragma once

#include "StreamId.h"

/// RecordableId is now deprecated as a name, but the concept is unchanged.
/// Please use StreamId everywhere RecordableId was used.
///
/// Why are we doing this?
///
/// We're open sourcing VRS, which gives us a last chance to clean-up VRS APIs before going public.
/// With megarepo, it is possible to make code refactors that would have been virtually impossible
/// just 6 months ago. Expect to see VRS mutate quite a bit until then.
///
/// RecordableId is a name that has caused confusion, in particular because pyvrs has adopted the
/// term streamId everywhere. It makes sense, because pyvrs is mostly used for data consumption (and
/// was read-only for a long time), and there was no notion of "Recordable" in pyvrs, which made the
/// name hollow. Since RecordableId objects really are streams identifiers, both while recording and
/// during playback, "StreamId" makes significantly more sense and allows us to make the C++ and
/// pyvrs terminologies converge on this basic concept.
///
/// This RecordableId.h header is now meant to allowing client teams to keep using RecordableId
/// during a transition phase. We're in the process of updating all the core vrs libraries to use
/// StreamId. We plan on providing codemode scripts to help client teams update their own code, and
/// eventually, delete this file entirely.

namespace vrs {
using RecordableId [[deprecated("Please use vrs::StreamId instead of vrs::RecordableId.")]] =
    vrs::StreamId;
}
