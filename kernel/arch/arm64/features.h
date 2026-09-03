/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * File: kernel/arch/arm64/features.h
 *
 * Purpose:
 *   Declares ARM64 feature-detection and configuration interfaces.
 *
 * Design:
 *   Callers must distinguish exposed architectural support from an enabled,
 *   tested runtime mechanism.
 */

#ifndef LETTUCE_ARCH_FEATURES_H
#define LETTUCE_ARCH_FEATURES_H

#pragma once

#include <stdbool.h>
#include <stdint.h>

bool lettuce_arch_has_pac(void);
bool lettuce_arch_has_mte(void);
bool lettuce_arch_has_poe(void);

void lettuce_arch_features_probe_and_print(void);

#endif /* LETTUCE_ARCH_FEATURES_H */
