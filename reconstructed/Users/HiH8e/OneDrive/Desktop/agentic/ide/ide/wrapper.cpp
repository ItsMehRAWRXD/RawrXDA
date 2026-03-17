#include "ide_wrapper.hpp"

#include "main.hpp" // TODO: replace with the header that exposes ide::IDE

namespace ide {

IDEWrapper::IDEWrapper() : ide_(std::make_unique<IDE>()) {}
IDEWrapper::~IDEWrapper() = default;

IDEWrapper::IDEWrapper(IDEWrapper&&) noexcept = default;
IDEWrapper& IDEWrapper::operator=(IDEWrapper&&) noexcept = default;

int IDEWrapper::run() {
    if (!ide_) {
        ide_ = std::make_unique<IDE>();
    }
    return ide_->runInteractive();
}

} // namespace ide
