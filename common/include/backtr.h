#include <cxxabi.h>
#include <cstdio>
#include <cstdlib>
#include <backtrace.h>
#include "log.h"

extern "C" void init_back_trace(const char *filename);
extern "C" void print_back_trace();

void *__bt_state = nullptr;

int bt_callback(void *, uintptr_t, const char *filename, int lineno, const char *function) {
  /// demangle function name
  const char *func_name = function;
  int status;
  char *demangled = abi::__cxa_demangle(function, nullptr, nullptr, &status);
  if (status == 0) {
    func_name = demangled;
  }

  /// print
  pmlLog(pml::LOG_CRITICAL) << filename << ":" << lineno << " in function " << func_name;
  return 0;
}

void bt_error_callback(void *, const char *msg, int errnum) {
  pmlLog(pml::LOG_CRITICAL) << "Error " << errnum << " occurred when getting the stacktrace: " << msg;
}

void bt_error_callback_create(void *, const char *msg, int errnum) {
  pmlLog(pml::LOG_CRITICAL) << "Error " << errnum << " occurred when initializing the stacktrace: " << msg;
}

void init_back_trace(const char *filename) {
  __bt_state = backtrace_create_state(filename, 0, bt_error_callback_create, nullptr);
}

void print_back_trace() {
  if (!__bt_state) { /// make sure init_back_trace() is called
    printf("Make sure init_back_trace() is called before calling print_stack_trace()\n");
    abort();
  }
  backtrace_full((backtrace_state *) __bt_state, 0, bt_callback, bt_error_callback, nullptr);
}