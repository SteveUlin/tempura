// Async Task Execution System - Sender/Receiver Model
//
// This file provides a compile-time task expression system based on the P2300
// sender/receiver model. Senders are types that describe asynchronous work
// without executing it. Each sender exposes its value types through a
// `ValueTypes` alias for compile-time type deduction.

#pragma once

#include "task/stop_token.h"
#include "task/env.h"
#include "task/concepts.h"
#include "task/receivers.h"
#include "task/just.h"
#include "task/then.h"
#include "task/upon_error.h"
#include "task/upon_stopped.h"
#include "task/let_value.h"
#include "task/let_error.h"
#include "task/let_stopped.h"
#include "task/on.h"
#include "task/repeat_effect.h"
#include "task/algorithms.h"
#include "task/schedulers.h"
#include "task/when_all.h"
#include "task/when_any.h"
#include "task/split.h"
#include "task/bulk.h"
