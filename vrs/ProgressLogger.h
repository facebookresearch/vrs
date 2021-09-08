// Facebook Technologies, LLC Proprietary and Confidential.

#pragma once

#include <string>

namespace vrs {

using std::string;

/// \brief The ProgressLogger class
/// Helper class to implement status logging when opening a VRS file
/// By default, logs to use XR_LOGI and XR_LOGE, but can be easily overwritten
/// to log anywhere. By default, this class never requests to stop the operation, but that can be
/// overridden.
class ProgressLogger {
 public:
  static constexpr double kDefaultUpdateDelay = 2;
  /// By default, only logs every 2 seconds, and after 2 seconds, so opening from a file will be
  /// silent, unless a slow reindexing operation is required.
  /// @param detailedProgress: pass true to log every new step, regardless of timing.
  /// @param updateDelay: time in seconds between updates.
  ProgressLogger(bool detailedProgress = false, double updateDelay = kDefaultUpdateDelay);
  virtual ~ProgressLogger();

  /// Set the number of steps anticipated, if expecting more than one step.
  /// The step counter is incremented each time a new step is logged.
  /// @param stepCount: total number of steps anticipated.
  virtual void setStepCount(int stepCount);
  /// Force logging at every step.
  /// @param detailedProgress: true if logging should be done of every new step.
  virtual void setDetailedProgress(bool detailedProgress);
  /// Get if progress details logging is enabled.
  /// @return True if progress details logging is enabled.
  bool getDetailedProgress() const {
    return detailedProgress_;
  }

  /// Start logging a new step.
  /// @param stepName: the name of the step.
  /// @param progress: the current progress in the step.
  /// @param maxProgress: the max value of the progress counter in the step.
  /// @return True if the operation should continue, false if it should be cancelled.
  virtual bool logNewStep(const string& stepName, size_t progress = 0, size_t maxProgress = 100);

  /// Log progress of a step that has an internal progress counter.
  /// logNewStep() should always be called first.
  /// @param stepName: the name of the step.
  /// @param progress: the current progress in the step.
  /// @param maxProgress: the max value of the progress counter in the step.
  /// @return True if the operation should continue, false if it should be cancelled.
  bool logProgress(const string& stepName, size_t progress = 0, size_t maxProgress = 100) {
    return logProgress(stepName, progress, maxProgress, false);
  }
  /// Convenience method for when a status is signed.
  /// Note that values are expected to be positive anyways!
  bool logProgress(const string& stepName, int64_t progress, int64_t maxProgress) {
    return logProgress(
        stepName, static_cast<size_t>(progress), static_cast<size_t>(maxProgress), false);
  }

  /// Log that a step is completed, with a specific status.
  /// @param stepName: the name of the step.
  /// @param status: 0 on success, otherwise, the step is considered failed.
  /// @return True if the operation should continue, false if it should be cancelled.
  virtual bool logStatus(const string& stepName, int status = 0);

  /// Log that an operation was performed in a specific duration.
  /// @param operationName: text describing the operation.
  /// @param duration: number of seconds the operation lasted.
  /// @param precision: precision to use when printing the duration.
  /// @return True if the operation should continue, false if it should be cancelled.
  virtual bool logDuration(const string& operationName, double duration, int precision = 1);

 protected:
  /// @param stepName: the name of the step.
  /// @param progress: the current progress in the step.
  /// @param maxProgress: the max value of the progress counter in the step.
  /// @param newStep: tells if the step is a new step (for internal use).
  /// @return True if the operation should continue, false if it should be cancelled.
  virtual bool
  logProgress(const string& stepName, size_t progress, size_t maxProgress, bool newStep);
  /// Log an actual message. This is called when some text needs to actually be logged after all the
  /// filtering logic has been applied.
  /// @param message: the text message to log.
  virtual void logMessage(const string& message);
  /// Log an error message. This is called when some text needs to actually be logged after all the
  /// filtering logic has been applied.
  /// @param message: the error text message to log.
  virtual void logError(const string& message);
  /// Callback to update the current step's progress, for instance, when displaying a progress bar.
  /// @param progress: the current progress in the step.
  /// @param maxProgress: the max value of the progress counter in the step.
  virtual void updateStep(size_t progress = 0, size_t maxProgress = 100);
  /// Callback to schedule the time of the next text update.
  virtual void updateNextProgessTime();
  /// Callback to tell if the operation should stop or keep going.
  /// Override this method to check if the operation should be cancelled, and probably cache
  /// the result.
  /// @return True if the operation should continue, false if it should be cancelled.
  virtual bool shouldKeepGoing();

 protected:
  bool detailedProgress_;
  double updateDelay_;
  int stepNumber_;
  int stepCount_;
  double nextProgressTime_;
};

class SilentLogger : public ProgressLogger {
 public:
  ~SilentLogger();
  virtual bool logProgress(const string&, size_t = 0, size_t = 100, bool = false) {
    return true;
  }
  virtual bool logStatus(const string&, int = 0) {
    return true;
  }
};

} // namespace vrs
