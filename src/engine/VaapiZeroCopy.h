#pragma once

// Preview zero-copy VAAPI opt-in. Resolved from DRIFT_VAAPI_ZEROCOPY (wins, unless
// the value is "0") then QSettings("preview/vaapiZeroCopy"). The settings half is
// cached after the first call so the GL import path never touches QSettings per frame.
//
// applyVaapiZeroCopyXcbEgl() must run after organization/application names are set
// and before QApplication, so Qt's xcb plugin can pick EGL instead of GLX.

namespace drift {

bool vaapiZeroCopyEnabled();
void applyVaapiZeroCopyXcbEgl();

} // namespace drift
