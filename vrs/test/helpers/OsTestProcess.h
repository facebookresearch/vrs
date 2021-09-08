// Facebook Technologies, LLC Proprietary and Confidential.

#include <string>

#include <boost/process/io.hpp>
#include <boost/process/system.hpp>

/// To test command line tools
/// Add a definition for the location of the tool in the BUCK file, as an env variable.
/// See definition of TESTEDTOOL_EXE used to test this class in os/BUCK.

namespace vrs::os {

namespace bp = boost::process;

class OsTestProcess {
 public:
  OsTestProcess(const char* processName) : processName_{processName} {}
  bool start(std::string arg, bp::ipstream& sout) {
    return start(arg, &sout);
  }
  bool start(std::string arg, bp::ipstream* sout = nullptr);

  std::string getJsonOutput(bp::ipstream& output) const;

  int runProcess();

 private:
  std::unique_ptr<bp::child> process;
  const char* processName_;

  bool looksLikeAFbCentOSServer();

  bool findBinary(std::string& inOutName);
};

} // namespace vrs::os
