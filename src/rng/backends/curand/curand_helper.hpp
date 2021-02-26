/*******************************************************************************
* Copyright 2020-2021 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions
* and limitations under the License.
*
*
* SPDX-License-Identifier: Apache-2.0
*******************************************************************************/

/**
 * @file curand_helper.cpp : contains the implementation of all the routines
 * for CUDA backend
 */
#ifndef _MKL_RNG_CURAND_HELPER_HPP_
#define _MKL_RNG_CURAND_HELPER_HPP_
#include <curand.h>
#include <cuda.h>
#include <complex>
#include "oneapi/mkl/types.hpp"

namespace oneapi {
namespace mkl {
namespace rng {
namespace curand {

class curand_error : virtual public std::runtime_error {
protected:
    inline const char* curand_error_map(curandStatus_t error) {
        switch (error) {
            case CURAND_STATUS_SUCCESS: return "CURAND_STATUS_SUCCESS";

            case CURAND_STATUS_VERSION_MISMATCH: return "CURAND_STATUS_VERSION_MISMATCH";

            case CURAND_STATUS_NOT_INITIALIZED: return "CURAND_STATUS_NOT_INITIALIZED";

            case CURAND_STATUS_ALLOCATION_FAILED: return "CURAND_STATUS_ALLOCATION_FAILED";

            case CURAND_STATUS_TYPE_ERROR: return "CURAND_STATUS_TYPE_ERROR";

            case CURAND_STATUS_OUT_OF_RANGE: return "CURAND_STATUS_OUT_OF_RANGE";

            case CURAND_STATUS_LENGTH_NOT_MULTIPLE: return "CURAND_STATUS_LENGTH_NOT_MULTIPLE";

            case CURAND_STATUS_DOUBLE_PRECISION_REQUIRED:
                return "CURAND_STATUS_DOUBLE_PRECISION_REQUIRED";

            case CURAND_STATUS_LAUNCH_FAILURE: return "CURAND_STATUS_LAUNCH_FAILURE";

            case CURAND_STATUS_PREEXISTING_FAILURE: return "CURAND_STATUS_PREEXISTING_FAILURE";

            case CURAND_STATUS_INITIALIZATION_FAILED: return "CURAND_STATUS_INITIALIZATION_FAILED";

            case CURAND_STATUS_ARCH_MISMATCH: return "CURAND_STATUS_ARCH_MISMATCH";

            case CURAND_STATUS_INTERNAL_ERROR: return "CURAND_STATUS_INTERNAL_ERROR";

            default: return "<unknown>";
        }
    }

    int error_number; ///< Error number
public:
    /** Constructor (C++ STL string, curandStatus_t).
   *  @param msg The error message
   *  @param err_num error number
   */
    explicit curand_error(std::string message, curandStatus_t result)
            : std::runtime_error((message + std::string(curand_error_map(result)))) {
        error_number = static_cast<int>(result);
    }

    /** Destructor.
   *  Virtual to allow for subclassing.
   */
    virtual ~curand_error() throw() {}

    /** Returns error number.
   *  @return #error_number
   */
    virtual int getErrorNumber() const throw() {
        return error_number;
    }
};

class cuda_error : virtual public std::runtime_error {
protected:
    inline const char* cuda_error_map(cudaError_t error) {
        switch (error) {
            case cudaSuccess: return "cudaSuccess";

            case cudaErrorNotPermitted: return "cudaErrorNotPermitted";

            case cudaErrorIncompatibleDriverContext: return "cudaErrorIncompatibleDriverContext";

            case cudaErrorInvalidDevice: return "cudaErrorInvalidDevice";

            case cudaErrorInvalidValue: return "cudaErrorInvalidValue";

            case cudaErrorMemoryAllocation: return "cudaErrorMemoryAllocation";

            case cudaErrorLaunchOutOfResources: return "cudaErrorLaunchOutOfResources";

            default: return "<unknown>";
        }
    }
    int error_number; ///< error number
public:
    /** Constructor (C++ STL string, cudaError_t).
   *  @param msg The error message
   *  @param err_num Error number
   */
    explicit cuda_error(std::string message, cudaError_t error)
            : std::runtime_error((message + std::string(cuda_error_map(error)))) {
        error_number = static_cast<int>(error);
    }

    /** Destructor.
   *  Virtual to allow for subclassing.
   */
    virtual ~cuda_error() throw() {}

    /** Returns error number.
   *  @return #error_number
   */
    virtual int getErrorNumber() const throw() {
        return error_number;
    }
};

#define CUDA_CALL(func, err, ...)                                       \
    err = func(__VA_ARGS__);                                            \
    if (err != cudaSuccess) {                                           \
        throw cuda_error(std::string(#func) + std::string(" : "), err); \
    }

#define CURAND_CALL(func, status, ...)                                       \
    status = func(__VA_ARGS__);                                              \
    if (status != CURAND_STATUS_SUCCESS) {                                   \
        throw curand_error(std::string(#func) + std::string(" : "), status); \
    }

// Static template function oneapi::mkl::rng::curand::range_transform_fp (Buffer API)
//
// cuRAND has no built-in functionality to specify a custom range for sampling
// random numbers; `curandGenerateUniform` generates uniform random numbers on [0, 1).
// This function is used to convert to range [a, b).
//
// Supported types:
//      float
//      double
//
// Input arguments:
//      cl::sycl::queue& queue      - the queue to submit the kernel to
//      T a                         - range lower bound (inclusive)
//      T b                         - range upper bound (exclusive)
//      std::int64_t n              - the range (number of values to process)
//
// Output arguments:
//      cl::sycl::buffer<T, 1>& r   - buffer to store transformed random numbers
template <typename T>
static inline void range_transform_fp(cl::sycl::queue& queue, T a, T b, std::int64_t n,
                                      cl::sycl::buffer<T, 1>& r) {
    queue
        .submit([&](cl::sycl::handler& cgh) {
            auto acc = r.template get_access<cl::sycl::access::mode::read_write>(cgh);
            cgh.parallel_for(cl::sycl::range<1>(n),
                             [=](cl::sycl::id<1> id) { acc[id] = acc[id] * (b - a) + a; });
        })
        .wait_and_throw();
}

// Static template function oneapi::mkl::rng::curand::range_transform_fp (USM API)
//
// cuRAND has no built-in functionality to specify a custom range for sampling
// random numbers; `curandGenerateUniform` generates uniform random numbers on [0, 1).
// This function is used to convert to range [a, b).
//
// Supported types:
//      float
//      double
//
// Input arguments:
//      cl::sycl::queue& queue  - the queue to submit the kernel to
//      T a                     - range lower bound (inclusive)
//      T b                     - range upper bound (exclusive)
//      std::int64_t n          - the range (number of values to process)
//
// Output arguments:
//      T* r                    - pointer to memory to store transformed random numbers
//
// Returns:
//      cl::sycl::event         - event for the submitted to the engine's queue task
template <typename T>
static inline cl::sycl::event range_transform_fp(cl::sycl::queue& queue, T a, T b, std::int64_t n,
                                                 T* r) {
    return queue.submit([&](cl::sycl::handler& cgh) {
        cgh.parallel_for(cl::sycl::range<1>(n),
                         [=](cl::sycl::id<1> id) { r[id] = r[id] * (b - a) + a; });
    });
}

// Static template function oneapi::mkl::rng::curand::range_transform_fp_accurate (Buffer API)
//
// Same basic functionality as oneapi::mkl::rng::curand::range_transform_fp but with the explicit
// enforcement of output lying strictly within [a, b).
//
// Supported types:
//      float
//      double
//
// Input arguments:
//      cl::sycl::queue& queue      - the queue to submit the kernel to
//      T a                         - range lower bound (inclusive)
//      T b                         - range upper bound (exclusive)
//      std::int64_t n              - the range (number of values to process)
//
// Output arguments:
//      cl::sycl::buffer<T, 1>& r   - buffer to store transformed random numbers
template <typename T>
static inline void range_transform_fp_accurate(cl::sycl::queue& queue, T a, T b, std::int64_t n,
                                               cl::sycl::buffer<T, 1>& r) {
    queue
        .submit([&](cl::sycl::handler& cgh) {
            auto acc = r.template get_access<cl::sycl::access::mode::read_write>(cgh);
            cgh.parallel_for(cl::sycl::range<1>(n), [=](cl::sycl::id<1> id) {
                acc[id] = acc[id] * (b - a) + a;
                if (acc[id] < a) {
                    acc[id] = a;
                }
                else if (acc[id] > b) {
                    acc[id] = b;
                }
            });
        })
        .wait_and_throw();
}

// Static template function oneapi::mkl::rng::curand::range_transform_fp_accurate (USM API)
//
// Same basic functionality as oneapi::mkl::rng::curand::range_transform_fp but with the explicit
// enforcement of output lying strictly within [a, b).
//
// Supported types:
//      float
//      double
//
// Input arguments:
//      cl::sycl::queue& queue  - the queue to submit the kernel to
//      T a                     - range lower bound (inclusive)
//      T b                     - range upper bound (exclusive)
//      std::int64_t n          - the range (number of values to process)
//
// Output arguments:
//      T* r                    - pointer to memory to store transformed random numbers
//
// Returns:
//      cl::sycl::event         - event for the submitted to the engine's queue task
template <typename T>
static inline cl::sycl::event range_transform_fp_accurate(cl::sycl::queue& queue, T a, T b,
                                                          std::int64_t n, T* r) {
    return queue.submit([&](cl::sycl::handler& cgh) {
        cgh.parallel_for(cl::sycl::range<1>(n), [=](cl::sycl::id<1> id) {
            r[id] = r[id] * (b - a) + a;
            if (r[id] < a) {
                r[id] = a;
            }
            else if (r[id] > b) {
                r[id] = b;
            }
        });
    });
}

// Static template function oneapi::mkl::rng::curand::range_transform_int (Buffer API)
//
// cuRAND has no built-in functionality to specify a custom range for sampling
// random numbers; `curandGenerate' generates unsigned integers.
// This function is used to convert to range [a, b).
//
// Supported types:
//      std::int32_t
//      std::uint32_t
//
// Input arguments:
//      cl::sycl::queue& queue      - the queue to submit the kernel to
//      T a                         - range lower bound (inclusive)
//      T b                         - range upper bound (exclusive)
//      std::int64_t n              - the range (number of values to process)
//
// Output arguments:
//      cl::sycl::buffer<T, 1> r    - pointer to memory to store transformed random numbers
template <typename T>
static inline void range_transform_int(cl::sycl::queue& queue, T a, T b, std::int64_t n,
                                       cl::sycl::buffer<std::uint32_t, 1>& in,
                                       cl::sycl::buffer<T, 1>& out) {
    queue
        .submit([&](cl::sycl::handler& cgh) {
            auto acc_in = in.template get_access<cl::sycl::access::mode::read>(cgh);
            auto acc_out = out.template get_access<cl::sycl::access::mode::write>(cgh);
            cgh.parallel_for(cl::sycl::range<1>(n),
                             [=](cl::sycl::id<1> id) { acc_out[id] = a + acc_in[id] % (b - a); });
        })
        .wait_and_throw();
}

// Static template function oneapi::mkl::rng::curand::range_transform_int (USM API)
//
// cuRAND has no built-in functionality to specify a custom range for sampling
// random numbers; `curandGenerate' generates unsigned integers.
// This function is used to convert to range [a, b).
//
// Supported types:
//      std::int32_t
//      std::uint32_t
//
// Input arguments:
//      cl::sycl::queue& queue  - the queue to submit the kernel to
//      T a                     - range lower bound (inclusive)
//      T b                     - range upper bound (exclusive)
//      std::int64_t n          - the range (number of values to process)
//
// Output arguments:
//      std::uint32_t* r        - pointer to memory to store transformed random numbers
//
// Returns:
//      cl::sycl::event         - event for the submitted to the engine's queue task
template <typename T>
static inline cl::sycl::event range_transform_int(cl::sycl::queue& queue, T a, T b, std::int64_t n,
                                                  std::uint32_t* in, T* out) {
    return queue.submit([&](cl::sycl::handler& cgh) {
        cgh.parallel_for(cl::sycl::range<1>(n),
                         [=](cl::sycl::id<1> id) { out[id] = a + in[id] % (b - a); });
    });
}

// Static template function oneapi::mkl::rng::curand::sample_bernoulli (Buffer API)
//
// cuRAND has no built-in functionality to sample from a Bernoulli distribution.
// The implementation here uses uniformly-generated random numbers and returns
// the corresponding Bernoulli distribution based on a probability.
//
// Supported types:
//      std::int32_t
//      std::uint32_t
//
// Input arguments:
//      cl::sycl::queue& queue          - the queue to submit the kernel to
//      float p                         - success probablity of a trial
//      std::int64_t n                  - the range (number of values to process)
//      cl::sycl::buffer<float, 1> in   - buffer containing uniformly-generated random numbers
//
// Output arguments:
//      cl::sycl::buffer<T, 1>& out     - buffer to store Bernoulli numbers
template <typename T>
static inline void sample_bernoulli_from_uniform(cl::sycl::queue& queue, float p, std::int64_t n,
                                                 cl::sycl::buffer<float, 1> in,
                                                 cl::sycl::buffer<T, 1>& out) {
    queue
        .submit([&](cl::sycl::handler& cgh) {
            auto acc_in = in.template get_access<cl::sycl::access::mode::read>(cgh);
            auto acc_out = out.template get_access<cl::sycl::access::mode::write>(cgh);
            cgh.parallel_for(cl::sycl::range<1>(n),
                             [=](cl::sycl::id<1> id) { acc_out[id] = acc_in[id] < p; });
        })
        .wait_and_throw();
}

// Static template function oneapi::mkl::rng::curand::sample_bernoulli (USM API)
//
// cuRAND has no built-in functionality to sample from a Bernoulli distribution.
// The implementation here uses uniformly-generated random numbers and returns
// the corresponding Bernoulli distribution based on a probability.
//
// Supported types:
//      std::int32_t
//      std::uint32_t
//
// Input arguments:
//      cl::sycl::queue& queue  - the queue to submit the kernel to
//      float p                 - success probablity of a trial
//      std::int64_t n          - the range (number of values to process)
//      float* in               - buffer containing uniformly-generated random numbers
//
// Output arguments:
//      T* out                  - pointer to memory to store Bernoulli numbers
//
// Returns:
//      cl::sycl::event         - event for the submitted to the engine's queue task
template <typename T>
static inline cl::sycl::event sample_bernoulli_from_uniform(cl::sycl::queue& queue, float p,
                                                            std::int64_t n, float* in, T* out) {
    return queue.submit([&](cl::sycl::handler& cgh) {
        cgh.parallel_for(cl::sycl::range<1>(n), [=](cl::sycl::id<1> id) { out[id] = in[id] < p; });
    });
}

} // namespace curand
} // namespace rng
} // namespace mkl
} // namespace oneapi

#endif // _MKL_RNG_CURAND_HELPER_HPP_
