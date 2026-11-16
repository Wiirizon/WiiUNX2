// WiiUNX2 GPL-3.0-or-later. Console updater entry points.
#pragma once
#include "UpdateCore.h"
#include <atomic>

namespace nx {
struct UpdateCheckResult {
    bool success=false,available=false;
    std::string version,message;
    UpdateSelection selection;
};
struct UpdateOperationResult {
    bool success=false;
    std::string message;
};

UpdateCheckResult checkLatestUpdate(const std::string &currentVersion,const std::atomic<bool> *cancel=nullptr);
UpdateOperationResult installUpdate(const std::string &currentVersion,const std::string &version,
                                    const UpdateSelection &selection,const std::atomic<bool> *cancel=nullptr);
}
