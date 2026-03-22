#pragma once

#define CL_HPP_ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION  120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120

#if defined(__has_include)
#if __has_include(<CL/opencl.hpp>)
#include <CL/opencl.hpp>
#elif __has_include(<CL/cl2.hpp>)
#include <CL/cl2.hpp>
#else
#error "OpenCL C++ headers not found: expected <CL/opencl.hpp> or <CL/cl2.hpp>"
#endif
#else
#include <CL/opencl.hpp>
#endif
