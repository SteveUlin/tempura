// Async Task Execution System - Sender/Receiver Model
//
// This file provides a compile-time task expression system based on the P2300
// sender/receiver model. Senders are types that describe asynchronous work
// without executing it. Each sender exposes its value types through a
// `ValueTypes` alias for compile-time type deduction.

#pragma once

// Core concepts and type traits
#include "concepts.h"

// Receiver implementations
#include "receivers.h"

// Fundamental sender types
#include "just.h"

// Sender adaptors
#include "then.h"
#include "upon_error.h"
#include "let_value.h"
#include "let_error.h"

// Algorithms
#include "algorithms.h"

namespace tempura {

// ============================================================================
// Parallel Composition
// ============================================================================

// TODO: Implement whenAll sender

// TODO: Implement whenAny sender

// ============================================================================
// Scheduler Implementations
// ============================================================================

// TODO: Implement InlineScheduler

// TODO: Implement ThreadPoolScheduler

// TODO: Implement SingleThreadScheduler

// ============================================================================
// Stop Token Support
// ============================================================================

// TODO: Implement StopToken

// TODO: Implement StopSource

}  // namespace tempura
