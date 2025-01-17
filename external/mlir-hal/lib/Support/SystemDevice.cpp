//===- SystemDevice.cpp - System device utilies ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the system device utilities.
//
//===----------------------------------------------------------------------===//

#include "mlir/Support/SystemDevice.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "support-system-device"

using namespace mlir;
using namespace mlir::mhal;

SystemDevice::SystemDevice(Type _type) : type(_type) {}

static const char *getDeviceTypeStr(SystemDevice::Type type) {
  switch (type) {
  case SystemDevice::Type::ECPU:
    return "CPU";
  case SystemDevice::Type::EGPU:
    return "GPU";
  case SystemDevice::Type::ENPU:
    return "NPU";
  default:
    break;
  }
  return "ALT";
}

LogicalResult SystemDevice::parse(llvm::StringRef arch) {
  llvm::StringRef maybeTriple, remainder;
  std::tie(maybeTriple, remainder) = arch.split(':');
  if (maybeTriple.contains('-')) {
    // First part is a triple, ignore it
    llvmTriple = maybeTriple;
  } else {
    llvmTriple = "";
    remainder = arch;
  }

  llvm::StringRef rawFeatures;
  std::tie(chip, rawFeatures) = remainder.split(':');

  if (!rawFeatures.empty()) {
    llvm::SmallVector<llvm::StringRef, 1> featureTokens;
    rawFeatures.split(featureTokens, ':'); // check for CSV
    for (llvm::StringRef feature : featureTokens) {
      feature = feature.trim();
      if (!feature.empty()) {
        features.insert_or_assign(feature.drop_back(), feature.back() == '+');
      }
    }
  }
  return success();
}

// Note: Device `this` is a real device (must specify `chip` and `type`)
// and `that` must be a subset of the spec.
bool SystemDevice::isCompatible(const SystemDevice &that) const {
  bool matches = (type == that.type) &&
                 (llvmTriple.empty() || that.llvmTriple.empty() ||
                  llvmTriple == that.llvmTriple) &&
                 (that.chip.empty() || chip == that.chip);
  if (matches && !that.features.empty()) {
    // If `that` does not specify a feature, it is compatible
    for (const llvm::StringMapEntry<bool> &feature : that.features) {
      llvm::StringRef key = feature.getKey();
      matches &= (features.count(key) != 0 &&
                  features.lookup(key) == feature.getValue());
    }
  }
  return matches;
}

std::string SystemDevice::getArch() const {
  std::string arch;
  arch.reserve(llvmTriple.size() + chip.size() + features.size() * 10);
  if (!llvmTriple.empty()) {
    arch = llvmTriple.str();
    arch += ':';
  }
  if (!chip.empty())
    arch += chip.str();

  for (const auto &entry : features) {
    arch += ':';
    arch += entry.getKey().str();
    arch += (entry.getValue() ? '+' : '-');
  }

  return arch;
}

void SystemDevice::dump() const {
  llvm::errs() << "Device(" << getDeviceTypeStr(type) << ") x " << count << "\n"
               << "\n  triple = " << llvmTriple << "\n  chip = " << chip;
  if (!features.empty()) {
    llvm::errs() << "\n  features = ";
    llvm::interleave(
        features, llvm::errs(),
        [](const llvm::StringMapEntry<bool> &entry) {
          llvm::errs() << entry.getKey() << (entry.getValue() ? "+" : "-");
        },
        ":");
  }
  if (!properties.empty()) {
    llvm::errs() << "\n  {\n";
    for (const auto &pair : properties) {
      llvm::errs() << "    " << pair.first() << " = " << pair.second << "\n";
    }
    llvm::errs() << "}\n";
  }
}
