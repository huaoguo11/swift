//===--- ImageInspectionELF.cpp - ELF image inspection --------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2016 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file includes routines that interact with ld*.so on ELF-based platforms
// to extract runtime metadata embedded in dynamically linked ELF images
// generated by the Swift compiler.
//
//===----------------------------------------------------------------------===//

#if defined(__ELF__) || defined(__ANDROID__)

#include "ImageInspection.h"
#include <elf.h>
#include <link.h>
#include <dlfcn.h>
#include <string.h>

using namespace swift;

/// The symbol name in the image that identifies the beginning of the
/// protocol conformances table.
static const char ProtocolConformancesSymbol[] =
  ".swift2_protocol_conformances_start";
/// The symbol name in the image that identifies the beginning of the
/// type metadata record table.
static const char TypeMetadataRecordsSymbol[] =
  ".swift2_type_metadata_start";

/// Context arguments passed down from dl_iterate_phdr to its callback.
struct InspectArgs {
  /// Symbol name to look up.
  const char *symbolName;
  /// Callback function to invoke with the metadata block.
  void (*addBlock)(const void *start, uintptr_t size);
};

static int iteratePHDRCallback(struct dl_phdr_info *info,
                               size_t size, void *data) {
  const InspectArgs *inspectArgs = reinterpret_cast<const InspectArgs *>(data);
  void *handle;
  if (!info->dlpi_name || info->dlpi_name[0] == '\0') {
    handle = dlopen(nullptr, RTLD_LAZY);
  } else {
    handle = dlopen(info->dlpi_name, RTLD_LAZY | RTLD_NOLOAD);
  }

  if (!handle) {
    // Not a shared library.
    return 0;
  }

  const char *conformances =
    reinterpret_cast<const char*>(dlsym(handle, inspectArgs->symbolName));

  if (!conformances) {
    // if there are no conformances, don't hold this handle open.
    dlclose(handle);
    return 0;
  }

  // Extract the size of the conformances block from the head of the section.
  uint64_t conformancesSize;
  memcpy(&conformancesSize, conformances, sizeof(conformancesSize));
  conformances += sizeof(conformancesSize);

  inspectArgs->addBlock(conformances, conformancesSize);

  dlclose(handle);
  return 0;
}

void swift::initializeProtocolConformanceLookup() {
  // Search the loaded dls. This only searches the already
  // loaded ones.
  // FIXME: Find a way to have this continue to happen for dlopen-ed images.
  // rdar://problem/19045112
  InspectArgs ProtocolConformanceArgs = {
    ProtocolConformancesSymbol,
    addImageProtocolConformanceBlockCallback
  };
  dl_iterate_phdr(iteratePHDRCallback, &ProtocolConformanceArgs);
}

void swift::initializeTypeMetadataRecordLookup() {
  InspectArgs TypeMetadataRecordArgs = {
    TypeMetadataRecordsSymbol,
    addImageTypeMetadataRecordBlockCallback
  };
  dl_iterate_phdr(iteratePHDRCallback, &TypeMetadataRecordArgs);
}

#endif // defined(__ELF__) || defined(__ANDROID__)
