#pragma once

#include <memory>
#include "../transport/ITransport.h"

std::unique_ptr<ITransport> createBleDriver();
